/*
 *------------------------------------------------------------------
 * test_lifetime_renewal.c
 *
 * March 15th, 2013, mbagljas
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
#ifndef WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include "pcpnatpmp.h"

#include "pcp_socket.h"
#include "pcp_utils.h"
#include "unp.h"
#include <time.h>

// terminal colors
#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"

static pcp_fstate_e test_wait(pcp_flow_t *flow, int timeout) {
    fd_set read_fds;
    int fdmax;
    struct timeval tout_end;
    struct timeval tout_select;
    int nflow_exit_states = pcp_eval_flow_state(flow, NULL);
    pcp_ctx_t *ctx = flow->ctx;

    gettimeofday(&tout_end, NULL);
    tout_end.tv_usec += (timeout * 1000) % 1000000;
    tout_end.tv_sec += tout_end.tv_usec / 1000000;
    tout_end.tv_usec = tout_end.tv_usec % 1000000;
    tout_end.tv_sec += timeout / 1000;

    fdmax = 0;

    FD_ZERO(&read_fds);

    // main loop
    for (;;) {
        pcp_fstate_e ret_state;

        // check expiration of wait timeout
        struct timeval ctv;
        gettimeofday(&ctv, NULL);
        if ((timeval_subtract(&tout_select, &tout_end, &ctv)) ||
            ((tout_select.tv_sec == 0) && (tout_select.tv_usec == 0)) ||
            (tout_select.tv_sec < 0)) {
            return pcp_state_processing;
        }

        // process all events and get timeout value for next select
        pcp_pulse(ctx, &tout_select);

        // check flow for reaching one of exit from wait states
        // (also handles case when flow is MAP for 0.0.0.0)
        if (pcp_eval_flow_state(flow, &ret_state) > nflow_exit_states) {
            return ret_state;
        }

        FD_ZERO(&read_fds);
        fdmax = pcp_get_socket(ctx);
        FD_SET(fdmax, &read_fds);
        fdmax++;

        select(fdmax, &read_fds, NULL, NULL, &tout_select);
    }

    return pcp_state_succeeded;
}

int main(int argc, char *argv[] UNUSED) {

    struct sockaddr_in destination_ip4;
    struct sockaddr_in source_ip4;
    struct sockaddr_in ext_ip4;
    time_t finish_time;
    time_t cur_time;
    pcp_flow_t *flow = NULL;

    const char *dest_ip = "0.0.0.0";
    const char *src_ip = "127.0.0.1";
    const char *ext_ip = "10.20.30.40";
    uint16_t dest_port = 1234;
    uint16_t src_port = 1235;
    uint8_t protocol = 6;
    uint32_t lifetime = 10;
    pcp_ctx_t *ctx;

    //    pcp_log_level = PCP_LOGLVL_DEBUG;

    PD_SOCKET_STARTUP();

    destination_ip4.sin_family = AF_INET;
    destination_ip4.sin_port = htons(dest_port);
    inet_pton(AF_INET, dest_ip, &destination_ip4.sin_addr.s_addr);

    source_ip4.sin_family = AF_INET;
    source_ip4.sin_port = htons(src_port);
    inet_pton(AF_INET, src_ip, &source_ip4.sin_addr.s_addr);

    ext_ip4.sin_family = AF_INET;
    ext_ip4.sin_port = htons(1235);
    inet_pton(AF_INET, ext_ip, &ext_ip4.sin_addr.s_addr);

    printf("####################################\n");
    printf("####   Lifetime renewal test    ####\n");
    printf("####################################\n");
    printf(">>> Test parameters  \n");
    printf(">>> dest_ip = 0.0.0.0 \n");
    printf(">>> src_ip = 127.0.0.1 \n");
    printf(">>> ext_ip = 10.20.30.40 \n");
    printf(">>> dest_port = 1234 \n");
    printf(">>> src_port = 1235 \n");
    printf(">>> protocol = 6 \n");
    printf(">>> lifetime = 10 \n");

    pcp_log_level = argc > 1 ? PCP_LOGLVL_DEBUG : PCP_LOGLVL_INFO;

    ctx = pcp_init(0, NULL);
    pcp_add_server(ctx, Sock_pton("127.0.0.1"), 2);

    flow = pcp_new_flow(ctx, (struct sockaddr *)&source_ip4,
                        (struct sockaddr *)&destination_ip4,
                        (struct sockaddr *)&ext_ip4, protocol, lifetime, NULL);

    finish_time = time(NULL) + 21;

    while ((cur_time = time(NULL)) < finish_time) {

        switch (test_wait(flow, (int)(finish_time - cur_time + 1) * 1000)) {
        case pcp_state_processing:
            printf("\nFlow signaling timed out.\n" KNRM);
            break;
        case pcp_state_succeeded:
            printf("\nFlow signaling succeeded.\n");
            break;
        case pcp_state_short_lifetime_error: // LCOV_EXCL_START
            printf("\nFlow signaling failed. Short lifetime error received.\n");
            break;
        case pcp_state_failed:
            printf("\nFlow signaling failed.\n");
            break;
        default:
            break;
        } // LCOV_EXCL_STOP
    }

    pcp_close_flow(flow);
    pcp_delete_flow(flow);
    flow = NULL;
    pcp_terminate(ctx, 1);

    PD_SOCKET_CLEANUP();

    return 0;
}
