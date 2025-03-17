#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include "pcp_api.c"
#include "pcp_event_handler.c"
#include "test_macro.h"
#include <stdio.h>
#include <time.h>

#include "pcp_socket.h"
#ifdef WIN32
#include "pcp_gettimeofday.h"
#include <Netioapi.h>
#include <Winsock2.h>
#endif /* WIN32 */

#if 0
int gettimeofday (struct timeval *__restrict __tv,
             __timezone_ptr_t __tz)
{
    static unsigned t = 1363871491;
    __tv->tv_sec = t;
    __tv->tv_usec = 12345;
}
#endif

void fill_in6_addr(struct in6_addr *dst_ip6, uint16_t *dst_port,
                   struct sockaddr *src);

int main(void) {
    struct sockaddr_in sa, sa2;
    int ret = 0;
    int cmp;
    struct timeval t, t2, t3;
    pcp_ctx_t *ctx;

    PD_SOCKET_STARTUP();

    pcp_log_level = PCP_LOGLVL_WARN;
    ctx = pcp_init(0, NULL);

    sa.sin_family = AF_INET;
    inet_pton(AF_INET, "100.2.1.1", &sa.sin_addr);
    sa.sin_port = htons(5351);

    sa2.sin_family = AF_INET;
    inet_pton(AF_INET, "1.2.1.1", &sa2.sin_addr);
    sa2.sin_port = htons(5350);

    gettimeofday(&t, NULL);
    // test 1
    t2 = t;
    t2.tv_sec += 1;
    t2.tv_usec += 15000;
    t3 = t;
    t3.tv_sec += 2;
    t3.tv_usec += 5000;
    cmp = timeval_comp(&t2, &t3);
    if (cmp >= 0) { // LCOV_EXCL_START
        printf("File: %s:%d: Test 1 failed: %ds %dus < %ds %dus\n", __FILE__,
               __LINE__, (int)t2.tv_sec, (int)t2.tv_usec, (int)t3.tv_sec,
               (int)t3.tv_usec);
        ret = 1;
    } else {
        printf("File: %s:%d: Test 1 OK\n", __FILE__, __LINE__);
    } // LCOV_EXCL_STOP

    // test 2
    t2 = t;
    t2.tv_sec += 1;
    t2.tv_usec += 15000;
    t3 = t;
    t3.tv_sec += 2;
    t3.tv_usec += 5000;
    cmp = timeval_comp(&t3, &t2);
    if (cmp < 0) { // LCOV_EXCL_START
        printf("File: %s:%d: Test 2 failed: %ds %dus > %ds %dus\n", __FILE__,
               __LINE__, (int)t2.tv_sec, (int)t2.tv_usec, (int)t3.tv_sec,
               (int)t3.tv_usec);
        ret = 2;
    } else {
        printf("File: %s:%d: Test 2 OK\n", __FILE__, __LINE__);
    } // LCOV_EXCL_STOP

    // test 3
    t2 = t;
    t2.tv_sec = 0;
    t2.tv_usec = -15000;
    t3 = t;
    t3.tv_sec += 1;
    t3.tv_usec = -5000;
    cmp = timeval_comp(&t3, &t2) < 0;
    if (cmp < 0) { // LCOV_EXCL_START
        printf("File: %s:%d: Test 3 failed: %ds %dus > %ds %dus\n", __FILE__,
               __LINE__, (int)t2.tv_sec, (int)t2.tv_usec, (int)t3.tv_sec,
               (int)t3.tv_usec);
        ret = 2;
    } else {
        printf("File: %s:%d: Test 3 OK\n", __FILE__, __LINE__);
    } // LCOV_EXCL_STOP

    TEST(0 == timeval_comp(&t3, &t3));

    {
        pcp_recv_msg_t msg;
        pcp_server_t s;

        memset(&s, 0, sizeof(s));
        s.epoch = ~0;
        msg.recv_epoch = 10;
        msg.received_time = 20;

        TEST(0 == compare_epochs(&msg, &s));
        TEST(0 == compare_epochs(&msg, &s));

        msg.recv_epoch += 5;
        msg.received_time += 5;
        TEST(0 == compare_epochs(&msg, &s));

        msg.recv_epoch += 10;
        msg.received_time += 12;
        TEST(0 == compare_epochs(&msg, &s));

        msg.received_time -= 4;
        TEST(0 == compare_epochs(&msg, &s));

        msg.received_time -= 4;
        TEST(0 != compare_epochs(&msg, &s));

        msg.received_time += 10;
        TEST(0 != compare_epochs(&msg, &s));
    }

    // test 7
    /*    printf("Testing retransmit delay calcul\n");
        int32_t r=0;
        int ii=0;
        for (ii=0; ii<20; ii++) {
            r=PCP_RT(r);
            printf("%d\n",r);
        }
        printf("\n");
        int ran;
        r=0;
        printf("Round 2\n");
        for (ii=0; ii<20; ii++) {
            ran = 8192+(1024-(random()&2047));
            r=r<<1;
            r = (((ran * ((r>3?r:3)<1024?(r>3?r:3):1024))>>13));
            printf("%d\n",r);
        }*/

    pcp_terminate(ctx, 1);

    PD_SOCKET_CLEANUP();

    return ret;
} /* main() */
