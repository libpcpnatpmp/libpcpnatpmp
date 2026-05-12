#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include "test_pcp_server_helper.h"

#include <string.h>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "pcp_socket.h"
#include "pcp_utils.h"

static int start_next_server(test_pcp_server_sequence_t *sequence) {
    const test_pcp_server_config_t *config;

    if (sequence->next_config >= sequence->config_count) {
        sequence->active = 0;
        return 0;
    }

    config = &sequence->configs[sequence->next_config++];
    if (pcp_test_server_start(&sequence->current_server, config->server_port,
                              config->server_address,
                              &config->server_info) != 0) {
        sequence->active = 0;
        return -1;
    }

    memset(&sequence->next_start_time, 0, sizeof(sequence->next_start_time));
    sequence->active = 1;
    return 0;
}

void test_pcp_server_info_init(server_info_t *server_info) {
    memset(server_info, 0, sizeof(*server_info));
    server_info->server_version = 2;
    server_info->default_result_code = 255;
    server_info->epoch_time_start = time(NULL);
}

void test_pcp_server_config_init(test_pcp_server_config_t *config) {
    memset(config, 0, sizeof(*config));
    test_pcp_server_info_init(&config->server_info);
}

void test_pcp_server_sequence_init(
    test_pcp_server_sequence_t *sequence,
    const test_pcp_server_config_t *configs, size_t config_count) {
    memset(sequence, 0, sizeof(*sequence));
    sequence->configs = configs;
    sequence->config_count = config_count;
    sequence->active = config_count > 0;
    if (sequence->active) {
        start_next_server(sequence);
    }
}

int test_pcp_server_sequence_pulse(test_pcp_server_sequence_t *sequence,
                                   int timeout_ms) {
    int ret;
    struct timeval now;

    if (!sequence->active) {
        return 0;
    }

    if (!pcp_test_server_is_running(&sequence->current_server)) {
        if (sequence->next_config >= sequence->config_count) {
            sequence->active = 0;
            return 0;
        }

        gettimeofday(&now, NULL);
        if (timeval_comp(&now, &sequence->next_start_time) < 0) {
            return 0;
        }

        return start_next_server(sequence);
    }

    ret = pcp_test_server_pulse(&sequence->current_server, timeout_ms);
    if (ret < 0) {
        sequence->active = 0;
        return ret;
    }

    if (!pcp_test_server_is_running(&sequence->current_server) &&
        sequence->next_config < sequence->config_count) {
        const test_pcp_server_config_t *config =
            &sequence->configs[sequence->next_config];

        gettimeofday(&now, NULL);
        sequence->next_start_time.tv_sec =
            now.tv_sec + (config->start_delay_ms / 1000);
        sequence->next_start_time.tv_usec =
            now.tv_usec + ((config->start_delay_ms % 1000) * 1000);
        sequence->next_start_time.tv_sec +=
            sequence->next_start_time.tv_usec / 1000000;
        sequence->next_start_time.tv_usec %= 1000000;
    }

    return ret;
}

void test_pcp_server_sequence_stop(test_pcp_server_sequence_t *sequence) {
    if (pcp_test_server_is_running(&sequence->current_server)) {
        pcp_test_server_stop(&sequence->current_server);
    }
    sequence->active = 0;
}

int test_pcp_server_sequence_is_active(
    const test_pcp_server_sequence_t *sequence) {
    return sequence->active;
}

pcp_fstate_e test_pcp_wait_with_servers(
    pcp_flow_t *flow, int timeout_ms, test_pcp_server_sequence_t *sequence) {
    fd_set read_fds;
    int fdmax;
    struct timeval tout_end;
    struct timeval tout_select;
    int nflow_exit_states = pcp_eval_flow_state(flow, NULL);
    pcp_ctx_t *ctx = flow->ctx;

    gettimeofday(&tout_end, NULL);
    tout_end.tv_usec += (timeout_ms * 1000) % 1000000;
    tout_end.tv_sec += tout_end.tv_usec / 1000000;
    tout_end.tv_usec %= 1000000;
    tout_end.tv_sec += timeout_ms / 1000;

    for (;;) {
        pcp_fstate_e ret_state;
        struct timeval ctv;
        struct timeval server_slice;

        gettimeofday(&ctv, NULL);
        if ((timeval_subtract(&tout_select, &tout_end, &ctv)) ||
            ((tout_select.tv_sec == 0) && (tout_select.tv_usec == 0)) ||
            (tout_select.tv_sec < 0)) {
            return pcp_state_processing;
        }

        if (sequence && test_pcp_server_sequence_pulse(sequence, 0) < 0) {
            return pcp_state_failed;
        }
        pcp_pulse(ctx, &tout_select);
        if (sequence && test_pcp_server_sequence_pulse(sequence, 0) < 0) {
            return pcp_state_failed;
        }

        if (pcp_eval_flow_state(flow, &ret_state) > nflow_exit_states) {
            return ret_state;
        }

        if (sequence && test_pcp_server_sequence_is_active(sequence)) {
            server_slice.tv_sec = 0;
            server_slice.tv_usec = 100000;
            if (timeval_comp(&server_slice, &tout_select) < 0) {
                tout_select = server_slice;
            }
        }

        FD_ZERO(&read_fds);
        fdmax = pcp_get_socket(ctx);
        FD_SET(fdmax, &read_fds);
        fdmax++;

        select(fdmax, &read_fds, NULL, NULL, &tout_select);
    }
}

void test_sleep_ms(int timeout_ms) {
#ifdef WIN32
    Sleep((DWORD)timeout_ms);
#else
    usleep((useconds_t)timeout_ms * 1000);
#endif
}
