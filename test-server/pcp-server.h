#ifndef PCP_TEST_SERVER_H_
#define PCP_TEST_SERVER_H_

#include <stdint.h>
#include <time.h>

#include "pcp_socket.h"

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

#include "pcp_gettimeofday.h"
#else
#include <netinet/in.h>
#include <sys/time.h>
#endif

#define MAX_LOG_FILE 64u

typedef struct server_info {
    uint8_t server_version;
    uint8_t end_after_recv;
    uint8_t default_result_code;
    struct in6_addr ext_ip;
    uint8_t app_bit;
    uint8_t ret_dscp;
    char log_file[MAX_LOG_FILE];
    struct timeval tv;
    time_t epoch_time_start;
} server_info_t;

typedef struct pcp_test_server {
    server_info_t server_info;
    PCP_SOCKET sockfd;
    struct timeval timeout_time;
    int remaining_messages;
    int running;
} pcp_test_server_t;

int execPCPServer(const char *serverPort, const char *serverAddress,
                  const server_info_t *server_info);
int pcp_test_server_start(pcp_test_server_t *server, const char *serverPort,
                          const char *serverAddress,
                          const server_info_t *server_info);
int pcp_test_server_pulse(pcp_test_server_t *server, int timeout_ms);
void pcp_test_server_stop(pcp_test_server_t *server);
int pcp_test_server_is_running(const pcp_test_server_t *server);

#endif /* PCP_TEST_SERVER_H_ */
