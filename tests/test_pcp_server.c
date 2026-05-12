/*
 *------------------------------------------------------------------
 * test_pcp_server.c
 *
 * May 12, 2026, Codex
 *
 *------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "pcpnatpmp.h"
#include "test_macro.h"
#include "test_pcp_server_helper.h"
#include "unp.h"

#ifdef WIN32
#define DUP _dup
#define DUP2 _dup2
#define FD_CLOSE _close
#define FILENO _fileno
#else
#define DUP dup
#define DUP2 dup2
#define FD_CLOSE close
#define FILENO fileno
#endif

static jmp_buf server_exit_env;
static int server_exit_code;

static void server_test_exit(int code) {
    server_exit_code = code;
    longjmp(server_exit_env, 1);
}

#define exit server_test_exit
#define main pcp_server_main
#include "../test-server/main.c"
#undef main
#undef exit

typedef struct server_run_result {
    int exit_code;
    char *stdout_data;
    char *stderr_data;
} server_run_result_t;

static char *read_stream(FILE *stream) {
    long size;
    char *buffer;

    TEST(fseek(stream, 0, SEEK_END) == 0);
    size = ftell(stream);
    TEST(size >= 0);
    TEST(fseek(stream, 0, SEEK_SET) == 0);

    buffer = (char *)malloc((size_t)size + 1);
    TEST(buffer != NULL);
    if (size > 0) {
        TEST(fread(buffer, 1, (size_t)size, stream) == (size_t)size);
    }
    buffer[size] = '\0';
    return buffer;
}

static void free_server_run_result(server_run_result_t *result) {
    free(result->stdout_data);
    free(result->stderr_data);
    result->stdout_data = NULL;
    result->stderr_data = NULL;
}

static void reset_getopt_state(void) {
#if defined(__GLIBC__)
    optind = 0;
#else
    optind = 1;
#endif
    opterr = 1;
    optopt = 0;
    optarg = NULL;
}

static server_run_result_t run_server_main(int argc, char **argv) {
    server_run_result_t result;
    FILE *stdout_capture;
    FILE *stderr_capture;
    int stdout_fd;
    int stderr_fd;
    int saved_stdout;
    int saved_stderr;

    memset(&result, 0, sizeof(result));
    stdout_capture = tmpfile();
    stderr_capture = tmpfile();
    TEST(stdout_capture != NULL);
    TEST(stderr_capture != NULL);

    stdout_fd = FILENO(stdout);
    stderr_fd = FILENO(stderr);
    saved_stdout = DUP(stdout_fd);
    saved_stderr = DUP(stderr_fd);
    TEST(saved_stdout >= 0);
    TEST(saved_stderr >= 0);

    fflush(stdout);
    fflush(stderr);
    TEST(DUP2(FILENO(stdout_capture), stdout_fd) >= 0);
    TEST(DUP2(FILENO(stderr_capture), stderr_fd) >= 0);

    reset_getopt_state();
    server_exit_code = -1;
    if (setjmp(server_exit_env) == 0) {
        result.exit_code = pcp_server_main(argc, argv);
    } else {
        result.exit_code = server_exit_code;
    }

    fflush(stdout);
    fflush(stderr);
    TEST(DUP2(saved_stdout, stdout_fd) >= 0);
    TEST(DUP2(saved_stderr, stderr_fd) >= 0);
    FD_CLOSE(saved_stdout);
    FD_CLOSE(saved_stderr);

    result.stdout_data = read_stream(stdout_capture);
    result.stderr_data = read_stream(stderr_capture);
    fclose(stdout_capture);
    fclose(stderr_capture);
    return result;
}

static int contains_string(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

static char *read_file(const char *path) {
    FILE *file;
    long size;
    char *buffer;

    file = fopen(path, "rb");
    TEST(file != NULL);
    TEST(fseek(file, 0, SEEK_END) == 0);
    size = ftell(file);
    TEST(size >= 0);
    TEST(fseek(file, 0, SEEK_SET) == 0);

    buffer = (char *)malloc((size_t)size + 1);
    TEST(buffer != NULL);
    if (size > 0) {
        TEST(fread(buffer, 1, (size_t)size, file) == (size_t)size);
    }
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

static void test_cli_usage(void) {
    char *argv_help_short[] = {"pcp-server", "-h"};
    char *argv_help_long[] = {"pcp-server", "--help"};
    server_run_result_t short_help = run_server_main(
        (int)(sizeof(argv_help_short) / sizeof(argv_help_short[0])),
        argv_help_short);
    server_run_result_t long_help = run_server_main(
        (int)(sizeof(argv_help_long) / sizeof(argv_help_long[0])),
        argv_help_long);

    TEST(short_help.exit_code == 1);
    TEST(long_help.exit_code == 1);
    TEST(contains_string(short_help.stdout_data, "Usage:"));
    TEST(strcmp(short_help.stdout_data, long_help.stdout_data) == 0);

    free_server_run_result(&short_help);
    free_server_run_result(&long_help);
}

static void test_cli_invalid_args(void) {
    char *argv_bad_short[] = {"pcp-server", "-q"};
    char *argv_bad_long[] = {"pcp-server", "--bad-argument"};
    server_run_result_t bad_short = run_server_main(
        (int)(sizeof(argv_bad_short) / sizeof(argv_bad_short[0])),
        argv_bad_short);
    server_run_result_t bad_long = run_server_main(
        (int)(sizeof(argv_bad_long) / sizeof(argv_bad_long[0])), argv_bad_long);

    TEST(bad_short.exit_code == 1);
    TEST(bad_long.exit_code == 1);
    TEST(contains_string(bad_short.stderr_data, "Unknown option"));
    TEST(contains_string(bad_long.stderr_data, "Unknown option"));

    free_server_run_result(&bad_short);
    free_server_run_result(&bad_long);
}

static void test_cli_timeout_and_port(void) {
    char *argv_timeout[] = {"pcp-server", "--timeout", "50"};
    char *argv_negative_timeout[] = {"pcp-server", "--timeout", "-500"};
    char *argv_bad_port[] = {"pcp-server", "-p", "-1", "--timeout", "50"};
    char *argv_default_port[] = {"pcp-server", "-p", "5351", "--timeout", "50"};
    char *argv_huge_port[] = {"pcp-server", "-p", "5555555555", "--timeout",
                              "50"};
    server_run_result_t result = run_server_main(
        (int)(sizeof(argv_timeout) / sizeof(argv_timeout[0])), argv_timeout);

    TEST(result.exit_code == 0);
    TEST(contains_string(result.stdout_data, "Server listening"));
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_negative_timeout) / sizeof(argv_negative_timeout[0])),
        argv_negative_timeout);
    TEST(result.exit_code == 1);
    TEST(contains_string(result.stdout_data,
                         "Value provided for timeout was negative"));
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_bad_port) / sizeof(argv_bad_port[0])), argv_bad_port);
    TEST(result.exit_code == 0);
    TEST(contains_string(result.stdout_data, "Bad value for option -p"));
    TEST(contains_string(result.stdout_data,
                         "Server listening on 0.0.0.0:5351"));
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_default_port) / sizeof(argv_default_port[0])),
        argv_default_port);
    TEST(result.exit_code == 0);
    TEST(contains_string(result.stdout_data,
                         "Server listening on 0.0.0.0:5351"));
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_huge_port) / sizeof(argv_huge_port[0])),
        argv_huge_port);
    TEST(result.exit_code == 0);
    TEST(contains_string(result.stdout_data, "Bad value for option -p"));
    free_server_run_result(&result);
}

static void test_cli_version_and_result_code(void) {
    char *argv_version_1[] = {"pcp-server", "-v", "1", "--timeout", "50"};
    char *argv_version_2[] = {"pcp-server", "-v", "2", "--timeout", "50"};
    char *argv_version_3[] = {"pcp-server", "-v", "3"};
    char *argv_version_0[] = {"pcp-server", "-v", "0"};
    char *argv_rc_1[] = {"pcp-server", "-r", "1", "--timeout", "50"};
    char *argv_rc_13[] = {"pcp-server", "-r", "13", "--timeout", "50"};
    char *argv_rc_100[] = {"pcp-server", "-r", "100"};
    char *argv_rc_255[] = {"pcp-server", "-r", "255", "--timeout", "50"};
    server_run_result_t result;

    result = run_server_main(
        (int)(sizeof(argv_version_1) / sizeof(argv_version_1[0])),
        argv_version_1);
    TEST(result.exit_code == 0);
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_version_2) / sizeof(argv_version_2[0])),
        argv_version_2);
    TEST(result.exit_code == 0);
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_version_3) / sizeof(argv_version_3[0])),
        argv_version_3);
    TEST(result.exit_code == 1);
    TEST(contains_string(result.stdout_data, "Version 3 is not supported"));
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_version_0) / sizeof(argv_version_0[0])),
        argv_version_0);
    TEST(result.exit_code == 1);
    TEST(contains_string(result.stdout_data, "Version 0 is not supported"));
    free_server_run_result(&result);

    result = run_server_main((int)(sizeof(argv_rc_1) / sizeof(argv_rc_1[0])),
                             argv_rc_1);
    TEST(result.exit_code == 0);
    free_server_run_result(&result);

    result = run_server_main((int)(sizeof(argv_rc_13) / sizeof(argv_rc_13[0])),
                             argv_rc_13);
    TEST(result.exit_code == 0);
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_rc_100) / sizeof(argv_rc_100[0])), argv_rc_100);
    TEST(result.exit_code == 1);
    TEST(contains_string(result.stdout_data, "Unsupported  RESULT CODE"));
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_rc_255) / sizeof(argv_rc_255[0])), argv_rc_255);
    TEST(result.exit_code == 0);
    free_server_run_result(&result);
}

static void test_cli_ret_dscp(void) {
    char *argv_ret_dscp[] = {"pcp-server", "--ret-dscp", "10", "--timeout",
                             "50"};
    char *argv_ret_dscp_app[] = {"pcp-server", "--app-bit", "--ret-dscp",
                                 "10",         "--timeout", "50"};
    server_run_result_t result = run_server_main(
        (int)(sizeof(argv_ret_dscp) / sizeof(argv_ret_dscp[0])), argv_ret_dscp);

    TEST(result.exit_code == 0);
    TEST(contains_string(result.stdout_data, "Server listening"));
    free_server_run_result(&result);

    result = run_server_main(
        (int)(sizeof(argv_ret_dscp_app) / sizeof(argv_ret_dscp_app[0])),
        argv_ret_dscp_app);
    TEST(result.exit_code == 0);
    TEST(contains_string(result.stdout_data, "Server listening"));
    free_server_run_result(&result);
}

static void remove_log_file(const char *path) { remove(path); }

static void copy_log_file_path(server_info_t *server_info, const char *path) {
    size_t len = strlen(path);

    TEST(len < sizeof(server_info->log_file));
    memcpy(server_info->log_file, path, len + 1);
}

static void run_logged_flow_case(const char *log_path, uint8_t version,
                                 struct sockaddr *source,
                                 struct sockaddr *destination,
                                 void (*flow_setup)(pcp_flow_t *),
                                 const char *required_line_1,
                                 const char *required_line_2,
                                 const char *required_line_3) {
    pcp_ctx_t *ctx;
    pcp_flow_t *flow;
    test_pcp_server_sequence_t server_sequence;
    test_pcp_server_config_t server_config;
    char *log_contents;

    remove_log_file(log_path);
    test_pcp_server_config_init(&server_config);
    server_config.server_port = "5351";
    server_config.server_address = "0.0.0.0";
    server_config.server_info.end_after_recv = 1;
    copy_log_file_path(&server_config.server_info, log_path);
    test_pcp_server_sequence_init(&server_sequence, &server_config, 1);
    test_sleep_ms(100);

    ctx = pcp_init(0, NULL);
    TEST(ctx != NULL);
    TEST(pcp_add_server(ctx, Sock_pton("127.0.0.1:5351"), version) == 0);

    flow = pcp_new_flow(ctx, source, destination, NULL, IPPROTO_TCP, 900, NULL);
    TEST(flow != NULL);
    if (flow_setup != NULL) {
        flow_setup(flow);
    }

    TEST(test_pcp_wait_with_servers(flow, 3000, &server_sequence) ==
         pcp_state_succeeded);

    pcp_close_flow(flow);
    pcp_delete_flow(flow);
    pcp_terminate(ctx, 0);
    test_pcp_server_sequence_stop(&server_sequence);

    log_contents = read_file(log_path);
    TEST(contains_string(log_contents, required_line_1));
    TEST(contains_string(log_contents, required_line_2));
    if (required_line_3 != NULL) {
        TEST(contains_string(log_contents, required_line_3));
    }
    free(log_contents);
    remove_log_file(log_path);
}

static void setup_prefer_failure(pcp_flow_t *flow) {
    pcp_flow_set_prefer_failure_opt(flow);
}

static void setup_filter(pcp_flow_t *flow) {
    struct sockaddr *filter_ip = Sock_pton("8.8.8.8:4444");

    pcp_flow_set_filter_opt(flow, filter_ip, 12);
}

static void test_log_file_cases(void) {
    struct sockaddr_storage source;
    struct sockaddr_storage destination;

    sock_pton("127.0.0.1:1234", (struct sockaddr *)&source);
    sock_pton("127.0.0.1:8888", (struct sockaddr *)&destination);

    run_logged_flow_case("test_pcp_server.log", 1, (struct sockaddr *)&source,
                         NULL, NULL, "PCP protocol VERSION 1.",
                         "MAP protocol:", NULL);
    run_logged_flow_case("test_pcp_server.log", 2, (struct sockaddr *)&source,
                         NULL, NULL, "PCP protocol VERSION 2.",
                         "MAP protocol:", NULL);
    run_logged_flow_case("test_pcp_server.log", 1, (struct sockaddr *)&source,
                         (struct sockaddr *)&destination, NULL,
                         "PCP protocol VERSION 1.",
                         "PEER Opcode specific information.", NULL);
    run_logged_flow_case("test_pcp_server.log", 2, (struct sockaddr *)&source,
                         (struct sockaddr *)&destination, NULL,
                         "PCP protocol VERSION 2.",
                         "PEER Opcode specific information.", NULL);
    run_logged_flow_case("test_pcp_server.log", 2, (struct sockaddr *)&source,
                         NULL, setup_prefer_failure, "PCP protocol VERSION 2.",
                         "MAP protocol:", "OPTION: \t Prefer fail");
    run_logged_flow_case("test_pcp_server.log", 2, (struct sockaddr *)&source,
                         NULL, setup_filter, "PCP protocol VERSION 2.",
                         "MAP protocol:", "FILTER PORT:");
}

static void send_malformed_packet(void) {
    int sockfd;
    struct sockaddr_in server_addr;
    unsigned char payload[3] = {0, 0, 0};

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    TEST(sockfd >= 0);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5351);
    TEST(inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) == 1);

    TEST(sendto(sockfd, (const char *)payload, sizeof(payload), 0,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == (int)sizeof(payload));
    FD_CLOSE(sockfd);
}

static void test_malformed_packet(void) {
    FILE *stdout_capture;
    int stdout_fd;
    int saved_stdout;
    test_pcp_server_config_t server_config;
    pcp_test_server_t server;
    char *stdout_data;

    stdout_capture = tmpfile();
    TEST(stdout_capture != NULL);
    stdout_fd = FILENO(stdout);
    saved_stdout = DUP(stdout_fd);
    TEST(saved_stdout >= 0);
    fflush(stdout);
    TEST(DUP2(FILENO(stdout_capture), stdout_fd) >= 0);

    test_pcp_server_config_init(&server_config);
    server_config.server_port = "5351";
    server_config.server_address = "127.0.0.1";
    server_config.server_info.end_after_recv = 1;
    TEST(pcp_test_server_start(&server, server_config.server_port,
                               server_config.server_address,
                               &server_config.server_info) == 0);

    send_malformed_packet();
    TEST(pcp_test_server_pulse(&server, 1000) == 1);
    if (pcp_test_server_is_running(&server)) {
        pcp_test_server_stop(&server);
    }

    fflush(stdout);
    TEST(DUP2(saved_stdout, stdout_fd) >= 0);
    FD_CLOSE(saved_stdout);

    stdout_data = read_stream(stdout_capture);
    fclose(stdout_capture);

    TEST(contains_string(stdout_data,
                         "Size of PCP packet is either smaller than 4 octets"));
    free(stdout_data);
}

int main(void) {
    PD_SOCKET_STARTUP();
    pcp_log_level = PCP_LOGLVL_DEBUG;

    test_cli_usage();
    test_cli_invalid_args();
    test_cli_timeout_and_port();
    test_cli_version_and_result_code();
    test_cli_ret_dscp();
    test_log_file_cases();
    test_malformed_packet();

    PD_SOCKET_CLEANUP();
    return 0;
}
