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
#include <time.h>
#ifdef WIN32
#include <winsock2.h>

#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "pcpnatpmp.h"

#include "test_pcp_server_helper.h"

#include "pcp_client_db.h"
#include "pcp_socket.h"
#include "test_macro.h"
#include "unp.h"

#define DAY 86400

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

    test_pcp_server_config_init(&server_config);
    server_config.server_port = "5351";
    server_config.server_address = "0.0.0.0";
    server_config.server_info.end_after_recv = 1;
    test_pcp_server_sequence_init(&server_sequence, &server_config, 1);
    test_sleep_ms(100);

    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);

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

int main(void) {
    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_WARN;
    check_lifetime(3600, 3600);
    check_lifetime(UINT32_MAX, DAY);
    printf("Tests succeeded.\n\n");
    PD_SOCKET_CLEANUP();

    return 0;
}
