/*
 * Copyright (c) 2014 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include "pcp.h"
#include "pcp_msg_structs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "getopt.h"

#ifdef WIN32

#include <malloc.h>
#include <errno.h>
#include <sys/types.h>
#include <stdarg.h>
#include <ws2tcpip.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include "pcp_win_defines.h"
#include "unp.h"
#include "pcp_utils.h"

#if 0
// function calling WSAStartup (used in pcp-server and pcp_app)
static int pcp_win_sock_startup() {
    int err;
    WORD wVersionRequested;
    WSADATA wsaData;
    OSVERSIONINFOEX osvi;
    /* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        perror("WSAStartup failed with error");
        return 1;
    }
    //find windows version
    ZeroMemory(&osvi, sizeof(osvi));
    osvi.dwOSVersionInfoSize = sizeof(osvi);

    if(!GetVersionEx((LPOSVERSIONINFO)(&osvi))){
        printf("pcp_app: GetVersionEx failed");
        return 1;
    }

    return 0;
}

/* function calling WSACleanup
*  returns 0 on success and 1 on failure
*/
static int pcp_win_sock_cleanup() {
    if (WSACleanup() == PCP_SOCKET_ERROR) {
        printf("WSACleanup failed.\n");
        return 1;
    }
    return 0;
}
#endif

#define PD_SOCKET_STARTUP pcp_win_sock_startup
#define PD_SOCKET_CLEANUP pcp_win_sock_cleanup

char *ctime_r(const time_t *timep, char *buf)
{
    ctime_s(buf, 26, timep);
    return buf;
}
#else //!WIN32

#include <alloca.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <ctype.h>
#include <getopt.h>
#include <sys/utsname.h>
#include "unp.h"
#include "pcp_utils.h"

#define PD_SOCKET_STARTUP()
#define PD_SOCKET_CLEANUP()

#endif // WIN32

#ifndef no_argument
#define no_argument 0
#endif

#ifndef required_argument
#define required_argument 1
#endif

typedef pcp_location_option_t pcp_app_location_t;
typedef pcp_userid_option_t   pcp_app_userid_t;
typedef pcp_deviceid_option_t pcp_app_deviceid_t;

#define NOSHORT ""
#define SHORT(a) "-"#a

#ifdef PCP_FLOW_PRIORITY
#define IFDEF_PCP_FLOW_PRIORITY(...)  __VA_ARGS__
#else
#define IFDEF_PCP_FLOW_PRIORITY(...)
#endif

#ifdef PCP_SADSCP
#define IFDEF_PCP_SADSCP(...)  __VA_ARGS__
#else
#define IFDEF_PCP_SADSCP(...)
#endif

#ifdef PCP_EXPERIMENTAL
#define IFDEF_PCP_EXPERIMENTAL(...) __VA_ARGS__
#else
#define IFDEF_PCP_EXPERIMENTAL(...)
#endif

#define IFDEF(A, B) IFDEF_##A(B)

#define TABS0
#define TABS1 "\t"
#define TABS2 "\t\t"
#define TABS3 "\t\t\t"
#define TABS4 "\t\t\t\t"

