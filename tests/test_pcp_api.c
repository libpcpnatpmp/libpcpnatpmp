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
    struct sockaddr_storage internal, suggested;

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
    // PREFER_FAILURE goes only into a MAP that suggests an external port
    TEST(pcp_flow_set_prefer_failure_opt(NULL) == PCP_ERR_BAD_ARGS);
    TEST(pcp_flow_set_prefer_failure_opt(f1) == PCP_ERR_BAD_ARGS);
    TEST(f1->pfailure_option_present == 0);
    TEST(pcp_flow_set_prefer_failure_opt(f2) == PCP_ERR_BAD_ARGS);
    TEST(f2->pfailure_option_present == 0);
    pcp_delete_flow(f2);
    // separate buffers: Sock_pton() returns the same static one every call
    TEST(sock_pton("127.0.0.1:1234", (struct sockaddr *)&internal) == 0);
    TEST(sock_pton("0.0.0.0:5678", (struct sockaddr *)&suggested) == 0);
    TEST((f2 = pcp_new_flow(ctx, (struct sockaddr *)&internal, NULL,
                            (struct sockaddr *)&suggested, IPPROTO_TCP, 100,
                            NULL)) != NULL);
    TEST(f2->map_peer.ext_port == htons(5678));
    TEST(pcp_flow_set_prefer_failure_opt(f2) == PCP_ERR_SUCCESS);
    TEST(pcp_flow_set_prefer_failure_opt(f2) == PCP_ERR_SUCCESS);
    TEST(f2->pfailure_option_present == 1);

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
        // a refused PREFER_FAILURE leaves the flow for every server unchanged
        {
            struct sockaddr_storage peer;
            pcp_flow_t *fp;

            TEST(sock_pton("127.0.0.3:4321", (struct sockaddr *)&peer) == 0);
            TEST((fp = pcp_new_flow(ctx, (struct sockaddr *)&internal,
                                    (struct sockaddr *)&peer, NULL, IPPROTO_TCP,
                                    100, NULL)) != NULL);
            TEST(fp->next_child != NULL);
            TEST(pcp_flow_set_prefer_failure_opt(fp) == PCP_ERR_BAD_ARGS);
            TEST(fp->pfailure_option_present == 0);
            TEST(fp->next_child->pfailure_option_present == 0);
            pcp_delete_flow(fp);
        }
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
        // a reply carrying the new nonce finds its flow
        TEST(pcp_get_flow(&f1->kd, get_pcp_server(ctx, f1->pcp_server_indx)) ==
             f1);
        TEST(pcp_get_flow(
                 &f1->next_child->kd,
                 get_pcp_server(ctx, f1->next_child->pcp_server_indx)) ==
             f1->next_child);

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

    // FILTER prefix length on the wire (RFC 6887 section 13.3)
    {
        struct sockaddr_storage unspec;

        // IPv4 1..32 is sent plus 96, on every child flow
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 12);
        TEST(f1->filter_option_present == 1);
        TEST(f1->filter_prefix == 108);
        TEST(f1->filter_port == htons(4444));
        TEST(IN6_IS_ADDR_V4MAPPED(&f1->filter_ip));
        TEST(f1->next_child->filter_option_present == 1);
        TEST(f1->next_child->filter_prefix == 108);
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 1);
        TEST(f1->filter_prefix == 97);
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 32);
        TEST(f1->filter_prefix == 128);
        // IPv4 96..128 is taken as already adjusted; 96 is any IPv4 peer
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 100);
        TEST(f1->filter_prefix == 100);
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 96);
        TEST(f1->filter_prefix == 96);
        TEST(IN6_IS_ADDR_V4MAPPED(&f1->filter_ip));
        // 0 removes all filters: ::/0, port 0, whatever peer is given
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 0);
        TEST(f1->filter_prefix == 0);
        TEST(f1->filter_port == 0);
        TEST(IN6_IS_ADDR_UNSPECIFIED(&f1->filter_ip));
        TEST(f1->next_child->filter_prefix == 0);
        TEST(IN6_IS_ADDR_UNSPECIFIED(&f1->next_child->filter_ip));
        // IPv4 given as IPv4-mapped IPv6 is adjusted the same way
        pcp_flow_set_filter_opt(f1, Sock_pton("[::ffff:8.8.8.8]:4444"), 12);
        TEST(f1->filter_prefix == 108);
        pcp_flow_set_filter_opt(f1, Sock_pton("[::ffff:8.8.8.8]:4444"), 32);
        TEST(f1->filter_prefix == 128);
        // IPv6 is sent unchanged
        pcp_flow_set_filter_opt(f1, Sock_pton("[2001:db8::1]:4444"), 64);
        TEST(f1->filter_prefix == 64);
        TEST(!IN6_IS_ADDR_V4MAPPED(&f1->filter_ip));
        pcp_flow_set_filter_opt(f1, Sock_pton("[2001:db8::1]:4444"), 0);
        TEST(f1->filter_prefix == 0);
        TEST(f1->filter_port == 0);
        TEST(IN6_IS_ADDR_UNSPECIFIED(&f1->filter_ip));
        pcp_flow_set_filter_opt(f1, Sock_pton("[2001:db8::1]:4444"), 128);
        TEST(f1->filter_prefix == 128);

        // invalid input leaves the flow unchanged
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 33);
        TEST(f1->filter_prefix == 128);
        TEST(!IN6_IS_ADDR_V4MAPPED(&f1->filter_ip));
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 95);
        TEST(f1->filter_prefix == 128);
        pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 129);
        TEST(f1->filter_prefix == 128);
        pcp_flow_set_filter_opt(f1, Sock_pton("[2001:db8::1]:4444"), 200);
        TEST(f1->filter_prefix == 128);
        pcp_flow_set_filter_opt(f1, NULL, 12);
        TEST(f1->filter_prefix == 128);
        pcp_flow_set_filter_opt(NULL, Sock_pton("8.8.8.8:4444"), 12);
        memset(&unspec, 0, sizeof(unspec));
        unspec.ss_family = AF_UNSPEC;
        pcp_flow_set_filter_opt(f1, (struct sockaddr *)&unspec, 12);
        TEST(f1->filter_prefix == 128);
        TEST(!IN6_IS_ADDR_V4MAPPED(&f1->filter_ip));

        // an invalid call on a flow without the option does not add it
        TEST(f1->next_child->next_child == NULL);
        TEST((f2 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1235"), NULL, NULL,
                                IPPROTO_TCP, 100, NULL)) != NULL);
        pcp_flow_set_filter_opt(f2, Sock_pton("8.8.8.8:4444"), 64);
        TEST(f2->filter_option_present == 0);
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

    // regression test: a global source keeps no scope, whatever the caller
    // passed, since replies arrive at a global address and carry none; only
    // a link-local source has one, which must be the server's
    {
        struct sockaddr_in6 global_src;
        struct sockaddr_in6 link_local_src;

        memcpy(&global_src, Sock_pton("[2001:db8::1]:1234"),
               sizeof(global_src));
        global_src.sin6_scope_id = 1;
        memcpy(&link_local_src, Sock_pton("[fe80::2]:1234"),
               sizeof(link_local_src));
        link_local_src.sin6_scope_id = 2;

        ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
        TEST(pcp_add_server(ctx, Sock_pton("[::1]:5351"), 2) == 0);
        TEST((f1 = pcp_new_flow(ctx, (struct sockaddr *)&global_src, NULL, NULL,
                                IPPROTO_TCP, 100, NULL)) != NULL);
        TEST(f1->kd.scope_id == 0);
        TEST(pcp_new_flow(ctx, (struct sockaddr *)&link_local_src, NULL, NULL,
                          IPPROTO_TCP, 100, NULL) == NULL);
        pcp_terminate(ctx, 1);
    }

    // a flow to a nonce-free server keeps the server's nonce, under which the
    // answers, which carry none, are looked up
    for (int version = 0; version < 2; ++version) {
        uint32_t nonce_words_be[3] = {htonl(0x00112233), htonl(0x44556677),
                                      htonl(0x8899aabb)};
        pcp_server_t *natpmp;

        ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
        TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);
        TEST(pcp_add_server(ctx, Sock_pton("127.0.0.2:5351"), 2) == 1);
        natpmp = get_pcp_server(ctx, 1);
        natpmp->pcp_version = version;
        TEST((f1 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                                IPPROTO_TCP, 100, NULL)) != NULL);
        TEST(f1->next_child != NULL);
        TEST(pcp_flow_set_nonce(f1, nonce_words_be) == 0);
        TEST(memcmp(f1->kd.nonce.n, nonce_words_be, sizeof(f1->kd.nonce.n)) ==
             0);
        TEST(memcmp(&f1->next_child->kd.nonce, &natpmp->nonce,
                    sizeof(natpmp->nonce)) == 0);
        pcp_terminate(ctx, 1);
    }

    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(ctx != NULL);
    pcp_set_restart_delay(ctx, UINT32_MAX);
    TEST(ctx->restart_delay_max_ms == 5000);
    pcp_set_restart_delay(ctx, 0);
    TEST(ctx->restart_delay_max_ms == 0);
    pcp_terminate(ctx, 1);

    printf("Tests succeeded.\n\n");

    PD_SOCKET_CLEANUP();
    return 0;
}
