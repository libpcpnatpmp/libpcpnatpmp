/*
 *------------------------------------------------------------------
 * test_natpmp_retransmit.c
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

// A NAT-PMP gateway that never answers, so the client retransmits.
#define SERVER_PORT 55375
#define MAX_REQUESTS 16
// RFC 6886 section 3.1: 250 ms, then doubling
#define FIRST_WAIT_MS 250
#define CHECKED_WAITS 4
#define SLACK_MS 120

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

static long ms_since(const struct timeval *start) {
    struct timeval now;

    gettimeofday(&now, NULL);
    return (long)(now.tv_sec - start->tv_sec) * 1000 +
           (long)(now.tv_usec - start->tv_usec) / 1000;
}

int main(void) {
    char server_addr[32];
    uint8_t buf[1100];
    long arrived[MAX_REQUESTS];
    int requests = 0, i;
    struct timeval start;
    PCP_SOCKET server;
    pcp_ctx_t *ctx;
    pcp_flow_t *flow;

    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();
    server = open_server();
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", SERVER_PORT);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 0) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);

    // long enough for the first five attempts: 250 + 500 + 1000 + 2000 ms
    gettimeofday(&start, NULL);
    while (ms_since(&start) < 4500 && requests < MAX_REQUESTS) {
        struct timeval tout = {0, 5000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            int got =
                (int)recvfrom(server, (char *)buf, sizeof(buf), 0, NULL, NULL);
            // NAT-PMP requests only: version 0
            if (got >= 2 && buf[0] == 0) {
                arrived[requests++] = ms_since(&start);
            }
        }
    }

    for (i = 0; i < requests; i++) {
        printf("NAT-PMP request %d at %ld ms\n", i + 1, arrived[i]);
    }
    TEST(requests >= CHECKED_WAITS + 1);
    for (i = 0; i < CHECKED_WAITS; i++) {
        long wait = arrived[i + 1] - arrived[i];
        long expected = (long)FIRST_WAIT_MS << i;

        if (wait < expected - SLACK_MS || wait > expected + SLACK_MS) {
            printf("wait %d: %ld ms, expected %ld ms\n", i + 1, wait, expected);
            TEST(0);
        }
    }

    pcp_terminate(ctx, 0);
    CLOSE(server);
    PD_SOCKET_CLEANUP();
    return 0;
}
