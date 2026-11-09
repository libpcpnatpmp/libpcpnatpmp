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
#define HEADER_LEN 24
#define MAP_LEN 36
#define PEER_LEN 56
#define OPT_HDR_LEN 4
#define THIRD_PARTY 1
#define UNKNOWN_OPTIONAL 200

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

// Answers the MAP or PEER request in `req` with success, assigning external
// port `ext_port`, and the `opts_len` octets of options in `opts`.
static void send_success(PCP_SOCKET server, const uint8_t *req,
                         const struct sockaddr_storage *to, uint16_t ext_port,
                         const uint8_t *opts, size_t opts_len) {
    uint8_t resp[HEADER_LEN + PEER_LEN + 64];
    uint32_t lifetime = htonl(3600);
    uint32_t epoch = htonl(1000);
    uint16_t port = htons(ext_port);
    size_t body = req[1] == 2 ? PEER_LEN : MAP_LEN;
    size_t len = HEADER_LEN + body + opts_len;

    TEST(len <= sizeof(resp));
    memset(resp, 0, HEADER_LEN);
    resp[0] = 2;                        // version
    resp[1] = (uint8_t)(0x80 | req[1]); // MAP or PEER response
    memcpy(resp + 4, &lifetime, 4);
    memcpy(resp + 8, &epoch, 4);
    // nonce, protocol, ports and addresses as in the request
    memcpy(resp + HEADER_LEN, req + HEADER_LEN, body);
    memcpy(resp + HEADER_LEN + 18, &port, 2);
    if (opts_len > 0) {
        memcpy(resp + HEADER_LEN + body, opts, opts_len);
    }
    TEST(sendto(server, (const char *)resp, (int)len, 0,
                (const struct sockaddr *)to,
                SA_LEN((struct sockaddr *)to)) == (int)len);
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

// A MAP, or a PEER to 127.0.0.1:4321, for 192.0.2.5 (THIRD_PARTY) takes only
// a success response that carries THIRD_PARTY 192.0.2.5.
static void check(int peer) {
    char server_addr[32];
    uint8_t req[1100], opts[64];
    size_t n;
    struct sockaddr_storage client, internal, remote, third;
    PCP_SOCKET server = open_server();
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_flow_t *flow;

    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", server_port);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);
    TEST(sock_pton("127.0.0.1:1234", (struct sockaddr *)&internal) == 0);
    TEST(sock_pton("127.0.0.1:4321", (struct sockaddr *)&remote) == 0);
    TEST(sock_pton("192.0.2.5", (struct sockaddr *)&third) == 0);
    flow = pcp_new_flow(ctx, (struct sockaddr *)&internal,
                        peer ? (struct sockaddr *)&remote : NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    pcp_flow_set_3rd_party_opt(flow, (struct sockaddr *)&third);
    TEST(pump(ctx, server, 500, req, sizeof(req), &client) > 0);
    TEST(req[1] == (peer ? 2 : 1));

    // no THIRD_PARTY: a mapping of the client itself
    send_success(server, req, &client, 40001, NULL, 0);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_info(flow).result != pcp_state_succeeded);

    // THIRD_PARTY for another internal host
    n = put_third_party(opts, "192.0.2.6");
    send_success(server, req, &client, 40002, opts, n);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_info(flow).result != pcp_state_succeeded);

    // THIRD_PARTY cut off by the end of the message
    n = put_third_party(opts, "192.0.2.5");
    send_success(server, req, &client, 40003, opts, n - 4);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_info(flow).result != pcp_state_succeeded);

    // THIRD_PARTY with the right address but a length other than 16
    n = put_third_party(opts, "192.0.2.5");
    opts[3] = 12;
    send_success(server, req, &client, 40003, opts, n);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_info(flow).result != pcp_state_succeeded);

    // the flow's THIRD_PARTY, after an option of 2 octets padded to 4
    memset(opts, 0, OPT_HDR_LEN + 4);
    opts[0] = UNKNOWN_OPTIONAL;
    opts[3] = 2;
    n = OPT_HDR_LEN + 4;
    n += put_third_party(opts + n, "192.0.2.5");
    send_success(server, req, &client, 40004, opts, n);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_info(flow).result == pcp_state_succeeded);
    TEST(flow_info(flow).ext_port == htons(40004));

    // a later mapping update without THIRD_PARTY is for another mapping
    send_success(server, req, &client, 40005, NULL, 0);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_info(flow).ext_port == htons(40004));

    pcp_terminate(ctx, 0);
    CLOSE(server);
}

int main(void) {
    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();
    check(0);
    check(1);
    PD_SOCKET_CLEANUP();
    return 0;
}