#define FOREACH_OPTION(OPTION, HELP_MSG, REQARG, NOARG) \
 HELP_MSG("Usage:") \
 OPTION(SHORT(h), help,       "help",                 NOARG,  TABS2 "Display this help.") \
 OPTION(SHORT(s), server,     "server",               REQARG, TABS2 "Address[:port] of the PCP server.") \
 OPTION(SHORT(v), version,    "pcp-version",          REQARG, TABS1 "Maximal PCP version to be used.") \
 OPTION(SHORT(d), disdisc,    "disable-autodiscovery",NOARG,  TABS0 "Disable auto-discovery of PCP servers.") \
 OPTION(SHORT(T), timeout,    "timeout",              REQARG, TABS2 "Receive response timeout in seconds.(DEFAULT 1)") \
 OPTION(SHORT(f), fast_ret,   "fast-return",          NOARG,  TABS1 "Exit immediately after first PCP response.") \
 HELP_MSG(                                                          "") \
 HELP_MSG(                                                          "MAP/PEER operation related options:") \
 OPTION(SHORT(i), int,        "internal",             REQARG, TABS1 "Internal address[:port] of the flow. Adding this\n" \
                                                              TABS4 "option will cause sending a MAP operation.") \
 OPTION(SHORT(p), peer,       "peer",                 REQARG, TABS2 "Peer address[:port] of the flow. Adding this\n" \
                                                              TABS4 "option will cause sending a PEER operation.") \
 OPTION(SHORT(e), ext,        "external",             REQARG, TABS1 "Suggested external address[:port] of the flow.") \
 OPTION(SHORT(t), tcp,        "tcp",                  NOARG , TABS2 "Set flow's protocol to TCP.") \
 OPTION(SHORT(u), udp,        "udp",                  NOARG , TABS2 "Set flow's protocol to UDP.") \
 OPTION(SHORT(n), prot,       "protocol",             REQARG, TABS1 "Set flow's protocol by protocol number.") \
 OPTION(SHORT(l), lifetime,   "lifetime",             REQARG, TABS1 "Set flow's lifetime(in seconds).") \
 HELP_MSG(                                                          "") \
 HELP_MSG(                                                          "PCP options for MAP/PEER operation:") \
 IFDEF(PCP_FLOW_PRIORITY, \
   HELP_MSG(                                                          "") \
   HELP_MSG(                                                          " Flow priority option:") \
   OPTION(SHORT(U), dscp_up,    "dscp-up",              REQARG, TABS2 "Flow priority DSCP UP value.") \
   OPTION(SHORT(D), dscp_down,  "dscp-down",            REQARG, TABS1 "Flow priority DSCP DOWN value.") )\
 HELP_MSG(                                                          "") \
 HELP_MSG(                                                          " Prefer failure option:") \
 OPTION(SHORT(P), pfailure,    "prefer-failure",      NOARG, TABS1 "Add prefer failure option to MAP opcode")\
 HELP_MSG(                                                          "") \
 HELP_MSG(                                                          " Filter option:") \
 OPTION(SHORT(F), filter,      "filter",              REQARG, TABS2 "Add filter option to MAP opcode\n"\
                                                              TABS4 "Permitted remote peer addresses MUST \n" \
                                                              TABS4 "have following format [ip_address/prefix]:port") \
 HELP_MSG(                                                          "") \
 HELP_MSG(                                                          " Third party option:") \
 OPTION(SHORT(3), thirdparty,   "third-party",         REQARG, TABS2 "Third-party IP address to send on behalf of.") \
 HELP_MSG(                                                          "") \
 IFDEF(PCP_EXPERIMENTAL, \
   HELP_MSG(                                                          " Metadata option:") \
   HELP_MSG(                                                    TABS4 "It's possible to add several metadata options by") \
   HELP_MSG(                                                    TABS4 "adding the following options several times.")\
   OPTION(SHORT(I), md_id,      "metadata-id",          REQARG, TABS1 "Metadata ID.")\
   OPTION(SHORT(V), md_val,     "metadata-value",       REQARG, TABS1 "Metadata value.")\
   HELP_MSG(                                                          "") \
   HELP_MSG(                                                          " Other MAP/PEER options:") \
   OPTION(NOSHORT,  dev_id,     "device-id",            NOARG , TABS1 "System Dependent. Usually Sysname + Machine Name")\
   OPTION(NOSHORT,  location,   "location",             NOARG , TABS1 "Latitude and Longitude.")\
   OPTION(NOSHORT,  user_id,    "user-id",              REQARG, TABS2 "user@domain")\
   HELP_MSG(                                                          "") )\
 IFDEF(PCP_SADSCP, \
   HELP_MSG(                                                          "Learn DSCP(SADSCP operation):") \
   HELP_MSG(                                                    TABS4 "Get DSCP value from PCP server for") \
   HELP_MSG(                                                    TABS4 "jitter/loss/delay tolerances and/or app name.") \
   OPTION(SHORT(J), sadscp_jit, "jitter-tolerance",     REQARG,       "Jitter tolerance (DEFAULT:3).")\
   OPTION(SHORT(L), sadscp_los, "loss-tolerance",       REQARG, TABS1 "Loss tolerance (DEFAULT:3).")\
   OPTION(SHORT(E), sadscp_del, "delay-tolerance",      REQARG, TABS1 "Delay tolerance (DEFAULT:3).")\
   OPTION(SHORT(A), sadscp_app, "app-name",             REQARG, TABS1 "Application name.") )\


