/*
 Copyright (c) 2014 by Cisco Systems, Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#include "pcp_win_defines.h"

#include <mswsock.h>
#include <ws2def.h>
#else // WIN32
#include <netinet/in.h>
#include <sys/socket.h>
#ifndef PCP_SOCKET_IS_VOIDPTR
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <unistd.h>
#endif // PCP_SOCKET_IS_VOIDPTR
#endif //! WIN32
#include "pcpnatpmp.h"

#include "pcp_socket.h"
#include "pcp_utils.h"
#include "unp.h"

// Where a client on another port listens for restart announcements; see
// pcp_socket_listen_announce()
#define PCP_ANNOUNCE_PORT 5350

#ifdef PCP_ANNOUNCE_LISTENER
// Lets another client's announce listener share port 5350 with this socket.
static int pcp_socket_share_port(PCP_SOCKET s) {
    int on = 1;

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) != 0) {
        return -1;
    }
#if defined(SO_REUSEPORT) && !defined(__linux__)
    // the BSDs want it to share a UDP port; on Linux it would balance instead
    if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (char *)&on, sizeof(on)) != 0) {
        return -1;
    }
#endif
    return 0;
}
#endif

static PCP_SOCKET pcp_socket_create_impl(int domain, int type, int protocol);
static ssize_t pcp_socket_recvfrom_impl(PCP_SOCKET sock, void *buf, size_t len,
                                        int flags, struct sockaddr *src_addr,
                                        socklen_t *addrlen,
                                        struct sockaddr_in6 *dst_addr);
static ssize_t pcp_socket_sendto_impl(PCP_SOCKET sock, const void *buf,
                                      size_t len, int flags,
                                      const struct sockaddr_in6 *src_addr,
                                      struct sockaddr *dest_addr,
                                      socklen_t addrlen);
static int pcp_socket_close_impl(PCP_SOCKET sock);

pcp_socket_vt_t default_socket_vt = {
    pcp_socket_create_impl, pcp_socket_recvfrom_impl, pcp_socket_sendto_impl,
    pcp_socket_close_impl};

#ifdef WIN32
// function calling WSAStartup (used in pcp-server and pcp_app)
int pcp_win_sock_startup() {
    int err;
    WORD wVersionRequested;
    WSADATA wsaData;

    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        perror("WSAStartup failed with error");
        return 1;
    }
    return 0;
}

/* function calling WSACleanup
 *  returns 0 on success and 1 on failure
 */
int pcp_win_sock_cleanup() {
    if (WSACleanup() == PCP_SOCKET_ERROR) {
        printf("WSACleanup failed.\n");
        return 1;
    }
    return 0;
}
#endif

void pcp_fill_in6_addr(struct in6_addr *dst_ip6, uint16_t *dst_port,
                       uint32_t *dst_scope_id, struct sockaddr *src) {
    if (src->sa_family == AF_INET) {
        struct sockaddr_in *src_ip4 = (struct sockaddr_in *)src;

        if (dst_ip6) {
            S6_ADDR32(dst_ip6)[0] = 0;
            S6_ADDR32(dst_ip6)[1] = 0;
            S6_ADDR32(dst_ip6)[2] = htonl(0xFFFF);
            S6_ADDR32(dst_ip6)[3] = src_ip4->sin_addr.s_addr;
        }
        if (dst_port) {
            *dst_port = src_ip4->sin_port;
        }
        if (dst_scope_id) {
            *dst_scope_id = 0;
        }
    } else if (src->sa_family == AF_INET6) {
        struct sockaddr_in6 *src_ip6 = (struct sockaddr_in6 *)src;

        if (dst_ip6) {
            memcpy(dst_ip6, src_ip6->sin6_addr.s6_addr, sizeof(*dst_ip6));
        }
        if (dst_port) {
            *dst_port = src_ip6->sin6_port;
        }
        if (dst_scope_id) {
            *dst_scope_id = src_ip6->sin6_scope_id;
        }
    }
}

