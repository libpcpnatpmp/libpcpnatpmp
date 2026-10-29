/*
 *------------------------------------------------------------------
 * test_error_lifetime.c
 *
 * October 29, 2026
 *
 *------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

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

#include "pcp_msg_structs.h"
#include "pcp_socket.h"
#include "test_macro.h"
#include "unp.h"

// A long lifetime error, such as NOT_AUTHORIZED, fails the flow, and
// pcp_flow_get_info() reports when the server stops refusing.
int main(void) {
    struct sockaddr_storage source;
    uint32_t lifetime = 100; // the test server answers with the lifetime asked
    pcp_flow_t *flow;
    pcp_flow_info_t *info;
    size_t info_count = 0;
    pcp_ctx_t *ctx;
    time_t before;
    test_pcp_server_sequence_t server_sequence;
    test_pcp_server_config_t server_config;

    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_WARN;

    test_pcp_server_config_init(&server_config);
    server_config.server_port = "5351";
    server_config.server_address = "0.0.0.0";
    server_config.server_info.default_result_code = PCP_RES_NOT_AUTHORIZED;
    server_config.server_info.end_after_recv = 1;
    test_pcp_server_sequence_init(&server_sequence, &server_config, 1);
    test_sleep_ms(100);

    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);

    sock_pton("127.0.0.1:1235", (struct sockaddr *)&source);
    before = time(NULL);
    flow = pcp_new_flow(ctx, (struct sockaddr *)&source, NULL, NULL,
                        IPPROTO_TCP, lifetime, NULL);
    TEST(flow != NULL);

    TEST(test_pcp_wait_with_servers(flow, 3000, &server_sequence) ==
         pcp_state_failed);

    info = pcp_flow_get_info(flow, &info_count);
    TEST(info != NULL);
    TEST(info_count == 1);
    TEST(info->result == pcp_state_failed);
    TEST(info->pcp_result_code == PCP_RES_NOT_AUTHORIZED);
    TEST(info->recv_lifetime_end >= before + (time_t)lifetime);
    TEST(info->recv_lifetime_end <= time(NULL) + (time_t)lifetime);
    free(info);

    pcp_delete_flow(flow);
    pcp_terminate(ctx, 0);
    test_pcp_server_sequence_stop(&server_sequence);

    printf("Tests succeeded.\n\n");

    PD_SOCKET_CLEANUP();

    return 0;
}
