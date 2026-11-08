/*
 *------------------------------------------------------------------
 * test_server_restart.c
 *
 * March 13th, 2013, mbagljas
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
#ifdef WIN32
#include <winsock2.h>

#include <windows.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#include "pcpnatpmp.h"

#include "pcp-server.h"
#include "pcp_client_db.h"
#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

pcp_flow_t *flow_to_wait = NULL;
uint32_t notified = 0;

#define LOG_BEFORE_RESTART "test_server_restart_before.log"
#define LOG_AFTER_RESTART "test_server_restart_after.log"

typedef struct server_thread_args {
    uint8_t second_server_requests;
} server_thread_args_t;

static void init_server_info(server_info_t *server_info, uint8_t end_after_recv,
                             const char *log_file) {
    memset(server_info, 0, sizeof(*server_info));
    strncpy(server_info->log_file, log_file, sizeof(server_info->log_file) - 1);
    server_info->server_version = 2;
    server_info->default_result_code = 255;
    server_info->end_after_recv = end_after_recv;
}

#ifdef WIN32
static DWORD WINAPI server_thread_main(LPVOID arg) {
#else
static void *server_thread_main(void *arg) {
#endif
    server_thread_args_t *args = (server_thread_args_t *)arg;
    server_info_t server_info;

    init_server_info(&server_info, 1, LOG_BEFORE_RESTART);
    inet_pton(AF_INET6, "::ffff:9.9.9.9", &server_info.ext_ip);
    server_info.epoch_time_start = time(NULL);
    execPCPServer("5351", "0.0.0.0", &server_info);

    init_server_info(&server_info, args->second_server_requests,
                     LOG_AFTER_RESTART);
    inet_pton(AF_INET6, "::ffff:9.9.9.9", &server_info.ext_ip);
    server_info.epoch_time_start = time(NULL);
    execPCPServer("5351", "0.0.0.0", &server_info);

#ifdef WIN32
    return 0;
#else
    return NULL;
#endif
}

static void notify_cb_1(pcp_flow_t *f, struct sockaddr *src_addr UNUSED,
                        struct sockaddr *ext_addr UNUSED, pcp_fstate_e s,
                        void *cb_arg UNUSED) {
    if ((f == flow_to_wait) && (s == pcp_state_succeeded)) {
        notified = 1;
    }
    sleep(1);
}

static char *read_whole_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long size;
    char *buf;

    TEST(f != NULL);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    rewind(f);
    buf = malloc((size_t)size + 1);
    TEST(buf != NULL);
    TEST(fread(buf, 1, (size_t)size, f) == (size_t)size);
    buf[size] = 0;
    fclose(f);
    return buf;
}

/* Copy of a server log without its "requested lifetime" and external address
 * lines. A client may re-send a reservation with the lifetime it has left, and
 * renews with the external address the server assigned instead of the one it
 * first suggested; nothing else about the reservation may change. */
static char *without_lifetime(const char *log) {
    char *out = malloc(strlen(log) + 1);
    char *w = out;
    const char *line = log;

    TEST(out != NULL);
    while (*line) {
        const char *eol = strchr(line, '\n');
        size_t len = eol ? (size_t)(eol - line) + 1 : strlen(line);
        if (strncmp(line, "PCP requested lifetime:", 23) != 0 &&
            strncmp(line, "MAP Ext IP:", 11) != 0 &&
            strncmp(line, "External IP:", 12) != 0) {
            memcpy(w, line, len);
            w += len;
        }
        line += len;
    }
    *w = 0;
    return out;
}

/* Rapid Recovery (RFC 6887, 8.5): after the server restarts, every request
 * the old server had seen must come back unchanged: same protocol, internal
 * and external port, external address and, because it is the same mapping,
 * the same nonce. */