void pcp_fill_sockaddr(struct sockaddr *dst, struct in6_addr *sip,
                       uint16_t sport, int ret_ipv6_mapped_ipv4,
                       uint32_t scope_id) {
    if ((!ret_ipv6_mapped_ipv4) && (IN6_IS_ADDR_V4MAPPED(sip))) {
        struct sockaddr_in *s = (struct sockaddr_in *)dst;

        s->sin_family = AF_INET;
        s->sin_addr.s_addr = S6_ADDR32(sip)[3];
        s->sin_port = sport;
        SET_SA_LEN(s, sizeof(struct sockaddr_in));
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)dst;

        s->sin6_family = AF_INET6;
        s->sin6_addr = *sip;
        s->sin6_port = sport;
        s->sin6_scope_id = scope_id;
        SET_SA_LEN(s, sizeof(struct sockaddr_in6));
    }
}

#ifndef PCP_SOCKET_IS_VOIDPTR
static pcp_errno pcp_get_error() {
#ifdef WIN32
    int errnum = WSAGetLastError();

    switch (errnum) {
    case WSAEADDRINUSE:
        return PCP_ERR_ADDRINUSE;
    case WSAEWOULDBLOCK:
        return PCP_ERR_WOULDBLOCK;
    default:
        return PCP_ERR_UNKNOWN;
    }
#else
    switch (errno) {
    case EADDRINUSE:
        return PCP_ERR_ADDRINUSE;
        //        case EAGAIN:
    case EWOULDBLOCK:
        return PCP_ERR_WOULDBLOCK;
    default:
        return PCP_ERR_UNKNOWN;
    }
#endif
}
#endif

PCP_SOCKET pcp_socket_create(struct pcp_ctx_s *ctx, int domain, int type,
                             int protocol) {
    assert(ctx && ctx->virt_socket_tb && ctx->virt_socket_tb->sock_create);

    return ctx->virt_socket_tb->sock_create(domain, type, protocol);
}

ssize_t pcp_socket_recvfrom(struct pcp_ctx_s *ctx, void *buf, size_t len,
                            int flags, struct sockaddr *src_addr,
                            socklen_t *addrlen, struct sockaddr_in6 *dst_addr) {
    assert(ctx && ctx->virt_socket_tb && ctx->virt_socket_tb->sock_recvfrom);

    return ctx->virt_socket_tb->sock_recvfrom(ctx->socket, buf, len, flags,
                                              src_addr, addrlen, dst_addr);
}

ssize_t pcp_socket_recvfrom_sock(struct pcp_ctx_s *ctx, PCP_SOCKET sock,
                                 void *buf, size_t len, int flags,
                                 struct sockaddr *src_addr, socklen_t *addrlen,
                                 struct sockaddr_in6 *dst_addr) {
    return ctx->virt_socket_tb->sock_recvfrom(sock, buf, len, flags, src_addr,
                                              addrlen, dst_addr);
}

ssize_t pcp_socket_sendto(struct pcp_ctx_s *ctx, const void *buf, size_t len,
                          int flags, struct sockaddr_in6 *src_addr,
                          struct sockaddr *dest_addr, socklen_t addrlen) {
    assert(ctx && ctx->virt_socket_tb && ctx->virt_socket_tb->sock_sendto);

    return ctx->virt_socket_tb->sock_sendto(ctx->socket, buf, len, flags,
                                            src_addr, dest_addr, addrlen);
}

int pcp_socket_close(struct pcp_ctx_s *ctx) {
    assert(ctx && ctx->virt_socket_tb && ctx->virt_socket_tb->sock_close);

    return ctx->virt_socket_tb->sock_close(ctx->socket);
}

