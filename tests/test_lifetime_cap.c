/*
 *------------------------------------------------------------------
 * test_lifetime_cap.c
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
#endif

#include "pcpnatpmp.h"

#include "test_pcp_server_helper.h"

#include "pcp_client_db.h"
#include "pcp_socket.h"
#include "test_macro.h"
#include "unp.h"

#define DAY 86400
#define HEADER_LEN 24
#define MAP_LEN 36
#define NETWORK_FAILURE 7
// the bounds on the wait after a short lifetime error: PCP_MIN_ERROR_WAIT
// and PCP_MAX_ERROR_WAIT
#define MIN_ERROR_WAIT 30
#define MAX_ERROR_WAIT 1800

// Binds 127.0.0.1 on a port the system picks, so tests running at the same
// time do not collide; `*port` gets it.
static PCP_SOCKET open_server(int *port) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    PCP_SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    TEST(sock != PCP_INVALID_SOCKET);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    TEST(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    TEST(getsockname(sock, (struct sockaddr *)&addr, &len) == 0);
    *port = ntohs(addr.sin_port);
    return sock;
}

// A MAP granted `lifetime` seconds is kept as if granted at most a day, so
// it is renewed within half a day (RFC 6887 section 15).
static void check_lifetime(uint32_t lifetime, uint32_t expected) {
    struct sockaddr_storage source;
    pcp_flow_t *flow;
    pcp_flow_info_t *info;
    size_t info_count = 0;
    pcp_ctx_t *ctx;
    time_t before;
    test_pcp_server_sequence_t server_sequence;
    test_pcp_server_config_t server_config;
    char port[8], server_addr[32];
    int free_port;

    // a port the system picked, so tests running at the same time do not
    // collide; freed for the test server to bind
    CLOSE(open_server(&free_port));
    snprintf(port, sizeof(port), "%d", free_port);
    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", free_port);
    test_pcp_server_config_init(&server_config);
    server_config.server_port = port;
    server_config.server_address = "0.0.0.0";
    server_config.server_info.end_after_recv = 1;
    test_pcp_server_sequence_init(&server_sequence, &server_config, 1);
    test_sleep_ms(100);

    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);

    sock_pton("127.0.0.1:1236", (struct sockaddr *)&source);
    before = time(NULL);
    // the test server grants the lifetime asked for
    flow = pcp_new_flow(ctx, (struct sockaddr *)&source, NULL, NULL,
                        IPPROTO_TCP, lifetime, NULL);
    TEST(flow != NULL);

    TEST(test_pcp_wait_with_servers(flow, 3000, &server_sequence) ==
         pcp_state_succeeded);

    info = pcp_flow_get_info(flow, &info_count);
    TEST(info != NULL);
    TEST(info_count == 1);
    TEST(info->recv_lifetime_end >= before + (time_t)expected);
    TEST(info->recv_lifetime_end <= time(NULL) + (time_t)expected);
    free(info);
    // renewed halfway through the lifetime it goes by, not at once
    TEST(flow->timeout.tv_sec >= before + (time_t)expected / 2 - 1);
    TEST(flow->timeout.tv_sec <= time(NULL) + (time_t)expected / 2);

    pcp_delete_flow(flow);
    pcp_terminate(ctx, 0);
    test_pcp_server_sequence_stop(&server_sequence);
}

// A MAP answered with NETWORK_FAILURE and `lifetime` is asked for again
// after the lifetime, but no sooner than MIN_ERROR_WAIT and no later than
// MAX_ERROR_WAIT (RFC 6887 sections 7.4, 8.3).
static void check_error_wait(uint32_t lifetime, uint32_t expected) {
    char server_addr[32];
    uint8_t req[1100], resp[HEADER_LEN + MAP_LEN];
    struct sockaddr_storage client;
    socklen_t client_len = sizeof(client);
    uint32_t l = htonl(lifetime), epoch = htonl(1000);
    int port, got = 0, ms;
    PCP_SOCKET server = open_server(&port);
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_flow_t *flow;
    time_t before;

    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", port);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1237"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    for (ms = 0; ms < 1000 && got < HEADER_LEN + MAP_LEN; ms += 10) {
        struct timeval tout = {0, 10000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            got = (int)recvfrom(server, (char *)req, sizeof(req), 0,
                                (struct sockaddr *)&client, &client_len);
        }
    }
    TEST(got >= HEADER_LEN + MAP_LEN);

    memset(resp, 0, sizeof(resp));
    resp[0] = 2;
    resp[1] = 0x81; // MAP response
    resp[3] = NETWORK_FAILURE;
    memcpy(resp + 4, &l, 4);
    memcpy(resp + 8, &epoch, 4);
    memcpy(resp + HEADER_LEN, req + HEADER_LEN, MAP_LEN);
    before = time(NULL);
    TEST(sendto(server, (const char *)resp, sizeof(resp), 0,
                (const struct sockaddr *)&client,
                SA_LEN((struct sockaddr *)&client)) == (int)sizeof(resp));
    for (ms = 0; ms < 300; ms += 10) {
        pcp_pulse(ctx, NULL);
        test_sleep_ms(10);
    }

    if (flow->timeout.tv_sec < before + (time_t)expected ||
        flow->timeout.tv_sec > time(NULL) + (time_t)expected) {
        printf("error lifetime %u: next request in %ld s, expected %u s\n",
               lifetime, (long)(flow->timeout.tv_sec - before), expected);
        TEST(0);
    }

    pcp_terminate(ctx, 0);
    CLOSE(server);
}

int main(void) {
    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_WARN;
    check_lifetime(3600, 3600);
    check_lifetime(DAY, DAY);
    check_lifetime(DAY + 1, DAY);
    check_lifetime(UINT32_MAX, DAY);
    check_error_wait(5, MIN_ERROR_WAIT);
    check_error_wait(600, 600);
    check_error_wait(UINT32_MAX, MAX_ERROR_WAIT);
    printf("Tests succeeded.\n\n");
    PD_SOCKET_CLEANUP();

    return 0;
}
