/*
 *------------------------------------------------------------------
 * test_natpmp_one_at_a_time.c
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

// The test plays a slow NAT-PMP gateway (RFC 6886): it answers each MAP
// request 200 ms after it arrives, and counts requests that arrive while an
// earlier one is still unanswered. Section 3.1 has a client send them one
// at a time.
#define SERVER_PORT 55378
#define FLOWS 3
#define ANSWER_AFTER_MS 200

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

static void send_to(PCP_SOCKET server, const struct sockaddr_storage *to,
                    const uint8_t *msg, int len) {
    TEST(sendto(server, (const char *)msg, len, 0, (const struct sockaddr *)to,
                SA_LEN((struct sockaddr *)to)) == len);
}

int main(void) {
    char addr[32];
    uint8_t req[1100], pending[16];
    struct sockaddr_storage client;
    PCP_SOCKET server;
    pcp_ctx_t *ctx;
    pcp_flow_t *flows[FLOWS];
    int i, ms, has_pending = 0, pending_since = 0, overlaps = 0, answered = 0;

    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();
    server = open_server();
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    snprintf(addr, sizeof(addr), "127.0.0.1:%d", SERVER_PORT);
    TEST(pcp_add_server(ctx, Sock_pton(addr), 0) == 0);
    for (i = 0; i < FLOWS; i++) {
        snprintf(addr, sizeof(addr), "127.0.0.1:%d", 1234 + i);
        flows[i] = pcp_new_flow(ctx, Sock_pton(addr), NULL, NULL, IPPROTO_TCP,
                                3600, NULL);
        TEST(flows[i] != NULL);
    }

    for (ms = 0; ms < 2000; ms += 10) {
        struct timeval tout = {0, 10000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        if (has_pending && ms - pending_since >= ANSWER_AFTER_MS) {
            send_to(server, &client, pending, 16);
            has_pending = 0;
            answered++;
        }
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            socklen_t len = sizeof(client);
            int got = (int)recvfrom(server, (char *)req, sizeof(req), 0,
                                    (struct sockaddr *)&client, &len);
            uint32_t epoch = htonl(1000);

            if (got < 2 || req[0] != 0) {
                continue;
            }
            if (req[1] == 0) {
                // external address request: answer at once with 9.9.9.9
                uint8_t resp[12] = {0, 0x80, 0, 0, 0, 0, 0, 0, 9, 9, 9, 9};

                memcpy(resp + 4, &epoch, 4);
                send_to(server, &client, resp, sizeof(resp));
            } else if (got >= 12) {
                if (has_pending &&
                    memcmp(pending + 8, req + 4, 2) != 0) { // another port
                    overlaps++;
                    continue;
                }
                // success: internal port, external port 40000 + n, lifetime
                memset(pending, 0, sizeof(pending));
                pending[1] = (uint8_t)(0x80 | req[1]);
                memcpy(pending + 4, &epoch, 4);
                memcpy(pending + 8, req + 4, 2);
                pending[10] = 0x9c;
                pending[11] = (uint8_t)(0x40 + answered);
                memcpy(pending + 12, req + 8, 4);
                if (!has_pending) {
                    pending_since = ms;
                }
                has_pending = 1;
            }
        }
    }

    if (overlaps) {
        printf("%d requests arrived while another was unanswered\n", overlaps);
        TEST(0);
    }
    for (i = 0; i < FLOWS; i++) {
        size_t count = 0;
        pcp_flow_info_t *info = pcp_flow_get_info(flows[i], &count);

        TEST(info != NULL && count == 1);
        TEST(info[0].result == pcp_state_succeeded);
        free(info);
    }

    pcp_terminate(ctx, 0);
    CLOSE(server);
    PD_SOCKET_CLEANUP();
    return 0;
}
