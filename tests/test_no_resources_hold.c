/*
 *------------------------------------------------------------------
 * test_no_resources_hold.c
 *
 * November 9, 2026
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
#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

// The test plays the PCP server itself. After a NO_RESOURCES answer the
// client must hold back requests for new mappings to that server, and every
// request if the answer was to a deletion, for the error's lifetime (RFC 6887
// sections 11.4, 12.4). A NAT-PMP gateway's Out of resources means the same.
#define HEADER_LEN 24
#define MAP_LEN 36
#define NO_RESOURCES 8
// NAT-PMP (RFC 6886 section 3.5)
#define NATPMP_MAP_LEN 12
#define NATPMP_OUT_OF_RESOURCES 4
#define PORTS 4
// the shortest hold: the client waits at least PCP_MIN_ERROR_WAIT, 30 s
#define MIN_HOLD_MS 30000

// how the server treats a request, per internal port 1234 + index;
// REFUSE_RETRY ignores the first request and refuses the retransmission
enum treatment { SUCCEED, REFUSE, NO_ANSWER, HOLD_BACK, REFUSE_RETRY };

static PCP_SOCKET server;
static struct sockaddr_storage client;
static enum treatment on_create[PORTS], on_delete[PORTS];
static int seen_create[PORTS], seen_delete[PORTS];
// each port's last request, to answer later
static uint8_t last_req[PORTS][HEADER_LEN + MAP_LEN];
// the server's epoch when start() ran; answers add the seconds since, as
// a server's clock would, so the client sees no restart a fixed epoch would
// show once 3 s have passed (RFC 6887 section 8.5)
static uint32_t epoch = 1000;
static time_t epoch_start;
// the protocol the server speaks: 2 for PCP, 0 for NAT-PMP
static uint8_t version = 2;

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

// Answers the NAT-PMP request `req` with `result`; a MAP refused has
// lifetime 0, so the hold is the shortest one.
static void answer_natpmp(const uint8_t *req, uint16_t result) {
    uint8_t resp[16];
    uint32_t e = htonl(epoch + (uint32_t)(time(NULL) - epoch_start));
    uint16_t code = htons(result);
    int len = 16;

    memset(resp, 0, sizeof(resp));
    resp[1] = (uint8_t)(0x80 | req[1]);
    memcpy(resp + 4, &e, 4);
    if (req[1] == 0) {
        // external address 9.9.9.9
        resp[8] = resp[9] = resp[10] = resp[11] = 9;
        len = 12;
    } else {
        memcpy(resp + 2, &code, 2);
        memcpy(resp + 8, req + 4, 2); // internal port
        if (result == 0) {
            memcpy(resp + 10, req + 4, 2); // the same external port
            memcpy(resp + 12, req + 8, 4); // the lifetime asked for
        }
    }
    TEST(sendto(server, (const char *)resp, len, 0,
                (const struct sockaddr *)&client,
                SA_LEN((struct sockaddr *)&client)) == len);
}

// Answers the MAP request `req` with `result`; an error has lifetime 0, so
// the hold is the shortest one.
static void answer(const uint8_t *req, uint8_t result) {
    uint8_t resp[HEADER_LEN + MAP_LEN];
    uint32_t lifetime;
    uint32_t e = htonl(epoch + (uint32_t)(time(NULL) - epoch_start));

    if (version == 0) {
        answer_natpmp(req, result);
        return;
    }
    memcpy(&lifetime, req + 4, 4);
    memset(resp, 0, HEADER_LEN);
    resp[0] = 2;
    resp[1] = 0x81;
    resp[3] = result;
    if (result != 0) {
        lifetime = 0;
    }
    memcpy(resp + 4, &lifetime, 4);
    memcpy(resp + 8, &e, 4);
    memcpy(resp + HEADER_LEN, req + HEADER_LEN, MAP_LEN);
    TEST(sendto(server, (const char *)resp, sizeof(resp), 0,
                (const struct sockaddr *)&client,
                SA_LEN((struct sockaddr *)&client)) == (int)sizeof(resp));
}

// Runs the client for `ms` milliseconds by the clock, answering requests;
// select() may sleep longer than asked.
static void pump(pcp_ctx_t *ctx, int ms) {
    uint8_t req[1100];
    struct timeval end, now;

    gettimeofday(&end, NULL);
    end.tv_sec += ms / 1000;
    end.tv_usec += (ms % 1000) * 1000;
    timeval_align(&end);
    for (gettimeofday(&now, NULL); timeval_comp(&now, &end) < 0;
         gettimeofday(&now, NULL)) {
        struct timeval tout = {0, 10000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            socklen_t len = sizeof(client);
            int got = (int)recvfrom(server, (char *)req, sizeof(req), 0,
                                    (struct sockaddr *)&client, &len);
            uint16_t port;
            uint32_t lifetime;
            enum treatment how;
            int i;

            if (version == 0 && got >= 2 && req[0] == 0 && req[1] == 0) {
                answer_natpmp(req, 0); // the gateway's external address
                continue;
            }
            if (version == 0 && got >= NATPMP_MAP_LEN && req[0] == 0 &&
                req[1] == 2) {
                memcpy(&port, req + 4, 2);
                memcpy(&lifetime, req + 8, 4);
            } else if (version == 2 && got >= HEADER_LEN + MAP_LEN &&
                       req[0] == 2 && req[1] == 1) {
                memcpy(&port, req + HEADER_LEN + 16, 2);
                memcpy(&lifetime, req + 4, 4);
            } else {
                continue;
            }
            i = ntohs(port) - 1234;
            if (i < 0 || i >= PORTS) {
                continue;
            }
            if (lifetime != 0) {
                seen_create[i]++;
                how = on_create[i];
            } else {
                seen_delete[i]++;
                how = on_delete[i];
            }
            if (how == REFUSE_RETRY) {
                how = (lifetime != 0 ? seen_create[i] : seen_delete[i]) == 1
                          ? NO_ANSWER
                          : REFUSE;
            }
            memcpy(last_req[i], req, sizeof(last_req[i]));
            if (how == SUCCEED || how == REFUSE) {
                answer(req, how == SUCCEED ? 0
                            : version == 0 ? NATPMP_OUT_OF_RESOURCES
                                           : NO_RESOURCES);
            }
        }
    }
}

static pcp_flow_t *map(pcp_ctx_t *ctx, int i) {
    char addr[32];
    pcp_flow_t *flow;

    snprintf(addr, sizeof(addr), "127.0.0.1:%d", 1234 + i);
    flow =
        pcp_new_flow(ctx, Sock_pton(addr), NULL, NULL, IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    return flow;
}

static pcp_ctx_t *start(void) {
    char addr[32];
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    int i;

    snprintf(addr, sizeof(addr), "127.0.0.1:%d", server_port);
    TEST(pcp_add_server(ctx, Sock_pton(addr), version) == 0);
    pcp_set_restart_delay(ctx, 0);
    epoch = 1000;
    epoch_start = time(NULL);
    for (i = 0; i < PORTS; i++) {
        on_create[i] = on_delete[i] = SUCCEED;
        seen_create[i] = seen_delete[i] = 0;
    }
    return ctx;
}

static void expect(const char *what, int got, int expected) {
    if (got != expected) {
        printf("%s: %d, expected %d\n", what, got, expected);
        TEST(0);
    }
}

// A refused creation holds back new mappings, but not a deletion.
static void new_mappings_held(void) {
    pcp_ctx_t *ctx = start();
    pcp_flow_t *kept;

    kept = map(ctx, 3);
    pump(ctx, 300);
    on_create[0] = REFUSE;
    map(ctx, 0);
    pump(ctx, 300);
    map(ctx, 1);
    pump(ctx, 1000);
    expect("new mapping requests during the hold", seen_create[1], 0);
    pcp_close_flow(kept);
    pump(ctx, 500);
    expect("deletions during the hold", seen_delete[3], 1);
    pcp_terminate(ctx, 0);
}

// A refused deletion holds back every request, here a retransmission due
// about 3 s after the first request; a refused creation answered afterwards
// does not narrow that hold.
static void everything_held(int narrowed) {
    pcp_ctx_t *ctx = start();
    pcp_flow_t *a;

    on_create[1] = narrowed ? HOLD_BACK : NO_ANSWER;
    on_create[2] = NO_ANSWER;
    on_delete[0] = REFUSE;
    a = map(ctx, 0);
    pump(ctx, 300);
    map(ctx, 1);
    map(ctx, 2);
    pump(ctx, 300);
    expect("first requests", seen_create[2], 1);
    pcp_close_flow(a);
    pump(ctx, 300);
    if (narrowed) {
        answer(last_req[1], NO_RESOURCES);
    }
    pump(ctx, 4200);
    expect("requests during the hold", seen_create[2], 1);
    pcp_terminate(ctx, 0);
}

// A server restart ends the hold: it described the server before.
static void restart_ends_hold(void) {
    pcp_ctx_t *ctx = start();

    map(ctx, 3);
    pump(ctx, 300);
    on_create[0] = REFUSE;
    map(ctx, 0);
    pump(ctx, 300);
    map(ctx, 1);
    pump(ctx, 500);
    expect("new mapping requests during the hold", seen_create[1], 0);
    // the kept mapping's answer again, with an epoch far behind: the server
    // has restarted
    epoch = 0;
    answer(last_req[3], 0);
    pump(ctx, 1000);
    expect("new mapping requests after the restart", seen_create[1], 1);
    pcp_terminate(ctx, 0);
}

// Held requests go out when the hold ends: a new mapping after a refused
// creation, a deletion after a refused deletion. The held flow and the
// server's next wake-up are both at the hold's end, at least MIN_HOLD_MS
// away; the test then brings all three forward by the same amount, so it
// does not wait the 30 s out.
static void hold_ends(int deletion) {
    pcp_ctx_t *ctx = start();
    pcp_server_t *s = get_pcp_server(ctx, 0);
    pcp_flow_t *refused, *held;
    int *seen;
    struct timeval now, end;

    TEST(s != NULL);
    refused = map(ctx, 0);
    if (deletion) {
        held = map(ctx, 3);
        pump(ctx, 300);
        on_delete[0] = REFUSE;
        pcp_close_flow(refused);
    }
    on_create[0] = REFUSE;
    gettimeofday(&now, NULL);
    pump(ctx, 300);
    if (deletion) {
        pcp_close_flow(held);
        seen = &seen_delete[3];
    } else {
        held = map(ctx, 1);
        seen = &seen_create[1];
    }
    pump(ctx, 1000);
    expect("held requests during the hold", *seen, 0);
    TEST(s->no_resources_all == deletion);
    TEST(s->no_resources_until.tv_sec >= now.tv_sec + MIN_HOLD_MS / 1000);
    TEST(timeval_comp(&held->timeout, &s->no_resources_until) == 0);
    TEST(s->next_timeout.tv_sec >= s->no_resources_until.tv_sec - 1 &&
         s->next_timeout.tv_sec <= s->no_resources_until.tv_sec + 1);

    gettimeofday(&end, NULL);
    end.tv_sec += 1;
    s->no_resources_until = end;
    held->timeout = end;
    s->next_timeout = end;
    pump(ctx, 700);
    expect("held requests just before the hold ends", *seen, 0);
    pump(ctx, 1000);
    expect("held requests after the hold", *seen, 1);
    pcp_terminate(ctx, 0);
}

// A refused deletion widens a running hold on new mappings to every request,
// also when the refusal answers the deletion's retransmission: a creation
// sent before the hold is then not retransmitted.
static void deletion_widens_hold(void) {
    pcp_ctx_t *ctx = start();
    pcp_server_t *s = get_pcp_server(ctx, 0);
    pcp_flow_t *kept;

    TEST(s != NULL);
    kept = map(ctx, 3);
    pump(ctx, 300);
    // deleted at 0 s: ignored, then retransmitted at about 3 s and refused
    on_delete[3] = REFUSE_RETRY;
    pcp_close_flow(kept);
    pump(ctx, 300);
    // asked for at 0.3 s and ignored: retransmitted at about 3.3 s unless held
    on_create[2] = NO_ANSWER;
    map(ctx, 2);
    pump(ctx, 300);
    // refused at 0.6 s: a hold on new mappings only
    on_create[0] = REFUSE;
    map(ctx, 0);
    pump(ctx, 300);
    TEST(s->no_resources_until.tv_sec != 0 && !s->no_resources_all);
    pump(ctx, 3000);
    expect("deletion requests", seen_delete[3], 2);
    TEST(s->no_resources_all);
    expect("creation requests after the hold widened", seen_create[2], 1);
    pcp_terminate(ctx, 0);
}

int main(void) {
    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();
    server = open_server();

    new_mappings_held();
    version = 0;
    new_mappings_held();
    version = 2;
    everything_held(0);
    everything_held(1);
    restart_ends_hold();
    hold_ends(0);
    hold_ends(1);
    deletion_widens_hold();

    CLOSE(server);
    PD_SOCKET_CLEANUP();
    return 0;
}