#define STRUCT_OPTION(a, b, c, d, e) {c, d, 0, 0},
#define ARG_IGNORE(a)
#define STRUCT_REQARG required_argument
#define STRUCT_NOARG no_argument


static struct option long_options[] = {
        FOREACH_OPTION(STRUCT_OPTION, ARG_IGNORE, STRUCT_REQARG, STRUCT_NOARG)
        {0, 0, 0, 0}
};

#undef NOSHORT
#define NOSHORT "  "

#define HELP_STRUCT_OPT(a, b, c, d, e) "   "a " --" c "\t" e "\n"
#define HELP_HELP_MSG(a) a "\n"

const char usage_string[] = FOREACH_OPTION(HELP_STRUCT_OPT, HELP_HELP_MSG, STRUCT_REQARG, STRUCT_NOARG);

#undef NOSHORT
#undef SHORT
#define NOSHORT ""
#define SHORT(a) #a
#define SHORT_OPT(a, b, c, d, e) a d
#define SHORT_OPT_REQARG ":"
#define SHORT_OPT_NOARG

const char short_opts_string[] = FOREACH_OPTION(SHORT_OPT, ARG_IGNORE, SHORT_OPT_REQARG, SHORT_OPT_NOARG);

#define ENUM_OPTION(a, b, c, d, e) E_##b,

typedef enum {
     FOREACH_OPTION(ENUM_OPTION, ARG_IGNORE, SHORT_OPT_REQARG, SHORT_OPT_NOARG)
} e_options;

static void print_usage(void){
    printf(usage_string);
}

static const char* decode_fresult(pcp_fstate_e s)
{
    switch (s) {
    case pcp_state_short_lifetime_error:
        return "slerr";
    case pcp_state_succeeded:
        return "succ";
    case pcp_state_failed:
        return "fail";
    default:
        return "proc";
    }
}

static int check_flow_info(pcp_flow_t* f)
{
    size_t cnt=0;
    pcp_flow_info_t *info_buf = NULL;
    pcp_flow_info_t *ret = pcp_flow_get_info(f,&cnt);
    int ret_val = 2;
    info_buf=ret;
    for (; cnt>0; cnt--, ret++) {
        switch(ret->result)
        {
        case pcp_state_succeeded:
            printf("\nFlow signaling succeeded.\n");
            ret_val = 0;
            break;
        case pcp_state_short_lifetime_error:
            printf("\nFlow signaling failed. Short lifetime error received.\n");
            ret_val = 3;
            break;
        case pcp_state_failed:
            printf("\nFlow signaling failed.\n");
            ret_val = 4;
            break;
        default:
            continue;
        }
        break;
    }

    if (info_buf) {
        free(info_buf);
    }

    return ret_val;
}

static void print_ext_addr(pcp_flow_t* f)
{
    size_t cnt=0;
    pcp_flow_info_t *info_buf = NULL;
    pcp_flow_info_t *ret = pcp_flow_get_info(f,&cnt);
    info_buf=ret;

    printf("%-20s %-4s %-20s %5s   %-20s %5s   %-20s %5s %3s %5s %s\n",
            "PCP Server IP",
            "Prot",
            "Int. IP", "port",
            "Dst. IP", "port",
            "Ext. IP", "port",
            "Res", "State","Ends");
    for (; cnt>0; cnt--, ret++) {
        char ntop_buffs[4][INET6_ADDRSTRLEN];
        char timebuf[32];

        printf("%-20s %-4s %-20s %5hu   %-20s %5hu   %-20s %5hu %3d %5s %s",
                inet_ntop(AF_INET6, &ret->pcp_server_ip, ntop_buffs[0],
                    sizeof(ntop_buffs[0])),
                ret->protocol == IPPROTO_TCP ? "TCP" : (
                   ret->protocol == IPPROTO_UDP ? "UDP" : "UNK"),
                inet_ntop(AF_INET6, &ret->int_ip, ntop_buffs[1],
                    sizeof(ntop_buffs[1])),
                ntohs(ret->int_port),
                inet_ntop(AF_INET6, &ret->dst_ip, ntop_buffs[2],
                    sizeof(ntop_buffs[2])),
                ntohs(ret->dst_port),
                inet_ntop(AF_INET6, &ret->ext_ip, ntop_buffs[3],
                    sizeof(ntop_buffs[3])),
                ntohs(ret->ext_port),
                ret->pcp_result_code,
                decode_fresult(ret->result),
                ret->recv_lifetime_end == 0 ? " -\n" :
                        ctime_r(&ret->recv_lifetime_end, timebuf));
    }
    if (info_buf) {
        free(info_buf);
    }
}

