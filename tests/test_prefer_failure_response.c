/*
 *------------------------------------------------------------------
 * test_prefer_failure_response.c
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

// The test plays the PCP server itself, to send a success response that
// assigns something other than what the request suggested.
#define HEADER_LEN 24
#define MAP_LEN 36
#define EXT_PORT_AT (HEADER_LEN + 18)
#define EXT_IP_AT (HEADER_LEN + 20)

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

// Answers the MAP request in `req` with success, assigning ext_ip:ext_port.
static void send_success(PCP_SOCKET server, const uint8_t *req,
                         const struct sockaddr_storage *to, const char *ext_ip,
                         uint16_t ext_port) {
    uint8_t resp[HEADER_LEN + MAP_LEN];
    uint32_t lifetime = htonl(3600);
    uint32_t epoch = htonl(1000);
    uint16_t port = htons(ext_port);
    struct in_addr ip4;

    memset(resp, 0, HEADER_LEN);
    resp[0] = 2;    // version
    resp[1] = 0x81; // MAP response
    memcpy(resp + 4, &lifetime, 4);
    memcpy(resp + 8, &epoch, 4);
    // nonce, protocol and internal port as in the request
    memcpy(resp + HEADER_LEN, req + HEADER_LEN, MAP_LEN);
    memcpy(resp + EXT_PORT_AT, &port, 2);
    memset(resp + EXT_IP_AT, 0, 10);
    memset(resp + EXT_IP_AT + 10, 0xff, 2);
    inet_pton(AF_INET, ext_ip, &ip4);
    memcpy(resp + EXT_IP_AT + 12, &ip4, 4);
    TEST(sendto(server, (const char *)resp, sizeof(resp), 0,
                (const struct sockaddr *)to,
                SA_LEN((struct sockaddr *)to)) == (int)sizeof(resp));
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

// A MAP with PREFER_FAILURE suggesting `suggested`. The first success
// response assigns `wrong_ip`:`wrong_port` and must be ignored; the second
// assigns `assigned_ip`:5678 and must be taken.
static void check_case(const char *suggested, const char *wrong_ip,
                       uint16_t wrong_port, const char *assigned_ip) {
    char server_addr[32];
    uint8_t req[1100];
    struct sockaddr_storage client, internal, ext;
    PCP_SOCKET server = open_server();
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_flow_t *flow;
    pcp_flow_info_t info;

    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", server_port);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);
    // separate buffers: Sock_pton() returns the same static one every call
    TEST(sock_pton("127.0.0.1:1234", (struct sockaddr *)&internal) == 0);
    TEST(sock_pton(suggested, (struct sockaddr *)&ext) == 0);
    flow = pcp_new_flow(ctx, (struct sockaddr *)&internal, NULL,
                        (struct sockaddr *)&ext, IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    TEST(pcp_flow_set_prefer_failure_opt(flow) == PCP_ERR_SUCCESS);

    TEST(pump(ctx, server, 500, req, sizeof(req), &client) > 0);

    send_success(server, req, &client, wrong_ip, wrong_port);
    pump(ctx, server, 200, req, sizeof(req), &client);
    TEST(flow_info(flow).result != pcp_state_succeeded);

    send_success(server, req, &client, assigned_ip, 5678);
    pump(ctx, server, 200, req, sizeof(req), &client);
    info = flow_info(flow);
    TEST(info.result == pcp_state_succeeded);
    TEST(info.ext_port == htons(5678));

    pcp_terminate(ctx, 0);
    CLOSE(server);
}

int main(void) {
    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();

    // a suggested address must be assigned as well as the port
    check_case("8.8.8.8:5678", "9.9.9.9", 40000, "8.8.8.8");
    check_case("8.8.8.8:5678", "9.9.9.9", 5678, "8.8.8.8");
    check_case("8.8.8.8:5678", "8.8.8.8", 40000, "8.8.8.8");
    // a zero address asks for none in particular, so only the port counts
    check_case("0.0.0.0:5678", "9.9.9.9", 40000, "7.7.7.7");

    PD_SOCKET_CLEANUP();
    return 0;
}
