#ifndef TEST_PCP_SERVER_HELPER_H_
#define TEST_PCP_SERVER_HELPER_H_

#include "pcp-server.h"
#include "pcpnatpmp.h"

typedef struct test_pcp_server_config {
    const char *server_port;
    const char *server_address;
    int start_delay_ms;
    server_info_t server_info;
} test_pcp_server_config_t;

typedef struct test_pcp_server_sequence {
    const test_pcp_server_config_t *configs;
    size_t config_count;
    size_t next_config;
    pcp_test_server_t current_server;
    struct timeval next_start_time;
    int active;
} test_pcp_server_sequence_t;

void test_pcp_server_info_init(server_info_t *server_info);
void test_pcp_server_config_init(test_pcp_server_config_t *config);
void test_pcp_server_sequence_init(test_pcp_server_sequence_t *sequence,
                                   const test_pcp_server_config_t *configs,
                                   size_t config_count);
int test_pcp_server_sequence_pulse(test_pcp_server_sequence_t *sequence,
                                   int timeout_ms);
void test_pcp_server_sequence_stop(test_pcp_server_sequence_t *sequence);
int test_pcp_server_sequence_is_active(
    const test_pcp_server_sequence_t *sequence);
pcp_fstate_e test_pcp_wait_with_servers(pcp_flow_t *flow, int timeout_ms,
                                        test_pcp_server_sequence_t *sequence);
void test_sleep_ms(int timeout_ms);

#endif /* TEST_PCP_SERVER_HELPER_H_ */
