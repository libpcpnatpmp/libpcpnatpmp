/*
 *------------------------------------------------------------------
 * test_scale.c
 *
 * A client with thousands of reservations: MAP, MAP with a FILTER option and
 * PEER flows created at once, against a test server in a process of its own.
 * Checks that every flow is one request of at most 1100 octets and gets
 * answered, and that after a server restart (its epoch starts again) every
 * reservation is re-sent and answered again.
 *
 * Usage: test_scale [flows [batch [server_delay_us]]]
 *   flows            default 10000, at most 50000
 *   batch            flows created before waiting for their answers;
 *                    0 (default) = all at once
 *   server_delay_us  time the server takes per request, like one that
 *                    installs a firewall rule for each
 *
 * Timings, datagram counts and kernel UDP receive-buffer drops go to stderr.
 *------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "pcpnatpmp.h"

#include "pcp-server.h"
#include "pcp_client_db.h"
#include "pcp_msg_structs.h"
#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

#define SERVER_PORT "5361"
#define SERVER_ADDR "127.0.0.1:" SERVER_PORT
#define DEFAULT_FLOWS 10000
#define MAX_FLOWS 50000
#define PHASE_TIMEOUT_S 120.0

extern pcp_socket_vt_t default_socket_vt;

static size_t succeeded_count;
static size_t sent_total, sent_map, sent_peer, sent_with_options;
static size_t sent_max_len;
static pcp_socket_vt_t counting_vt;
static pid_t server_pid = -1;

static void cleanup_server(void) {
    if (server_pid > 0) {
        if (kill(server_pid, SIGTERM) < 0 && errno != ESRCH) {
            perror("kill scale server");
        }
        while (waitpid(server_pid, NULL, 0) < 0) {
            if (errno != EINTR) {
                perror("waitpid scale server");
                break;
            }
        }
        server_pid = -1;
    }
}

/* Wraps the library's sendto to see every request it puts on the wire. */
static ssize_t counting_sendto(PCP_SOCKET sock, const void *buf, size_t len,
                               int flags, const struct sockaddr_in6 *src,
                               struct sockaddr *dest, socklen_t addrlen) {
    const uint8_t *p = buf;

    sent_total++;
    if (len > sent_max_len) {
        sent_max_len = len;
    }
    if (len >= 2 && p[0] == 2) {
        if ((p[1] & 0x7F) == PCP_OPCODE_MAP) {
            sent_map++;
            if (len > 60) { /* a MAP longer than 60 octets carries options */
                sent_with_options++;
            }
        } else if ((p[1] & 0x7F) == PCP_OPCODE_PEER) {
            sent_peer++;
        }
    }
    return default_socket_vt.sock_sendto(sock, buf, len, flags, src, dest,
                                         addrlen);
}

static void on_change(pcp_flow_t *f, struct sockaddr *src UNUSED,
                      struct sockaddr *ext UNUSED, pcp_fstate_e s,
                      void *arg UNUSED) {
    unsigned char *succeeded = pcp_flow_get_user_data(f);

    if (s == pcp_state_succeeded && !*succeeded) {
        *succeeded = 1;
        succeeded_count++;
    }
}

static double now_s(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
}

/* Runs the client until `want` flows have reported success since
 * succeeded_count was last reset, or the time is up. pcp_pulse reads one
 * reply per call, so drain what has arrived before sleeping. */
static double drive(pcp_ctx_t *ctx, size_t want, double max_s) {
    double start = now_s();

    while (succeeded_count < want && now_s() - start < max_s) {
        struct timeval tout;
        int i;

        for (i = 0; i < 200; i++) {
            tout.tv_sec = 0;
            tout.tv_usec = 0;
            pcp_pulse(ctx, &tout);
        }
        tout.tv_sec = 0;
        tout.tv_usec = 1000;
        select(0, NULL, NULL, NULL, &tout);
    }
    return now_s() - start;
}

