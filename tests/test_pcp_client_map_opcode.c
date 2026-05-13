/*
 *------------------------------------------------------------------
 * test_pcp_client_map_opcode.c
 *
 * March 15th, 2013, mbagljas
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

#include "pcp_socket.h"
#include "test_macro.h"
#include "unp.h"

int main(void) {
    struct sockaddr_storage destination;
    struct sockaddr_storage source;
    struct sockaddr_storage ext;
    pcp_ctx_t *ctx;

    uint8_t protocol = 6;
    uint32_t lifetime = 10;

    pcp_flow_t *flow = NULL;
    test_pcp_server_sequence_t server_sequence;
    test_pcp_server_config_t server_config;

    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_DEBUG;

    test_pcp_server_config_init(&server_config);
    server_config.server_port = "5351";
    server_config.server_address = "0.0.0.0";
    server_config.server_info.end_after_recv = 1;
    test_pcp_server_sequence_init(&server_sequence, &server_config, 1);
    test_sleep_ms(100);

    TEST((ctx = pcp_init(0, NULL)));
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);

    sock_pton("0.0.0.0:1234", (struct sockaddr *)&destination);
    sock_pton("127.0.0.1:1235", (struct sockaddr *)&source);
    sock_pton("10.20.30.40", (struct sockaddr *)&ext);

    printf("\n");
    printf("#########################################\n");
    printf("####   *************************     ####\n");
    printf("####   * Begin MAP opcode test *     ####\n");
    printf("####   *************************     ####\n");
    printf("#########################################\n");

    flow = pcp_new_flow(ctx, (struct sockaddr *)&source,
                        (struct sockaddr *)&destination,
                        (struct sockaddr *)&ext, protocol, lifetime, NULL);

    TEST(test_pcp_wait_with_servers(flow, 3000, &server_sequence) ==
         pcp_state_succeeded);

    pcp_close_flow(flow);
    pcp_delete_flow(flow);
    flow = NULL;
    pcp_terminate(ctx, 0);
    test_pcp_server_sequence_stop(&server_sequence);

    PD_SOCKET_CLEANUP();
    return 0;
}
