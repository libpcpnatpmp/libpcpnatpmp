/*
 *------------------------------------------------------------------
 * test_filter_close.c
 *
 * A MAP flow with FILTER options is sent with them, but closing it sends a
 * MAP with lifetime 0 and no FILTER option: RFC 6887 section 13.3 makes a
 * delete with a FILTER option MALFORMED_OPTION. The datagrams are looked at
 * as the library hands them to its socket; no server answers.
 *------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "pcpnatpmp.h"

#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

extern pcp_socket_vt_t default_socket_vt;

static unsigned char last[1100];
static size_t last_len;

static ssize_t recording_sendto(PCP_SOCKET sock, const void *buf, size_t len,
                                int flags, const struct sockaddr_in6 *src,
                                struct sockaddr *dest, socklen_t addrlen) {
    if (len <= sizeof(last)) {
        memcpy(last, buf, len);
        last_len = len;
    }
    return default_socket_vt.sock_sendto(sock, buf, len, flags, src, dest,
                                         addrlen);
}

static uint32_t last_lifetime(void) {
    uint32_t l;

    memcpy(&l, last + 4, 4);
    return ntohl(l);
}

int main(void) {
    pcp_socket_vt_t vt = default_socket_vt;
    struct sockaddr_storage src, filter;
    pcp_ctx_t *ctx;
    pcp_flow_t *flow;
    struct timeval tout = {0, 0};

    PD_SOCKET_STARTUP();
    vt.sock_sendto = recording_sendto;
    ctx = pcp_init(DISABLE_AUTODISCOVERY, &vt);
    TEST(ctx != NULL);
    /* no server listens there: the requests are only sent */
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5399"), 2) >= 0);

    TEST(sock_pton("127.0.0.1:4000", (struct sockaddr *)&src) == 0);
    flow = pcp_new_flow(ctx, (struct sockaddr *)&src, NULL, NULL, IPPROTO_UDP,
                        600, NULL);
    TEST(flow != NULL);
    TEST(sock_pton("198.18.0.1:1111", (struct sockaddr *)&filter) == 0);
    TEST(pcp_flow_set_filter_opt(flow, (struct sockaddr *)&filter, 32) ==
         PCP_ERR_SUCCESS);
    TEST(sock_pton("198.18.0.2:2222", (struct sockaddr *)&filter) == 0);
    TEST(pcp_flow_set_filter_opt(flow, (struct sockaddr *)&filter, 32) ==
         PCP_ERR_SUCCESS);

    pcp_pulse(ctx, &tout);
    /* the MAP with its two FILTER options: 24 + 36 + 2 x 24 octets */
    TEST(last_len == 108);
    TEST((last[1] & 0x7f) == 1 && last_lifetime() == 600);

    last_len = 0;
    pcp_close_flow(flow);
    pcp_pulse(ctx, &tout);
    /* the delete: lifetime 0, no FILTER option */
    TEST(last_len == 60);
    TEST((last[1] & 0x7f) == 1 && last_lifetime() == 0);

    pcp_delete_flow(flow);
    pcp_terminate(ctx, 0);
    printf("test_filter_close: OK\n");
    return 0;
}
