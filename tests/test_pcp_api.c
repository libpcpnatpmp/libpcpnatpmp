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
#include "pcp_msg.h"
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

    // regression test: explicitly set nonce for all child flows and serialize
    {
        uint32_t nonce_words_be[3] = {htonl(0x00112233), htonl(0x44556677),
                                      htonl(0x8899aabb)};
        pcp_request_t *req;
        pcp_map_v2_t *map;

        pcp_terminate(ctx, 1);
        ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
        TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);
        TEST(pcp_add_server(ctx, Sock_pton("127.0.0.2:5351"), 2) == 1);
        TEST((f1 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                                IPPROTO_TCP, 100, NULL)) != NULL);
        TEST(f1->next_child != NULL);
        TEST(f1->next_child->next_child == NULL);
        TEST(pcp_flow_set_nonce(NULL, nonce_words_be) == 1);
        TEST(pcp_flow_set_nonce(f1, NULL) == 1);
        TEST(pcp_flow_set_nonce(f1, nonce_words_be) == 0);
        TEST(memcmp(f1->kd.nonce.n, nonce_words_be, sizeof(f1->kd.nonce.n)) ==
             0);
        TEST(memcmp(f1->next_child->kd.nonce.n, nonce_words_be,
                    sizeof(f1->next_child->kd.nonce.n)) == 0);

        req = (pcp_request_t *)build_pcp_msg(f1);
        TEST(req != NULL);
        TEST(req->ver == 2);
        TEST((req->r_opcode & 0x7f) == PCP_OPCODE_MAP);
        map = (pcp_map_v2_t *)req->next_data;
        TEST(memcmp(map->nonce.n, nonce_words_be, sizeof(map->nonce.n)) == 0);

        req = (pcp_request_t *)build_pcp_msg(f1->next_child);
        TEST(req != NULL);
        map = (pcp_map_v2_t *)req->next_data;
        TEST(memcmp(map->nonce.n, nonce_words_be, sizeof(map->nonce.n)) == 0);
    }

    pcp_terminate(ctx, 1);

    // regression test: pcp_terminate(close_flows=1) closes and removes flows
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);
    TEST((f1 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                            IPPROTO_TCP, 100, NULL)) != NULL);
    TEST(ctx->pcp_db.flow_cnt > 0);
    pcp_terminate(ctx, 1);
    TEST(ctx->pcp_db.flow_cnt == 0);

    printf("Tests succeeded.\n\n");

    PD_SOCKET_CLEANUP();
    return 0;
}
