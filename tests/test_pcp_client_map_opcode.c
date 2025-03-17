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

#include "pcp.h"
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

    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_DEBUG;

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

    TEST(pcp_wait(flow, 3000, 0) == pcp_state_succeeded);

    pcp_close_flow(flow);
    pcp_delete_flow(flow);
    flow = NULL;
    pcp_terminate(ctx, 0);

    PD_SOCKET_CLEANUP();
    return 0;
}