static void print_get_dscp(pcp_flow_t* f)
{
    size_t cnt=0;
     pcp_flow_info_t *info_buf = NULL;
     pcp_flow_info_t *ret = pcp_flow_get_info(f, &cnt);
     info_buf=ret;

     printf("%-20s %5s %3s %5s %s\n",
             "Int. IP", "DSCP",
             "Res", "State","Ends");
     for (; cnt>0; cnt--, ret++) {
         char ntop_buff[INET6_ADDRSTRLEN];
         char timebuf[32];
         printf("%-20s %5d %3d %5s %s",
                 inet_ntop(AF_INET6, &ret->int_ip, ntop_buff,
                     sizeof(ntop_buff)),
                 (int)ret->learned_dscp,
                 (int)ret->pcp_result_code,
                 decode_fresult(ret->result),
                 ret->recv_lifetime_end == 0 ? " -\n" :
                         ctime_r(&ret->recv_lifetime_end, timebuf)); //LCOV_EXCL_LINE
     }
     if (info_buf) {
         free(info_buf);
     }
}

struct pcp_server_list {
        char *server;
        struct pcp_server_list *next;
        int version;
};

// CLI options
struct pcp_params {
    char *hostname;
    char *int_addr;
    char *ext_addr;
    char *peer_addr;
    char *filter_addr;
    char *opt_thirdparty_addr;
    uint8_t opt_pfailure;
    uint8_t opt_filter;
    int opt_filter_prefix;
    uint8_t opt_dscp_up;
    uint8_t opt_dscp_down;
    uint8_t opt_flowp;
    uint8_t dis_auto_discovery;
    uint8_t opt_protocol;
    uint32_t opt_lifetime;
    uint8_t pcp_version;
    uint8_t fast_return;
    uint32_t timeout;
    uint32_t opt_mdid;
    char opt_md_val[256];
    size_t opt_md_val_len;
    uint32_t is_md;
    uint8_t has_sadscp_data;
    uint8_t delay_tolerance;
    uint8_t loss_tolerance;
    uint8_t jitter_tolerance;
    char* app_name;

    pcp_app_deviceid_t app_deviceid;
    pcp_app_userid_t app_userid;
    pcp_app_location_t app_location;

    uint8_t has_mappeer_data;
    pcp_ctx_t *ctx;

    struct pcp_server_list *pcp_servers;
};

static void parse_params(struct pcp_params *p, int argc, char *argv[]);


static inline unsigned long mix(unsigned long a, unsigned long b, unsigned long c)
{
     a=a-b;  a=a-c;  a=a^(c >> 13);
     b=b-c;  b=b-a;  b=b^(a << 8);
     c=c-a;  c=c-b;  c=c^(b >> 13);
     a=a-b;  a=a-c;  a=a^(c >> 12);
     b=b-c;  b=b-a;  b=b^(a << 16);
     c=c-a;  c=c-b;  c=c^(b >> 5);
     a=a-b;  a=a-c;  a=a^(c >> 3);
     b=b-c;  b=b-a;  b=b^(a << 10);
     c=c-a;  c=c-b;  c=c^(b >> 15);

     return c;
}