static PCP_SOCKET pcp_socket_create_impl(int domain, int type, int protocol) {
#ifdef PCP_SOCKET_IS_VOIDPTR
    return PCP_INVALID_SOCKET;
#else
    PCP_SOCKET s;
    uint32_t flg;
    unsigned long iMode = 1;
    struct sockaddr_storage sas;
    struct sockaddr_in *sin = (struct sockaddr_in *)&sas;
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&sas;

    OSDEP(iMode);
    OSDEP(flg);

    memset(&sas, 0, sizeof(sas));
    sas.ss_family = domain;
    if (domain == AF_INET) {
        sin->sin_port = htons(5350);
        SET_SA_LEN(sin, sizeof(struct sockaddr_in));
    } else if (domain == AF_INET6) {
        sin6->sin6_port = htons(5350);
        SET_SA_LEN(sin6, sizeof(struct sockaddr_in6));
    } else {
        PCP_LOG(PCP_LOGLVL_ERR, "Unsupported socket domain:%d", domain);
    }

    s = (PCP_SOCKET)socket(domain, type, protocol);
    if (s == PCP_INVALID_SOCKET)
        return PCP_INVALID_SOCKET;

#ifdef WIN32
    if (ioctlsocket(s, FIONBIO, &iMode)) {
        PCP_LOG(PCP_LOGLVL_ERR, "%s",
                "Unable to set nonblocking mode for socket.");
        CLOSE(s);
        return PCP_INVALID_SOCKET;
    }
#else  // WIN32
    flg = fcntl(s, F_GETFL, 0);
    if (fcntl(s, F_SETFL, flg | O_NONBLOCK)) {
        PCP_LOG(PCP_LOGLVL_ERR, "%s",
                "Unable to set nonblocking mode for socket.");
        CLOSE(s);
        return PCP_INVALID_SOCKET;
    }
#endif //! WIN32
#ifdef PCP_USE_IPV6_SOCKET
    flg = 0;
    if (PCP_SOCKET_ERROR ==
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&flg, sizeof(flg))) {
        PCP_LOG(PCP_LOGLVL_ERR, "%s",
                "Dual-stack sockets are not supported on this platform. "
                "Recompile library with disabled IPv6 support.");
        CLOSE(s);
        return PCP_INVALID_SOCKET;
    }
#endif // PCP_USE_IPV6_SOCKET
#if defined(IP_PKTINFO) || defined(IPV6_RECVPKTINFO) || defined(IPV6_PKTINFO)
    {
        int optval = 1;
#if defined WIN32 && defined IPV6_PKTINFO
        if (setsockopt(s, IPPROTO_IPV6, IPV6_PKTINFO, (char *)&optval,
                       sizeof(optval)) < 0) {
            PCP_LOG(PCP_LOGLVL_ERR, "%s",
                    "Unable to set IPV6_PKTINFO option for socket.");
            CLOSE(s);
            return PCP_INVALID_SOCKET;
        }
#endif // WIN32 && IPV6_PKTINFO
#ifdef IP_PKTINFO
        if (setsockopt(s, IPPROTO_IP, IP_PKTINFO, (char *)&optval,
                       sizeof(optval)) < 0) {
            PCP_LOG(PCP_LOGLVL_ERR, "%s",
                    "Unable to set IP_PKTINFO option for socket.");
        }
#endif // IP_PKTINFO
#ifdef IPV6_RECVPKTINFO
        if (domain == AF_INET6) {
            if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO, (char *)&optval,
                           sizeof(optval)) < 0) {
                PCP_LOG(PCP_LOGLVL_ERR, "%s",
                        "Unable to set IPV6_RECVPKTINFO option for socket.");
                CLOSE(s);
                return PCP_INVALID_SOCKET;
            }
        }
#endif // IPV6_RECVPKTINFO
    }
#endif // IP_PKTINFO && IPV6_RECVPKTINFO
    while (bind(s, (struct sockaddr *)&sas,
                (socklen_t)SA_LEN((struct sockaddr *)&sas)) ==
           PCP_SOCKET_ERROR) {
        if (pcp_get_error() == PCP_ERR_ADDRINUSE) {
            if (sas.ss_family == AF_INET) {
                sin->sin_port = htons(ntohs(sin->sin_port) + 1);
            } else {
                sin6->sin6_port = htons(ntohs(sin6->sin6_port) + 1);
            }
        } else {
            PCP_LOG(PCP_LOGLVL_ERR, "%s", "bind error");
            CLOSE(s);
            return PCP_INVALID_SOCKET;
        }
    }
