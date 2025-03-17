/*
 *------------------------------------------------------------------
 * test_pcp_client_db.c
 *
 * May 7, 2013, ptatrai
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
#include "pcp_utils.h"
#include "test_macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ret_0_func(pcp_server_t *f UNUSED, void *data UNUSED) { return 0; }

static int ret_1_func(pcp_server_t *f UNUSED, void *data UNUSED) { return 1; }

static void test_pcp_server_functions(pcp_ctx_t *ctx) {
    struct in6_addr ip4;
#ifdef PCP_USE_IPV6_SOCKET
    struct in6_addr ip6;
#endif
    pcp_server_t *s1 = NULL, *s2 = NULL;
    pcp_server_t *sret;
    int si1, si2;
    S6_ADDR32(&ip4)[0] = 0;
    S6_ADDR32(&ip4)[1] = 0;
    S6_ADDR32(&ip4)[2] = htonl(0xffff);
    S6_ADDR32(&ip4)[3] = 0x08080808;

    TEST(get_pcp_server(ctx, 1) == NULL);
    TEST(get_pcp_server(ctx, 0) == NULL);
    TEST(get_pcp_server(ctx, -1) == NULL);
    si1 = pcp_new_server(ctx, &ip4, PCP_SERVER_PORT, 0);
    TEST(si1 == 0);
    s1 = get_pcp_server(ctx, si1);
    TEST(s1 != NULL);
    TEST(get_pcp_server(ctx, 1) == NULL);
    TEST(IN6_ARE_ADDR_EQUAL(&ip4, (struct in6_addr *)s1->pcp_ip));

    si2 = pcp_new_server(ctx, &ip4, PCP_SERVER_PORT, 0);
    TEST(si2 == 1);
    s2 = get_pcp_server(ctx, 1);
    TEST(s2 != NULL);
    TEST(IN6_ARE_ADDR_EQUAL(&ip4, (struct in6_addr *)s2->pcp_ip));
    TEST(get_pcp_server(ctx, 5) == NULL);

#ifdef PCP_USE_IPV6_SOCKET
    S6_ADDR32(&ip6)[0] = 0;
    S6_ADDR32(&ip6)[1] = 1;
    S6_ADDR32(&ip6)[2] = 2;
    S6_ADDR32(&ip6)[3] = 3;

    si2 = pcp_new_server(ctx, &ip6, PCP_SERVER_PORT, 0);
    TEST(si2 == 2);
    s2 = get_pcp_server(ctx, 2);
    TEST(s2 != NULL);
    TEST(IN6_ARE_ADDR_EQUAL(&ip6, (struct in6_addr *)s2->pcp_ip));
    TEST(get_pcp_server(ctx, 5) == NULL);

    TEST(pcp_new_server(ctx, &ip6, PCP_SERVER_PORT, 0) == 3);
    TEST(pcp_new_server(ctx, &ip6, PCP_SERVER_PORT, 0) == 4);
    TEST(pcp_new_server(ctx, &ip6, PCP_SERVER_PORT, 0) == 5);
    TEST(pcp_new_server(ctx, &ip6, PCP_SERVER_PORT, 0) == 6);

    sret = get_pcp_server_by_ip(ctx, &ip6, 0);
    TEST(sret != NULL);
    TEST(si2 == (int)sret->index);
#endif
    sret = get_pcp_server_by_ip(ctx, &ip4, 0);
    TEST(sret != NULL);
    TEST(si1 == (int)sret->index);

    pcp_db_free_pcp_servers(ctx);

    TEST(get_pcp_server(ctx, 1) == NULL);
    TEST(get_pcp_server(ctx, 0) == NULL);
    TEST(get_pcp_server(ctx, -1) == NULL);
    si1 = pcp_new_server(ctx, &ip4, PCP_SERVER_PORT, 0);
    TEST(si1 == 0);
    s1 = get_pcp_server(ctx, si1);
    TEST(s1 != NULL);
    TEST(get_pcp_server(ctx, 1) == NULL);
    TEST(IN6_ARE_ADDR_EQUAL(&ip4, (struct in6_addr *)s1->pcp_ip));

#ifdef PCP_USE_IPV6_SOCKET
    si2 = pcp_new_server(ctx, &ip6, PCP_SERVER_PORT, 0);
    TEST(si2 == 1);
    s2 = get_pcp_server(ctx, si2);
    TEST(s2 != NULL);
    TEST((s2->af == AF_INET6) &&
         (IN6_ARE_ADDR_EQUAL(&ip6, (struct in6_addr *)s2->pcp_ip)));
    TEST(get_pcp_server(ctx, 5) == NULL);
#endif
    TEST(pcp_db_foreach_server(ctx, ret_0_func, NULL) == PCP_ERR_MAX_SIZE);
    TEST(pcp_db_foreach_server(ctx, ret_1_func, NULL) == 0);
}

static int ret_func(pcp_flow_t *f, void *data) {
    *(pcp_flow_t **)data = f;
    return 1;
}

static void test_pcp_flow_funcs(pcp_ctx_t *ctx) {
    pcp_flow_t *f1, *f2, *f3;
    uint32_t bucket, cnt;
    struct flow_key_data fkd;
    struct flow_key_data fkd2;
    fkd.operation = 1;
    memset(&fkd.src_ip, 0, sizeof(fkd.src_ip));
    fkd2 = fkd;
    cnt = 0;

    fkd.map_peer.protocol = 1;
    fkd2.map_peer.protocol = 1;
    fkd2.map_peer.dst_port++;

    f1 = pcp_create_flow(get_pcp_server(ctx, 0), &fkd);
    TEST(f1 != NULL);
    TEST(pcp_db_add_flow(f1) == PCP_ERR_SUCCESS);
    bucket = f1->key_bucket;

    f2 = pcp_create_flow(get_pcp_server(ctx, 0), &fkd);
    TEST(f2 != NULL);
    TEST(pcp_db_rem_flow(f2) != PCP_ERR_SUCCESS);
    TEST(pcp_db_add_flow(f2) == PCP_ERR_SUCCESS);
    TEST(f2->key_bucket == f1->key_bucket);

    f3 = pcp_create_flow(get_pcp_server(ctx, 0), &fkd2);
    TEST(f3 != NULL);
    TEST(pcp_db_add_flow(f3) == PCP_ERR_SUCCESS);
    TEST(f3->key_bucket != bucket);

    TEST(pcp_db_add_flow(NULL) != PCP_ERR_SUCCESS);

    TEST(pcp_db_add_flow(pcp_create_flow(get_pcp_server(ctx, 0), &fkd)) ==
         PCP_ERR_SUCCESS);
    TEST(pcp_db_add_flow(pcp_create_flow(get_pcp_server(ctx, 0), &fkd2)) ==
         PCP_ERR_SUCCESS);
    TEST(pcp_db_add_flow(pcp_create_flow(get_pcp_server(ctx, 0), &fkd2)) ==
         PCP_ERR_SUCCESS);
    TEST(pcp_db_rem_flow(f2) == PCP_ERR_SUCCESS);

    f2 = pcp_create_flow(get_pcp_server(ctx, 0), &fkd);
    TEST(f2 != NULL);
    TEST(pcp_db_add_flow(f2) == PCP_ERR_SUCCESS);
    TEST(f2->key_bucket == f1->key_bucket);

    f2->pcp_msg_buffer = (char *)malloc(10);
    f2->pcp_msg_len = 10;
    TEST(pcp_db_rem_flow(f2) == PCP_ERR_SUCCESS);
    TEST(pcp_db_rem_flow(f2) != PCP_ERR_SUCCESS);
    TEST((f2->pcp_msg_buffer != NULL) && (f2->pcp_msg_len == 10));

    TEST(pcp_get_flow(&fkd, get_pcp_server(ctx, 0)) == f1);
    TEST(pcp_get_flow(&fkd, get_pcp_server(ctx, 1)) == NULL);
    TEST(pcp_get_flow(&fkd2, get_pcp_server(ctx, 0)) == f3);

    pcp_flow_clear_msg_buf(NULL);
    f3->pcp_msg_buffer = (char *)malloc(10);
    f3->pcp_msg_len = 10;
    pcp_flow_clear_msg_buf(f3);
    TEST((f3->pcp_msg_len == 0) && (f3->pcp_msg_buffer == NULL));
    f3->pcp_msg_buffer = (char *)malloc(10);
    f3->pcp_msg_len = 10;

    TEST(pcp_delete_flow_intern(f3) == PCP_ERR_SUCCESS);
    fkd2.map_peer.src_port++;
    TEST(pcp_get_flow(&fkd2, get_pcp_server(ctx, 0)) == NULL);
    TEST(pcp_get_flow(NULL, get_pcp_server(ctx, 0)) == NULL);

#ifdef PCP_EXPERIMENTAL
    pcp_db_add_md(f1, 11, "test", sizeof("test"));
    TEST(f1->md_val_count == 1);
    pcp_db_add_md(f1, 11, "atest", sizeof("atest"));
    TEST(f1->md_val_count == 1);
    pcp_db_add_md(f1, 13, "atest", sizeof("atest"));
    pcp_db_add_md(f1, 13, NULL, 0);
    pcp_db_add_md(f1, 0, NULL, 0);
    TEST(f1->md_val_count == 3);
#endif

    TEST(pcp_delete_flow_intern(f1) == PCP_ERR_SUCCESS);
    TEST(pcp_delete_flow_intern(f2) == PCP_ERR_SUCCESS);

    while (pcp_db_foreach_flow(ctx, ret_func, &f1) == PCP_ERR_SUCCESS) {
        pcp_delete_flow_intern(f1);
        cnt++;
    }
    TEST(cnt == 3);
}

int main(void) {
    pcp_ctx_t *ctx;
    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_NONE;
    ctx = pcp_init(0, NULL);
    TEST(ctx != NULL);
    test_pcp_server_functions(ctx);
    test_pcp_flow_funcs(ctx);

    PD_SOCKET_CLEANUP();
    printf("Tests succeeded.\n\n");
    return 0;
}
