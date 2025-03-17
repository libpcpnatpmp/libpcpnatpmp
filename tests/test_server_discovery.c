/*
 *------------------------------------------------------------------
 * test_server_discovery.c
 *
 * Feb 4, 2013, ptatrai
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
#include <string.h>

#ifndef WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "gateway.h"
#include "pcp.h"
#include "pcp_logger.h"
#include "pcp_server_discovery.h"
#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

static int print_status(pcp_server_t *s, void *data UNUSED) {
    pcp_logger(PCP_LOGLVL_INFO, "Server index %d, addr %s, status: %d",
               s->index, s->pcp_server_paddr, s->server_state);
    return 0;
}

int main(void) {
    pcp_flow_t *f;
    pcp_fstate_e ret;
    pcp_server_t *s;
    int sindx, sindx1;
#ifdef PCP_USE_IPV6_SOCKET
    int sindx2;
    struct sockaddr_in6 src6;
#endif
    struct sockaddr_in src4, dst4;
    struct sockaddr *src = NULL;
    struct sockaddr *dst = NULL;
    pcp_ctx_t *ctx;

    PD_SOCKET_STARTUP();

    pcp_log_level = PCP_LOGLVL_INFO;

    ctx = pcp_init(ENABLE_AUTODISCOVERY, NULL);

    TEST((sindx1 = pcp_add_server(ctx, Sock_pton("100.2.1.1:5351"), 1)) >= 0);
    TEST((sindx = pcp_add_server(ctx, Sock_pton("100.2.1.1:5351"),
                                 PCP_MAX_SUPPORTED_VERSION)) == sindx1);
    TEST(pcp_add_server(ctx, Sock_pton("100.2.1.1"),
                        PCP_MAX_SUPPORTED_VERSION + 1) < 0);
#ifdef PCP_USE_IPV6_SOCKET
    TEST((sindx2 = pcp_add_server(ctx, Sock_pton("[::1]:5350"),
                                  PCP_MAX_SUPPORTED_VERSION)) >= 0);
#endif
    s = get_pcp_server(ctx, sindx);
    TEST(s != NULL);

    memset(&src4, 0, sizeof(src4));
    memset(&dst4, 0, sizeof(dst4));
    src4.sin_family = dst4.sin_family = AF_INET;
    src4.sin_addr.s_addr = s->src_ip[3];
    dst4.sin_addr.s_addr = 0x08080808;
    src4.sin_port = htons(1234);
    dst4.sin_port = 0x0909;
    src = (struct sockaddr *)&src4;
    dst = (struct sockaddr *)&dst4;
    f = pcp_new_flow(ctx, src, dst, NULL, IPPROTO_TCP, 600, NULL);
#ifdef PCP_USE_IPV6_SOCKET
    s = get_pcp_server(ctx, sindx2);
    s->af = AF_UNSPEC;
    memset(&src6, 0, sizeof(src6));
    src6.sin6_family = AF_INET6;
    memcpy(&src6.sin6_addr, s->src_ip, sizeof(src6.sin6_addr));
    src6.sin6_port = htons(1234);
    src = (struct sockaddr *)&src6;
    f = pcp_new_flow(ctx, src, dst, NULL, IPPROTO_TCP, 600, NULL);
#endif
    ret = pcp_wait(f, 13000, 1);
    pcp_logger(PCP_LOGLVL_INFO, "1st wait finished with result: %d\n", ret);

    ret = pcp_wait(f, 20000, 1);
    pcp_logger(PCP_LOGLVL_INFO, "2nd wait finished with result: %d\n", ret);

    printf("\nFinal statuses of PCP servers:\n");
    pcp_db_foreach_server(ctx, print_status, NULL);

    PD_SOCKET_CLEANUP();

    return 0;
}