static void check_reservations_resent(const char *const *must_appear) {
    char *before = read_whole_file(LOG_BEFORE_RESTART);
    char *after = read_whole_file(LOG_AFTER_RESTART);
    char *before_n = without_lifetime(before);
    char *after_n = without_lifetime(after);
    char *rec = strstr(before_n, "PCP version:");
    int records = 0;

    while (rec != NULL) {
        char *next = strstr(rec + 1, "PCP version:");
        size_t len = next ? (size_t)(next - rec) : strlen(rec);
        char *copy = malloc(len + 1);

        TEST(copy != NULL);
        memcpy(copy, rec, len);
        copy[len] = 0;
        if (strstr(after_n, copy) == NULL) {
            printf("not re-sent after restart:\n%s\nlog after restart:\n%s\n",
                   copy, after);
            TEST(0);
        }
        free(copy);
        records++;
        rec = next;
    }
    TEST(records >= 1);
    /* the address the first server assigned is the one asked for again */
    TEST(strstr(after, "MAP Ext IP: \t\t ::ffff:9.9.9.9") != NULL);
    for (; *must_appear != NULL; must_appear++) {
        if (strstr(after, *must_appear) == NULL) {
            printf("log after restart lacks '%s':\n%s\n", *must_appear, after);
            TEST(0);
        }
    }
    free(before);
    free(after);
    free(before_n);
    free(after_n);
}

static int run_scenario(uint8_t second_server_requests, int expect_success) {
    struct sockaddr_storage destination1_ip4;
    struct sockaddr_storage source1_ip4;
    struct sockaddr_storage ext1_ip4;
    uint8_t protocol1 = 6;
    uint32_t lifetime1 = 10;
    struct sockaddr_storage destination2_ip4;
    struct sockaddr_storage source2_ip4;
    struct sockaddr_storage ext2_ip4;
    uint8_t protocol2 = 17;
    uint32_t lifetime2 = 15;
    pcp_flow_t *flow1 = NULL;
    pcp_flow_t *flow2 = NULL;
    pcp_ctx_t *ctx;
    server_thread_args_t thread_args;
#ifdef WIN32
    HANDLE thread_handle;
#else
    pthread_t thread;
#endif

    thread_args.second_server_requests = second_server_requests;
    remove(LOG_BEFORE_RESTART);
    remove(LOG_AFTER_RESTART);

#ifdef WIN32
    thread_handle =
        CreateThread(NULL, 0, server_thread_main, &thread_args, 0, NULL);
    TEST(thread_handle != NULL);
    Sleep(4000);
#else
    TEST(pthread_create(&thread, NULL, server_thread_main, &thread_args) == 0);
    sleep(4);
#endif

    ctx = pcp_init(0, NULL);
    TEST(ctx);
    /* these tests are about what is re-sent, not when */
    pcp_set_restart_delay(ctx, 0);
    pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2);

    sock_pton("0.0.0.0:0", (struct sockaddr *)&destination1_ip4);
    sock_pton("127.0.0.1:2222", (struct sockaddr *)&source1_ip4);
    sock_pton("2.2.2.2", (struct sockaddr *)&ext1_ip4);

    sock_pton("0.0.0.0:0", (struct sockaddr *)&destination2_ip4);
    sock_pton("127.0.0.1:1111", (struct sockaddr *)&source2_ip4);
    sock_pton("1.1.1.1", (struct sockaddr *)&ext2_ip4);

    printf("\n");
    printf("#############################################\n");
    printf("####   *****************************     ####\n");
    printf("####   * Begin server timeout test *     ####\n");
    printf("####   *****************************     ####\n");
    printf("#############################################\n");

    flow1 =
        pcp_new_flow(ctx, (struct sockaddr *)&source1_ip4,
                     (struct sockaddr *)&destination1_ip4,
                     (struct sockaddr *)&ext1_ip4, protocol1, lifetime1, NULL);

    flow2 =
        pcp_new_flow(ctx, (struct sockaddr *)&source2_ip4,
                     (struct sockaddr *)&destination2_ip4,
                     (struct sockaddr *)&ext2_ip4, protocol2, lifetime2, NULL);

    if (flow1->key_bucket > flow2->key_bucket) { // LCOV_EXCL_START
        pcp_flow_t *tmp = flow1;
        flow1 = flow2;
        flow2 = tmp;
    } // LCOV_EXCL_STOP

    pcp_set_flow_change_cb(ctx, notify_cb_1, NULL);
    flow_to_wait = flow2;
    notified = 0;

    TEST(pcp_wait(flow1, 2000, 0) == pcp_state_succeeded);

    TEST(pcp_wait(flow2, 2000, 0) == pcp_state_succeeded);
    if (expect_success) {
        pcp_fstate_e e = pcp_wait(flow1, 200, 0);
        printf("e=%d", e);
        TEST(e == pcp_state_succeeded);
        TEST(notified == 1);
    } else {
        pcp_fstate_e e = pcp_wait(flow1, 200, 0);
        printf("e=%d", e);
        TEST(e != pcp_state_succeeded);
        TEST(notified == 1);
    }

    pcp_close_flow(flow1);
    pcp_delete_flow(flow1);
    flow1 = NULL;
    pcp_close_flow(flow2);
    pcp_delete_flow(flow2);
    flow2 = NULL;

    pcp_terminate(ctx, 1);