int main(int argc, char *argv[])
{
    struct pcp_params p;
    struct sockaddr_storage destination_ip;
    struct sockaddr_storage source_ip;
    struct sockaddr_storage ext_ip;
    struct sockaddr_storage filter_ip;
    struct sockaddr_storage thirdparty_ip;
    int ret_val = 1;
    pcp_flow_t* flow = NULL;
    struct pcp_server_list *server;

    srand(mix(clock(), (unsigned long)time(NULL), getpid()));

    PD_SOCKET_STARTUP();

    memset(&source_ip, 0, sizeof(source_ip));
    memset(&destination_ip, 0, sizeof(destination_ip));

    parse_params(&p, argc, argv);

    if ((p.int_addr)&&(0!=sock_pton(p.int_addr, (struct sockaddr*)&source_ip))) {
        fprintf(stderr, "Entered invalid internal address!\n");
        exit(1);
    }

    if ((p.peer_addr)&&(0!=sock_pton(p.peer_addr, (struct sockaddr*)&destination_ip))) {
        fprintf(stderr, "Entered invalid peer address!\n");
        exit(1);
    }
    if ((p.ext_addr)&&(0!=sock_pton(p.ext_addr, (struct sockaddr*)&ext_ip))) {
        fprintf(stderr, "Entered invalid external address!\n");
        exit(1);
    }

    if ((p.peer_addr) && (p.opt_pfailure)){
        fprintf(stderr,
                "Prefer failure option can be added only to MAP message!\n");
        exit(1);
    }

    if ((p.peer_addr) && (p.opt_filter)){
        fprintf(stderr,
                "Filter option can be added only to MAP message!\n");
        exit(1);
    }

    if ((p.opt_filter)&&(p.filter_addr)&&
            (0!=sock_pton_with_prefix(p.filter_addr,
                    (struct sockaddr*)&filter_ip, &p.opt_filter_prefix))) {
        fprintf(stderr,
                "Invalid address for Filter option!\n");
        exit(1);

    }
    if ((p.opt_thirdparty_addr)&&(0!=sock_pton(p.opt_thirdparty_addr, (struct sockaddr*)&thirdparty_ip))) {
        fprintf(stderr, "Entered invalid third-party address!\n");
        exit(1);
    }

    if (!p.dis_auto_discovery) {
        p.ctx = pcp_init(ENABLE_AUTODISCOVERY, NULL);
    } else {
        p.ctx = pcp_init(DISABLE_AUTODISCOVERY, NULL);
    }

    for (server = p.pcp_servers; server!=NULL; server=server->next) {
        struct sockaddr *sa;

        sa=Sock_pton(server->server);
        if (sa) {
            pcp_add_server(p.ctx, sa, server->version);
        }
    }

    if (p.has_mappeer_data) {
        flow = pcp_new_flow(p.ctx, (struct sockaddr*)&source_ip,
                (struct sockaddr*)&destination_ip,
                p.ext_addr ? (struct sockaddr*)&ext_ip : NULL,
                p.opt_protocol, p.opt_lifetime, p.ctx);

        if (flow == NULL) {
            fprintf(stderr, "%s:%d Could not create flow \n", __FUNCTION__, __LINE__);
            exit(1);
        }

#ifdef PCP_FLOW_PRIORITY
        if (p.opt_flowp) {
            pcp_flow_set_flowp(flow, p.opt_dscp_up, p.opt_dscp_down);
        }
#endif

#ifdef PCP_EXPERIMENTAL
        if ( p.is_md ) {
            pcp_flow_add_md(flow, p.opt_mdid, p.opt_md_val, p.opt_md_val_len);
        }


        if (p.app_deviceid.deviceid[0] != '\0') {
            pcp_flow_set_deviceid(flow, &p.app_deviceid);
        }

        if (p.app_location.location[0] != '\0') {
            pcp_flow_set_location(flow, &p.app_location);
        }

        if (p.app_userid.userid[0] != '\0') {
            pcp_flow_set_userid(flow, &p.app_userid);
        }
#endif

        if (p.opt_filter) {
            pcp_flow_set_filter_opt(flow, (struct sockaddr*)&filter_ip,
                (uint8_t)p.opt_filter_prefix);
        }

        if (p.opt_pfailure) {
            pcp_flow_set_prefer_failure_opt(flow);
        }

        if (p.opt_thirdparty_addr) {
            pcp_flow_set_3rd_party_opt(flow, (struct sockaddr *)&thirdparty_ip);
        }

#ifdef PCP_SADSCP
    } else if (p.has_sadscp_data) {
        flow = pcp_learn_dscp(p.ctx, p.delay_tolerance, p.loss_tolerance,
                p.jitter_tolerance, p.app_name);
#endif
    }
    switch (pcp_wait(flow, p.timeout, p.fast_return)) {
    case pcp_state_processing:
        printf("\nFlow signaling timed out.\n");
        ret_val = 2;
        break;
    case pcp_state_succeeded:
        printf("\nFlow signaling succeeded.\n");
        ret_val = 0;
        break;
    case pcp_state_short_lifetime_error:
        printf("\nFlow signaling failed. Short lifetime error received.\n");
        ret_val = 3;
        break;
    case pcp_state_failed:
        printf("\nFlow signaling failed.\n");
        ret_val = 4;
        break;
    default:
        ret_val = check_flow_info(flow);
        break;
    }

    if (p.has_mappeer_data) {
        print_ext_addr(flow);
    }
    if (p.has_sadscp_data) {
        print_get_dscp(flow);
    }
    printf("\n");
    pcp_terminate(p.ctx, 0);

    PD_SOCKET_CLEANUP();
    return ret_val;
}

