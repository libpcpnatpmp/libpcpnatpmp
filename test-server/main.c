#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include "pcp-server.h"

#include "getopt.h"
#include "pcp_utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>

#include "pcp_gettimeofday.h"
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>

#else // WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#endif // WIN32

#define PCP_PORT "5351"
#define PCP_TEST_MAX_VERSION 2

static void print_usage(void) {

    printf("\n");
    printf("Usage: \n");
    printf("-h, --help \t  Display this help \n");
    printf("-v \t\t  Set server version.\n");
    printf("   \t\t   Only versions 1 and 2 are supported for now.\n");
    printf("   \t\t   Default: By default server processes both versions.\n");
    printf("-p \t\t  Lets you set the port on which server listens.\n");
    printf("   \t\t   Default: 5351.\n");
    printf("-r \t\t  Set result code that will be returned by server \n"
           "   \t\t   to every request no matter what.\n");
    printf("   \t\t   Possible values from range {0..13} \n");
    printf("--ear #num \t  Terminates server after #num requests have been \n"
           "          \t   received and responded to. \n");
    printf("--ip \t\t  Sets IP address on which server is listening.\n");
    printf("     \t\t   Default: 0.0.0.0 (listening on all interfaces)\n");
    printf("--timeout \t  Set timeout time of the server in miliseconds.\n");
    printf("     \t\t   Default 0 (Running indefinitely)\n");
    printf("--app-bit \t  set application bit in SADSCP opcode response\n");
    printf("--ret-dscp\t  return DSCP value for SADSCP opcode\n");
    printf("--log-file \t  Log Requests to file \n");
}

#ifndef no_argument
#define no_argument 0
#endif

#ifndef required_argument
#define required_argument 1
#endif

#define PCP_PORT "5351"
#define PCP_TEST_MAX_VERSION 2

int main(int argc, char *argv[]) {

    const char *port = PCP_PORT;
    int test_port;
    uint8_t pcp_version = PCP_TEST_MAX_VERSION;
    uint8_t end_after_recv = 0;
    uint8_t default_result_code = 255;
#ifdef WIN32
    long timeout_us = 0;
#else
    suseconds_t timeout_us = 0;
#endif
    server_info_t server_info_storage;
    const char *server_ip = "0.0.0.0";
    uint8_t app_bit = 0;
    uint8_t ret_dscp = 0;
    char *log_file = NULL;

    {
        int c;
        int option_index = 0;

        static struct option long_options[] = {
            {"ear", required_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"ip", required_argument, 0, 0},
            {"ext-ip", required_argument, 0, 0},
            {"timeout", required_argument, 0, 0},
            {"app-bit", no_argument, 0, 0},
            {"ret-dscp", required_argument, 0, 0},
            {"log-file", required_argument, 0, 0},
            {0, 0, 0, 0}};

        opterr = 0;
        while ((c = getopt_long(argc, argv, "hr:v:p:", long_options,
                                &option_index)) != -1) {
            switch (c) {
            // assign values for long options
            case 0:
                if (!long_options[option_index].name) {
                    break;
                }

                if (!strcmp(long_options[option_index].name, "ear"))
                    end_after_recv = (uint8_t)atoi(optarg);

                if (!strcmp(long_options[option_index].name, "timeout")) {
                    int temp_timeout = atoi(optarg);
                    if (temp_timeout < 0) {
                        printf("Value provided for timeout was negative %d. "
                               "Please provide correct value.\n",
                               temp_timeout);
                        exit(1);
                    } else {
#ifdef WIN32
                        timeout_us = (long)atoi(optarg);
#else
                        timeout_us = (suseconds_t)atoi(optarg);
#endif
                    }
                }

                if (!strcmp(long_options[option_index].name, "app-bit")) {
                    app_bit = 1;
                }

                if (!strcmp(long_options[option_index].name, "ret-dscp")) {
                    ret_dscp = (uint8_t)atoi(optarg);
                }

                if (!strcmp(long_options[option_index].name, "log-file")) {
                    log_file = optarg;
                }

                if (!strcmp(long_options[option_index].name, "ip"))
                    server_ip = optarg;

                if (!strcmp(long_options[option_index].name, "ext-ip")) {
                    inet_pton(AF_INET6, optarg, &server_info_storage.ext_ip);
                }

                if (!strcmp(long_options[option_index].name, "help")) {
                    print_usage();
                    exit(1);
                }

                break;
            case 'h':
                print_usage();
                exit(1);
                break;
            case 'p':
                test_port = atoi(optarg);
                if ((test_port < 1) || (test_port > 65535)) {
                    printf("Bad value for option -p %d \n", test_port);
                    printf("Port value can be in range 1-65535. \n");
                    printf("Default value will be used. \n");
                } else {
                    port = optarg;
                }
                break;
            case 'r':
                default_result_code = (uint8_t)atoi(optarg);
                if (default_result_code > 13 && default_result_code != 255) {
                    printf("Unsupported  RESULT CODE %d (acceptable values "
                           "0 <= result_code <= 13 or result_code == 255)\n",
                           default_result_code);
                    exit(1);
                }
                break;
            case 'v':
                pcp_version = (uint8_t)atoi(optarg);
                if (pcp_version < 1 || pcp_version > 2) {
                    printf("Version %d is not supported! \n", pcp_version);
                    exit(1);
                }
                break;
            case '?':
                if (isprint(optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n",
                            optopt);
                print_usage();
                exit(1);
            default: // LCOV_EXCL_START
                print_usage();
                exit(1);
                break; // LCOV_EXCL_STOP
            }
        }
    }

    server_info_storage.default_result_code = default_result_code;
    server_info_storage.end_after_recv = end_after_recv;
    server_info_storage.server_version = pcp_version;
    server_info_storage.tv.tv_sec = timeout_us / 1000;
    server_info_storage.tv.tv_usec = (timeout_us % 1000) * 1000;
    server_info_storage.epoch_time_start = time(NULL);
    server_info_storage.app_bit = app_bit;
    server_info_storage.ret_dscp = ret_dscp;
    if (log_file != NULL) {
        memcpy(&(server_info_storage.log_file[0]), log_file,
               min(strlen(log_file) + 1, MAX_LOG_FILE));
    } else {
        server_info_storage.log_file[0] = 0;
    }

    printf("Server listening on %s:%s \n", server_ip, port);
    return execPCPServer(port, server_ip, &server_info_storage);
}