#ifdef WIN32
    WaitForSingleObject(thread_handle, INFINITE);
    CloseHandle(thread_handle);
#else
    pthread_join(thread, NULL);
#endif

    if (expect_success) {
        static const char *const must_appear[] = {
            "MAP int port: \t\t 2222", "MAP int port: \t\t 1111", NULL};
        check_reservations_resent(must_appear);
    }
    remove(LOG_BEFORE_RESTART);
    remove(LOG_AFTER_RESTART);

    return 0;
}

#ifndef WIN32 /* the scenario below uses pthreads and select() */
static void *reservation_server_main(void *arg UNUSED) {
    server_info_t server_info;

    /* before the restart: up for an hour, so the epoch after the restart is
     * clearly lower than expected; answers the three reservations, then goes
     * away */
    init_server_info(&server_info, 3, LOG_BEFORE_RESTART);
    inet_pton(AF_INET6, "::ffff:9.9.9.9", &server_info.ext_ip);
    server_info.epoch_time_start = time(NULL) - 3600;
    execPCPServer("5351", "0.0.0.0", &server_info);

    /* after the restart: epoch starts again; logs until it times out */
    init_server_info(&server_info, 0, LOG_AFTER_RESTART);
    inet_pton(AF_INET6, "::ffff:9.9.9.9", &server_info.ext_ip);
    server_info.tv.tv_sec = 14;
    server_info.epoch_time_start = time(NULL);
    execPCPServer("5351", "0.0.0.0", &server_info);
    return NULL;
}

/* MAP, MAP with a FILTER option and PEER reservations all come back, with
 * their options, when the server restarts. The restart shows in the epoch of
 * the first renewal's reply. */
