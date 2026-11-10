/*
 *------------------------------------------------------------------
 * test_natpmp_retransmit.c
 *
 * November 9, 2026
 *
 *------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>

#include <windows.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include "pcpnatpmp.h"

#include "pcp_client_db.h"
#include "pcp_socket.h"
#include "pcp_utils.h"
#include "test_macro.h"
#include "unp.h"

// A NAT-PMP gateway that never answers, so the client retransmits.
#define MAX_REQUESTS 16
// RFC 6886 section 3.1: 250 ms, then doubling, for nine attempts
#define FIRST_WAIT_MS 250
#define CHECKED_WAITS 4
#define ATTEMPTS 9
// without a time limit the client resends past ATTEMPTS, its wait doubling
// up to the 64 s RFC 6886 waits before its last attempt
#define UNLIMITED_ATTEMPTS (3 * ATTEMPTS)
#define LAST_WAIT_MS (FIRST_WAIT_MS << (ATTEMPTS - 1))
// a time limit that cuts the attempts short: requests at 0, 250 and 750 ms
#define SHORT_LIMIT_MS 1000
#define ATTEMPTS_IN_SHORT_LIMIT 3
// PCP (RFC 6887 section 8.1.1) has no attempt limit: more than its old 3
#define PCP_ATTEMPTS_CHECKED 12
// PCP's requests at 0 and about 3 s, then the limit
#define PCP_LIMIT_MS 4000
#define PCP_ATTEMPTS_IN_LIMIT 2
// each wait is at least the expected one, less timer granularity, and at
// most twice it plus 500 ms, so a loaded machine still passes; PCP's first
// wait, about 3 s, is far outside the first NAT-PMP ones
#define EARLY_MS 20
#define LATE_MS(expected) (2 * (expected) + 500)
// the default time limit, PCP_RESPONSE_TIMEOUT_MS
#define DEFAULT_LIMIT_S 10

// the port open_server() got, chosen by the system so tests running at the
// same time do not collide
static int server_port;

static PCP_SOCKET open_server(void) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    PCP_SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    TEST(sock != PCP_INVALID_SOCKET);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    TEST(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    TEST(getsockname(sock, (struct sockaddr *)&addr, &len) == 0);
    server_port = ntohs(addr.sin_port);
    return sock;
}

static long ms_since(const struct timeval *start) {
    struct timeval now;

    gettimeofday(&now, NULL);
    return (long)(now.tv_sec - start->tv_sec) * 1000 +
           (long)(now.tv_usec - start->tv_usec) / 1000;
}

// Runs a flow to the silent gateway at `server_addr`, a server of `version`
// with time limit `limit_ms`, until it fails or 3 times the limit passes.
// Returns when it failed, in ms, or -1; `*requests` gets the requests sent.
static long run_until_failed(const char *server_addr, uint8_t version,
                             uint32_t limit_ms, PCP_SOCKET server,
                             int *requests) {
    uint8_t buf[1100];
    struct timeval start;
    long failed_at = -1;
    pcp_ctx_t *ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_flow_t *flow;

    pcp_set_response_timeout(ctx, limit_ms);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), version) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    *requests = 0;
    gettimeofday(&start, NULL);
    while (ms_since(&start) < 3 * (long)limit_ms && failed_at < 0) {
        struct timeval tout = {0, 5000};
        fd_set fds;
        size_t count = 0;
        pcp_flow_info_t *info;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            int got =
                (int)recvfrom(server, (char *)buf, sizeof(buf), 0, NULL, NULL);
            if (got >= 2 && buf[0] == version) {
                (*requests)++;
            }
        }
        info = pcp_flow_get_info(flow, &count);
        TEST(info != NULL && count == 1);
        if (info[0].result == pcp_state_failed) {
            failed_at = ms_since(&start);
        }
        free(info);
    }
    pcp_terminate(ctx, 0);
    return failed_at;
}

// Moves the flow's and the server's timeouts to now, pulses once, and
// returns how many NAT-PMP requests reached the gateway.
static int pulse_now(pcp_ctx_t *ctx, pcp_flow_t *flow, PCP_SOCKET server) {
    struct timeval tout = {0, 5000};
    uint8_t buf[1100];
    fd_set fds;
    pcp_server_t *s = get_pcp_server(ctx, 0);
    int requests = 0;

    gettimeofday(&flow->timeout, NULL);
    if (s) {
        s->next_timeout = flow->timeout;
    }
    pcp_pulse(ctx, NULL);
    FD_ZERO(&fds);
    FD_SET(server, &fds);
    while (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
        int got =
            (int)recvfrom(server, (char *)buf, sizeof(buf), 0, NULL, NULL);
        if (got >= 2 && buf[0] == 0) {
            requests++;
        }
        tout.tv_usec = 5000;
    }
    return requests;
}

static pcp_fstate_e flow_state(pcp_flow_t *flow) {
    size_t count = 0;
    pcp_flow_info_t *info = pcp_flow_get_info(flow, &count);
    pcp_fstate_e state;

    TEST(info != NULL && count == 1);
    state = info[0].result;
    free(info);
    return state;
}

int main(void) {
    char server_addr[32];
    uint8_t buf[1100];
    long arrived[MAX_REQUESTS];
    int requests = 0, i;
    struct timeval start;
    PCP_SOCKET server;
    pcp_ctx_t *ctx;
    pcp_flow_t *flow;

    pcp_log_level = PCP_LOGLVL_NONE;
    PD_SOCKET_STARTUP();
    server = open_server();
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    snprintf(server_addr, sizeof(server_addr), "127.0.0.1:%d", server_port);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 0) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);

    // long enough for the first five attempts: 250 + 500 + 1000 + 2000 ms
    gettimeofday(&start, NULL);
    while (ms_since(&start) < 4500 && requests < MAX_REQUESTS) {
        struct timeval tout = {0, 5000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            int got =
                (int)recvfrom(server, (char *)buf, sizeof(buf), 0, NULL, NULL);
            // NAT-PMP requests only: version 0
            if (got >= 2 && buf[0] == 0) {
                arrived[requests++] = ms_since(&start);
            }
        }
    }

    for (i = 0; i < requests; i++) {
        printf("NAT-PMP request %d at %ld ms\n", i + 1, arrived[i]);
    }
    TEST(requests >= CHECKED_WAITS + 1);
    for (i = 0; i < CHECKED_WAITS; i++) {
        long wait = arrived[i + 1] - arrived[i];
        long expected = (long)FIRST_WAIT_MS << i;

        if (wait < expected - EARLY_MS || wait > LATE_MS(expected)) {
            printf("wait %d: %ld ms, expected %ld ms\n", i + 1, wait, expected);
            TEST(0);
        }
    }

    pcp_terminate(ctx, 0);

    // A PCP server keeps PCP's schedule: the first retry after PCP_RETX_IRT
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    requests = 0;
    gettimeofday(&start, NULL);
    while (ms_since(&start) < PCP_RETX_IRT + 500 && requests < 2) {
        struct timeval tout = {0, 5000};
        fd_set fds;

        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            int got =
                (int)recvfrom(server, (char *)buf, sizeof(buf), 0, NULL, NULL);
            if (got >= 2 && buf[0] == 2) {
                arrived[requests++] = ms_since(&start);
            }
        }
    }
    TEST(requests == 2);
    printf("PCP retry after %ld ms\n", arrived[1] - arrived[0]);
    TEST(arrived[1] - arrived[0] >= PCP_RETX_IRT - EARLY_MS);
    pcp_terminate(ctx, 0);

    // Without a time limit, the client keeps resending past the ninth
    // attempt and the flow does not fail. Waiting the attempts out takes
    // many minutes, so each wait is cut short: the flow's and the server's
    // timeouts are moved to now before every pulse.
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_set_response_timeout(ctx, 0);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 0) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    // drop what the earlier parts' clients left at the gateway
    for (;;) {
        struct timeval none = {0, 0};
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(server, &fds);
        if (select((int)server + 1, &fds, NULL, NULL, &none) <= 0) {
            break;
        }
        recvfrom(server, (char *)buf, sizeof(buf), 0, NULL, NULL);
    }
    requests = 0;
    for (i = 0; i < UNLIMITED_ATTEMPTS; i++) {
        requests += pulse_now(ctx, flow, server);
    }
    printf("%d NAT-PMP requests without a time limit\n", requests);
    TEST(requests > ATTEMPTS);
    {
        size_t count = 0;
        pcp_flow_info_t *info = pcp_flow_get_info(flow, &count);
        struct timeval now;
        long wait_ms;

        TEST(info != NULL && count == 1);
        TEST(info[0].result != pcp_state_failed);
        free(info);
        // the last request's wait stopped doubling at LAST_WAIT_MS
        gettimeofday(&now, NULL);
        wait_ms = (long)(flow->timeout.tv_sec - now.tv_sec) * 1000 +
                  (long)(flow->timeout.tv_usec - now.tv_usec) / 1000;
        printf("NAT-PMP waits %ld ms after its last request\n", wait_ms);
        TEST(wait_ms > LAST_WAIT_MS - 1000 && wait_ms <= LAST_WAIT_MS);
    }

    pcp_terminate(ctx, 0);

    // With a time limit, the client gives up when it passes, here before the
    // fourth attempt.
    {
        long failed_at =
            run_until_failed(server_addr, 0, SHORT_LIMIT_MS, server, &requests);

        printf("%d NAT-PMP requests, failed after %ld ms\n", requests,
               failed_at);
        TEST(requests == ATTEMPTS_IN_SHORT_LIMIT);
        TEST(failed_at >= SHORT_LIMIT_MS - EARLY_MS);
        TEST(failed_at <= LATE_MS(SHORT_LIMIT_MS));
    }

    // The default limit gives up 10 s after the first request. Instead of
    // waiting, the first request is moved back in time: 9 s back the next
    // attempt is sent, waited for no later than the limit; 10 s back the
    // flow is given up on.
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 0) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    for (i = 0; i < 100 && (flow->first_sent.tv_sec == 0); i++) {
        struct timeval tout = {0, 5000};

        pcp_pulse(ctx, NULL);
        select(0, NULL, NULL, NULL, &tout);
    }
    TEST(flow->first_sent.tv_sec != 0);
    for (i = 0; i < 2; i++) {
        pcp_server_t *s = get_pcp_server(ctx, 0);
        size_t count = 0;
        pcp_flow_info_t *info;

        flow->first_sent.tv_sec -= i == 0 ? DEFAULT_LIMIT_S - 1 : 1;
        gettimeofday(&flow->timeout, NULL);
        if (s) {
            s->next_timeout = flow->timeout;
        }
        pcp_pulse(ctx, NULL);
        info = pcp_flow_get_info(flow, &count);
        TEST(info != NULL && count == 1);
        if (i == 0) {
            TEST(info[0].result != pcp_state_failed);
            TEST(flow->timeout.tv_sec <=
                 flow->first_sent.tv_sec + DEFAULT_LIMIT_S + 1);
        } else {
            TEST(info[0].result == pcp_state_failed);
        }
        free(info);
    }
    pcp_terminate(ctx, 0);

    // PCP without a time limit keeps retrying: each wait is cut short as
    // above, and the flow has not failed after PCP_ATTEMPTS_CHECKED requests.
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_set_response_timeout(ctx, 0);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 2) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    requests = 0;
    for (i = 0; i < 4 * PCP_ATTEMPTS_CHECKED; i++) {
        struct timeval tout = {0, 5000};
        fd_set fds;
        pcp_server_t *s = get_pcp_server(ctx, 0);

        gettimeofday(&flow->timeout, NULL);
        if (s) {
            s->next_timeout = flow->timeout;
        }
        pcp_pulse(ctx, NULL);
        FD_ZERO(&fds);
        FD_SET(server, &fds);
        while (select((int)server + 1, &fds, NULL, NULL, &tout) > 0) {
            int got =
                (int)recvfrom(server, (char *)buf, sizeof(buf), 0, NULL, NULL);
            if (got >= 2 && buf[0] == 2) {
                requests++;
            }
            tout.tv_usec = 5000;
        }
    }
    printf("%d PCP requests without a time limit\n", requests);
    TEST(requests >= PCP_ATTEMPTS_CHECKED);
    {
        size_t count = 0;
        pcp_flow_info_t *info = pcp_flow_get_info(flow, &count);

        TEST(info != NULL && count == 1);
        TEST(info[0].result != pcp_state_failed);
        free(info);
    }
    pcp_terminate(ctx, 0);

    // PCP with a time limit gives up when it passes
    {
        long failed_at =
            run_until_failed(server_addr, 2, PCP_LIMIT_MS, server, &requests);

        printf("%d PCP requests, failed after %ld ms\n", requests, failed_at);
        TEST(requests == PCP_ATTEMPTS_IN_LIMIT);
        TEST(failed_at >= PCP_LIMIT_MS - EARLY_MS);
        TEST(failed_at <= LATE_MS(PCP_LIMIT_MS));
    }

    // A new request starts a new count: after five unanswered attempts, a
    // changed lifetime sends the flow again, and that request gets all
    // ATTEMPTS. The time limit is far off, so only the count can end it.
    ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    pcp_set_response_timeout(ctx, 3600000);
    TEST(pcp_add_server(ctx, Sock_pton(server_addr), 0) == 0);
    flow = pcp_new_flow(ctx, Sock_pton("127.0.0.1:1234"), NULL, NULL,
                        IPPROTO_TCP, 3600, NULL);
    TEST(flow != NULL);
    requests = 0;
    while (requests < 5) {
        requests += pulse_now(ctx, flow, server);
    }
    pcp_flow_set_lifetime(flow, 3600);
    requests = 0;
    for (i = 0; i < 2 * ATTEMPTS && flow_state(flow) != pcp_state_failed; i++) {
        requests += pulse_now(ctx, flow, server);
    }
    printf("%d NAT-PMP requests after the flow was sent again\n", requests);
    TEST(flow_state(flow) == pcp_state_failed);
    TEST(requests == ATTEMPTS);
    pcp_terminate(ctx, 0);

    CLOSE(server);
    PD_SOCKET_CLEANUP();
    return 0;
}
