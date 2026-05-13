#ifndef PCP_CLI_NONCE_H_
#define PCP_CLI_NONCE_H_

#include <stdint.h>

int pcp_cli_parse_nonce_words_be(const char *txt, uint32_t nonce_words_be[3]);

#endif /* PCP_CLI_NONCE_H_ */
