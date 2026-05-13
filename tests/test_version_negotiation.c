/*
 *------------------------------------------------------------------
 * test_version_negotiation.c
 *
 * March 13th, 2013, mbagljas
 *
 * Copyright (c) 2013-2013 by cisco Systems, Inc.
 * All rights reserved.
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
#include "pcp_win_defines.h"
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

int main(int argc, char *argv[]) {
    struct sockaddr_storage destination;
    struct sockaddr_storage source;
    struct sockaddr_storage ext;
    int version;
    pcp_server_t *s;
    uint8_t protocol = 6;
    uint32_t lifetime = 10;
    pcp_flow_t *flow = NULL;
    pcp_flow_info_t *flow_info = NULL;
    size_t flow_count;
    pcp_ctx_t *ctx;
    test_pcp_server_sequence_t server_sequence;
    test_pcp_server_config_t server_config;

    PD_SOCKET_STARTUP();
    version = (argc == 2) ? (uint8_t)atoi(argv[1]) : PCP_MAX_SUPPORTED_VERSION;

    pcp_log_level = 0;

    test_pcp_server_config_init(&server_config);
    server_config.server_port = "5351";
    server_config.server_address = "0.0.0.0";
    server_config.server_info.server_version = 1;
    server_config.server_info.end_after_recv = 2;
    test_pcp_server_sequence_init(&server_sequence, &server_config, 1);
    test_sleep_ms(100);

    printf("\n");
    fprintf(stdout, "###########################################\n");
    printf("####   *****************************   ####\n");
    printf("####   * Begin version negotiation *   ####\n");
    printf("####   *           test            *   ####\n");
    printf("####   *****************************   ####\n");
    printf("###########################################\n");
    printf(">>> PCP server version =  %d \n", version);
    ctx = pcp_init(0, NULL);
    pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), version);

    sock_pton("127.0.0.1:1234", (struct sockaddr *)&destination);
    sock_pton("127.0.0.1:1235", (struct sockaddr *)&source);
    sock_pton("10.20.30.40", (struct sockaddr *)&ext);

    flow = pcp_new_flow(ctx, (struct sockaddr *)&source,
                        (struct sockaddr *)&destination,
                        (struct sockaddr *)&ext, protocol, lifetime, NULL);

    TEST(test_pcp_wait_with_servers(flow, 2000, &server_sequence) ==
         pcp_state_succeeded);
    flow_info = pcp_flow_get_info(flow, &flow_count);
    TEST(flow_info);
    printf("Flow result code %d \n", flow_info->pcp_result_code);
    s = get_pcp_server(ctx, flow->pcp_server_indx);
    TEST((s) && (s->pcp_version == 1));

    pcp_close_flow(flow);
    pcp_delete_flow(flow);
    flow = NULL;
    free(flow_info);

    PD_SOCKET_CLEANUP();
    pcp_terminate(ctx, 0);
    test_pcp_server_sequence_stop(&server_sequence);
    return 0;
}
