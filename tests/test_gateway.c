/*
 *------------------------------------------------------------------
 * test_gateway.c
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

#include "gateway.h"
#include "pcpnatpmp.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef WIN32
#include <Netioapi.h>
#include <Winsock2.h>
#else /* WIN32 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include "pcp_socket.h"
#include "test_macro.h"

#ifdef __linux__
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <string.h>
#include <unistd.h>

static int dump_reads;
static int dump_flags;
static int dump_status;
static int dump_truncated;
static int dump_short_completion;

static int dump_socket(int domain, int type, int protocol) {
    TEST(domain == PF_NETLINK);
    TEST(type == SOCK_DGRAM);
    TEST(protocol == NETLINK_ROUTE);
    return 123;
}

static ssize_t dump_send(int fd, const void *buf, size_t len, int flags) {
    const struct nlmsghdr *header = buf;
    TEST(fd == 123);
    TEST(flags == 0);
    TEST(header->nlmsg_type == RTM_GETROUTE);
    TEST(header->nlmsg_seq == 1);
    return (ssize_t)len;
}

static ssize_t dump_recvmsg(int fd, struct msghdr *msg, int flags) {
    struct nlmsghdr *header = msg->msg_iov[0].iov_base;
    TEST(fd == 123);
    TEST(flags == 0);
    TEST(dump_reads < 3);
    memset(header, 0, msg->msg_iov[0].iov_len);
    header->nlmsg_seq = 1;
    if (dump_reads++ < 2) {
        struct rtmsg *route = NLMSG_DATA(header);
        struct rtattr *attribute;
        struct in_addr address;
        header->nlmsg_type = RTM_NEWROUTE;
        header->nlmsg_flags = NLM_F_MULTI;
        header->nlmsg_len = NLMSG_LENGTH(sizeof(*route));
        route->rtm_family = AF_INET;
        route->rtm_table = RT_TABLE_MAIN;
        attribute =
            (struct rtattr *)((char *)header + NLMSG_ALIGN(header->nlmsg_len));
        attribute->rta_type = RTA_GATEWAY;
        attribute->rta_len = RTA_LENGTH(sizeof(address));
        address.s_addr = htonl(0xc0000201);
        memcpy(RTA_DATA(attribute), &address, sizeof(address));
        header->nlmsg_len += RTA_SPACE(sizeof(address));
        // Ignore an unrelated message, then parse a gateway in the next read.
        if (dump_reads == 1) {
            header->nlmsg_seq = 2;
        } else if (dump_truncated) {
            msg->msg_flags |= MSG_TRUNC;
        }
    } else {
        header->nlmsg_type = NLMSG_DONE;
        header->nlmsg_flags = dump_flags;
        header->nlmsg_len =
            NLMSG_LENGTH(dump_short_completion ? 1 : sizeof(dump_status));
        memcpy(NLMSG_DATA(header), &dump_status, sizeof(dump_status));
    }
    return header->nlmsg_len;
}

static int dump_close(int fd) {
    TEST(fd == 123);
    return 0;
}

#define socket dump_socket
#define send dump_send
#define recvmsg dump_recvmsg
#define close dump_close
#define getgateways getgateways_test_dump
#include "../lib/src/net/gateway.c"
#undef getgateways
#undef close
#undef recvmsg
#undef send
#undef socket

static void test_netlink_dump(void) {
    struct sockaddr_in6 *gws = NULL;
    int scenario;
    for (scenario = 0; scenario < 5; ++scenario) {
        dump_reads = 0;
        dump_flags = scenario == 1 ? NLM_F_DUMP_INTR : 0;
        dump_status = scenario == 2 ? -EINTR : 0;
        dump_truncated = scenario == 3;
        dump_short_completion = scenario == 4;
        if (scenario == 0) {
            TEST(getgateways_test_dump(&gws) == 1);
            TEST(dump_reads == 3);
            TEST(gws != NULL);
            TEST(IN6_IS_ADDR_V4MAPPED(&gws[0].sin6_addr));
            free(gws);
            gws = NULL;
        } else {
            TEST(getgateways_test_dump(&gws) == PCP_ERR_RECV_FAILED);
            TEST(gws == NULL);
        }
    }
}
#endif

static void test_getgateways(void) {
    struct sockaddr_in6 *gws = NULL, *gw;
    struct in_addr;
    int rCount = getgateways(&gws);
    gw = gws;

    TEST(rCount > 0)
    for (; rCount > 0; rCount--) {
        char pb[128];
        printf("gw : %-20s\n",
               inet_ntop(AF_INET6, &gw->sin6_addr, pb, sizeof(pb)));
        gw++;
    }
    free(gws);

    TEST(getgateways(NULL) < 0)
}

static void test_sa_len(void) {
    struct sockaddr sa;

    printf("Test sockaddr_in\n");
    sa.sa_family = AF_INET;
    SET_SA_LEN(&sa, sizeof(struct sockaddr_in));
    TEST(SA_LEN(&sa) == sizeof(struct sockaddr_in));

    printf("Test sockaddr_in6\n");
    sa.sa_family = AF_INET6;
    SET_SA_LEN(&sa, sizeof(struct sockaddr_in6));
    TEST(SA_LEN(&sa) == sizeof(struct sockaddr_in6));

    printf("Test sockadd\n");
    sa.sa_family = AF_UNSPEC;
    SET_SA_LEN(&sa, sizeof(struct sockaddr));
    TEST(SA_LEN(&sa) == sizeof(struct sockaddr));
}

int main(void) {
    PD_SOCKET_STARTUP();
#ifdef __linux__
    test_netlink_dump();
#endif

    printf("Testing get gateways:\n");
    test_getgateways();

    printf("Testing SA_LEN  \n");
    test_sa_len();

    PD_SOCKET_CLEANUP();

    return 0;
}
