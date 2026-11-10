/*
 *------------------------------------------------------------------
 * test_multicast_announce.c
 *
 * A restarted PCP server announces itself with an unsolicited ANNOUNCE
 * response to 224.0.0.1:5350 (RFC 6887, 14.2.1). The client must see it:
 * when the epoch shows a restart it waits a random time of up to 5 seconds
 * and then re-sends its mappings; when the epoch is where the client expects
 * it it does nothing.
 *
 * The fake server is a plain UDP socket bound to the address of a
 * multicast-capable interface, since loopback does not do multicast. The test
 * is skipped when there is no such interface.
 *------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "pcpnatpmp.h"

#include "pcp_client_db.h"
#include "pcp_msg_structs.h"
#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

#define SERVER_PORT 5362
#define ANNOUNCE_PORT 5350
#define FIRST_EPOCH 100

static double now_s(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

/* Address of an up, multicast-capable, non-loopback IPv4 interface. */
static int find_multicast_address(char *out, size_t out_len) {
    struct ifaddrs *list, *a;
    int found = 0;

    if (getifaddrs(&list) != 0) {
        return 0;
    }
    for (a = list; a != NULL && !found; a = a->ifa_next) {
        if (a->ifa_addr != NULL && a->ifa_addr->sa_family == AF_INET &&
            (a->ifa_flags & IFF_UP) && (a->ifa_flags & IFF_MULTICAST) &&
            !(a->ifa_flags & IFF_LOOPBACK)) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)a->ifa_addr)->sin_addr,
                      out, (socklen_t)out_len);
            found = 1;
        }
    }
    freeifaddrs(list);
    return found;
}

/* The port the library's socket of `ctx` is bound to. */
static int client_port(pcp_ctx_t *ctx) {
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);

    TEST(getsockname(pcp_get_socket(ctx), (struct sockaddr *)&ss, &len) == 0);
    if (ss.ss_family == AF_INET6) {
        return ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
    }
    return ntohs(((struct sockaddr_in *)&ss)->sin_port);
}

static int join_warnings;
static int listen_warnings;

/* Prints what the library logs, and counts the warnings about joining the
 * groups and about listening on port 5350. */
static void test_logger(pcp_loglvl_e level UNUSED, const char *msg) {
    if (strstr(msg, "Could not join") != NULL) {
        join_warnings++;
    }
    if (strstr(msg, "Could not listen on port 5350") != NULL) {
        listen_warnings++;
    }
    fprintf(stderr, "%s\n", msg);
}

static int serve_socket(const char *address) {
    struct sockaddr_in sa;
    struct in_addr ifa;
    int one = 1;
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    TEST(fd >= 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(SERVER_PORT);
    TEST(inet_pton(AF_INET, address, &sa.sin_addr) == 1);
    TEST(bind(fd, (struct sockaddr *)&sa, sizeof(sa)) == 0);
    ifa = sa.sin_addr;
    TEST(setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &ifa, sizeof(ifa)) == 0);
    return fd;
}

/* Waits up to `seconds` for a request from the client while it runs. */
static int wait_for_request(pcp_ctx_t *ctx, int fd, double seconds,
                            unsigned char *buf, size_t buf_len,
                            struct sockaddr_in *peer) {
    double end = now_s() + seconds;

    while (now_s() < end) {
        struct timeval tout = {0, 0};
        struct pollfd p = {fd, POLLIN, 0};
        socklen_t peer_len = sizeof(*peer);
        ssize_t n;

        pcp_pulse(ctx, &tout);
        if (poll(&p, 1, 5) > 0 &&
            (n = recvfrom(fd, buf, buf_len, 0, (struct sockaddr *)peer,
                          &peer_len)) > 0) {
            return (int)n;
        }
    }
    return 0;
}

static void send_multicast_announce(int fd, uint32_t epoch) {
    unsigned char a[24];
    struct sockaddr_in group;

    memset(a, 0, sizeof(a));
    a[0] = 2;    /* version */
    a[1] = 0x80; /* response, opcode ANNOUNCE */
    a[8] = (unsigned char)(epoch >> 24);
    a[9] = (unsigned char)(epoch >> 16);
    a[10] = (unsigned char)(epoch >> 8);
    a[11] = (unsigned char)epoch;
    memset(&group, 0, sizeof(group));
    group.sin_family = AF_INET;
    group.sin_port = htons(ANNOUNCE_PORT);
    TEST(inet_pton(AF_INET, "224.0.0.1", &group.sin_addr) == 1);
    TEST(sendto(fd, a, sizeof(a), 0, (struct sockaddr *)&group,
                sizeof(group)) == (ssize_t)sizeof(a));
}

/* A socket table which only wraps sendto, as an application does to count or
 * log its datagrams: the socket is still the library's own. */
static ssize_t wrapped_sendto(PCP_SOCKET sockfd, const void *buf, size_t len,
                              int flags, const struct sockaddr_in6 *src_addr,
                              struct sockaddr *dest_addr, socklen_t addrlen) {
    return default_socket_vt.sock_sendto(sockfd, buf, len, flags, src_addr,
                                         dest_addr, addrlen);
}

