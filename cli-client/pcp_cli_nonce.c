#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include "pcp_cli_nonce.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

int pcp_cli_parse_nonce_words_be(const char *txt, uint32_t nonce_words_be[3]) {
    char chunk[9];
    unsigned long parsed;
    int i;
    char *endptr;

    if (!txt || strlen(txt) != 24) {
        return 1;
    }
    for (i = 0; i < 24; i++) {
        if (!isxdigit((unsigned char)txt[i])) {
            return 1;
        }
    }

    chunk[8] = '\0';
    for (i = 0; i < 3; i++) {
        memcpy(chunk, txt + (i * 8), 8);
        errno = 0;
        parsed = strtoul(chunk, &endptr, 16);
        if ((errno != 0) || (*endptr != '\0') || (parsed > 0xFFFFFFFFUL)) {
            return 1;
        }
        nonce_words_be[i] = htonl((uint32_t)parsed);
    }

    return 0;
}
