/*
 *------------------------------------------------------------------
 * test_result_codes.c
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

// The test plays the PCP server itself, to answer with result codes the
// bundled test server does not send.
#define SERVER_PORT 55374
#define HEADER_LEN 24
#define MAP_LEN 36

static PCP_SOCKET open_server(void) {
    struct sockaddr_in addr;
    PCP_SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    TEST(sock != PCP_INVALID_SOCKET);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    TEST(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
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

// Answers the MAP request in `req` with `result` and a lifetime of 1800 s.
static void send_result(PCP_SOCKET server, const uint8_t *req,
                        const struct sockaddr_storage *to, uint8_t result) {
    uint8_t resp[HEADER_LEN + MAP_LEN];
    uint32_t lifetime = htonl(1800);
    uint32_t epoch = htonl(1000);

    memset(resp, 0, HEADER_LEN);
    resp[0] = 2;    // version
    resp[1] = 0x81; // MAP response
    resp[3] = result;
    memcpy(resp + 4, &lifetime, 4);
    memcpy(resp + 8, &epoch, 4);
    memcpy(resp + HEADER_LEN, req + HEADER_LEN, MAP_LEN);
    TEST(sendto(server, (const char *)resp, sizeof(resp), 0,
                (const struct sockaddr *)to,
                SA_LEN((struct sockaddr *)to)) == (int)sizeof(resp));
}

// A MAP answered with `result` fails at once and reports that code.
static void check_fails(uint8_t result) {
    char server_addr[32];
    uint8_t req[1100];
    struct sockaddr_storage client;
    PCP_SOCKET server = open_server();
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_flow_t *flow;
    pcp_flow_info_t *info;
    size_t count = 0;

    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", SERVER_PORT);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    TEST(pump(ctx, server, 500, req, sizeof(req), &client) > 0);

    send_result(server, req, &client, result);
    pump(ctx, server, 200, req, sizeof(req), &client);
    info = pcp_flow_get_info(flow, &count);
    TEST(info != NULL && count == 1);
    if (info[0].result != pcp_state_failed ||
        info[0].pcp_result_code != result) {
        printf("result code %u: state %d, reported code %u\n", result,
               info[0].result, info[0].pcp_result_code);
        TEST(0);
    }
    free(info);

    pcp_terminate(ctx, 0);
    CLOSE(server);
}

int main(void) {
    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();

    // MALFORMED_OPTION, a long lifetime error (RFC 6887 section 7.4)
    check_fails(6);
    // codes RFC 6887 does not define
    check_fails(14);
    check_fails(255);

    PD_SOCKET_CLEANUP();
    return 0;
}