static int last_client_port;

/* One client with one mapping, using the socket table `socket_vt` (NULL for
 * the library's). The server answers with epoch FIRST_EPOCH, then announces
 * itself with `announce_epoch` to the multicast group. Returns how long the
 * client took to send its request again, in seconds, or -1 if it did not
 * within `wait_s`. restart_max_ms is the longest random wait before
 * re-sending; UINT32_MAX leaves the library's default. */
static double announce_resend_delay(const char *address,
                                    uint32_t announce_epoch,
                                    uint32_t restart_max_ms, double wait_s,
                                    pcp_socket_vt_t *socket_vt) {
    char server_str[64];
    char client_str[64];
    unsigned char buf[PCP_MAX_LEN];
    struct sockaddr_in peer;
    struct sockaddr_storage src;
    pcp_ctx_t *ctx;
    pcp_flow_t *flow;
    int fd = serve_socket(address);
    int n;
    double announced, delay = -1;

    snprintf(server_str, sizeof(server_str), "%s:%d", address, SERVER_PORT);
    snprintf(client_str, sizeof(client_str), "%s:2000", address);

    ctx = pcp_init(DISABLE_AUTODISCOVERY, socket_vt);
    TEST(ctx != NULL);
    last_client_port = client_port(ctx);
    listen_warnings = 0;
    if (restart_max_ms != UINT32_MAX) {
        pcp_set_restart_delay(ctx, restart_max_ms);
    }
    TEST(pcp_add_server(ctx, Sock_pton(server_str), 2) == 0);
    if (listen_warnings != 0) {
        /* another program holds 5350 and does not share it */
        printf("SKIP: cannot listen on port %d\n", ANNOUNCE_PORT);
        exit(0);
    }
    sock_pton(client_str, (struct sockaddr *)&src);
    flow = pcp_new_flow(ctx, (struct sockaddr *)&src, NULL, NULL, IPPROTO_TCP,
                        600, NULL);
    TEST(flow != NULL);

    /* the first request: answer it with a MAP success */
    n = wait_for_request(ctx, fd, 3.0, buf, sizeof(buf), &peer);
    TEST(n == 60);
    buf[1] |= 0x80; /* response */
    buf[3] = 0;     /* result: success */
    buf[4] = 0;     /* lifetime 600 */
    buf[5] = 0;
    buf[6] = 0x02;
    buf[7] = 0x58;
    memset(buf + 8, 0, 16); /* epoch, then reserved */
    buf[11] = FIRST_EPOCH;
    buf[42] = 0x9c; /* assigned external port 40000 */
    buf[43] = 0x40;
    TEST(sendto(fd, buf, (size_t)n, 0, (struct sockaddr *)&peer,
                sizeof(peer)) == n);
    TEST(pcp_wait(flow, 2000, 0) == pcp_state_succeeded);
    (void)wait_for_request(ctx, fd, 1.0, buf, sizeof(buf), &peer);

    if (restart_max_ms == 0) {
        pcp_server_t *server = get_pcp_server(ctx, flow->pcp_server_indx);
        struct timeval original_timeout = flow->timeout;
        struct timeval original_server_timeout = server->next_timeout;
        time_t expiry = flow->recv_lifetime;
        pcp_flow_state_e states[] = {pfs_wait_for_lifetime_renew,
                                     pfs_wait_resp};
        size_t i;

        // Another server's timeout also scans this context's flows.
        TEST(pcp_add_server(ctx, Sock_pton("127.0.0.2:5363"), 2) == 1);
        for (i = 0; i < sizeof(states) / sizeof(*states); ++i) {
            flow->state = states[i];
            flow->timeout.tv_sec = 1;
            flow->timeout.tv_usec = 0;
            server->restart_pending = 1;
            gettimeofday(&server->restart_resend_at, NULL);
            server->restart_resend_at.tv_sec += 2;
            server->next_timeout = server->restart_resend_at;
            TEST(wait_for_request(ctx, fd, 0.1, buf, sizeof(buf), &peer) == 0);
            TEST(flow->state == states[i]);
            TEST(flow->recv_lifetime == expiry);
            server->restart_pending = 0;
        }
        flow->state = pfs_wait_for_lifetime_renew;
        flow->timeout = original_timeout;
        server->next_timeout = original_server_timeout;
    }

    announced = now_s();
    send_multicast_announce(fd, announce_epoch);
    if (wait_for_request(ctx, fd, wait_s, buf, sizeof(buf), &peer) == 60) {
        delay = now_s() - announced;
    }

    pcp_delete_flow(flow);
    pcp_terminate(ctx, 0);
    close(fd);
    return delay;
}

/* Adding a server again (the library keys servers by address, so a new port
 * updates the existing one) joins a group that is already joined. That is not
 * a failure: adding the server works and nothing is logged about it. */
