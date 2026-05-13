/*
 *------------------------------------------------------------------
 * test_pcp_cli_nonce.c
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

#include "test_macro.h"
#include "test_process_helper.h"

#include "../cli-client/pcp_cli_nonce.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

static int contains_string(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

static void test_nonce_parser(void) {
    uint32_t nonce_words_be[3];

    TEST(pcp_cli_parse_nonce_words_be(NULL, nonce_words_be) == 1);
    TEST(pcp_cli_parse_nonce_words_be("1234abcd", nonce_words_be) == 1);
    TEST(pcp_cli_parse_nonce_words_be("12345678901234567890123Z",
                                      nonce_words_be) == 1);

    TEST(pcp_cli_parse_nonce_words_be("00112233445566778899aabb",
                                      nonce_words_be) == 0);
    TEST(nonce_words_be[0] == htonl(0x00112233));
    TEST(nonce_words_be[1] == htonl(0x44556677));
    TEST(nonce_words_be[2] == htonl(0x8899aabb));

    TEST(pcp_cli_parse_nonce_words_be("AABBCCDDEEFF001122334455",
                                      nonce_words_be) == 0);
    TEST(nonce_words_be[0] == htonl(0xaabbccdd));
    TEST(nonce_words_be[1] == htonl(0xeeff0011));
    TEST(nonce_words_be[2] == htonl(0x22334455));
}

static void test_nonce_cli_inputs(void) {
    char *argv_valid[] = {"pcpnatpmpc",
                          "--nonce",
                          "00112233445566778899aabb",
                          "-l",
                          "120",
                          "--internal",
                          ":1234",
                          "-d",
                          "--server",
                          "127.0.0.1:5351",
                          "--timeout",
                          "0"};
    char *argv_bad_len[] = {"pcpnatpmpc", "--nonce", "1234abcd", "--internal",
                            ":1234"};
    char *argv_bad_hex[] = {"pcpnatpmpc", "--nonce", "12345678901234567890123Z",
                            "--internal", ":1234"};
    test_process_result_t result;

    TEST(test_process_run(PCP_CLI_CLIENT_EXE,
                          (int)(sizeof(argv_valid) / sizeof(argv_valid[0])),
                          argv_valid, &result) == 0);
    TEST(result.exit_code == 2);
    TEST(contains_string(result.stdout_data, "timed out"));
    TEST(!contains_string(result.stderr_data, "Invalid nonce format"));
    test_process_result_free(&result);

    TEST(test_process_run(PCP_CLI_CLIENT_EXE,
                          (int)(sizeof(argv_bad_len) / sizeof(argv_bad_len[0])),
                          argv_bad_len, &result) == 0);
    TEST(result.exit_code == 1);
    TEST(contains_string(result.stderr_data,
                         "Invalid nonce format. Expected exactly 24 hex "
                         "chars."));
    test_process_result_free(&result);

    TEST(test_process_run(PCP_CLI_CLIENT_EXE,
                          (int)(sizeof(argv_bad_hex) / sizeof(argv_bad_hex[0])),
                          argv_bad_hex, &result) == 0);
    TEST(result.exit_code == 1);
    TEST(contains_string(result.stderr_data,
                         "Invalid nonce format. Expected exactly 24 hex "
                         "chars."));
    test_process_result_free(&result);
}

int main(void) {
    test_nonce_parser();
    test_nonce_cli_inputs();
    return 0;
}
