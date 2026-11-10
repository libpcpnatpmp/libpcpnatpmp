/*
 *------------------------------------------------------------------
 * test_epoch_check.c
 *
 * November 10, 2026
 *
 *------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef WIN32
#include <winsock2.h>

#include <windows.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "pcpnatpmp.h"

#include "pcp_client_db.h"
#include "pcp_event_handler.h"
#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

// The test plays the server. When the client's mappings are far from
// renewal, the client must ask the server for its epoch about
// PCP_EPOCH_CHECK_S later, and act on a restart the answer shows.
#define HEADER_LEN 24
#define MAP_LEN 36
#define NATPMP_MAP_LEN 12
// renewal at half the lifetime: 3600 s is past PCP_EPOCH_CHECK_AFTER_S,
// 1000 s is not
#define LONG_LIFETIME 7200
#define SHORT_LIFETIME 2000

static PCP_SOCKET server;
static struct sockaddr_storage client;
// the protocol the server speaks: 2 for PCP, 0 for NAT-PMP
static uint8_t version;
static uint32_t epoch, lifetime;
static int seen_map, seen_announce;
// a silent server counts requests but answers none
static int silent;
// the result code of the server's ANNOUNCE answers; with an error, the
// server falls silent after sending it
static uint8_t announce_result;

// the port open_server() got, chosen by the system so tests running at the
// same time do not collide
static int server_port;

static PCP_SOCKET open_server(void) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    PCP_SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    TEST(sock != PCP_INVALID_SOCKET);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    TEST(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    TEST(getsockname(sock, (struct sockaddr *)&addr, &len) == 0);
    server_port = ntohs(addr.sin_port);
    return sock;
}

static void reply(const uint8_t *resp, int len) {
    TEST(sendto(server, (const char *)resp, len, 0,
                (const struct sockaddr *)&client,
                SA_LEN((struct sockaddr *)&client)) == len);
}

// Answers a PCP ANNOUNCE or MAP request with success.
static void answer_pcp(const uint8_t *req, int len) {
    uint8_t resp[HEADER_LEN + MAP_LEN];
    uint32_t e = htonl(epoch), l = htonl(lifetime);
    uint8_t opcode = req[1] & 0x7f;
    int resp_len = opcode == 1 ? HEADER_LEN + MAP_LEN : HEADER_LEN;

    TEST(len >= resp_len);
    memset(resp, 0, sizeof(resp));
    resp[0] = 2;
    resp[1] = (uint8_t)(0x80 | opcode);
    if (opcode == 1) {
        memcpy(resp + 4, &l, 4);
        memcpy(resp + HEADER_LEN, req + HEADER_LEN, MAP_LEN);
        resp[HEADER_LEN + 18] = 0x9c; // external port 40000
        resp[HEADER_LEN + 19] = 0x40;
    }
    if (opcode == 0) {
        resp[3] = announce_result;
        silent = announce_result != 0;
    }
    memcpy(resp + 8, &e, 4);
    reply(resp, resp_len);
}

// Answers a NAT-PMP external address or MAP request with success.
static void answer_natpmp(const uint8_t *req) {
    uint8_t resp[16];
    uint32_t e = htonl(epoch), l = htonl(lifetime);

    memset(resp, 0, sizeof(resp));
    resp[1] = (uint8_t)(0x80 | req[1]);
    memcpy(resp + 4, &e, 4);
    if (req[1] == 0) {
        // external address 9.9.9.9
        resp[8] = resp[9] = resp[10] = resp[11] = 9;
        reply(resp, 12);
        return;
    }
    memcpy(resp + 8, req + 4, 2); // internal port
    resp[10] = 0x9c;              // external port 40000
    resp[11] = 0x40;
    memcpy(resp + 12, &l, 4);
    reply(resp, 16);
}

// Runs the client for `ms` milliseconds, answering and counting requests.
static void pump(pcp_ctx_t *ctx, int ms) {
    uint8_t req[1100];

    for (; ms > 0; ms -= 10) {
        struct timeval tout = {0, 10000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            socklen_t len = sizeof(client);
            int got = (int)recvfrom(server, (char *)req, sizeof(req), 0,
                                    (struct sockaddr *)&client, &len);

            if (got < 2 || req[0] != version) {
                continue;
            }
            if (version == 0) {
                if (req[1] == 0) {
                    seen_announce++;
                } else if (req[1] == 2 && got >= NATPMP_MAP_LEN) {
                    seen_map++;
                } else {
                    continue;
                }
                if (!silent) {
                    answer_natpmp(req);
                }
            } else if (got >= HEADER_LEN) {
                if ((req[1] & 0x7f) == 0) {
                    seen_announce++;
                } else if ((req[1] & 0x7f) == 1) {
                    seen_map++;
                } else {
                    continue;
                }
                if (!silent) {
                    answer_pcp(req, got);
                }
            }
        }
    }
}

static pcp_flow_info_t flow_info(pcp_flow_t *flow) {
    size_t count = 0;
    pcp_flow_info_t *info = pcp_flow_get_info(flow, &count);
    pcp_flow_info_t first;

    TEST(info != NULL && count == 1);
    first = info[0];
    free(info);
    return first;
}

static void expect(const char *what, long got, long expected) {
    if (got != expected) {
        printf("version %u, %s: %ld, expected %ld\n", version, what, got,
               expected);
        TEST(0);
    }
}

// Maps internal port 1234 with `map_lifetime` and runs the client until the
// mapping is done, the epoch check planned if it is far from renewal.
static pcp_ctx_t *mapped(uint32_t map_lifetime, pcp_flow_t **flow) {
    char addr[32];
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);

    epoch = 1000;
    lifetime = map_lifetime;
    seen_map = seen_announce = 0;
    pcp_set_restart_delay(ctx, 0);
    snprintf(addr, sizeof(addr), "127.0.0.1:%d", server_port);
    TEST(pcp_add_server(ctx, Sock_pton(addr), version) == 0);
    *flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                         IPPROTO_TCP, map_lifetime, NULL);
    TEST(*flow != NULL);
    pump(ctx, 500);
    TEST(flow_info(*flow).result == pcp_state_succeeded);
    expect("mapping requests", seen_map, 1);
    return ctx;
}

// Seconds from now to the server's planned epoch check, or -1 for none.
static long epoch_check_in(pcp_ctx_t *ctx) {
    pcp_server_t *s = get_pcp_server(ctx, 0);
    struct timeval now;

    TEST(s != NULL);
    if (s->epoch_check_at.tv_sec == 0 && s->epoch_check_at.tv_usec == 0) {
        return -1;
    }
    gettimeofday(&now, NULL);
    return (long)(s->epoch_check_at.tv_sec - now.tv_sec);
}

// Brings the planned epoch check forward to now.
static void check_epoch_now(pcp_ctx_t *ctx) {
    pcp_server_t *s = get_pcp_server(ctx, 0);

    TEST(s != NULL);
    gettimeofday(&s->epoch_check_at, NULL);
    s->next_timeout = s->epoch_check_at;
}

// A mapping renewed in 3600 s: the epoch is asked for after 480 to 720 s;
// an answer showing a restart makes the client map again.
static void far_from_renewal(void) {
    pcp_flow_t *flow;
    pcp_ctx_t *ctx;
    time_t before = time(NULL);
    long in;

    ctx = mapped(LONG_LIFETIME, &flow);
    {
        time_t last = flow_info(flow).server_last_response;

        if (last < before || last > time(NULL)) {
            printf("version %u: last response %ld, expected %ld to %ld\n",
                   version, (long)last, (long)before, (long)time(NULL));
            TEST(0);
        }
    }
    in = epoch_check_in(ctx);
    if (in < PCP_EPOCH_CHECK_S * 8 / 10 - 1 ||
        in > PCP_EPOCH_CHECK_S * 12 / 10) {
        printf("version %u: epoch check in %ld s\n", version, in);
        TEST(0);
    }
    seen_announce = 0;

    // the same epoch: the server did not restart
    check_epoch_now(ctx);
    pump(ctx, 500);
    expect("epoch checks", seen_announce, 1);
    expect("mapping requests after an unchanged epoch", seen_map, 1);
    // the next check is planned again
    TEST(epoch_check_in(ctx) >= PCP_EPOCH_CHECK_S * 8 / 10 - 1);

    // an epoch far behind: the server restarted and lost the mapping
    epoch = 0;
    check_epoch_now(ctx);
    pump(ctx, 500);
    expect("epoch checks", seen_announce, 2);
    expect("mapping requests after a restart", seen_map, 2);

    pcp_terminate(ctx, 0);
}

// An epoch check the server does not answer changes nothing: the mapping
// stands, the check is not repeated at once, and the next is planned.
static void no_reply(void) {
    pcp_flow_t *flow;
    pcp_ctx_t *ctx = mapped(LONG_LIFETIME, &flow);

    seen_announce = 0;
    silent = 1;
    check_epoch_now(ctx);
    pump(ctx, 500);
    silent = 0;
    expect("unanswered epoch checks", seen_announce, 1);
    expect("mapping requests after an unanswered check", seen_map, 1);
    TEST(flow_info(flow).result == pcp_state_succeeded);
    TEST(epoch_check_in(ctx) >= PCP_EPOCH_CHECK_S * 8 / 10 - 1);
    pcp_terminate(ctx, 0);
}

// A server found not working has no epoch check planned. PCP only: an
// ADDRESS_MISMATCH answer to the check sets the server not working, and the
// server then stays silent, as an answer would show it working again.
static void not_working(void) {
    pcp_flow_t *flow;
    pcp_ctx_t *ctx = mapped(LONG_LIFETIME, &flow);

    announce_result = 12; // ADDRESS_MISMATCH
    check_epoch_now(ctx);
    pump(ctx, 500);
    announce_result = 0;
    silent = 0;
    expect("epoch checks", seen_announce, 1);
    expect("planned epoch check of a server not working", epoch_check_in(ctx),
           -1);
    pcp_terminate(ctx, 0);
}

// A mapping renewed in 1000 s needs no epoch check.
static void near_renewal(void) {
    pcp_flow_t *flow;
    pcp_ctx_t *ctx = mapped(SHORT_LIFETIME, &flow);

    expect("planned epoch check", epoch_check_in(ctx), -1);
    pcp_terminate(ctx, 0);
}

int main(void) {
    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();
    server = open_server();

    version = 2;
    far_from_renewal();
    near_renewal();
    no_reply();
    not_working();
    version = 0;
    far_from_renewal();
    near_renewal();
    no_reply();

    CLOSE(server);
    PD_SOCKET_CLEANUP();
    return 0;
}
