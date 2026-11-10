/*
 *------------------------------------------------------------------
 * test_natpmp_result_codes.c
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
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

// The test plays a NAT-PMP gateway (RFC 6886) that answers the external
// address request with success and a MAP request with a chosen result.

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

// Answers one NAT-PMP request in `req`; a MAP gets `result`.
static void answer(PCP_SOCKET server, const uint8_t *req, int len,
                   const struct sockaddr_storage *to, uint16_t result) {
    uint8_t resp[16];
    uint32_t epoch = htonl(1000);
    uint16_t code;
    int resp_len;

    memset(resp, 0, sizeof(resp));
    resp[0] = 0;
    resp[1] = (uint8_t)(0x80 | req[1]);
    memcpy(resp + 4, &epoch, 4);
    if (req[1] == 0) {
        // external address 9.9.9.9
        resp[8] = resp[9] = resp[10] = resp[11] = 9;
        resp_len = 12;
    } else {
        TEST(len >= 12);
        code = htons(result);
        memcpy(resp + 2, &code, 2);
        memcpy(resp + 8, req + 4, 2); // internal port
        if (result == 0) {
            resp[10] = 0x9c; // external port 40000
            resp[11] = 0x40;
            memcpy(resp + 12, req + 8, 4); // the lifetime asked for
        } // a failure has external port and lifetime 0
        resp_len = 16;
    }
    TEST(sendto(server, (const char *)resp, resp_len, 0,
                (const struct sockaddr *)to,
                SA_LEN((struct sockaddr *)to)) == resp_len);
}

// A NAT-PMP MAP answered with `natpmp_code` ends in `state`, reporting the
// PCP result code `pcp_code`.
static void check(uint16_t natpmp_code, pcp_fstate_e state, uint8_t pcp_code) {
    char server_addr[32];
    uint8_t req[1100];
    struct sockaddr_storage client;
    PCP_SOCKET server = open_server();
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_flow_t *flow;
    pcp_flow_info_t *info;
    size_t count = 0;
    int ms;

    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", server_port);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 0) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);

    for (ms = 0; ms < 1000; ms += 10) {
        struct timeval tout = {0, 10000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            socklen_t from_len = sizeof(client);
            int got = (int)recvfrom(server, (char *)req, sizeof(req), 0,
                                    (struct sockaddr *)&client, &from_len);

            if (got >= 2 && req[0] == 0) {
                answer(server, req, got, &client, natpmp_code);
            }
        }
    }

    info = pcp_flow_get_info(flow, &count);
    TEST(info != NULL && count == 1);
    if (info[0].result != state || info[0].pcp_result_code != pcp_code) {
        printf("NAT-PMP code %u: state %d, PCP code %u; expected %d, %u\n",
               natpmp_code, info[0].result, info[0].pcp_result_code, state,
               pcp_code);
        TEST(0);
    }
    free(info);

    pcp_terminate(ctx, 0);
    CLOSE(server);
}

int main(void) {
    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();

    // RFC 6886 section 3.5 codes, as RFC 6887 section 7.4 codes
    check(0, pcp_state_succeeded, 0);            // Success
    check(2, pcp_state_failed, 2);               // Not Authorized/Refused
    check(3, pcp_state_short_lifetime_error, 7); // Network Failure
    check(4, pcp_state_short_lifetime_error, 8); // Out of resources
    check(5, pcp_state_failed, 4);               // Unsupported opcode
    check(9, pcp_state_failed, 255);             // undefined: fatal

    PD_SOCKET_CLEANUP();
    return 0;
}
