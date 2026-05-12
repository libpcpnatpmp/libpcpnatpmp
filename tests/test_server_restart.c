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
#include <sys/socket.h>
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

typedef struct server_thread_args {
    uint8_t second_server_requests;
} server_thread_args_t;

static void init_server_info(server_info_t *server_info,
                             uint8_t end_after_recv) {
    memset(server_info, 0, sizeof(*server_info));
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

    init_server_info(&server_info, 1);
    server_info.epoch_time_start = time(NULL);
    execPCPServer("5351", "0.0.0.0", &server_info);

    init_server_info(&server_info, args->second_server_requests);
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

    return 0;
}

int main(void) {

    PD_SOCKET_STARTUP();
    pcp_log_level = 5;

    run_scenario(2, 1);
    run_scenario(1, 0);

    PD_SOCKET_CLEANUP();
    return 0;
}
