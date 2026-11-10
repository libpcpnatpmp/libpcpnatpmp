/*
 *------------------------------------------------------------------
 * test_response_validation.c
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

#include "pcp_socket.h"
#include "test_macro.h"
#include "unp.h"

// The test plays the PCP server itself, to send responses that RFC 6887
// section 8.3 says a client drops.
#define HEADER_LEN 24
#define MAP_LEN 36
#define MAX_LEN 1100

// Binds `ip`:`*port`, or a port the system picks if `*port` is 0, so tests
// running at the same time do not collide; `*port` gets the bound port.
// Returns PCP_INVALID_SOCKET if `ip` cannot be bound.
static PCP_SOCKET try_open_socket(const char *ip, int *port) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    PCP_SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    TEST(sock != PCP_INVALID_SOCKET);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)*port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        CLOSE(sock);
        return PCP_INVALID_SOCKET;
    }
    TEST(getsockname(sock, (struct sockaddr *)&addr, &len) == 0);
    *port = ntohs(addr.sin_port);
    return sock;
}

static PCP_SOCKET open_socket(const char *ip, int *port) {
    PCP_SOCKET sock = try_open_socket(ip, port);

    TEST(sock != PCP_INVALID_SOCKET);
    return sock;
}

// Runs the client for `ms` milliseconds. Returns the length of the first
// datagram the server socket receives in that time, or 0.
static int pump(pcp_ctx_t *ctx, PCP_SOCKET server, int ms, uint8_t *buf,
                size_t buf_len, struct sockaddr_storage *from) {
    int got = 0;

    for (; ms > 0; ms -= 10) {
        struct timeval tout = {0, 10000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0 && !got) {
            socklen_t from_len = sizeof(*from);
            got = (int)recvfrom(server, (char *)buf, (int)buf_len, 0,
                                (struct sockaddr *)from, &from_len);
            TEST(got >= HEADER_LEN + MAP_LEN);
        }
    }
    return got;
}

// counts warnings, the level every dropped response is logged at
static int warnings;

static void count_warnings(pcp_loglvl_e level, const char *msg) {
    (void)msg;
    if (level == PCP_LOGLVL_WARN) {
        warnings++;
    }
}

// Answers the MAP request in `req` with success from `sock`, padded with
// zero octets to `len` octets.
static void send_success(PCP_SOCKET sock, const uint8_t *req,
                         const struct sockaddr_storage *to, size_t len) {
    uint8_t resp[MAX_LEN + 4];
    uint32_t lifetime = htonl(3600);
    uint32_t epoch = htonl(1000);

    TEST(len >= HEADER_LEN + MAP_LEN && len <= sizeof(resp));
    memset(resp, 0, sizeof(resp));
    resp[0] = 2;    // version
    resp[1] = 0x81; // MAP response
    memcpy(resp + 4, &lifetime, 4);
    memcpy(resp + 8, &epoch, 4);
    memcpy(resp + HEADER_LEN, req + HEADER_LEN, MAP_LEN);
    TEST(sendto(sock, (const char *)resp, (int)len, 0,
                (const struct sockaddr *)to,
                SA_LEN((struct sockaddr *)to)) == (int)len);
}

static pcp_fstate_e flow_result(pcp_flow_t *flow) {
    size_t count = 0;
    pcp_flow_info_t *info = pcp_flow_get_info(flow, &count);
    pcp_fstate_e result;

    TEST(info != NULL && count == 1);
    result = info[0].result;
    free(info);
    return result;
}

int main(void) {
    char server_addr[32];
    uint8_t req[MAX_LEN];
    struct sockaddr_storage client;
    PCP_SOCKET server, other_port, other_addr;
    int port = 0, unused = 0;
    pcp_ctx_t *ctx;
    pcp_flow_t *flow;

    pcp_log_level = PCP_LOGLVL_WARN;
    pcp_set_loggerfn(count_warnings);
    PD_SOCKET_STARTUP();
    server = open_socket("127.0.0.1", &port);
    other_port = open_socket("127.0.0.1", &unused);
    // macOS has only 127.0.0.1 on loopback; that case is skipped there
    other_addr = try_open_socket("127.0.0.2", &port);
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", port);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    TEST(pump(ctx, server, 500, req, sizeof(req), &client) > 0);

    // from the server's address, but not from its port
    send_success(other_port, req, &client, HEADER_LEN + MAP_LEN);
    warnings = 0;
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_result(flow) != pcp_state_succeeded);
    TEST(warnings > 0);

    // from the server's port, but not from its address
    if (other_addr != PCP_INVALID_SOCKET) {
        send_success(other_addr, req, &client, HEADER_LEN + MAP_LEN);
        warnings = 0;
        pump(ctx, server, 200, req, sizeof(req), &client);
        TEST(flow_result(flow) != pcp_state_succeeded);
        TEST(warnings > 0);
    } else {
        printf("127.0.0.2 cannot be bound here; skipping the response from "
               "another address\n");
    }

    // not a multiple of 4 octets
    send_success(server, req, &client, HEADER_LEN + MAP_LEN + 2);
    warnings = 0;
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_result(flow) != pcp_state_succeeded);
    TEST(warnings > 0);

    // longer than 1100 octets
    send_success(server, req, &client, MAX_LEN + 4);
    warnings = 0;
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_result(flow) != pcp_state_succeeded);
    TEST(warnings > 0);

    // valid, and exactly 1100 octets; taking it also shows the source port
    // check compares the two ports in the same byte order
    send_success(server, req, &client, MAX_LEN);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_result(flow) == pcp_state_succeeded);

    pcp_terminate(ctx, 0);
    if (other_addr != PCP_INVALID_SOCKET) {
        CLOSE(other_addr);
    }
    CLOSE(other_port);
    CLOSE(server);
    PD_SOCKET_CLEANUP();
    return 0;
}