#ifdef PCP_ANNOUNCE_LISTENER
    /* Only now, not before bind(): with the option set before it, a second
     * client's socket would bind 5350 as well and take this one's replies. It
     * still fails and moves on; only an announce listener, which also has the
     * option, can share the port. */
    if (ntohs(sas.ss_family == AF_INET ? sin->sin_port : sin6->sin6_port) ==
            PCP_ANNOUNCE_PORT &&
        pcp_socket_share_port(s) != 0) {
        PCP_LOG(PCP_LOGLVL_WARN, "%s",
                "Could not share port 5350, another client on this host will "
                "not receive restart announcements");
    }
#endif
    PCP_LOG(PCP_LOGLVL_DEBUG, "%s: return %d", __FUNCTION__, s);
    return s;
#endif
}

#ifndef PCP_SOCKET_IS_VOIDPTR
#if defined(PCP_USE_IPV6_SOCKET) && !defined(WIN32)
// Index of the interface that has the address `ip`, or 0.
static unsigned interface_index_of(const struct in6_addr *ip) {
    struct ifaddrs *list, *a;
    unsigned index = 0;

    if (getifaddrs(&list) != 0) {
        return 0;
    }
    for (a = list; a != NULL && index == 0; a = a->ifa_next) {
        if (a->ifa_addr != NULL && a->ifa_addr->sa_family == AF_INET6 &&
            IN6_ARE_ADDR_EQUAL(&((struct sockaddr_in6 *)a->ifa_addr)->sin6_addr,
                               ip)) {
            index = if_nametoindex(a->ifa_name);
        }
    }
    freeifaddrs(list);
    return index;
}
#endif
#endif

#ifndef PCP_SOCKET_IS_VOIDPTR
// Joining a group a second time, for another server on the same interface,
// fails with "address in use": that is fine. Anything else means restart
// announcements are not received; say so, but do not fail adding the server.
// Call right after the failed setsockopt, so the error is still its error.
static int log_join_failure(const char *group) {
    char buff[ERR_BUF_LEN];
#ifdef WIN32
    int error = WSAGetLastError();
    int in_use = (error == WSAEADDRINUSE);
#else
    int error = errno;
    int in_use = (error == EADDRINUSE);
#endif

    if (in_use) {
        return 0;
    }
    pcp_strerror(error, buff, sizeof(buff));
    PCP_LOG(PCP_LOGLVL_WARN,
            "Could not join %s (%s), restart announcements are not received",
            group, buff);
    return -1;
}
#endif

/*
 * A restarted server announces itself to 224.0.0.1 / ff02::1, port 5350
 * (RFC 6887, 14.2.1), the port this socket is bound to unless another
 * client has it. A socket that has not joined the group does not always get
 * those datagrams: on Linux an IPv6 socket, even a dual-stack one, does not
 * receive IPv4 multicast without IP_ADD_MEMBERSHIP. Join both groups on the
 * interface that reaches the server. A failure is logged but does not fail
 * adding the server; the client then notices a restart from the epoch in the
 * next ordinary reply.
 */
int pcp_socket_join_announce_groups(struct pcp_ctx_s *ctx,
                                    const struct in6_addr *src_ip,
                                    uint32_t scope_id) {
#ifdef PCP_SOCKET_IS_VOIDPTR
    (void)ctx;
    (void)src_ip;
    (void)scope_id;
    return -1;
#else
    PCP_SOCKET sock = ctx->socket;

    /* The library can join only on a socket it made itself. A socket table
     * which wraps some functions (to count or log the datagrams) but keeps
     * the library's sock_create still has the library's socket. */
    if (ctx->virt_socket_tb->sock_create != pcp_socket_create_impl ||
        sock == PCP_INVALID_SOCKET) {
        return -1; // the application owns the socket
    }

    if (IN6_IS_ADDR_V4MAPPED(src_ip)) {
#if defined(PCP_USE_IPV6_SOCKET) && !defined(__linux__) && !defined(WIN32)
        /* Missing: macOS and the BSDs refuse IPv4 options on an IPv6 socket
         * and do not hand it IPv4 multicast, so hearing 224.0.0.1 there needs
         * a separate IPv4 socket bound to 5350. Until then a restart of an
         * IPv4 server is noticed from the epoch of the next ordinary reply. */
        PCP_LOG(PCP_LOGLVL_DEBUG, "%s",
                "Not joining 224.0.0.1: not supported on this platform");
        return -1;
#else
        struct ip_mreq mreq;

        memset(&mreq, 0, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = htonl(0xE0000001); // 224.0.0.1
        mreq.imr_interface.s_addr = S6_ADDR32(src_ip)[3];
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
                       sizeof(mreq)) != 0) {
            return log_join_failure("224.0.0.1");
        }
        return 0;
#endif
    }

