/*
 *------------------------------------------------------------------
 * test_third_party_response.c
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

// The test plays the PCP server itself, to send success responses with
// and without the THIRD_PARTY option.
#define SERVER_PORT 55371
#define HEADER_LEN 24
#define MAP_LEN 36
#define OPT_HDR_LEN 4
#define THIRD_PARTY 1
#define UNKNOWN_OPTIONAL 200

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

// Appends a THIRD_PARTY option for the IPv4 address `ip` at `at`.
static size_t put_third_party(uint8_t *at, const char *ip) {
    uint16_t len = htons(16);
    struct in_addr ip4;

    at[0] = THIRD_PARTY;
    at[1] = 0;
    memcpy(at + 2, &len, 2);
    memset(at + OPT_HDR_LEN, 0, 10);
    memset(at + OPT_HDR_LEN + 10, 0xff, 2);
    inet_pton(AF_INET, ip, &ip4);
    memcpy(at + OPT_HDR_LEN + 12, &ip4, 4);
    return OPT_HDR_LEN + 16;
}

// Answers the MAP request in `req` with success and the `opts_len` octets
// of options in `opts`.
static void send_success(PCP_SOCKET server, const uint8_t *req,
                         const struct sockaddr_storage *to, const uint8_t *opts,
                         size_t opts_len) {
    uint8_t resp[HEADER_LEN + MAP_LEN + 64];
    uint32_t lifetime = htonl(3600);
    uint32_t epoch = htonl(1000);
    size_t len = HEADER_LEN + MAP_LEN + opts_len;

    TEST(len <= sizeof(resp));
    memset(resp, 0, HEADER_LEN);
    resp[0] = 2;    // version
    resp[1] = 0x81; // MAP response
    memcpy(resp + 4, &lifetime, 4);
    memcpy(resp + 8, &epoch, 4);
    // nonce, protocol, ports and suggested address as in the request
    memcpy(resp + HEADER_LEN, req + HEADER_LEN, MAP_LEN);
    memcpy(resp + HEADER_LEN + MAP_LEN, opts, opts_len);
    TEST(sendto(server, (const char *)resp, (int)len, 0,
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
    uint8_t req[1100], opts[64];
    size_t n;
    struct sockaddr_storage client, internal, third;
    PCP_SOCKET server;
    pcp_ctx_t *ctx;
    pcp_flow_t *flow;

    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();
    server = open_server();
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", SERVER_PORT);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);
    TEST(sock_pton("127.0.0.1:1234", (struct sockaddr *)&internal) == 0);
    TEST(sock_pton("192.0.2.5", (struct sockaddr *)&third) == 0);
    flow = pcp_new_flow(ctx, (struct sockaddr *)&internal, NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    pcp_flow_set_3rd_party_opt(flow, (struct sockaddr *)&third);
    TEST(pump(ctx, server, 500, req, sizeof(req), &client) > 0);

    // no THIRD_PARTY: a mapping of the client itself
    send_success(server, req, &client, NULL, 0);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_result(flow) != pcp_state_succeeded);

    // THIRD_PARTY for another internal host
    n = put_third_party(opts, "192.0.2.6");
    send_success(server, req, &client, opts, n);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_result(flow) != pcp_state_succeeded);

    // THIRD_PARTY cut off by the end of the message
    n = put_third_party(opts, "192.0.2.5");
    send_success(server, req, &client, opts, n - 4);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_result(flow) != pcp_state_succeeded);

    // the flow's THIRD_PARTY, after an option of 2 octets padded to 4
    memset(opts, 0, OPT_HDR_LEN + 4);
    opts[0] = UNKNOWN_OPTIONAL;
    opts[3] = 2;
    n = OPT_HDR_LEN + 4;
    n += put_third_party(opts + n, "192.0.2.5");
    send_success(server, req, &client, opts, n);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_result(flow) == pcp_state_succeeded);

    pcp_terminate(ctx, 0);
    CLOSE(server);
    PD_SOCKET_CLEANUP();
    return 0;
}
