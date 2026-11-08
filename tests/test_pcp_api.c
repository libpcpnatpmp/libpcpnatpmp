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

/* build the request of the MAP flow f and count its options of type code;
 * *first_prefix gets the prefix length of the first FILTER option */
static int count_options(pcp_flow_t *f, uint8_t code, int *first_prefix) {
    pcp_request_t *req = (pcp_request_t *)build_pcp_msg(f);
    const uint8_t *p, *end;
    int n = 0;

    if (first_prefix) {
        *first_prefix = -1;
    }
    if (req == NULL) {
        return -1;
    }
    p = ((pcp_map_v2_t *)req->next_data)->next_data;
    end = (const uint8_t *)f->pcp_msg_buffer + f->pcp_msg_len;
    while (p + sizeof(pcp_options_hdr_t) <= end) {
        uint16_t len = (uint16_t)((p[2] << 8) | p[3]);

        if (p[0] == code) {
            if (n == 0 && first_prefix && code == PCP_OPTION_FILTER) {
                *first_prefix = ((const pcp_filter_option_t *)p)->filter_prefix;
            }
            n++;
        }
        p += sizeof(pcp_options_hdr_t) + len;
    }
    return n;
}

int main(void) {
    pcp_flow_t *f1, *f2;
    pcp_ctx_t *ctx;
    struct sockaddr_storage internal, suggested;
    struct sockaddr_storage int_port0, int_port1, peer_port0, peer_port1;
    struct sockaddr_storage int6, peer6;

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
    // no IPv6 server; separate buffers, as Sock_pton() returns the same
    // static one each call
    TEST(sock_pton("[::1]:1234", (struct sockaddr *)&int6) == 0);
    TEST(sock_pton("[::1]:4321", (struct sockaddr *)&peer6) == 0);
    TEST((pcp_new_flow(ctx, (struct sockaddr *)&int6, (struct sockaddr *)&peer6,
                       NULL, IPPROTO_TCP, 100, NULL)) == NULL);

    pcp_pulse(NULL, NULL);
    pcp_pulse(ctx, NULL);
    pcp_flow_get_info(NULL, NULL);

    // PCP PEER/MAP tests
    TEST(pcp_new_flow(NULL, NULL, NULL, NULL, 0, 0, NULL) == NULL);
    TEST(pcp_new_flow(ctx, NULL, NULL, NULL, 0, 0, NULL) == NULL);
    // a MAP for all protocols (0) is for all ports too
    TEST(pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL, 0, 100,
                      NULL) == NULL);
    TEST((f1 = pcp_new_flow(ctx, Sock_pton("127.0.0.1"), NULL, NULL, 0, 100,
                            NULL)) != NULL);
    pcp_delete_flow(f1);

    // a PEER needs a protocol, an internal port and a remote peer port
    TEST(sock_pton("127.0.0.1", (struct sockaddr *)&int_port0) == 0);
    TEST(sock_pton("127.0.0.1:1234", (struct sockaddr *)&int_port1) == 0);
    TEST(sock_pton("127.0.0.2", (struct sockaddr *)&peer_port0) == 0);
    TEST(sock_pton("127.0.0.2:4321", (struct sockaddr *)&peer_port1) == 0);
    TEST(pcp_new_flow(ctx, (struct sockaddr *)&int_port1,
                      (struct sockaddr *)&peer_port1, NULL, 0, 100,
                      NULL) == NULL);
    TEST(pcp_new_flow(ctx, (struct sockaddr *)&int_port0,
                      (struct sockaddr *)&peer_port1, NULL, IPPROTO_TCP, 100,
                      NULL) == NULL);
    TEST(pcp_new_flow(ctx, (struct sockaddr *)&int_port1,
                      (struct sockaddr *)&peer_port0, NULL, IPPROTO_TCP, 100,
                      NULL) == NULL);
    TEST((f1 = pcp_new_flow(ctx, (struct sockaddr *)&int_port1,
                            (struct sockaddr *)&peer_port1, NULL, IPPROTO_TCP,
                            100, NULL)) != NULL);
    pcp_delete_flow(f1);

    TEST((f1 = pcp_new_flow(ctx, (struct sockaddr *)&int_port1,
                            (struct sockaddr *)&peer_port1, NULL, IPPROTO_TCP,
                            100, NULL)) != NULL);
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

        // one FILTER option that each call replaces, so every call can be
        // checked on filters[0]
        TEST(pcp_flow_set_filter_mode(f1, PCP_FILTER_MODE_ONE) ==
             PCP_ERR_SUCCESS);

        // IPv4 1..32 is sent plus 96, on every child flow
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 12) ==
             PCP_ERR_SUCCESS);
        TEST(f1->filter_count == 1);
        TEST(f1->filters[0].prefix == 108);
        TEST(f1->filters[0].port == htons(4444));
        TEST(IN6_IS_ADDR_V4MAPPED(&f1->filters[0].ip));
        TEST(f1->next_child->filter_count == 1);
        TEST(f1->next_child->filters[0].prefix == 108);
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 1) ==
             PCP_ERR_SUCCESS);
        TEST(f1->filters[0].prefix == 97);
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 32) ==
             PCP_ERR_SUCCESS);
        TEST(f1->filters[0].prefix == 128);
        // IPv4 96..128 is taken as already adjusted; 96 is any IPv4 peer
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 100) ==
             PCP_ERR_SUCCESS);
        TEST(f1->filters[0].prefix == 100);
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 96) ==
             PCP_ERR_SUCCESS);
        TEST(f1->filters[0].prefix == 96);
        TEST(IN6_IS_ADDR_V4MAPPED(&f1->filters[0].ip));
        // 0 removes all filters, whatever peer is given
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 0) ==
             PCP_ERR_SUCCESS);
        TEST(f1->filter_count == 0);
        TEST(f1->next_child->filter_count == 0);
        // IPv4 given as IPv4-mapped IPv6 is adjusted the same way
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("[::ffff:8.8.8.8]:4444"),
                                     12) == PCP_ERR_SUCCESS);
        TEST(f1->filters[0].prefix == 108);
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("[::ffff:8.8.8.8]:4444"),
                                     32) == PCP_ERR_SUCCESS);
        TEST(f1->filters[0].prefix == 128);
        // IPv6 is sent unchanged
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("[2001:db8::1]:4444"), 64) ==
             PCP_ERR_SUCCESS);
        TEST(f1->filters[0].prefix == 64);
        TEST(!IN6_IS_ADDR_V4MAPPED(&f1->filters[0].ip));
        // IPv6 prefix length 0 removes the filters too
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("[2001:db8::1]:4444"), 0) ==
             PCP_ERR_SUCCESS);
        TEST(f1->filter_count == 0);
        TEST(f1->next_child->filter_count == 0);
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("[2001:db8::1]:4444"),
                                     128) == PCP_ERR_SUCCESS);
        TEST(f1->filters[0].prefix == 128);

        // invalid input leaves the flow unchanged
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 33) ==
             PCP_ERR_BAD_ARGS);
        TEST(f1->filters[0].prefix == 128);
        TEST(!IN6_IS_ADDR_V4MAPPED(&f1->filters[0].ip));
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 95) ==
             PCP_ERR_BAD_ARGS);
        TEST(f1->filters[0].prefix == 128);
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("8.8.8.8:4444"), 129) ==
             PCP_ERR_BAD_ARGS);
        TEST(f1->filters[0].prefix == 128);
        TEST(pcp_flow_set_filter_opt(f1, Sock_pton("[2001:db8::1]:4444"),
                                     200) == PCP_ERR_BAD_ARGS);
        TEST(f1->filters[0].prefix == 128);
        TEST(pcp_flow_set_filter_opt(f1, NULL, 12) == PCP_ERR_BAD_ARGS);
        TEST(f1->filters[0].prefix == 128);
        TEST(pcp_flow_set_filter_opt(NULL, Sock_pton("8.8.8.8:4444"), 12) ==
             PCP_ERR_BAD_ARGS);
        memset(&unspec, 0, sizeof(unspec));
        unspec.ss_family = AF_UNSPEC;
        TEST(pcp_flow_set_filter_opt(f1, (struct sockaddr *)&unspec, 12) ==
             PCP_ERR_BAD_ARGS);
        TEST(f1->filters[0].prefix == 128);
        TEST(!IN6_IS_ADDR_V4MAPPED(&f1->filters[0].ip));

        // an invalid call on a flow without FILTER options (in the default
        // PCP_FILTER_MODE_MULTI) does not add one
        TEST(f1->next_child->next_child == NULL);
        TEST((f2 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1235"), NULL, NULL,
                                IPPROTO_TCP, 100, NULL)) != NULL);
        TEST(pcp_flow_set_filter_opt(f2, Sock_pton("8.8.8.8:4444"), 64) ==
             PCP_ERR_BAD_ARGS);
        TEST(f2->filter_count == 0);
        // nor does it add a filter next to a valid one
        TEST(pcp_flow_set_filter_opt(f2, Sock_pton("[::ffff:8.8.8.8]:4444"),
                                     12) == PCP_ERR_SUCCESS);
        TEST(f2->filter_count == 1);
        TEST(f2->filters[0].prefix == 108);
        TEST(pcp_flow_set_filter_opt(f2, Sock_pton("9.9.9.9:4444"), 33) ==
             PCP_ERR_BAD_ARGS);
        TEST(pcp_flow_set_filter_opt(f2, Sock_pton("[2001:db8::1]:4444"),
                                     129) == PCP_ERR_BAD_ARGS);
        TEST(f2->filter_count == 1);
        // the same filter given as AF_INET is not added again
        TEST(pcp_flow_set_filter_opt(f2, Sock_pton("8.8.8.8:4444"), 12) ==
             PCP_ERR_SUCCESS);
        TEST(f2->filter_count == 1);
        // no request carried the replaced filters of f1: no reset needed
        TEST(f1->filter_reset == 0);
    }

    // the limit of FILTER options per flow
    TEST(ctx->max_filter_opts == PCP_MAX_FILTER_OPTIONS);
    TEST(pcp_set_max_filter_opts(NULL, 10) == PCP_ERR_BAD_ARGS);
    TEST(pcp_set_max_filter_opts(ctx, 0) == PCP_ERR_BAD_ARGS);
    TEST(ctx->max_filter_opts == PCP_MAX_FILTER_OPTIONS);

    // FILTER options in requests: written last, as many as fit; the next
    // request carries the ones the server does not have yet
    {
        pcp_flow_t *f3;
        struct sockaddr_storage f3_int, f3_ext;
        int i, n1, first_prefix;
        char addr[32];

        TEST(pcp_set_max_filter_opts(ctx, 60) == PCP_ERR_SUCCESS);
        // PREFER_FAILURE needs a suggested port; separate buffers, as
        // Sock_pton() returns the same static one every call
        TEST(sock_pton("127.0.0.1:1236", (struct sockaddr *)&f3_int) == 0);
        TEST(sock_pton("0.0.0.0:5678", (struct sockaddr *)&f3_ext) == 0);
        TEST((f3 = pcp_new_flow(ctx, (struct sockaddr *)&f3_int, NULL,
                                (struct sockaddr *)&f3_ext, IPPROTO_TCP, 100,
                                NULL)) != NULL);
        TEST(pcp_flow_set_prefer_failure_opt(f3) == PCP_ERR_SUCCESS);
        pcp_flow_set_3rd_party_opt(f3, Sock_pton("10.0.0.1"));
        for (i = 0; i < 60; i++) {
            snprintf(addr, sizeof(addr), "10.1.0.%d:80", i);
            TEST(pcp_flow_set_filter_opt(f3, Sock_pton(addr), 32) ==
                 PCP_ERR_SUCCESS);
        }
        // one more is refused; a filter already there is still accepted
        TEST(pcp_flow_set_filter_opt(f3, Sock_pton("10.9.9.9:80"), 32) ==
             PCP_ERR_MAX_SIZE);
        TEST(pcp_flow_set_filter_opt(f3, Sock_pton("10.1.0.0:80"), 32) ==
             PCP_ERR_SUCCESS);
        TEST(f3->filter_count == 60);
        TEST(f3->next_child->filter_count == 60);

        n1 = count_options(f3, PCP_OPTION_FILTER, &first_prefix);
        TEST(n1 > 0 && n1 < 60);
        TEST(first_prefix == 128);
        TEST(count_options(f3, PCP_OPTION_PREF_FAIL, NULL) == 1);
        TEST(count_options(f3, PCP_OPTION_3RD_PARTY, NULL) == 1);
        TEST(f3->pcp_msg_len <= PCP_MAX_LEN);
        TEST(count_options(f3, PCP_OPTION_FILTER, NULL) == n1);
        TEST(pcp_flow_filters_in_request(f3));
        // the server accepted them: the next request carries the rest
        TEST(pcp_flow_filters_confirmed(f3));
        TEST(!pcp_flow_filters_in_request(f3));
        TEST(pcp_flow_filters_accepted(f3) == n1);
        TEST(pcp_flow_filters_pending(f3));
        TEST(count_options(f3, PCP_OPTION_FILTER, NULL) == 60 - n1);
        TEST(pcp_flow_filters_confirmed(f3));
        TEST(pcp_flow_filters_accepted(f3) == 60);
        TEST(!pcp_flow_filters_pending(f3));
        // a renewal carries no FILTER option, but the other options
        TEST(count_options(f3, PCP_OPTION_FILTER, NULL) == 0);
        TEST(!pcp_flow_filters_in_request(f3));
        TEST(count_options(f3, PCP_OPTION_PREF_FAIL, NULL) == 1);
        TEST(count_options(f3, PCP_OPTION_3RD_PARTY, NULL) == 1);
        // the server restarted: all of them go out again
        pcp_flow_filters_resend(f3);
        TEST(count_options(f3, PCP_OPTION_FILTER, NULL) == n1);
        TEST(pcp_flow_filters_confirmed(f3));
        TEST(count_options(f3, PCP_OPTION_FILTER, NULL) == 60 - n1);
        TEST(pcp_flow_filters_confirmed(f3));
        TEST(f3->filter_reset == 0);
        // a request that deletes the mapping carries none (RFC 6887 section
        // 13.3: the server answers MALFORMED_OPTION)
        pcp_flow_filters_resend(f3);
        f3->lifetime = 0;
        TEST(count_options(f3, PCP_OPTION_FILTER, NULL) == 0);
        TEST(!pcp_flow_filters_pending(f3));
        f3->lifetime = 100;
        TEST(pcp_flow_filters_pending(f3));
#ifdef PCP_EXPERIMENTAL
        // a long user ID leaves room for fewer FILTER options, written last
        {
            pcp_userid_option_t uid;
            int n;

            memset(&uid, 0, sizeof(uid));
            memset(uid.userid, 'u', MAX_USER_ID - 1);
            pcp_flow_set_userid(f3, &uid);
            n = count_options(f3, PCP_OPTION_FILTER, NULL);
            TEST(n > 0 && n < n1);
            TEST(count_options(f3, PCP_OPTION_USERID, NULL) == 1);
            TEST(count_options(f3, PCP_OPTION_3RD_PARTY, NULL) == 1);
            TEST(f3->pcp_msg_len <= PCP_MAX_LEN);
            memset(&uid, 0, sizeof(uid));
            pcp_flow_set_userid(f3, &uid);
        }
#endif
        for (i = 0; i < f3->filter_count; i++) {
            f3->filters[i].state = PCP_FILTER_CONFIRMED;
        }

        // PCP_FILTER_MODE_ONE keeps the last filter in the list; the filters
        // went out, so the request first removes the server's filters
        TEST(pcp_flow_set_filter_mode(f3, PCP_FILTER_MODE_ONE) ==
             PCP_ERR_SUCCESS);
        TEST(f3->filter_count == 1);
        TEST(f3->filters[0].ip.s6_addr[15] == 59);
        TEST(f3->next_child->filter_count == 1);
        TEST(f3->filter_reset == 1);
        TEST(count_options(f3, PCP_OPTION_FILTER, &first_prefix) == 2);
        TEST(first_prefix == 0);

        // clearing sends only the FILTER that removes the server's filters,
        // which counts as a FILTER the request carried
        pcp_flow_clear_filter_opts(f3);
        TEST(f3->filter_count == 0);
        TEST(f3->next_child->filter_count == 0);
        TEST(count_options(f3, PCP_OPTION_FILTER, &first_prefix) == 1);
        TEST(first_prefix == 0);
        TEST(pcp_flow_filters_in_request(f3));

        // an invalid mode is refused and ignored
        TEST(pcp_flow_set_filter_mode(f3, (pcp_filter_mode_e)7) ==
             PCP_ERR_BAD_ARGS);
        TEST(f3->filter_mode == PCP_FILTER_MODE_ONE);
        TEST(pcp_flow_set_filter_mode(NULL, PCP_FILTER_MODE_ONE) ==
             PCP_ERR_BAD_ARGS);
    }

    // a filter goes to the flow and all its children, or to none of them
    {
        pcp_flow_t *f4;
        int i;
        char addr[32];

        TEST(pcp_set_max_filter_opts(ctx, 10) == PCP_ERR_SUCCESS);
        TEST((f4 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1237"), NULL, NULL,
                                IPPROTO_TCP, 100, NULL)) != NULL);
        TEST(f4->next_child != NULL);
        for (i = 0; i < 9; i++) {
            snprintf(addr, sizeof(addr), "10.2.0.%d:80", i);
            TEST(pcp_flow_set_filter_opt(f4, Sock_pton(addr), 32) ==
                 PCP_ERR_SUCCESS);
        }
        // the child alone gets one more filter, so it is full
        TEST(pcp_flow_set_filter_opt(f4->next_child, Sock_pton("10.2.0.100:80"),
                                     32) == PCP_ERR_SUCCESS);
        TEST(f4->filter_count == 9);
        TEST(f4->next_child->filter_count == 10);
        // a new filter fits on the flow but not on the child: neither gets it
        TEST(pcp_flow_set_filter_opt(f4, Sock_pton("10.2.0.101:80"), 32) ==
             PCP_ERR_MAX_SIZE);
        TEST(f4->filter_count == 9);
        TEST(f4->next_child->filter_count == 10);
        // the child's extra filter is already there, so the flow takes it
        TEST(pcp_flow_set_filter_opt(f4, Sock_pton("10.2.0.100:80"), 32) ==
             PCP_ERR_SUCCESS);
        TEST(f4->filter_count == 10);
        TEST(f4->next_child->filter_count == 10);
    }

    // at the largest limit the count of filters cannot wrap: the next
    // filter is refused
    {
        pcp_flow_t *f6;

        TEST(pcp_set_max_filter_opts(ctx, UINT16_MAX) == PCP_ERR_SUCCESS);
        TEST((f6 = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1239"), NULL, NULL,
                                IPPROTO_TCP, 100, NULL)) != NULL);
        f6->filters =
            (struct pcp_flow_filter *)calloc(UINT16_MAX, sizeof(*f6->filters));
        TEST(f6->filters != NULL);
        f6->filter_cap = UINT16_MAX;
        f6->filter_count = UINT16_MAX;
        TEST(pcp_flow_set_filter_opt(f6, Sock_pton("10.4.0.1:80"), 32) ==
             PCP_ERR_MAX_SIZE);
        TEST(f6->filter_count == UINT16_MAX);
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
