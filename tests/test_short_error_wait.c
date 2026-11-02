/*
 *------------------------------------------------------------------
 * test_short_error_wait.c
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

// A short lifetime error answered with lifetime 0 isn't resent at once:
// the flow waits at least PCP_MIN_ERROR_WAIT seconds.
int main(void) {
    struct sockaddr_storage source;
    pcp_flow_t *flow;
    pcp_ctx_t *ctx;
    test_pcp_server_sequence_t server_sequence;
    test_pcp_server_config_t server_config;
    const int end_after_recv = 20;

    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_WARN;

    test_pcp_server_config_init(&server_config);
    server_config.server_port = "5351";
    server_config.server_address = "0.0.0.0";
    server_config.server_info.default_result_code = PCP_RES_NO_RESOURCES;
    server_config.server_info.override_lifetime = 1;
    server_config.server_info.lifetime = 0;
    server_config.server_info.end_after_recv = end_after_recv;
    test_pcp_server_sequence_init(&server_sequence, &server_config, 1);
    test_sleep_ms(100);

    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);

    sock_pton("127.0.0.1:1235", (struct sockaddr *)&source);
    flow = pcp_new_flow(ctx, (struct sockaddr *)&source, NULL, NULL,
                        IPPROTO_TCP, 100, NULL);
    TEST(flow != NULL);

    TEST(test_pcp_wait_with_servers(flow, 3000, &server_sequence) ==
         pcp_state_short_lifetime_error);

    // two more seconds bring no second request
    TEST(test_pcp_wait_with_servers(flow, 2000, &server_sequence) ==
         pcp_state_processing);
    TEST(server_sequence.current_server.remaining_messages ==
         end_after_recv - 1);

    pcp_delete_flow(flow);
    pcp_terminate(ctx, 0);
    test_pcp_server_sequence_stop(&server_sequence);

    printf("Tests succeeded.\n\n");

    PD_SOCKET_CLEANUP();

    return 0;
}