/* Kernel count of UDP datagrams dropped for lack of receive buffer. */
static long udp_rcvbuf_errors(void) {
    char line[512];
    char *names = NULL;
    long result = -1;
    FILE *f = fopen("/proc/net/snmp", "r");

    if (f == NULL) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Udp:", 4) != 0) {
            continue;
        }
        if (names == NULL) {
            names = strdup(line);
        } else {
            char *save_n = NULL, *save_v = NULL;
            char *n = strtok_r(names, " \n", &save_n);
            char *v = strtok_r(line, " \n", &save_v);

            while (n != NULL && v != NULL) {
                if (strcmp(n, "RcvbufErrors") == 0) {
                    result = atol(v);
                    break;
                }
                n = strtok_r(NULL, " \n", &save_n);
                v = strtok_r(NULL, " \n", &save_v);
            }
            break;
        }
    }
    free(names);
    fclose(f);
    return result;
}

/* The server runs in a process of its own and answers until it is killed, so
 * it drains its socket while the client sends. */
static pid_t start_server(int uptime_s, int delay_us) {
    int ready_pipe[2];
    char ready;
    pid_t pid;

    TEST(pipe(ready_pipe) == 0);
    pid = fork();
    TEST(pid >= 0);
    if (pid == 0) {
        server_info_t info;
        pcp_test_server_t server;

        close(ready_pipe[0]);
        memset(&info, 0, sizeof(info));
        info.server_version = 2;
        info.default_result_code = 255;
        inet_pton(AF_INET6, "::ffff:9.9.9.9", &info.ext_ip);
        info.epoch_time_start = time(NULL) - uptime_s;

        if (freopen("/dev/null", "w", stdout) == NULL ||
            pcp_test_server_start(&server, SERVER_PORT, "127.0.0.1", &info) !=
                0) {
            _exit(1);
        }
        if (write(ready_pipe[1], "R", 1) != 1) {
            _exit(1);
        }
        close(ready_pipe[1]);
        for (;;) {
            int result = pcp_test_server_pulse(&server, 1000);
            if (result < 0) {
                _exit(1);
            }
            if (result > 0 && delay_us > 0) {
                usleep((useconds_t)delay_us);
            }
        }
    }
    server_pid = pid;
    close(ready_pipe[1]);
    {
        fd_set read_fds;
        struct timeval timeout = {5, 0};
        FD_ZERO(&read_fds);
        FD_SET(ready_pipe[0], &read_fds);
        TEST(select(ready_pipe[0] + 1, &read_fds, NULL, NULL, &timeout) == 1);
    }
    TEST(read(ready_pipe[0], &ready, 1) == 1 && ready == 'R');
    close(ready_pipe[0]);
    return pid;
}

static void stop_server(pid_t pid) {
    TEST(pid == server_pid);
    cleanup_server();
}