static void run_reservation_scenario(void) {
    struct sockaddr_storage src[3], dst, filter_ip;
    pcp_flow_t *flow[3];
    pcp_ctx_t *ctx;
    pthread_t thread;
    struct timeval end, now;
    int i;
    static const char *const must_appear[] = {
        "MAP int port: \t\t 3001",
        "MAP int port: \t\t 3002",
        "OPTION: \t Filter",
        "PREFIX: 108",
        "FILTER PORT: 4444",
        "PEER Opcode specific information.",
        "Internal port: \t\t 3003",
        "PEER port: \t\t 8888",
        "MAP Ext IP: \t\t ::ffff:9.9.9.9",
        NULL};

    remove(LOG_BEFORE_RESTART);
    remove(LOG_AFTER_RESTART);
    TEST(pthread_create(&thread, NULL, reservation_server_main, NULL) == 0);
    sleep(1);

    ctx = pcp_init(0, NULL);
    TEST(ctx);
    pcp_set_restart_delay(ctx, 0);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);
    sock_pton("127.0.0.1:3001", (struct sockaddr *)&src[0]);
    sock_pton("127.0.0.1:3002", (struct sockaddr *)&src[1]);
    sock_pton("127.0.0.1:3003", (struct sockaddr *)&src[2]);
    sock_pton("127.0.0.1:8888", (struct sockaddr *)&dst);
    sock_pton("8.8.8.8:4444", (struct sockaddr *)&filter_ip);

    flow[0] = pcp_new_flow(ctx, (struct sockaddr *)&src[0], NULL, NULL,
                           IPPROTO_TCP, 10, NULL);
    flow[1] = pcp_new_flow(ctx, (struct sockaddr *)&src[1], NULL, NULL,
                           IPPROTO_UDP, 10, NULL);
    pcp_flow_set_filter_opt(flow[1], (struct sockaddr *)&filter_ip, 12);
    flow[2] =
        pcp_new_flow(ctx, (struct sockaddr *)&src[2], (struct sockaddr *)&dst,
                     NULL, IPPROTO_TCP, 10, NULL);
    for (i = 0; i < 3; i++) {
        TEST(flow[i] != NULL);
        TEST(pcp_wait(flow[i], 3000, 0) == pcp_state_succeeded);
    }

    /* renewals go to the restarted server, whose epoch makes the client
     * re-send every reservation */
    gettimeofday(&end, NULL);
    end.tv_sec += 12;
    do {
        struct timeval tout = {0, 50000};
        pcp_pulse(ctx, &tout);
        select(0, NULL, NULL, NULL, &tout);
        gettimeofday(&now, NULL);
    } while (timercmp(&now, &end, <));

    for (i = 0; i < 3; i++) {
        pcp_close_flow(flow[i]);
        pcp_delete_flow(flow[i]);
    }
    pcp_terminate(ctx, 0);
    pthread_join(thread, NULL);

    check_reservations_resent(must_appear);
    remove(LOG_BEFORE_RESTART);
    remove(LOG_AFTER_RESTART);
}

static atomic_int announce_server_done;

typedef struct announce_server_args {
    uint8_t requests_before_restart;
    /* 0: the announcements go to a port nobody listens on, so the client
     * misses all of them */
    int announce_to_client;
} announce_server_args_t;

static void *announce_server_main(void *arg) {
    const announce_server_args_t *args = (const announce_server_args_t *)arg;
    server_info_t server_info;
    pcp_test_server_t server;
    struct sockaddr_storage client;

    /* before the restart: up for an hour, answers the mappings, then goes
     * away */
    init_server_info(&server_info, args->requests_before_restart,
                     LOG_BEFORE_RESTART);
    inet_pton(AF_INET6, "::ffff:9.9.9.9", &server_info.ext_ip);
    server_info.epoch_time_start = time(NULL) - 3600;
    TEST(pcp_test_server_start(&server, "5351", "0.0.0.0", &server_info) == 0);
    while (pcp_test_server_is_running(&server)) {
        TEST(pcp_test_server_pulse(&server, -1) >= 0);
    }
    client = server.last_client;
    if (!args->announce_to_client) {
        ((struct sockaddr_in *)&client)->sin_port = htons(9);
    }

    /* after the restart: epoch from 0, five restart announcements to the
     * client it remembers (the last one 3.75 s after the first) */
    init_server_info(&server_info, 0, LOG_AFTER_RESTART);
    inet_pton(AF_INET6, "::ffff:9.9.9.9", &server_info.ext_ip);
    server_info.epoch_time_start = time(NULL);
    server_info.tv.tv_sec = 6;
    server_info.announce_count = 5;
    server_info.announce_to = client;
    execPCPServer("5351", "0.0.0.0", &server_info);
    atomic_store(&announce_server_done, 1);
    return NULL;
}

static int count_occurrences(const char *text, const char *needle) {
    int n = 0;

    while ((text = strstr(text, needle)) != NULL) {
        n++;
        text += strlen(needle);
    }
    return n;
}

/* the server log after the restart, empty if no request reached it (the
 * server writes its log at the first request it gets) */
static char *read_log_after_restart(void) {
    FILE *f = fopen(LOG_AFTER_RESTART, "r");
    char *log;

    if (f != NULL) {
        fclose(f);
        return read_whole_file(LOG_AFTER_RESTART);
    }
    log = calloc(1, 1);
    TEST(log != NULL);
    return log;
}

static void expect_sent_once(const char *log, const char *line) {
    if (count_occurrences(log, line) != 1) {
        printf("expected '%s' once after the restart:\n%s\n", line, log);
        TEST(0);
    }
}

