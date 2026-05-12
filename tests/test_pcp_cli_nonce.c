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
#include <stdint.h>

#define main pcpnatpmpc_main
#include "../cli-client/pcpnatpmpc.c"
#undef main

int main(void) {
    struct pcp_params params;
    uint32_t nonce_words_be[3];
    char *argv[] = {"pcpnatpmpc",
                    "--nonce",
                    "00112233445566778899aabb",
                    "-l",
                    "120",
                    "--internal",
                    ":1234",
                    "-d",
                    "--server",
                    "127.0.0.1:5351"};
    int argc = sizeof(argv) / sizeof(argv[0]);

    TEST(parse_nonce_words_be(NULL, nonce_words_be) == 1);
    TEST(parse_nonce_words_be("1234abcd", nonce_words_be) == 1);
    TEST(parse_nonce_words_be("12345678901234567890123Z", nonce_words_be) == 1);

    TEST(parse_nonce_words_be("00112233445566778899aabb", nonce_words_be) == 0);
    TEST(nonce_words_be[0] == htonl(0x00112233));
    TEST(nonce_words_be[1] == htonl(0x44556677));
    TEST(nonce_words_be[2] == htonl(0x8899aabb));

    TEST(parse_nonce_words_be("AABBCCDDEEFF001122334455", nonce_words_be) == 0);
    TEST(nonce_words_be[0] == htonl(0xaabbccdd));
    TEST(nonce_words_be[1] == htonl(0xeeff0011));
    TEST(nonce_words_be[2] == htonl(0x22334455));

    optind = 0;
    parse_params(&params, argc, argv);
    TEST(params.has_nonce == 1);
    TEST(params.opt_lifetime == 120);
    TEST(params.has_mappeer_data == 1);
    TEST(params.nonce_words_be[0] == htonl(0x00112233));
    TEST(params.nonce_words_be[1] == htonl(0x44556677));
    TEST(params.nonce_words_be[2] == htonl(0x8899aabb));

    return 0;
}