int main(int argc, char **argv) {
    size_t n = argc > 1 ? (size_t)strtoul(argv[1], NULL, 10) : DEFAULT_FLOWS;
    size_t batch = argc > 2 ? (size_t)strtoul(argv[2], NULL, 10) : 0;
    int delay_us = argc > 3 ? atoi(argv[3]) : 0;
    size_t i, before;
    pcp_flow_t **flows;
    unsigned char *succeeded;
    pcp_ctx_t *ctx;
    pid_t server;
    struct sockaddr_storage src, dst, filter_ip;
    double t_answer = 0, t_restart;
    long drops0, drops1;

    TEST(n >= 3 && n <= MAX_FLOWS);
    TEST(atexit(cleanup_server) == 0);
    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_ERR;

    drops0 = udp_rcvbuf_errors();
    server = start_server(100, delay_us); /* up for a while: epoch is 100 */

    counting_vt = default_socket_vt;
    counting_vt.sock_sendto = counting_sendto;
    ctx = pcp_init(DISABLE_AUTODISCOVERY, &counting_vt);
    TEST(ctx != NULL);
    TEST(pcp_add_server(ctx, Sock_pton(SERVER_ADDR), 2) == 0);
    pcp_set_flow_change_cb(ctx, on_change, NULL);
    sock_pton("127.0.0.1:8888", (struct sockaddr *)&dst);
    sock_pton("8.8.8.8:4444", (struct sockaddr *)&filter_ip);

    flows = calloc(n, sizeof(*flows));
    TEST(flows != NULL);
    succeeded = calloc(n, sizeof(*succeeded));
    TEST(succeeded != NULL);

    for (i = 0; i < n; i++) {
        char addr[32];

        snprintf(addr, sizeof(addr), "127.0.0.1:%u", (unsigned)(1024 + i));
        sock_pton(addr, (struct sockaddr *)&src);
        switch (i % 3) {
        case 0: /* plain MAP */
            flows[i] = pcp_new_flow(ctx, (struct sockaddr *)&src, NULL, NULL,
                                    IPPROTO_TCP, 600, &succeeded[i]);
            break;
        case 1: /* MAP with a FILTER option */
            flows[i] = pcp_new_flow(ctx, (struct sockaddr *)&src, NULL, NULL,
                                    IPPROTO_UDP, 600, &succeeded[i]);
            TEST(flows[i] != NULL);
            pcp_flow_set_filter_opt(flows[i], (struct sockaddr *)&filter_ip,
                                    12);
            break;
        default: /* PEER */
            flows[i] = pcp_new_flow(ctx, (struct sockaddr *)&src,
                                    (struct sockaddr *)&dst, NULL, IPPROTO_TCP,
                                    600, &succeeded[i]);
            break;
        }
        TEST(flows[i] != NULL);
        if (batch > 0 && ((i + 1) % batch == 0 || i + 1 == n)) {
            t_answer += drive(ctx, i + 1, PHASE_TIMEOUT_S);
        }
    }
    if (batch == 0) {
        t_answer = drive(ctx, n, PHASE_TIMEOUT_S);
    }
    fprintf(stderr,
            "%zu flows: answered in %.2fs, %zu/%zu succeeded; sent %zu "
            "datagrams (%zu MAP, %zu with options, %zu PEER), largest %zu "
            "octets\n",
            n, t_answer, succeeded_count, n, sent_total, sent_map,
            sent_with_options, sent_peer, sent_max_len);
    TEST(succeeded_count == n);
    TEST(sent_max_len <= PCP_MAX_LEN);
    /* every flow is a request of its own: one datagram per reservation */
    TEST(sent_total >= n);
    TEST(sent_map == n - n / 3);
    TEST(sent_with_options >= n / 3);
    TEST(sent_peer >= n / 3);

    /* Make every renewal due together and check that one pulse is paced. */
    for (i = 0; i < n; ++i) {
        flows[i]->timeout.tv_sec = 1;
        flows[i]->timeout.tv_usec = 0;
    }
    get_pcp_server(ctx, 0)->next_timeout = flows[0]->timeout;
    before = sent_total;
    {
        struct timeval timeout = {0, 0};
        pcp_pulse(ctx, &timeout);
    }
    if (n >= 200) {
        TEST(sent_total - before < n);
    }
    for (i = 0; i < n; ++i) {
        TEST(flows[i]->recv_lifetime > time(NULL));
    }

    /* Finish the renewals before restarting the server. */
    {
        double deadline = now_s() + PHASE_TIMEOUT_S;
        size_t pending;
        do {
            struct timeval timeout = {0, 0};
            pcp_pulse(ctx, &timeout);
            pending = 0;
            for (i = 0; i < n; ++i) {
                if (flows[i]->state == pfs_send_renew ||
                    flows[i]->timeout.tv_sec < time(NULL) + 100) {
                    ++pending;
                }
            }
        } while (pending && now_s() < deadline);
        TEST(pending == 0);
    }

    /* The server restarts and its epoch starts again from zero. One
     * refreshed flow gets a reply with the lower epoch; the client must then
     * re-send every reservation (Rapid Recovery). */
    stop_server(server);
    sleep(1);
    server = start_server(0, delay_us);
    before = sent_total;
    pcp_flow_set_lifetime(flows[0], 601);
    succeeded_count = 0;
    memset(succeeded, 0, n * sizeof(*succeeded));
    t_restart = drive(ctx, n, PHASE_TIMEOUT_S);
    drops1 = udp_rcvbuf_errors();
    fprintf(stderr,
            "after restart: %zu/%zu flows answered again in %.2fs, %zu "
            "datagrams sent, %ld UDP datagrams dropped for lack of buffer\n",
            succeeded_count, n, t_restart, sent_total - before,
            drops0 < 0 ? -1 : drops1 - drops0);
    TEST(succeeded_count >= n);
    TEST(sent_total - before >= n);

    for (i = 0; i < n; i++) {
        pcp_delete_flow(flows[i]);
    }
    free(flows);
    free(succeeded);
    pcp_terminate(ctx, 0);
    stop_server(server);
    PD_SOCKET_CLEANUP();
    return 0;
}
