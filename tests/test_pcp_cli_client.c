/*
 *------------------------------------------------------------------
 * test_pcp_cli_client.c
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
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#include "test_macro.h"
#include "test_pcp_server_helper.h"

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

static jmp_buf cli_exit_env;
static int cli_exit_code;

static void cli_test_exit(int code) {
    cli_exit_code = code;
    longjmp(cli_exit_env, 1);
}

#define exit cli_test_exit
#define main pcpnatpmpc_main
#include "../cli-client/pcpnatpmpc.c"
#undef main
#undef exit

typedef struct cli_run_result {
    int exit_code;
    char *stdout_data;
    char *stderr_data;
} cli_run_result_t;

typedef struct server_runner {
    test_pcp_server_sequence_t sequence;
    const test_pcp_server_config_t *configs;
    size_t config_count;
    volatile int stop_requested;
    int last_error;
#ifdef WIN32
    HANDLE thread_handle;
#else
    pthread_t thread;
#endif
} server_runner_t;

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

static void free_cli_run_result(cli_run_result_t *result) {
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

static cli_run_result_t run_cli(int argc, char **argv) {
    cli_run_result_t result;
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
    cli_exit_code = -1;
    if (setjmp(cli_exit_env) == 0) {
        result.exit_code = pcpnatpmpc_main(argc, argv);
    } else {
        result.exit_code = cli_exit_code;
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

#ifdef WIN32
static DWORD WINAPI server_runner_main(LPVOID arg) {
#else
static void *server_runner_main(void *arg) {
#endif
    server_runner_t *runner = (server_runner_t *)arg;

    test_pcp_server_sequence_init(&runner->sequence, runner->configs,
                                  runner->config_count);
    while (!runner->stop_requested &&
           test_pcp_server_sequence_is_active(&runner->sequence)) {
        if (test_pcp_server_sequence_pulse(&runner->sequence, 100) < 0) {
            runner->last_error = 1;
            break;
        }
    }

    test_pcp_server_sequence_stop(&runner->sequence);

#ifdef WIN32
    return 0;
#else
    return NULL;
#endif
}

static void start_server_runner(server_runner_t *runner,
                                const test_pcp_server_config_t *configs,
                                size_t config_count) {
    memset(runner, 0, sizeof(*runner));
    runner->configs = configs;
    runner->config_count = config_count;

#ifdef WIN32
    runner->thread_handle =
        CreateThread(NULL, 0, server_runner_main, runner, 0, NULL);
    TEST(runner->thread_handle != NULL);
#else
    TEST(pthread_create(&runner->thread, NULL, server_runner_main, runner) ==
         0);
#endif

    test_sleep_ms(100);
}

static void stop_server_runner(server_runner_t *runner) {
    runner->stop_requested = 1;

#ifdef WIN32
    TEST(WaitForSingleObject(runner->thread_handle, INFINITE) == WAIT_OBJECT_0);
    CloseHandle(runner->thread_handle);
#else
    TEST(pthread_join(runner->thread, NULL) == 0);
#endif

    TEST(runner->last_error == 0);
    test_sleep_ms(50);
}

static int contains_string(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

static cli_run_result_t
run_cli_with_servers(int argc, char **argv,
                     const test_pcp_server_config_t *configs,
                     size_t config_count) {
    cli_run_result_t result;
    server_runner_t runner;

    start_server_runner(&runner, configs, config_count);
    result = run_cli(argc, argv);
    stop_server_runner(&runner);

    return result;
}

static void test_usage_output(void) {
    char *argv_no_args[] = {"pcpnatpmpc"};
    char *argv_help_long[] = {"pcpnatpmpc", "--help"};
    char *argv_help_short[] = {"pcpnatpmpc", "-h"};
    cli_run_result_t no_args = run_cli(1, argv_no_args);
    cli_run_result_t help_long = run_cli(2, argv_help_long);
    cli_run_result_t help_short = run_cli(2, argv_help_short);

    TEST(no_args.exit_code == 1);
    TEST(help_long.exit_code == 0);
    TEST(help_short.exit_code == 0);
    TEST(strcmp(no_args.stdout_data, help_long.stdout_data) == 0);
    TEST(strcmp(no_args.stdout_data, help_short.stdout_data) == 0);

    free_cli_run_result(&no_args);
    free_cli_run_result(&help_long);
    free_cli_run_result(&help_short);
}

static void test_invalid_inputs(void) {
    char bad_short_opt[] = {'-', '\2', '\0'};
    char *cases[][16] = {
        {"pcpnatpmpc", "--bad-argument-input"},
        {"pcpnatpmpc", "--peer", "::0bad:ip6"},
        {"pcpnatpmpc", "--external", "::0bad:ip6"},
        {"pcpnatpmpc", "--external", "[::2]:99999"},
        {"pcpnatpmpc", "--internal", "[::0bad:ip6]:1234"},
        {"pcpnatpmpc", "--nonce", "1234abcd", "--internal", ":1234"},
        {"pcpnatpmpc", "--nonce", "12345678901234567890123Z", "--internal",
         ":1234"},
        {"pcpnatpmpc", "-v", "3", "--internal", ":1234"},
        {"pcpnatpmpc", "-d"},
        {"pcpnatpmpc", "--pcp-version", "3", "--internal", ":1234"},
        {"pcpnatpmpc", "-v", "1", "-q"},
        {"pcpnatpmpc", bad_short_opt},
        {"pcpnatpmpc", "--internal", ":1234", "--jitter-tolerance", "1"},
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int argc = 0;
        cli_run_result_t result;

        while (cases[i][argc] != NULL) {
            argc++;
        }

        result = run_cli(argc, cases[i]);
        TEST(result.exit_code != 0);
        free_cli_run_result(&result);
    }
}

static void test_timeout_and_parse_cases(void) {
    char *argv_long_opt[] = {"pcpnatpmpc",    "--disable-autodiscovery",
                             "--server",      "1.2.3.4:1234",
                             "--peer",        "1.2.3.4:1234",
                             "--internal",    "0.0.0.0:4321",
                             "--external",    "1.1.1.1:4321",
                             "--protocol",    "17",
                             "--lifetime",    "120",
                             "--pcp-version", "1",
                             "--timeout",     "0"};
    char *argv_timeout[] = {
        "pcpnatpmpc",    "-s", "8.8.8.2", "--pcp-version", "2",
        "--fast-return", "-i", ":1234",   "--timeout",     "0"};
    cli_run_result_t result = run_cli(
        (int)(sizeof(argv_long_opt) / sizeof(argv_long_opt[0])), argv_long_opt);

    TEST(result.exit_code == 2);
    free_cli_run_result(&result);

    result = run_cli((int)(sizeof(argv_timeout) / sizeof(argv_timeout[0])),
                     argv_timeout);
    TEST(result.exit_code == 2);
    TEST(contains_string(result.stdout_data, "timed out"));
    free_cli_run_result(&result);
}

static void test_ipv4_server_cases(void) {
    test_pcp_server_config_t config;
    cli_run_result_t result;

    test_pcp_server_config_init(&config);
    config.server_port = "5351";
    config.server_info.end_after_recv = 1;
    config.server_info.default_result_code = 8;

    {
        char *argv[] = {"pcpnatpmpc",
                        "--server",
                        "127.0.0.1",
                        "-f",
                        "--disable-autodiscovery",
                        "--pcp-version",
                        "1",
                        "--peer",
                        "127.0.0.1:1234",
                        "--internal",
                        ":1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 3);
        free_cli_run_result(&result);
    }

    config.server_info.default_result_code = 3;
    {
        char *argv[] = {"pcpnatpmpc", "--server",       "127.0.0.1",
                        "-d",         "--pcp-version",  "1",
                        "--peer",     "127.0.0.1:1111", "--fast-return",
                        "--internal", ":1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 4);
        free_cli_run_result(&result);
    }

    config.server_info.default_result_code = 8;
    {
        char *argv[] = {"pcpnatpmpc",    "--server", "127.0.0.1",       "-f",
                        "--pcp-version", "1",        "--internal=:1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 2 || result.exit_code == 3 ||
             result.exit_code == 4);
        free_cli_run_result(&result);
    }

    config.server_info.default_result_code = 3;
    {
        char *argv[] = {
            "pcpnatpmpc", "--server",      "127.0.0.1", "--pcp-version",
            "1",          "--fast-return", "-i",        ":1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 2);
        free_cli_run_result(&result);
    }

    config.server_info.default_result_code = 255;
    {
        char *argv[] = {
            "pcpnatpmpc",    "-s", "127.0.0.1", "--pcp-version", "2",
            "--fast-return", "-i", ":1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 0);
        free_cli_run_result(&result);
    }
}

static void test_ipv6_server_cases(void) {
    const char *use_ipv6_socket = getenv("PCP_USE_IPV6_SOCKET");
    test_pcp_server_config_t config;
    cli_run_result_t result;

    if (use_ipv6_socket == NULL || strcmp(use_ipv6_socket, "1") != 0) {
        return;
    }

    test_pcp_server_config_init(&config);
    config.server_port = "5351";
    config.server_address = "::1";
    config.server_info.server_version = 1;
    config.server_info.end_after_recv = 2;

    {
        char *argv[] = {"pcpnatpmpc", "--pcp-version", "2",         "--server",
                        "::1",        "--internal",    "[::]:1234", "--peer",
                        "[::]:4321",  "--fast-return"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 0);
        free_cli_run_result(&result);
    }

    {
        char *argv[] = {"pcpnatpmpc", "--pcp-version", "2",  "-s",        "::1",
                        "-i",         "[::]:1234",     "-p", "[::1]:1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 0);
        free_cli_run_result(&result);
    }

    {
        char *argv[] = {"pcpnatpmpc", "--pcp-version", "2", "-i", "[::]:1234",
                        "-p",         "[::1]:4321"};
        result = run_cli((int)(sizeof(argv) / sizeof(argv[0])), argv);
        TEST(result.exit_code > 0);
        free_cli_run_result(&result);
    }
}

static void test_map_options(void) {
    test_pcp_server_config_t config;
    cli_run_result_t result;

    test_pcp_server_config_init(&config);
    config.server_port = "5351";
    config.server_info.end_after_recv = 1;

    {
        char *argv[] = {"pcpnatpmpc", "-d",    "-s", "127.0.0.1",
                        "-i",         ":1234", "-P"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 0);
        free_cli_run_result(&result);
    }

    {
        char *argv[] = {"pcpnatpmpc", "-d", "-s",           "127.0.0.1", "-i",
                        ":1234",      "-p", "8.8.8.8:3333", "-P"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 1);
        free_cli_run_result(&result);
    }

    {
        char *argv[] = {"pcpnatpmpc", "-d",    "-s", "127.0.0.1",
                        "-i",         ":1234", "-F", "[8.8.8.8/12]:4444"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 0);
        free_cli_run_result(&result);
    }

    {
        char *argv[] = {"pcpnatpmpc",   "-s",    "127.0.0.1",
                        "-i",           ":1234", "-p",
                        "8.8.8.8:3333", "-F",    "[8.8.8.8/12]:4444"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 1);
        free_cli_run_result(&result);
    }
}

int main(void) {
    PD_SOCKET_STARTUP();
    pcp_log_level = 5;

    test_usage_output();
    test_invalid_inputs();
    test_timeout_and_parse_cases();
    test_ipv4_server_cases();
    test_ipv6_server_cases();
    test_map_options();

    PD_SOCKET_CLEANUP();
    return 0;
}