#if defined(PCP_USE_IPV6_SOCKET) && !defined(WIN32)
    {
        struct ipv6_mreq mreq6;

        memset(&mreq6, 0, sizeof(mreq6));
        mreq6.ipv6mr_multiaddr.s6_addr[0] = 0xff; // ff02::1
        mreq6.ipv6mr_multiaddr.s6_addr[1] = 0x02;
        mreq6.ipv6mr_multiaddr.s6_addr[15] = 0x01;
        mreq6.ipv6mr_interface =
            scope_id != 0 ? scope_id : interface_index_of(src_ip);
        if (mreq6.ipv6mr_interface == 0) {
            PCP_LOG(PCP_LOGLVL_WARN, "%s",
                    "Could not join ff02::1: no interface has the server's "
                    "source address, restart announcements are not received");
            return -1;
        }
        if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mreq6,
                       sizeof(mreq6)) != 0) {
            return log_join_failure("ff02::1");
        }
        return 0;
    }
#else
    (void)scope_id;
    PCP_LOG(PCP_LOGLVL_WARN, "%s",
            "Could not join ff02::1: this build cannot join IPv6 groups, "
            "restart announcements are not received");
    return -1;
#endif
#endif // PCP_SOCKET_IS_VOIDPTR
}

#ifdef PCP_ANNOUNCE_LISTENER
// The port the library's socket is bound to, or 0.
static uint16_t bound_port(PCP_SOCKET sock) {
    struct sockaddr_storage sas;
    socklen_t len = (socklen_t)sizeof(sas);

    if (getsockname(sock, (struct sockaddr *)&sas, &len) != 0) {
        return 0;
    }
    return ntohs(sas.ss_family == AF_INET
                     ? ((struct sockaddr_in *)&sas)->sin_port
                     : ((struct sockaddr_in6 *)&sas)->sin6_port);
}

/* A socket bound to the group address on port 5350, which has joined it. */
static PCP_SOCKET announce_listener(const struct in6_addr *src_ip,
                                    uint32_t scope_id) {
    int v4 = IN6_IS_ADDR_V4MAPPED(src_ip);
    PCP_SOCKET s = (PCP_SOCKET)socket(v4 ? AF_INET : AF_INET6, SOCK_DGRAM, 0);
    int ok;

    if (s == PCP_INVALID_SOCKET) {
        return s;
    }
    ok = fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK) == 0 &&
         pcp_socket_share_port(s) == 0;
    if (ok && v4) {
        struct sockaddr_in sin;
        struct ip_mreq mreq;

        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(PCP_ANNOUNCE_PORT);
        sin.sin_addr.s_addr = htonl(0xE0000001); // 224.0.0.1
        SET_SA_LEN(&sin, sizeof(sin));
        memset(&mreq, 0, sizeof(mreq));
        mreq.imr_multiaddr = sin.sin_addr;
        mreq.imr_interface.s_addr = S6_ADDR32(src_ip)[3];
        ok = bind(s, (struct sockaddr *)&sin, sizeof(sin)) == 0 &&
             setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mreq,
                        sizeof(mreq)) == 0;
    } else if (ok) {
        struct sockaddr_in6 sin6;
        struct ipv6_mreq mreq6;

        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;
        sin6.sin6_port = htons(PCP_ANNOUNCE_PORT);
        sin6.sin6_addr.s6_addr[0] = 0xff; // ff02::1
        sin6.sin6_addr.s6_addr[1] = 0x02;
        sin6.sin6_addr.s6_addr[15] = 0x01;
        sin6.sin6_scope_id =
            scope_id != 0 ? scope_id : interface_index_of(src_ip);
        SET_SA_LEN(&sin6, sizeof(sin6));
        memset(&mreq6, 0, sizeof(mreq6));
        mreq6.ipv6mr_multiaddr = sin6.sin6_addr;
        mreq6.ipv6mr_interface = sin6.sin6_scope_id;
        ok = sin6.sin6_scope_id != 0 &&
             bind(s, (struct sockaddr *)&sin6, sizeof(sin6)) == 0 &&
             setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&mreq6,
                        sizeof(mreq6)) == 0;
    }
    if (!ok) {
        CLOSE(s);
        return PCP_INVALID_SOCKET;
    }
    return s;
}
#endif // PCP_ANNOUNCE_LISTENER

