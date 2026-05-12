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
    uint32_t nonce_words_be[3];

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

    return 0;
}
