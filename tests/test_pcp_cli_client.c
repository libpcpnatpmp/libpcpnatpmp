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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pcp_socket.h"
#include "test_macro.h"
#include "test_pcp_server_helper.h"
#include "test_process_helper.h"

typedef test_process_result_t cli_run_result_t;

static void free_cli_run_result(cli_run_result_t *result) {
    test_process_result_free(result);
}

static int contains_string(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

static void build_server_arg(char *buffer, size_t buffer_size,
                             const char *address, const char *port) {
    snprintf(buffer, buffer_size, "%s:%s", address, port);
}

static void expect_exit_code(const cli_run_result_t *result, int expected) {
    if (result->exit_code != expected) {
        printf("Unexpected CLI exit code %d, expected "
               "%d\nstdout:\n%s\nstderr:\n%s\n",
               result->exit_code, expected, result->stdout_data,
               result->stderr_data);
    }
    TEST(result->exit_code == expected);
}

static cli_run_result_t run_cli(int argc, char **argv) {
    cli_run_result_t result;

    TEST(test_process_run(PCP_CLI_CLIENT_EXE, argc, argv, &result) == 0);
    return result;
}

static cli_run_result_t
run_cli_with_servers(int argc, char **argv,
                     const test_pcp_server_config_t *configs,
                     size_t config_count) {
    cli_run_result_t result;
    test_process_t process;
    test_pcp_server_sequence_t sequence;
    int exited = 0;
    int exit_code = -1;
    int elapsed_ms = 0;

    test_pcp_server_sequence_init(&sequence, configs, config_count);
    test_sleep_ms(500);
    TEST(test_process_start(&process, PCP_CLI_CLIENT_EXE, argc, argv) == 0);

    while (!exited && elapsed_ms < 10000) {
        TEST(test_process_try_wait(&process, &exited, &exit_code) == 0);
        if (exited) {
            break;
        }

        if (test_pcp_server_sequence_is_active(&sequence)) {
            TEST(test_pcp_server_sequence_pulse(&sequence, 50) >= 0);
        } else {
            test_sleep_ms(50);
        }
        elapsed_ms += 50;
    }

    TEST(exited);
    TEST(test_process_finish(&process, &result) == 0);
    test_pcp_server_sequence_stop(&sequence);

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
    char server_arg[32];

    test_pcp_server_config_init(&config);
    config.server_info.end_after_recv = 1;
    config.server_info.default_result_code = 8;

    {
        config.server_port = "55351";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {"pcpnatpmpc",
                        "--server",
                        server_arg,
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
        expect_exit_code(&result, 3);
        free_cli_run_result(&result);
    }

    config.server_info.default_result_code = 3;
    {
        config.server_port = "55352";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {"pcpnatpmpc", "--server",       server_arg,
                        "-d",         "--pcp-version",  "1",
                        "--peer",     "127.0.0.1:1111", "--fast-return",
                        "--internal", ":1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        expect_exit_code(&result, 4);
        free_cli_run_result(&result);
    }

    config.server_info.default_result_code = 8;
    {
        config.server_port = "55353";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {"pcpnatpmpc",    "--server", server_arg,        "-f",
                        "--pcp-version", "1",        "--internal=:1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 2 || result.exit_code == 3 ||
             result.exit_code == 4);
        free_cli_run_result(&result);
    }

    config.server_info.default_result_code = 3;
    {
        config.server_port = "55354";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {
            "pcpnatpmpc", "--server",      server_arg, "--pcp-version",
            "1",          "--fast-return", "-i",       ":1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        expect_exit_code(&result, 2);
        free_cli_run_result(&result);
    }

    config.server_info.default_result_code = 255;
    {
        config.server_port = "55355";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {"pcpnatpmpc",    "-s", server_arg, "--pcp-version", "2",
                        "--fast-return", "-i", ":1234"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        expect_exit_code(&result, 0);
        free_cli_run_result(&result);
    }
}

static void test_ipv6_server_cases(void) {
    const char *use_ipv6_socket = getenv("PCP_USE_IPV6_SOCKET");
    test_pcp_server_config_t config;
    cli_run_result_t result;
    char server_arg[40];

    if (use_ipv6_socket == NULL || strcmp(use_ipv6_socket, "1") != 0) {
        return;
    }

    test_pcp_server_config_init(&config);
    config.server_port = "55356";
    config.server_address = "::1";
    config.server_info.server_version = 1;
    config.server_info.end_after_recv = 2;
    build_server_arg(server_arg, sizeof(server_arg), "::1", config.server_port);

    {
        char *argv[] = {"pcpnatpmpc", "--pcp-version", "2",         "--server",
                        server_arg,   "--internal",    "[::]:1234", "--peer",
                        "[::]:4321",  "--fast-return"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        expect_exit_code(&result, 0);
        free_cli_run_result(&result);
    }

    {
        char *argv[] = {
            "pcpnatpmpc", "--pcp-version", "2",  "-s",        server_arg,
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
    char server_arg[32];

    test_pcp_server_config_init(&config);
    config.server_info.end_after_recv = 1;

    {
        config.server_port = "55357";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {"pcpnatpmpc", "-d",    "-s", server_arg,
                        "-i",         ":1234", "-P"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        TEST(result.exit_code == 0);
        free_cli_run_result(&result);
    }

    {
        config.server_port = "55358";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {"pcpnatpmpc", "-d", "-s",           server_arg, "-i",
                        ":1234",      "-p", "8.8.8.8:3333", "-P"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        expect_exit_code(&result, 1);
        free_cli_run_result(&result);
    }

    {
        config.server_port = "55359";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {"pcpnatpmpc", "-d",    "-s", server_arg,
                        "-i",         ":1234", "-F", "[8.8.8.8/12]:4444"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        expect_exit_code(&result, 0);
        free_cli_run_result(&result);
    }

    {
        config.server_port = "55360";
        build_server_arg(server_arg, sizeof(server_arg), "127.0.0.1",
                         config.server_port);
        char *argv[] = {"pcpnatpmpc",   "-s",    server_arg,
                        "-i",           ":1234", "-p",
                        "8.8.8.8:3333", "-F",    "[8.8.8.8/12]:4444"};
        result = run_cli_with_servers((int)(sizeof(argv) / sizeof(argv[0])),
                                      argv, &config, 1);
        expect_exit_code(&result, 1);
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