/*
 * Servers announce a restart to port 5350 only, and only one client on a host
 * can have its socket there: the next gets 5351 (see pcp_socket_create_impl).
 * Joining the groups on that socket gets it nothing, so such a client must
 * listen on 5350 separately, or it hears of a restart only from the reply to
 * its next renewal. The listener is bound to the group address rather than to
 * the wildcard, so it shares the port without taking the unicast replies of
 * the client that holds it.
 */
void pcp_socket_listen_announce(struct pcp_server *s) {
#ifdef PCP_ANNOUNCE_LISTENER
    pcp_ctx_t *ctx = s->ctx;

    if (s->announce_listening || bound_port(ctx->socket) == PCP_ANNOUNCE_PORT) {
        return;
    }
    s->announce_socket =
        announce_listener((const struct in6_addr *)s->src_ip, s->pcp_scope_id);
    if (s->announce_socket == PCP_INVALID_SOCKET) {
        PCP_LOG(PCP_LOGLVL_WARN,
                "Could not listen on port 5350 beside another program, "
                "restart announcements of %s are not received",
                s->pcp_server_paddr);
        return;
    }
    s->announce_listening = 1;
#else
    (void)s;
#endif
}

void pcp_socket_close_announce(struct pcp_server *s) {
#ifdef PCP_ANNOUNCE_LISTENER
    if (s->announce_listening) {
        CLOSE(s->announce_socket);
        s->announce_listening = 0;
    }
#else
    (void)s;
#endif
}

#ifdef WIN32
#define PCP_CMSG_DATA(msg) (WSA_CMSG_DATA(msg))
#else // WIN32
#define PCP_CMSG_DATA(msg) (CMSG_DATA(msg))
#endif // WIN32

static ssize_t pcp_socket_recvfrom_impl(PCP_SOCKET sock, void *buf, size_t len,
                                        int flags, struct sockaddr *src_addr,
                                        socklen_t *addrlen,
                                        struct sockaddr_in6 *dst_addr) {
    ssize_t ret = -1;

#ifndef PCP_SOCKET_IS_VOIDPTR
#if defined(IPV6_PKTINFO) && defined(IP_PKTINFO)
    char control_buf[1024] = {0};

#ifndef WIN32
    struct msghdr msg;
    struct iovec iov;
    struct cmsghdr *cmsg;

    iov.iov_base = buf;
    iov.iov_len = len;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = src_addr;
    msg.msg_namelen = addrlen ? *addrlen : 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);

    // Receive message with IP_PKTINFO
    ret = recvmsg(sock, &msg, 0);

#else // WIN32
    WSAMSG msg;
    WSABUF iov;
    WSACMSGHDR *cmsg;
    iov.buf = buf;
    if (len > ULONG_MAX) {
        return PCP_ERR_RECV_FAILED;
    }
    iov.len = (ULONG)len;

    memset(&msg, 0, sizeof(msg));
    msg.name = (struct sockaddr *)src_addr;
    msg.namelen = addrlen ? *addrlen : 0;
    msg.lpBuffers = &iov;
    msg.dwBufferCount = 1;
    msg.Control.buf = control_buf;
    msg.Control.len = sizeof(control_buf);
    msg.dwFlags = 0;
    // Get WSARecvMsg function pointer
    LPFN_WSARECVMSG WSARecvMsg;
    GUID guid = WSAID_WSARECVMSG;
    DWORD bytesReturned;
    if (WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
                 &WSARecvMsg, sizeof(WSARecvMsg), &bytesReturned, NULL,
                 NULL) == SOCKET_ERROR) {
        perror("WSAIoctl failed");
        return 1;
    }

    DWORD bytesReceived;
    if (WSARecvMsg(sock, &msg, &bytesReceived, NULL, NULL) == SOCKET_ERROR) {
        ret = -1;
    } else {
#ifdef _WIN32
        if (bytesReceived > INT32_MAX) {
            ret = -1; // Handle overflow error
        } else {
            ret = (ssize_t)bytesReceived;
        }
#else
        ret = bytesReceived;
#endif
    }
