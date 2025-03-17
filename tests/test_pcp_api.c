/*
 *------------------------------------------------------------------
 * test_pcp_api.c
 *
 * May 10, 2013, ptatrai
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

#include "pcp_client_db.h"
#include "pcp_socket.h"
#include "test_macro.h"
#include "unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {
    pcp_flow_t *f1, *f2;
    pcp_ctx_t *ctx;

    pcp_log_level = PCP_LOGLVL_NONE;

    PD_SOCKET_STARTUP();
    // test pcp_init & terminate
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(get_pcp_server(ctx, 0) == NULL);
    pcp_terminate(ctx, 1);
    ctx = pcp_init(ENABLE_AUTODISCOVERY, NULL);
    TEST(get_pcp_server(ctx, 0) != NULL);
    pcp_terminate(ctx, 1);
    TEST(get_pcp_server(ctx, 0) == NULL);

    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);
#ifdef PCP_USE_IPV6_SOCKET
    TEST(pcp_add_server(ctx, Sock_pton("[::1]:5351"), 1) == 1);
#endif
    pcp_terminate(ctx, 1);

#ifdef PCP_SADSCP
    // TEST learn DSCP
    {
        pcp_flow_t *l1;
        ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
        TEST((l1 = pcp_learn_dscp(ctx, 1, 1, 1, NULL)) ==
             NULL); // NO PCP server to send req

        TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);
        TEST((l1 = pcp_learn_dscp(ctx, 1, 1, 1, NULL)) != NULL);
        TEST(l1->kd.operation == PCP_OPCODE_SADSCP);
        TEST(l1->sadscp_app_name == NULL);
        TEST(l1->sadscp.app_name_length == 0);
        TEST(l1->sadscp.toler_fields == 84);
        pcp_close_flow(l1);
        pcp_delete_flow(l1);

        TEST((l1 = pcp_learn_dscp(ctx, 2, 2, 2, "test")) != NULL);
        TEST(l1->sadscp.app_name_length == 4);
        TEST(strncmp(l1->sadscp_app_name, "test", l1->sadscp.app_name_length) ==
             0);
        TEST(l1->sadscp.toler_fields == 168);

        pcp_terminate(ctx, 1);
    }
#endif

    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);
    TEST((pcp_new_flow(ctx, Sock_pton("[::1]:1234"), Sock_pton("[::1]"), NULL,
                       IPPROTO_TCP, 100, NULL)) == NULL);

    pcp_pulse(NULL, NULL);
    pcp_pulse(ctx, NULL);
    pcp_flow_get_info(NULL, NULL);

    // PCP PEER/MAP tests
    TEST(pcp_new_flow(NULL, NULL, NULL, NULL, 0, 0, NULL) == NULL);
    TEST(pcp_new_flow(ctx, NULL, NULL, NULL, 0, 0, NULL) == NULL);

    TEST((f1 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"),
                            Sock_pton("127.0.0.1"), NULL, IPPROTO_TCP, 100,
                            NULL)) != NULL);
    TEST((f2 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                            IPPROTO_TCP, 100, NULL)) != NULL);
    pcp_flow_set_prefer_failure_opt(f2);
    pcp_flow_set_prefer_failure_opt(f2);

    pcp_flow_set_lifetime(f1, 1000);
    TEST((f1->lifetime) >= 99);
    TEST((f1->timeout.tv_sec > 0) || (f1->timeout.tv_usec > 0));

    f1->timeout.tv_sec = 0;
    f1->timeout.tv_usec = 0;
    pcp_flow_set_lifetime(f1, 0);
    TEST(f1->lifetime == 0);
    TEST((f1->timeout.tv_sec > 0) || (f1->timeout.tv_usec > 0));

    printf("Tests succeeded.\n\n");

    PD_SOCKET_CLEANUP();
    return 0;
}
