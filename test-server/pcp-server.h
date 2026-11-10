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
    // answer with `lifetime` instead of the one asked
    uint8_t override_lifetime;
    uint32_t lifetime;
    char log_file[MAX_LOG_FILE];
    struct timeval tv;
    time_t epoch_time_start;
    // Restart announcements (RFC 6887, 14.1.3): unsolicited ANNOUNCE
    // responses sent after the server starts, up to PCP_TEST_MAX_ANNOUNCE.
    // The first goes at once, the second 250 ms later, and each interval
    // doubles. They go to announce_to when its family is set, as from a
    // server that remembers its clients; otherwise to 224.0.0.1:5350 and, on
    // an IPv6 socket, [ff02::1]:5350.
    uint8_t announce_count;
    struct sockaddr_storage announce_to;
    // a request with more FILTER options than this is answered
    // EXCESSIVE_REMOTE_PEERS (0: no limit); each request is counted alone
    uint8_t max_filters;
} server_info_t;

#define PCP_TEST_MAX_ANNOUNCE 10

typedef struct pcp_test_server {
    server_info_t server_info;
    PCP_SOCKET sockfd;
    struct timeval timeout_time;
    int remaining_messages;
    // restart announcements still to send, and when the next one is due
    int announces_left;
    long announce_interval_ms;
    struct timeval next_announce;
    // the address of the last request, for a test to announce to it after
    // the server restarts
    struct sockaddr_storage last_client;
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