static inline void parse_opt_help(struct pcp_params *p UNUSED)
{
    print_usage();
    exit(0);
}

static inline void parse_opt_fast_ret(struct pcp_params *p)
{
    p->fast_return=1;
}

static inline void parse_opt_server(struct pcp_params *p)
{
    struct pcp_server_list* l ;
    l = (struct pcp_server_list*)malloc(sizeof(*l));
    if (l) {
        l->server = optarg;
        l->version = p->pcp_version;
        l->next = p->pcp_servers;
        p->pcp_servers = l;
     }
}

static inline void parse_opt_version(struct pcp_params *p)
{
    p->pcp_version = (uint8_t)atoi(optarg);
    //check if the version entered is supported or not
    if (p->pcp_version > PCP_MAX_SUPPORTED_VERSION) {
        fprintf(stderr, "Unsupported version %d \n", p->pcp_version);
        exit(1);
    }
}

static inline void parse_opt_disdisc(struct pcp_params *p)
{
    p->dis_auto_discovery=1;
}

static inline void parse_opt_timeout(struct pcp_params *p)
{
    p->timeout = atoi(optarg) * 1000;
}

static inline void parse_opt_int(struct pcp_params *p)
{
    p->int_addr=optarg;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_peer(struct pcp_params *p)
{
    p->peer_addr=optarg;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_ext(struct pcp_params *p)
{
    p->ext_addr=optarg;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_tcp(struct pcp_params *p)
{
    p->opt_protocol=IPPROTO_TCP;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_udp(struct pcp_params *p)
{
    p->opt_protocol=IPPROTO_UDP;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_prot(struct pcp_params *p)
{
    p->opt_protocol=atoi(optarg);
    p->has_mappeer_data = 1;
}

static inline void parse_opt_lifetime(struct pcp_params *p)
{
    p->opt_lifetime = (uint32_t)atoi(optarg);
}

#ifdef PCP_FLOW_PRIORITY
static inline void parse_opt_dscp_up(struct pcp_params *p)
{
    p->opt_dscp_up=(uint8_t)atoi(optarg);
    p->opt_flowp = 1;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_dscp_down(struct pcp_params *p)
{
    p->opt_dscp_down=(uint8_t)atoi(optarg);
    p->opt_flowp = 1;
    p->has_mappeer_data = 1;
}
#endif

#ifdef PCP_EXPERIMENTAL
static inline void parse_opt_md_id(struct pcp_params *p)
{
    p->opt_mdid = (uint32_t)atoi(optarg);
    p->is_md = 1;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_md_val(struct pcp_params *p)
{
    p->opt_md_val_len = (optarg==NULL)?0:strlen(optarg);
    if (p->opt_md_val_len > sizeof(p->opt_md_val)) {
        p->opt_md_val_len = sizeof(p->opt_md_val);
    }
    memcpy(p->opt_md_val, optarg, p->opt_md_val_len);
    p->is_md = 1;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_dev_id(struct pcp_params *p)
{
#ifndef WIN32
    struct utsname sysinfo;
    int sysname_len, dev_len, machine_len;

    /* uname is not support on 4.3BSD and earlier */
    uname (&sysinfo);
    sysname_len = strlen((const char *)&sysinfo.sysname);
    dev_len = MAX_DEVICE_ID - sysname_len - 1;
    if (dev_len>0) {
        strcpy(&(p->app_deviceid.deviceid[0]), (const char *) &sysinfo.sysname);
        strcpy(p->app_deviceid.deviceid + sysname_len, " ");

        machine_len = strlen((const char *)&sysinfo.machine);
        if (dev_len - machine_len > 0) {
            strcpy(p->app_deviceid.deviceid+sysname_len+1,
                    (const char *) &sysinfo.machine);
        }
    }
    fprintf(stderr, "Device-id: %s \n", &(p->app_deviceid.deviceid[0]));

    /* BSD 4.3 version
    int len = 0, size = 0;
    sysctlbyname("hw.machine", NULL, &size, NULL, 0);

    if (len) {
     char *machine = malloc(len*sizeof(char));
     sysctlbyname("hw.machine", machine, &size, NULL, 0);
     fprintf (stderr, "Machine: %s \n", machine);
     free(machine);
     exit(0);
    }
    *
    */
#endif //WIN32
}

static inline void parse_opt_location(struct pcp_params *p)
{
 /*
  *    Latitude and Longitude in Degrees:
    ±DD.DDDD±DDD.DDDD/         (eg +12.345-098.765/)
  */

            /* In the future should use an API to get lat and long */

    memcpy(&(p->app_location.location), "37.4111, -121.9262",
                min(MAX_GEO_STR, strlen("37.4111, -121.9262")));
    fprintf(stderr, "Location: %s \n", &(p->app_location.location[0]));
}

static inline void parse_opt_user_id(struct pcp_params *p)
{
    strncpy(p->app_userid.userid, optarg, sizeof(p->app_userid.userid));
    fprintf(stderr, "Userid: %s \n", &(p->app_userid.userid[0]));
}

#endif
static inline void parse_opt_filter(struct pcp_params *p)
{
    p->filter_addr=optarg;
    p->opt_filter = 1;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_pfailure(struct pcp_params *p)
{
    p->opt_pfailure = 1;
    p->has_mappeer_data = 1;
}

static inline void parse_opt_thirdparty(struct pcp_params *p)
{
    p->opt_thirdparty_addr=optarg;
    p->has_mappeer_data = 1;
}



#ifdef PCP_SADSCP
static inline void parse_opt_sadscp_jit(struct pcp_params *p)
{
    p->jitter_tolerance=(uint8_t)atoi(optarg);
    p->has_sadscp_data=1;
}

static inline void parse_opt_sadscp_los(struct pcp_params *p)
{
    p->loss_tolerance=(uint8_t)atoi(optarg);
    p->has_sadscp_data=1;
}

static inline void parse_opt_sadscp_del(struct pcp_params *p)
{
    p->delay_tolerance=(uint8_t)atoi(optarg);
    p->has_sadscp_data=1;
}

static inline void parse_opt_sadscp_app(struct pcp_params *p)
{
    p->app_name=optarg;
    p->has_sadscp_data=1;
}
#endif

#define PARSE_OPTION(A, B, C, D, E) if (((c==0) && (option_index==E_##B))||(c==A[0]&&(c!=0))) {\
    parse_opt_##B(p);\
} else

static void parse_params(struct pcp_params *p, int argc, char *argv[])
{
    int c;
    int option_index = 0;

    memset(p, 0, sizeof(*p));

    p->opt_protocol = 6;
    p->opt_lifetime = 900;
    p->pcp_version = PCP_MAX_SUPPORTED_VERSION;
    p->timeout = 1000;
    pcp_log_level = PCP_LOGLVL_DEBUG;

    p->delay_tolerance=3;
    p->loss_tolerance=3;
    p->jitter_tolerance=3;



    if ( argc == 1 ){
        print_usage();
        exit(1);
    }

    opterr = 0;
    while ((c = getopt_long(argc, argv, short_opts_string, long_options,
            &option_index)) != -1)
    {

        FOREACH_OPTION(PARSE_OPTION, ARG_IGNORE, ARG_IGNORE, ARG_IGNORE)
        { // default action
            if (c=='?') {
                if (optopt != 0) {
                    if (isprint (optopt))
                        fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                    else
                        fprintf (stderr,"Unknown option character `\\x%x'.\n",
                           optopt);
                } else {
                    fprintf (stderr,"Unknown option `%s'.\n",argv[optind-1]);
                }
                print_usage();
                exit(1);
            } else {  //LCOV_EXCL_START
                print_usage();
                exit(1);
            } //LCOV_EXCL_STOP
        }
    }
    if ((p->has_mappeer_data)&&(p->has_sadscp_data)) {
        fprintf (stderr,
            "Cannot mix parameters for SADSCP operation and MAP/PEER op.\n");
        print_usage();
        exit(1);
    }
    if ((!p->has_mappeer_data)&&(!p->has_sadscp_data)) {
        fprintf (stderr,
            "give at least one parameter for MAP/PEER op. or for SADSCP op\n");
        print_usage();
        exit(1);
    }
 }