#endif // WIN32

    // Processing control message
    if (ret > 0) {
        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == IPPROTO_IP &&
                cmsg->cmsg_type == IP_PKTINFO) {
                struct in_pktinfo *pktinfo =
                    (struct in_pktinfo *)PCP_CMSG_DATA(cmsg);
                S6_ADDR32(&dst_addr->sin6_addr)[0] = 0;
                S6_ADDR32(&dst_addr->sin6_addr)[1] = 0;
                S6_ADDR32(&dst_addr->sin6_addr)[2] = htonl(0xFFFF);
                S6_ADDR32(&dst_addr->sin6_addr)[3] = pktinfo->ipi_addr.s_addr;
                dst_addr->sin6_scope_id = 0;
                dst_addr->sin6_family = AF_INET6;
            }
            if (cmsg->cmsg_level == IPPROTO_IPV6 &&
                cmsg->cmsg_type == IPV6_PKTINFO) {
                struct in6_pktinfo *pktinfo6 =
                    (struct in6_pktinfo *)PCP_CMSG_DATA(cmsg);
                IPV6_ADDR_COPY(&dst_addr->sin6_addr, &pktinfo6->ipi6_addr);
                dst_addr->sin6_family = AF_INET6;
                if (IN6_IS_ADDR_LINKLOCAL(&pktinfo6->ipi6_addr)) {
                    dst_addr->sin6_scope_id = pktinfo6->ipi6_ifindex;
                } else {
                    dst_addr->sin6_scope_id = 0;
                }
            }
        }
    }
#else  // IPV6_PKTINFO && IP_PKTINFO
    ret = recvfrom(sock, buf, len, flags, src_addr, addrlen);
#endif // IPV6_PKTINFO && IP_PKTINFO
    if (ret == PCP_SOCKET_ERROR) {
        if (pcp_get_error() == PCP_ERR_WOULDBLOCK) {
            ret = PCP_ERR_WOULDBLOCK;
        } else {
            ret = PCP_ERR_RECV_FAILED;
        }
    }
#endif // PCP_SOCKET_IS_VOIDPTR

    return ret;
}