static void server_added_twice(const char *address) {
    char first[80], second[80]; /* address, ":" and a port */
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);

    snprintf(first, sizeof(first), "%s:%d", address, SERVER_PORT);
    snprintf(second, sizeof(second), "%s:%d", address, SERVER_PORT + 1);
    TEST(ctx != NULL);
    join_warnings = 0;
    TEST(pcp_add_server(ctx, Sock_pton(first), 2) >= 0);
    TEST(pcp_add_server(ctx, Sock_pton(second), 2) >= 0);
    TEST(join_warnings == 0);
    pcp_terminate(ctx, 0);
}

/* Each client on the host has a random port of its own (RFC 6887, 8.1), not
 * 5350, to which the server announces. A second client must hear the
 * announcement while the first is open, and the listeners sharing 5350 must
 * not take either client's replies. */
static void second_client_on_host(const char *address) {
    struct sockaddr_in to;
    struct pollfd p;
    unsigned char c = 1;
    pcp_ctx_t *first = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    int fd;

    TEST(first != NULL);
    TEST(client_port(first) != ANNOUNCE_PORT);

    /* a server restart seen by a second client */
    TEST(announce_resend_delay(address, 0, 0, 2.0, NULL) >= 0);
    TEST(last_client_port != ANNOUNCE_PORT);
    TEST(last_client_port != client_port(first));
    {
        pcp_ctx_t *second = pcp_init(DISABLE_AUTODISCOVERY, NULL);
        pcp_server_t *server;
        char server_address[80];
        struct in6_addr original;

        TEST(second != NULL);
        snprintf(server_address, sizeof(server_address), "%s:%d", address,
                 SERVER_PORT);
        TEST(pcp_add_server(second, Sock_pton(server_address), 2) == 0);
        server = get_pcp_server(second, 0);
        TEST(server->announce_listening);
        original = server->announce_src_ip;
        TEST(inet_pton(AF_INET6, "::ffff:127.0.0.1", server->src_ip) == 1);
        pcp_socket_listen_announce(server);
        TEST(server->announce_listening);
        TEST(IN6_ARE_ADDR_EQUAL(&server->announce_src_ip,
                                (struct in6_addr *)server->src_ip));
        TEST(!IN6_ARE_ADDR_EQUAL(&server->announce_src_ip, &original));
        memcpy(server->src_ip, &original, sizeof(original));
        pcp_socket_listen_announce(server);
        TEST(server->announce_listening);
        TEST(IN6_ARE_ADDR_EQUAL(&server->announce_src_ip, &original));
        pcp_terminate(second, 0);
    }

    /* unicast to the first client's port goes to it */
    fd = serve_socket(address);
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(client_port(first));
    TEST(inet_pton(AF_INET, address, &to.sin_addr) == 1);
    TEST(sendto(fd, &c, 1, 0, (struct sockaddr *)&to, sizeof(to)) == 1);
    p.fd = pcp_get_socket(first);
    p.events = POLLIN;
    p.revents = 0;
    TEST(poll(&p, 1, 1000) == 1);
    close(fd);
    pcp_terminate(first, 0);
}

#define RANDOM_TRIALS 8

int main(void) {
    char address[64];
    pcp_socket_vt_t wrapped_socket = default_socket_vt;
    double delay, shortest = 1e9, longest = -1;
    int i;

#ifndef __linux__
    /* missing elsewhere: see pcp_socket_join_announce_groups() */
    printf("SKIP: IPv4 restart announcements are only received on Linux\n");
    return 0;
#endif
    pcp_log_level = PCP_LOGLVL_WARN;
    pcp_set_loggerfn(test_logger);
    if (!find_multicast_address(address, sizeof(address))) {
        printf("SKIP: no multicast-capable IPv4 interface\n");
        return 0;
    }
    printf("using %s\n", address);

    /* the server restarted: its epoch is back near zero. With the wait
     * turned off the client re-sends at once. */
    delay = announce_resend_delay(address, 0, 0, 2.0, NULL);
    TEST(delay >= 0 && delay < 0.2);

    /* the server has been up as long as the client expects: no restart, so
     * nothing is sent even after the longest wait */
    TEST(announce_resend_delay(address, FIRST_EPOCH + 1, 1000, 2.0, NULL) < 0);

    /* RFC 6887, 14.2.1: the client waits a random time of up to max_ms before
     * it re-sends, and different clients pick different times. */
    for (i = 0; i < RANDOM_TRIALS; i++) {
        delay = announce_resend_delay(address, 0, 1000, 2.0, NULL);
        TEST(delay >= 0 && delay <= 1.2);
        if (delay < shortest) {
            shortest = delay;
        }
        if (delay > longest) {
            longest = delay;
        }
    }
    printf("random wait over %d trials: %.3f s to %.3f s\n", RANDOM_TRIALS,
           shortest, longest);

    /* the default is the longest the RFC allows: 5 seconds */
    delay = announce_resend_delay(address, 0, UINT32_MAX, 6.5, NULL);
    TEST(delay >= 0 && delay <= 5.2);

    /* an application that wraps sendto still has the library's socket */
    wrapped_socket.sock_sendto = wrapped_sendto;
    TEST(announce_resend_delay(address, 0, 0, 2.0, &wrapped_socket) >= 0);

    server_added_twice(address);
    second_client_on_host(address);
    return 0;
}