/* A server that has been up for an hour restarts and announces itself (RFC
 * 6887, 14.1.3). The client has a mapping with FILTER and PREFER_FAILURE
 * options that it renews only after 1800 s, so it goes out again during the
 * test only because the client saw the restart: once, and the same as the
 * request that created it, options included. Seen either way, a restart is
 * an epoch lower than expected (8.5):
 *  - announced: the first announcement; the later ones carry the expected
 *    epoch and change nothing.
 *  - announcements missed: the reply to the renewal of a second mapping,
 *    renewed every 2 s. */
static void run_announce_scenario(int client_hears_announcements) {
    struct sockaddr_storage src, filter_ip;
    pcp_flow_t *flow, *short_flow = NULL;
    pcp_ctx_t *ctx;
    pthread_t thread;
    struct timeval end, now;
    announce_server_args_t args;
    char *after;

    remove(LOG_BEFORE_RESTART);
    remove(LOG_AFTER_RESTART);
    atomic_store(&announce_server_done, 0);
    args.requests_before_restart = client_hears_announcements ? 1 : 2;
    args.announce_to_client = client_hears_announcements;
    TEST(pthread_create(&thread, NULL, announce_server_main, &args) == 0);
    sleep(1);

    ctx = pcp_init(0, NULL);
    TEST(ctx);
    pcp_set_restart_delay(ctx, 0);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), 2) == 0);
    sock_pton("127.0.0.1:3004", (struct sockaddr *)&src);
    sock_pton("8.8.8.8:4444", (struct sockaddr *)&filter_ip);
    flow = pcp_new_flow(ctx, (struct sockaddr *)&src, NULL,
                        Sock_pton("0.0.0.0:3004"), IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    pcp_flow_set_filter_opt(flow, (struct sockaddr *)&filter_ip, 32);
    TEST(pcp_flow_set_prefer_failure_opt(flow) == PCP_ERR_SUCCESS);
    TEST(pcp_wait(flow, 3000, 0) == pcp_state_succeeded);
    if (!client_hears_announcements) {
        sock_pton("127.0.0.1:3005", (struct sockaddr *)&src);
        short_flow = pcp_new_flow(ctx, (struct sockaddr *)&src, NULL, NULL,
                                  IPPROTO_TCP, 4, NULL);
        TEST(short_flow != NULL);
        TEST(pcp_wait(short_flow, 3000, 0) == pcp_state_succeeded);
    }

    gettimeofday(&end, NULL);
    end.tv_sec += 10;
    do {
        struct timeval tout = {0, 50000};
        pcp_pulse(ctx, &tout);
        select(0, NULL, NULL, NULL, &tout);
        gettimeofday(&now, NULL);
    } while (!atomic_load(&announce_server_done) && timercmp(&now, &end, <));
    pthread_join(thread, NULL);

    pcp_close_flow(flow);
    pcp_delete_flow(flow);
    if (short_flow != NULL) {
        pcp_close_flow(short_flow);
        pcp_delete_flow(short_flow);
    }
    pcp_terminate(ctx, 0);

    after = read_log_after_restart();
    expect_sent_once(after, "MAP int port: \t\t 3004");
    free(after);
    {
        static const char *const must_appear[] = {
            "MAP int port: \t\t 3004", "OPTION: \t Filter",
            "FILTER PORT: 4444",       "FILTER IP: ::ffff:8.8.8.8",
            "OPTION: \t Prefer fail",  NULL};
        /* every request of the server before the restart, 3005 included
         * when it is there, comes back unchanged */
        check_reservations_resent(must_appear);
    }
    remove(LOG_BEFORE_RESTART);
    remove(LOG_AFTER_RESTART);
}
#endif /* !WIN32 */

int main(void) {

    PD_SOCKET_STARTUP();
    pcp_log_level = 5;

    run_scenario(2, 1);
    run_scenario(1, 0);
#ifndef WIN32
    run_reservation_scenario();
    run_announce_scenario(1);
    run_announce_scenario(0);
#endif

    PD_SOCKET_CLEANUP();
    return 0;
}