static ssize_t pcp_socket_sendto_impl(PCP_SOCKET sock, const void *buf,
                                      size_t len, int flags UNUSED,
                                      const struct sockaddr_in6 *src_addr,
                                      struct sockaddr *dest_addr,
                                      socklen_t addrlen) {
    ssize_t ret = -1;

#ifndef PCP_SOCKET_IS_VOIDPTR

#if defined(IPV6_PKTINFO)
    if (src_addr) {
#ifndef WIN32
        struct in6_pktinfo ipi6 = {0};

        uint8_t c[CMSG_SPACE(sizeof(struct in6_pktinfo))] = {0};
        struct iovec iov;
        struct msghdr msg;
        struct cmsghdr *cmsg;

        iov.iov_base = (void *)buf;
        iov.iov_len = len;
        memset(&msg, 0, sizeof(msg));
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        ipi6.ipi6_addr = src_addr->sin6_addr;
        ipi6.ipi6_ifindex = src_addr->sin6_scope_id;
        msg.msg_control = c;
        msg.msg_controllen = sizeof(c);
        cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = IPPROTO_IPV6;
        cmsg->cmsg_type = IPV6_PKTINFO;
        cmsg->cmsg_len = CMSG_LEN(sizeof(ipi6));
        memcpy(CMSG_DATA(cmsg), &ipi6, sizeof(ipi6));
        msg.msg_name = (void *)dest_addr;
        msg.msg_namelen = addrlen;
        ret = sendmsg(sock, &msg, flags);
#else  // WIN32
        WSABUF wsaBuf;
        wsaBuf.buf = (void *)buf;
        if (len > ULONG_MAX) {
            return PCP_ERR_SEND_FAILED;
        }
        wsaBuf.len = (ULONG)len;
        uint8_t c[WSA_CMSG_SPACE(sizeof(struct in6_pktinfo))] = {0};

        WSAMSG wsaMsg;
        memset(&wsaMsg, 0, sizeof(wsaMsg));
        wsaMsg.name = (struct sockaddr *)dest_addr;
        wsaMsg.namelen = addrlen;
        wsaMsg.lpBuffers = &wsaBuf;
        wsaMsg.dwBufferCount = 1;
        wsaMsg.Control.buf = (void *)c;

        // Set the source address inside the control message
        if (IN6_IS_ADDR_V4MAPPED(&src_addr->sin6_addr)) {
            wsaMsg.Control.len = WSA_CMSG_SPACE(sizeof(struct in_pktinfo));
            struct cmsghdr *cmsg = WSA_CMSG_FIRSTHDR(&wsaMsg);
            cmsg->cmsg_level = IPPROTO_IP;
            cmsg->cmsg_type = IP_PKTINFO;
            cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in_pktinfo));
            struct in_pktinfo *pktinfo =
                (struct in_pktinfo *)WSA_CMSG_DATA(cmsg);
            pktinfo->ipi_addr.s_addr = S6_ADDR32(&src_addr->sin6_addr)[3];
        } else {
            wsaMsg.Control.len = WSA_CMSG_SPACE(sizeof(struct in6_pktinfo));
            struct cmsghdr *cmsg = WSA_CMSG_FIRSTHDR(&wsaMsg);
            cmsg->cmsg_level = IPPROTO_IPV6;
            cmsg->cmsg_type = IPV6_PKTINFO;
            cmsg->cmsg_len = WSA_CMSG_LEN(sizeof(struct in6_pktinfo));
            struct in6_pktinfo *pktinfo =
                (struct in6_pktinfo *)WSA_CMSG_DATA(cmsg);
            IPV6_ADDR_COPY(&pktinfo->ipi6_addr, &src_addr->sin6_addr);
            pktinfo->ipi6_ifindex = src_addr->sin6_scope_id;
        }

        LPFN_WSARECVMSG WSARecvMsg;
        GUID WSARecvMsg_GUID = WSAID_WSARECVMSG;
        DWORD dwBytesReturned;

        if (WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &WSARecvMsg_GUID,
                     sizeof(GUID), &WSARecvMsg, sizeof(WSARecvMsg),
                     &dwBytesReturned, NULL, NULL) == SOCKET_ERROR) {
            pcp_logger(PCP_LOGLVL_PERR, "WSAIoctl failed");
            return 1;
        }

        // Send the packet
        DWORD bytesSent = 0;
        if (WSASendMsg(sock, &wsaMsg, 0, &bytesSent, NULL, NULL) ==
            SOCKET_ERROR) {
            PCP_LOG(PCP_LOGLVL_PERR, "WSASendMsg failed: %d",
                    WSAGetLastError());
        } else {
            ret = bytesSent;
        }
#endif // WIN32
    } else
#else  // IPV6_PKTINFO
    ret = sendto(sock, buf, len, 0, dest_addr, addrlen);
#endif /* IPV6_PKTINFO */

        if ((ret == PCP_SOCKET_ERROR) || (ret != (ssize_t)len)) {
            if (pcp_get_error() == PCP_ERR_WOULDBLOCK) {
                ret = PCP_ERR_WOULDBLOCK;
            } else {
                ret = PCP_ERR_SEND_FAILED;
            }
        }
#endif
    return ret;
}

static int pcp_socket_close_impl(PCP_SOCKET sock) {
#ifndef PCP_SOCKET_IS_VOIDPTR
    return CLOSE(sock);
#else
    return PCP_SOCKET_ERROR;
#endif
}
