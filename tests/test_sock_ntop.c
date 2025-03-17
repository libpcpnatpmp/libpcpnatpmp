/*
 *------------------------------------------------------------------
 * test_sock_ntop.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/un.h> /* for Unix domain sockets */
#endif
#include "gateway.h"
#include "pcp_client_db.h"
#include "pcp_socket.h"
#include "test_macro.h"
#include "unp.h"

int main(void) {
    struct sockaddr_storage sa;
    int prefix;

    PD_SOCKET_STARTUP();
    TEST(0 == sock_pton("[::1]:1234", (struct sockaddr *)&sa));
    TEST(strcmp(
             Sock_ntop((struct sockaddr *)&sa, SA_LEN((struct sockaddr *)&sa)),
             "[::1]:1234") == 0);
    TEST(0 == sock_pton("::1", (struct sockaddr *)&sa));
    TEST(strcmp(
             Sock_ntop((struct sockaddr *)&sa, SA_LEN((struct sockaddr *)&sa)),
             "::1") == 0);
    TEST(0 == sock_pton("10.250.22.11:1234", (struct sockaddr *)&sa));
    TEST(strcmp(
             Sock_ntop((struct sockaddr *)&sa, SA_LEN((struct sockaddr *)&sa)),
             "10.250.22.11:1234") == 0);
    TEST(0 == sock_pton("10.250.22.11", (struct sockaddr *)&sa));
    TEST(strcmp(
             Sock_ntop((struct sockaddr *)&sa, SA_LEN((struct sockaddr *)&sa)),
             "10.250.22.11") == 0);
    TEST(-2 == sock_pton("10.10.250.22.11:1234", (struct sockaddr *)&sa));
    TEST(-2 == sock_pton("10.10.250.22:11:1234", (struct sockaddr *)&sa));
    TEST(-2 == sock_pton("10.10.250d.22.11:1234", (struct sockaddr *)&sa));
    TEST(0 == sock_pton("localhost:1234", (struct sockaddr *)&sa));
    TEST(0 == sock_pton(":1234", (struct sockaddr *)&sa));
    TEST(0 == sock_pton("localhost", (struct sockaddr *)&sa));
    TEST(0 == sock_pton("::1", (struct sockaddr *)&sa));
    TEST(-2 == sock_pton("[::1:1234", (struct sockaddr *)&sa));
    TEST(0 == sock_pton("  [::1]:1234", (struct sockaddr *)&sa));
    TEST(0 == sock_pton("  0.0.0.0:1234", (struct sockaddr *)&sa));
    TEST(-1 == sock_pton(NULL, (struct sockaddr *)&sa));
    TEST(-1 == sock_pton("  0.0.0.0:1234", NULL));
    TEST(-1 == sock_pton(NULL, NULL));
    sa.ss_family = AF_UNIX;
#ifndef WIN32 // Unix sockets not supported on Windows
    TEST(NULL !=
         Sock_ntop((struct sockaddr *)&sa, SA_LEN((struct sockaddr *)&sa)))
    ((struct sockaddr_un *)&sa)->sun_path[0] = 0;
#endif
    TEST(NULL !=
         Sock_ntop((struct sockaddr *)&sa, SA_LEN((struct sockaddr *)&sa)))
    sa.ss_family = 255;
    TEST(strncmp(
             "sock_ntop: unknown AF",
             Sock_ntop((struct sockaddr *)&sa, SA_LEN((struct sockaddr *)&sa)),
             21) == 0);

    // testing filter option ip.prefix parsing
    TEST(0 == sock_pton_with_prefix("[10.10.250.22/16]:1234",
                                    (struct sockaddr *)&sa, &prefix));
    TEST(-2 == sock_pton_with_prefix("[10.10.250d.22/16]:1234",
                                     (struct sockaddr *)&sa, &prefix));
    TEST(-2 == sock_pton_with_prefix("[10.10.250.22/34]:1234",
                                     (struct sockaddr *)&sa, &prefix));
    TEST(0 == sock_pton_with_prefix("[10.10.250.22/16]", (struct sockaddr *)&sa,
                                    &prefix));
    TEST(0 == sock_pton_with_prefix(
                  "[10.10.250.22/16]:", (struct sockaddr *)&sa, &prefix));
    // TEST( -2 == sock_pton_with_prefix("[]:",(struct sockaddr*)&sa, &prefix)
    // );
    TEST(0 == sock_pton_with_prefix("[10.10.250.22/]:", (struct sockaddr *)&sa,
                                    &prefix));
    TEST(0 == sock_pton_with_prefix("[10.10.250.22/0]:", (struct sockaddr *)&sa,
                                    &prefix));
    TEST(0 == sock_pton_with_prefix("[10.10.250.22/0]:0",
                                    (struct sockaddr *)&sa, &prefix));
    TEST(0 == sock_pton_with_prefix("[::1/64]:4444", (struct sockaddr *)&sa,
                                    &prefix));
    TEST(0 == sock_pton_with_prefix("[::1/0]:4444", (struct sockaddr *)&sa,
                                    &prefix));
    TEST(-2 == sock_pton_with_prefix("[::1/0]:444d", (struct sockaddr *)&sa,
                                     &prefix));
    TEST(-2 == sock_pton_with_prefix("[::1/129]:4444", (struct sockaddr *)&sa,
                                     &prefix));

    PD_SOCKET_CLEANUP();

    exit(0);
}
