/*
 *------------------------------------------------------------------
 * test_pcp_msg.c
 *
 * May, 2013, ptatrai
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

#include "pcpnatpmp.h"

#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    pcp_ctx_t *ctx;
    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();
    ctx = pcp_init(0, NULL);
#ifndef PCP_DISABLE_NATPMP
    { // TEST NAT-PMP - parsing
        nat_pmp_announce_resp_t natpmp_a;
        nat_pmp_map_resp_t natpmp_mt;
        nat_pmp_map_resp_t natpmp_mu;
        pcp_recv_msg_t msg;

        natpmp_a.ver = 0;
        natpmp_a.opcode = 0;
        natpmp_a.result = htons(11);
        natpmp_a.epoch = htonl(1234);
        natpmp_a.ext_ip = 0x01020304;

        natpmp_mt.ver = 0;
        natpmp_mt.opcode = NATPMP_OPCODE_MAP_TCP;
        natpmp_mt.result = htons(11);
        natpmp_mt.epoch = htonl(1234);
        natpmp_mt.ext_port = htons(1234);
        natpmp_mt.int_port = htons(3456);
        natpmp_mt.lifetime = htonl(2233);

        natpmp_mu.ver = 0;
        natpmp_mu.opcode = NATPMP_OPCODE_MAP_UDP;
        natpmp_mu.result = htons(11);
        natpmp_mu.epoch = htonl(1234);
        natpmp_mu.ext_port = htons(1234);
        natpmp_mu.int_port = htons(3456);
        natpmp_mu.lifetime = htonl(2233);

        memset(&msg, 0, sizeof(msg));
        memcpy(&msg.pcp_msg_buffer, &natpmp_a, sizeof(natpmp_a));
        msg.pcp_msg_len = sizeof(natpmp_a);
        TEST(parse_response(&msg) == PCP_ERR_SUCCESS);
        TEST(msg.recv_version == 0);
        TEST(msg.recv_epoch == 1234);
        TEST(msg.recv_result == 11);
        msg.pcp_msg_len = sizeof(natpmp_a) - 1;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);

        natpmp_a.ver++;
        memset(&msg, 0, sizeof(msg));
        memcpy(&msg.pcp_msg_buffer, &natpmp_a, sizeof(natpmp_a));
        msg.pcp_msg_len = sizeof(natpmp_a);
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);

        msg.pcp_msg_len = sizeof(pcp_response_t);
        TEST(parse_response(&msg) == PCP_ERR_SUCCESS);
        TEST(msg.recv_version == 1);
        TEST(msg.recv_result == 11);

        natpmp_a.ver++;
        memset(&msg, 0, sizeof(msg));
        memcpy(&msg.pcp_msg_buffer, &natpmp_a, sizeof(natpmp_a));
        msg.pcp_msg_len = sizeof(natpmp_a);
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);

        msg.pcp_msg_len = sizeof(pcp_response_t);
        TEST(parse_response(&msg) == PCP_ERR_SUCCESS);
        TEST(msg.recv_version == 2);
        TEST(msg.recv_result == 11);

        memset(&msg, 0, sizeof(msg));
        memcpy(&msg.pcp_msg_buffer, &natpmp_mt, sizeof(natpmp_mt));
        msg.pcp_msg_len = sizeof(natpmp_mt);
        TEST(parse_response(&msg) == PCP_ERR_SUCCESS);
        TEST(msg.assigned_ext_port == htons(1234));
        TEST(msg.kd.map_peer.src_port == htons(3456));
        TEST(msg.kd.map_peer.protocol == IPPROTO_TCP);
        TEST(msg.recv_version == 0);
        TEST(msg.recv_epoch == 1234);
        TEST(msg.recv_lifetime == 2233);
        TEST(msg.recv_result == 11);

        memset(&msg, 0, sizeof(msg));
        memcpy(&msg.pcp_msg_buffer, &natpmp_mu, sizeof(natpmp_mu));
        msg.pcp_msg_len = sizeof(natpmp_mu);
        TEST(parse_response(&msg) == PCP_ERR_SUCCESS);
        TEST(msg.assigned_ext_port == htons(1234));
        TEST(msg.kd.map_peer.src_port == htons(3456));
        TEST(msg.kd.map_peer.protocol == IPPROTO_UDP);
        TEST(msg.recv_version == 0);
        TEST(msg.recv_epoch == 1234);
        TEST(msg.recv_lifetime == 2233);
        TEST(msg.recv_result == 11);

        msg.pcp_msg_len = sizeof(natpmp_mu) - 1;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);

        natpmp_mu.opcode = 3;
        memset(&msg, 0, sizeof(msg));
        memcpy(&msg.pcp_msg_buffer, &natpmp_mu, sizeof(natpmp_mu));

        msg.pcp_msg_len = sizeof(natpmp_mu);
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
    }
#endif
    { // TEST validate MSG
        pcp_recv_msg_t msg;
        pcp_response_t *resp = (pcp_response_t *)msg.pcp_msg_buffer;

        memset(&msg, 0, sizeof(msg));
        resp->r_opcode = 0x81;
        resp->ver = 1;
        msg.pcp_msg_len = sizeof(pcp_response_t);

        TEST(validate_pcp_msg(&msg));
        msg.pcp_msg_len = sizeof(pcp_response_t) - 1;
        TEST(!validate_pcp_msg(&msg));
        msg.pcp_msg_len = sizeof(pcp_response_t) - 4;
        TEST(validate_pcp_msg(&msg));
        msg.pcp_msg_len = 1104;
        TEST(!validate_pcp_msg(&msg));
        msg.pcp_msg_len = 0;
        TEST(!validate_pcp_msg(&msg));

        msg.pcp_msg_len = sizeof(pcp_response_t);
        resp->ver = 2;
        TEST(validate_pcp_msg(&msg));
        resp->ver = 3;
        TEST(!validate_pcp_msg(&msg));
        resp->ver = 2;
        resp->r_opcode = 0x1;
        TEST(!validate_pcp_msg(&msg));
        resp->r_opcode = 0x7f;
        TEST(!validate_pcp_msg(&msg));

        resp->r_opcode = 0x81;
        msg.pcp_msg_len = sizeof(pcp_response_t) - 1;
        resp->ver = 1;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->ver = 2;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->r_opcode = 0x82;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->ver = 1;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->r_opcode = 0x83;
        resp->ver = 2;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);

        resp->r_opcode = 0x81;
        msg.pcp_msg_len = sizeof(pcp_response_t) + 1;
        resp->ver = 1;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->ver = 2;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->r_opcode = 0x82;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->ver = 1;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->r_opcode = 0x83;
        resp->ver = 2;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);

        msg.pcp_msg_len = sizeof(pcp_response_t);
        resp->r_opcode = 0x84;
        resp->ver = 2;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->ver = 1;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
        resp->ver = 3;
        resp->r_opcode = 0x82;
        TEST(parse_response(&msg) != PCP_ERR_SUCCESS);
    }
    { // TEST build msg
        struct pcp_flow_s fs;
        pcp_server_t *s;

        memset(&fs, 0, sizeof(fs));
        fs.ctx = ctx;
        TEST(build_pcp_msg(&fs) == NULL);
        fs.pcp_server_indx = pcp_add_server(ctx, Sock_pton("127.0.0.1"), 1);
        s = get_pcp_server(ctx, fs.pcp_server_indx);
        fs.kd.operation = PCP_OPCODE_ANNOUNCE;
        TEST(build_pcp_msg(&fs) != NULL);
        fs.kd.operation = 0x7f;
        TEST(build_pcp_msg(&fs) == NULL);
        fs.kd.operation = PCP_OPCODE_SADSCP;
        TEST(build_pcp_msg(&fs) == NULL);
        s->pcp_version = 3;
        TEST(build_pcp_msg(&fs) == NULL);
        fs.kd.operation = PCP_OPCODE_MAP;
        TEST(build_pcp_msg(&fs) == NULL);
        fs.kd.operation = PCP_OPCODE_PEER;
        TEST(build_pcp_msg(&fs) == NULL);
        s->pcp_version = 2;
#ifdef PCP_EXPERIMENTAL
        {
            uint16_t i;

            pcp_db_add_md(&fs, 0, NULL, sizeof("string"));
            pcp_db_add_md(&fs, 1, "string", 0);
            for (i = 2; i < 256; ++i) {
                pcp_db_add_md(&fs, i, "string", sizeof("string"));
            }
            TEST(build_pcp_msg(&fs) != NULL);
            TEST(fs.pcp_msg_len <= PCP_MAX_LEN);
        }
#endif
        TEST(build_pcp_msg(NULL) == NULL);
    }

    PD_SOCKET_CLEANUP();
    return 0;
}
