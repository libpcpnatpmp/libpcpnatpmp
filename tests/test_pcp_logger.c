/*
 *------------------------------------------------------------------
 * test_pcp_logger.c
 *
 * May, 2013, ptatrai
 *
 * Copyright (c) 2013-2013 by cisco Systems, Inc.
 * All rights reserved.
 *
 *------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include "pcp.h"
#include "pcp_logger.h"
#include "test_macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_mode;
static char test_msg[10240];

static void test_logfn(pcp_loglvl_e mode, const char *msg) {
    test_mode = mode;
    strncpy(test_msg, msg, sizeof(test_msg) - 1);
    test_msg[sizeof(test_msg) - 1] = 0;
}

int main(void) {
    char big_string[] = "012345678911234567892123456789312345678941234567895123"
                        "4567896123456789712345678981234567899123456789"
                        "012345678911234567892123456789312345678941234567895123"
                        "4567896123456789712345678981234567899123456789"
                        "012345678911234567892123456789312345678941234567895123"
                        "4567896123456789712345678981234567899123456789";

    PCP_LOG(PCP_LOGLVL_NONE, "%s", "none");
    PCP_LOG(PCP_LOGLVL_ERR, "%s", "error");
    PCP_LOG(PCP_LOGLVL_WARN, "%s", "warn");
    PCP_LOG(PCP_LOGLVL_INFO, "%s", "info");
    PCP_LOG(PCP_LOGLVL_PERR, "%s", "perr");
    PCP_LOG(PCP_LOGLVL_DEBUG, "%s", "debug");

    pcp_set_loggerfn(NULL);
    PCP_LOG(PCP_LOGLVL_NONE, "big msg %s %s %s %s %s %s %s", big_string,
            big_string, big_string, big_string, big_string, big_string,
            big_string);

    pcp_set_loggerfn(test_logfn);
    PCP_LOG(PCP_LOGLVL_NONE, "%s", "none");
    TEST((PCP_LOGLVL_INFO <= PCP_MAX_LOG_LEVEL) ||
         ((test_mode == PCP_LOGLVL_NONE) && (strcmp("none", test_msg) == 0)));
    PCP_LOG(PCP_LOGLVL_ERR, "%s", "error");
    TEST((PCP_LOGLVL_INFO <= PCP_MAX_LOG_LEVEL) ||
         ((test_mode == PCP_LOGLVL_ERR) && (strcmp("error", test_msg) == 0)));
    PCP_LOG(PCP_LOGLVL_WARN, "%s", "warn");
    TEST((PCP_LOGLVL_INFO <= PCP_MAX_LOG_LEVEL) ||
         ((test_mode == PCP_LOGLVL_WARN) && (strcmp("warn", test_msg) == 0)));
    PCP_LOG(PCP_LOGLVL_INFO, "%s", "info");
    TEST((PCP_LOGLVL_INFO <= PCP_MAX_LOG_LEVEL) ||
         ((test_mode == PCP_LOGLVL_INFO) && (strcmp("info", test_msg) == 0)));
    PCP_LOG(PCP_LOGLVL_PERR, "%s", "perr");
    TEST((PCP_LOGLVL_INFO <= PCP_MAX_LOG_LEVEL) ||
         ((test_mode == PCP_LOGLVL_PERR) && (strcmp("perr", test_msg) == 0)));
    PCP_LOG(PCP_LOGLVL_DEBUG, "%s", "debug");
    TEST((PCP_LOGLVL_INFO <= PCP_MAX_LOG_LEVEL) ||
         ((test_mode == PCP_LOGLVL_DEBUG) &&
          (strcmp("debug   ", test_msg) == 0)));

    PCP_LOG(PCP_LOGLVL_NONE, "big msg %s %s %s %s %s %s %s", big_string,
            big_string, big_string, big_string, big_string, big_string,
            big_string);
    TEST(strlen(test_msg) == 2114);

    return 0;
}
