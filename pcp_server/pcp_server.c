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
 *
 * Author: bagljas
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#include "default_config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include "getopt.h"
#include <unistd.h>
#include "pcp_socket.h"

#ifdef WIN32
#include <winsock2.h>
#include "pcp_gettimeofday.h"
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
/* "PI" for Platform Independent*/
#define PI_TIMEOUT_STRUCT DWORD
#define SET_PI_TIMEOUT(dest, source) do {\
    dest =  (source).tv_sec * 1000;\
    dest += (source).tv_usec / 1000;\
} while(0)

#else //WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PI_TIMEOUT_STRUCT struct timeval
#define SET_PI_TIMEOUT(dest, source) do {\
    dest.tv_sec = (source).tv_sec; \
    dest.tv_usec = (source).tv_usec;\
    } while (0)
#endif //WIN32

#include "pcp_msg_structs.h"
#include "pcp_utils.h"
#include "pcp.h"

#define PCP_PORT "5351"
#define PCP_TEST_MAX_VERSION 2

#define MAX_LOG_FILE 64u

typedef struct options_occur {
    int third_party_occur;
    int pfailure_occur;
} options_occur_t;

typedef struct server_info {
    uint8_t server_version;
    uint8_t end_after_recv;
    uint8_t default_result_code;
    struct in6_addr ext_ip;
    uint8_t app_bit;
    uint8_t ret_dscp;
    char log_file[MAX_LOG_FILE];
    struct timeval tv;
    time_t epoch_time_start;
} server_info_t;

static void reset_option_occur(options_occur_t *opt_occ)
{

    opt_occ->third_party_occur = 0;
    opt_occ->pfailure_occur = 0;

}

// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*) sa)->sin6_addr);
}

static PCP_SOCKET createPCPsocket(const char* serverPort,
    const char* serverAddress)
{
    PCP_SOCKET sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int yes = 1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(serverAddress, serverPort, &hints, &servinfo)) != 0) {//LCOV_EXCL_START
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
        // LCOV_EXCL_STOP
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol))
                == PCP_INVALID_SOCKET) {//LCOV_EXCL_START
            perror("PCP server: socket");
            continue;
            //LCOV_EXCL_STOP
        }

        // lose the pesky "address already in use" error message
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == PCP_SOCKET_ERROR) { //LCOV_EXCL_START
            CLOSE(sockfd);
            perror("PCP server: bind");
            continue;
            //LCOV_EXCL_STOP
        }

        break;
    }

    if (p == NULL) {//LCOV_EXCL_START
        fprintf(stderr, "PCP server: failed to bind socket\n");
        exit(2);
    }//LCOV_EXCL_STOP

    freeaddrinfo(servinfo);

    return sockfd;
}

static void print_MAP_opcode_ver1(pcp_map_v1_t *map_buf, FILE *log_file)
{
    char map_addr[INET6_ADDRSTRLEN];
    DUPPRINT(log_file,"PCP protocol VERSION 1. \n");
    DUPPRINT(log_file,"MAP protocol: \t\t %d\n", map_buf->protocol);
    DUPPRINT(log_file,"MAP int port: \t\t %d\n", ntohs(map_buf->int_port));
    DUPPRINT(log_file,"MAP ext port: \t\t %d\n", ntohs(map_buf->ext_port));
    DUPPRINT(log_file,"MAP Ext IP: \t\t %s\n",
            inet_ntop(AF_INET6, map_buf->ext_ip, map_addr, INET6_ADDRSTRLEN));
}

static void print_MAP_opcode_ver2(pcp_map_v2_t *map_buf, FILE *log_file)
{
    char map_addr[INET6_ADDRSTRLEN];
    DUPPRINT(log_file,"PCP protocol VERSION 2. \n");
    DUPPRINT(log_file,"MAP protocol: \t\t %d\n", map_buf->protocol);
    DUPPRINT(log_file,"MAP int port: \t\t %d\n", ntohs(map_buf->int_port));
    DUPPRINT(log_file,"MAP ext port: \t\t %d\n", ntohs(map_buf->ext_port));
    DUPPRINT(log_file,"MAP Ext IP: \t\t %s\n",
            inet_ntop(AF_INET6, map_buf->ext_ip, map_addr, INET6_ADDRSTRLEN));
}

static void print_PEER_opcode_ver1(pcp_peer_v1_t *peer_buf, FILE *log_file)
{

    char ext_addr[INET6_ADDRSTRLEN];
    char peer_addr[INET6_ADDRSTRLEN];
    DUPPRINT(log_file,"PCP protocol VERSION 1. \n");
    DUPPRINT(log_file,"PEER Opcode specific information. \n");
    DUPPRINT(log_file,"Protocol: \t\t %d\n", peer_buf->protocol);
    DUPPRINT(log_file,"Internal port: \t\t %d\n", ntohs(peer_buf->int_port));
    DUPPRINT(log_file,"External IP: \t\t %s\n",
            inet_ntop(AF_INET6, peer_buf->ext_ip, ext_addr, INET6_ADDRSTRLEN));
    DUPPRINT(log_file,"External port port: \t\t %d\n", ntohs(peer_buf->ext_port));
    DUPPRINT(log_file,"PEER IP: \t\t %s\n",
            inet_ntop(AF_INET6, peer_buf->peer_ip, peer_addr,
                    INET6_ADDRSTRLEN));
    DUPPRINT(log_file,"PEER port port: \t\t %d\n", ntohs(peer_buf->peer_port));

}

static void print_PEER_opcode_ver2(pcp_peer_v2_t *peer_buf, FILE *log_file)
{

    char ext_addr[INET6_ADDRSTRLEN];
    char peer_addr[INET6_ADDRSTRLEN];


    DUPPRINT(log_file, "PCP protocol VERSION 2. \n");
    DUPPRINT(log_file, "PEER Opcode specific information. \n");
    DUPPRINT(log_file,"Protocol: \t\t %d\n", peer_buf->protocol);
    DUPPRINT(log_file,"Internal port: \t\t %d\n", ntohs(peer_buf->int_port));
    DUPPRINT(log_file,"External IP: \t\t %s\n",
            inet_ntop(AF_INET6, peer_buf->ext_ip, ext_addr, INET6_ADDRSTRLEN));
    DUPPRINT(log_file,"External port: \t\t %d\n", ntohs(peer_buf->ext_port));
    if (IN6_IS_ADDR_V4MAPPED((struct in6_addr*) peer_buf->peer_ip)) {
        DUPPRINT(log_file,"PEER IP: \t\t %s\n",
            inet_ntop(AF_INET, &(peer_buf->peer_ip[3]), peer_addr,
                    INET6_ADDRSTRLEN));
    } else {
        DUPPRINT(log_file,"PEER IP: \t\t %s\n",
            inet_ntop(AF_INET6, &(peer_buf->peer_ip[0]), peer_addr,
                    INET6_ADDRSTRLEN));
    }
    DUPPRINT(log_file,"PEER port: \t\t %d\n", ntohs(peer_buf->peer_port));

}

static int print_SADSCP_opcode_ver2(pcp_sadscp_req_t *sadscp_buf,
    FILE *log_file)
{

    DUPPRINT(log_file,"PCP protocol VERSION 2. \n");
    DUPPRINT(log_file,"SADSCP Opcode specific information. \n");
    DUPPRINT(log_file,"Jitter tolerance:\t %d\n",
            (sadscp_buf->tolerance_fields >> 2) & 3);
    DUPPRINT(log_file,"Loss tolerance: \t %d\n",
            (sadscp_buf->tolerance_fields >> 4) & 3);
    DUPPRINT(log_file,"Delay tolerance:  \t %d\n",
            (sadscp_buf->tolerance_fields >> 6) & 3);
    DUPPRINT(log_file,"App name length: \t %d\n", sadscp_buf->app_name_length);
    DUPPRINT(log_file,"App name:        \t %.*s\n",
            sadscp_buf->app_name_length, sadscp_buf->app_name);
    return sizeof(pcp_sadscp_req_t) + sadscp_buf->app_name_length;
}

static int print_PCP_options(void* pcp_buf, int* remainingSize,
    int* processedSize, options_occur_t *opt_occ, FILE *log_file)
{

    int remain = *remainingSize;
    int processed = *processedSize;
    int pcp_result_code = PCP_RES_SUCCESS;
    char third_addr[INET6_ADDRSTRLEN];
    char filter_addr[INET6_ADDRSTRLEN];
    pcp_request_t* common_req = (pcp_request_t*) pcp_buf;
    uint8_t opcode = common_req->r_opcode & 0x7F;
    uint8_t* pcp_buf_helper = (uint8_t*)pcp_buf;
    char metadata[PCP_MAX_LEN]; //used in metadata option

    pcp_options_hdr_t* opt_hdr = (pcp_options_hdr_t*)(pcp_buf_helper+processed);

    pcp_3rd_party_option_t* opt_3rd;
    pcp_flow_priority_option_t* opt_flp;
    pcp_metadata_option_t* opt_md;
    pcp_filter_option_t* opt_filter;
    pcp_location_option_t* opt_loc;
    pcp_userid_option_t* opt_user;
    pcp_deviceid_option_t* opt_dev;


    switch (opt_hdr->code) {

    case PCP_OPTION_3RD_PARTY:
        if (opt_occ->third_party_occur != 0) {
            DUPPRINT(log_file, "THIRD PARTY OPTION was already present. \n");
            pcp_result_code = PCP_RES_MALFORMED_OPTION;
        } else {
            opt_occ->third_party_occur = 1;
        }
        DUPPRINT(log_file, "\n");
        DUPPRINT(log_file, "OPTION: \t Third party \n");
        opt_3rd = (pcp_3rd_party_option_t*) (pcp_buf_helper
                + processed);
        DUPPRINT(log_file, "Third PARTY IP: \t %s\n",
                inet_ntop(AF_INET6, opt_3rd->ip, third_addr, INET6_ADDRSTRLEN));

        processed += sizeof(pcp_3rd_party_option_t);
        remain -= sizeof(pcp_3rd_party_option_t);
        break;

    case PCP_OPTION_PREF_FAIL:
        if (opcode != PCP_OPCODE_MAP) {
            DUPPRINT(log_file, "Unsupported OPTION for given OPCODE.\n");
            pcp_result_code = PCP_RES_MALFORMED_REQUEST;
        }
        if (opt_occ->pfailure_occur != 0) {
            DUPPRINT(log_file, "PREFER FAILURE OPTION was already present. \n");
            pcp_result_code = PCP_RES_MALFORMED_OPTION;
        } else {
            opt_occ->pfailure_occur = 1;
        }
        DUPPRINT(log_file, "\n");
        DUPPRINT(log_file, "OPTION: \t Prefer fail \n");
        processed += sizeof(pcp_prefer_fail_option_t);
        remain -= sizeof(pcp_prefer_fail_option_t);
        break;

    case PCP_OPTION_FILTER:

        if (opcode != PCP_OPCODE_MAP) {
            DUPPRINT(log_file, "Unsupported OPTION for given OPCODE.\n");
            pcp_result_code = PCP_RES_MALFORMED_REQUEST;
        }
        opt_filter =
                (pcp_filter_option_t*) (pcp_buf_helper+processed);
        DUPPRINT(log_file, "\n");
        DUPPRINT(log_file, "OPTION: \t Filter \n");
        DUPPRINT(log_file, "\t PREFIX: %d \n", opt_filter->filter_prefix);
        DUPPRINT(log_file, "\t FILTER PORT: %d \n",
                                htons(opt_filter->filter_peer_port));
        DUPPRINT(log_file, "\t FILTER IP: %s \n",
                inet_ntop(AF_INET6, &opt_filter->filter_peer_ip,
                        filter_addr, sizeof(filter_addr)));
        processed += sizeof(pcp_filter_option_t);
        remain -= sizeof(pcp_filter_option_t);
        break;

    case PCP_OPTION_FLOW_PRIORITY:
        DUPPRINT(log_file, "\n");
        DUPPRINT(log_file, "OPTION: \t Flow priority\n");
        opt_flp =
                (pcp_flow_priority_option_t*) (pcp_buf_helper + processed);
        DUPPRINT(log_file, "\t DSCP UP: \t %d \n", opt_flp->dscp_up);
        DUPPRINT(log_file, "\t DSCP DOWN: \t %d \n", opt_flp->dscp_down);

        processed += sizeof(pcp_flow_priority_option_t);
        remain -= sizeof(pcp_flow_priority_option_t);
        break;

    case PCP_OPTION_METADATA:
        DUPPRINT(log_file, "\n");
        DUPPRINT(log_file, "OPTION: \t Metadata \n");
        opt_md = (pcp_metadata_option_t*) (pcp_buf_helper
                + processed);

        DUPPRINT(log_file, "\t METADATA ID \t %lu \n", (unsigned long) htonl(opt_md->metadata_id));
        memcpy(metadata, opt_md->metadata, htons(opt_md->len));
        DUPPRINT(log_file, "\t METADATA \t %s \n", metadata);

        processed += sizeof(pcp_options_hdr_t) + htons(opt_md->len);
        remain -= (sizeof(pcp_options_hdr_t) + htons(opt_md->len));
        break;

    case PCP_OPTION_LOCATION:
        DUPPRINT(log_file, "\n");
        DUPPRINT(log_file, "OPTION: \t Location \n");
        opt_loc =
                (pcp_location_option_t *) (pcp_buf_helper + processed);
        DUPPRINT(log_file, "\t Location: \t %s\n", &(opt_loc->location[0]));

        processed += sizeof(pcp_location_option_t);
        remain -= sizeof(pcp_location_option_t);
        break;

    case PCP_OPTION_USERID:
        DUPPRINT(log_file, "\n");
        DUPPRINT(log_file, "OPTION: \t User ID\n");
        opt_user =
                (pcp_userid_option_t *) (pcp_buf_helper + processed);
        DUPPRINT(log_file, "\t User ID: \t %s\n", &(opt_user->userid[0]));

        processed += sizeof(pcp_userid_option_t);
        remain -= sizeof(pcp_userid_option_t);
        break;

    case PCP_OPTION_DEVICEID:
        DUPPRINT(log_file, "\n");
        DUPPRINT(log_file, "OPTION: \t Device ID\n");
        opt_dev =
                (pcp_deviceid_option_t *) (pcp_buf_helper + processed);
        DUPPRINT(log_file, "\t Device ID: \t %s\n", &(opt_dev->deviceid[0]));

        processed += sizeof(pcp_deviceid_option_t);
        remain -= sizeof(pcp_deviceid_option_t);
        break;


    default:
        DUPPRINT(log_file, "Unrecognized PCP OPTION: %d \n", opt_hdr->code);
        DUPPRINT(log_file, "Unrecognized PCP OPTION remain: %d \n", remain);
        DUPPRINT(log_file, "Unrecognized PCP OPTION processed: %d \n", processed);
        remain = 0;
        break;
    }

    // shift processed and remaining values to new values
    *remainingSize = remain;
    *processedSize = processed;
    return pcp_result_code;
}


#define xstr(s) str(s)
#define str(s) #s

/*
 * return value indicates whether the request is valid or not.
 * Based on the return value simple response can be formed.
 */
static int printPCPreq(void * req, int req_size, options_occur_t *opt_occ,
        uint8_t version, const char *server_log_file)
{
    // while request is being printed we can set analyze it as well, and decide
    // on return code which is then used to create response to request
    int pcp_return_val = PCP_RES_SUCCESS;
    int remainingSize;
    int processedSize;
    char s[INET6_ADDRSTRLEN];
    FILE *log_file = NULL;
    pcp_request_t* common_req;
    uint8_t* req_help = (uint8_t*)req;

    // discard request that exceeds maximal length, is shorter than 3,
    // or is not the multiple of 4
    if ((req_size > PCP_MAX_LEN) || (req_size < 4) || ((req_size & 3) != 0)) {
        printf(
            "Size of PCP packet is either smaller than 4 octets or larger "
            "than "xstr(PCP_MAX_LEN)" bytes or the size is not multiple of 4.\n");
        printf("The size was: %d \n", req_size);
        return PCP_RES_MALFORMED_REQUEST;
    }

    if ((server_log_file != NULL)&&(server_log_file[0]!=0)) {
#ifdef WIN32
        _chmod(server_log_file, _S_IWRITE);
#else
        umask(0);
#endif
        CHECK_NULL_EXIT((log_file = fopen(server_log_file, "w+")));
        //filefd = open(server_log_file, O_CREAT | O_SYNC | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }

    remainingSize = req_size;
    processedSize = 0;
    //first print out info from common request header
    common_req = (pcp_request_t*) req_help;
    DUPPRINT(log_file, "\n");
    DUPPRINT(log_file, "PCP version:             %i\n", common_req->ver);
    DUPPRINT(log_file, "PCP opcode:              %i\n", common_req->r_opcode & 0x7F);
    DUPPRINT(log_file, "PCP R bit:               %i\n", (common_req->r_opcode & 0x80) != 0);
    DUPPRINT(log_file, "PCP requested lifetime:  %lu\n", (unsigned long)
                                              htonl( common_req->req_lifetime));

    if (IN6_IS_ADDR_V4MAPPED((struct in6_addr*)common_req->ip)) {
        DUPPRINT(log_file,"PEER IP: \t\t %s\n",
            inet_ntop(AF_INET, &(common_req->ip[3]), s,
                    INET6_ADDRSTRLEN));
    } else {
        DUPPRINT(log_file,"PEER IP: \t\t %s\n",
            inet_ntop(AF_INET6, &(common_req->ip[0]), s,
                    INET6_ADDRSTRLEN));
    }

    remainingSize -= sizeof(pcp_request_t);
    processedSize += sizeof(pcp_request_t);
    DUPPRINT(log_file, "\n");


    if ((common_req->ver > version)) {
        return PCP_RES_UNSUPP_VERSION;
    }

    // analyze two possible versions of protocol,
    // if message is not version 1 or 2, return UNSUPP_VERSION
    if (common_req->ver == 1) {

        if ((common_req->r_opcode & 0x7F) == PCP_OPCODE_MAP) {
            pcp_map_v1_t* map;
            remainingSize -= sizeof(pcp_map_v1_t);
            if (remainingSize < 0) {
                return PCP_RES_MALFORMED_OPTION;
            }
            map = (pcp_map_v1_t*) (req_help + processedSize);
            print_MAP_opcode_ver1(map, log_file);
            processedSize += sizeof(pcp_map_v1_t);

            printf("Remaining size is %d \n", remainingSize);
            while (remainingSize > 0) {
                pcp_return_val = print_PCP_options(req, &remainingSize,
                        &processedSize, opt_occ, log_file);
            }

        }

        if ((common_req->r_opcode & 0x7F) == PCP_OPCODE_PEER) {

            pcp_peer_v1_t* peer;
            remainingSize -= sizeof(pcp_map_v1_t);
            printf("Remaining size is %d \n", remainingSize);
            if (remainingSize < 0) {
                return PCP_RES_MALFORMED_OPTION;
            }

            peer = (pcp_peer_v1_t*) (req_help + processedSize);
            print_PEER_opcode_ver1(peer, log_file);

            processedSize += sizeof(pcp_peer_v1_t);

            while (remainingSize > 0) {
                pcp_return_val = print_PCP_options(req, &remainingSize,
                        &processedSize, opt_occ, log_file);
            }

        }

    } else if (common_req->ver == 2) {

        pcp_map_v2_t* map;
        if ((common_req->r_opcode & 0x7F) == PCP_OPCODE_MAP) {
            remainingSize -= sizeof(pcp_map_v2_t);
            if (remainingSize < 0) {
                return PCP_RES_MALFORMED_OPTION;
            }
            map = (pcp_map_v2_t*) (req_help + processedSize);
            print_MAP_opcode_ver2(map, log_file);
            processedSize += sizeof(pcp_map_v2_t);

            printf("Remaining size is %d \n", remainingSize);
            while (remainingSize > 0) {
                pcp_return_val = print_PCP_options(req, &remainingSize,
                        &processedSize, opt_occ, log_file);
            }

        }

        if ((common_req->r_opcode & 0x7F) == PCP_OPCODE_PEER) {

            pcp_peer_v2_t* peer;
            remainingSize -= sizeof(pcp_peer_v2_t);
            printf("Remaining size is %d \n", remainingSize);
            if (remainingSize < 0) {
                return PCP_RES_MALFORMED_OPTION;
            }

            peer = (pcp_peer_v2_t*) (req_help + processedSize);
            print_PEER_opcode_ver2(peer, log_file);

            processedSize += sizeof(pcp_peer_v2_t);

            while (remainingSize > 0) {
                pcp_return_val = print_PCP_options(req, &remainingSize,
                        &processedSize, opt_occ, log_file);
            }

        }

        if ((common_req->r_opcode & 0x7F) == PCP_OPCODE_SADSCP) {

            pcp_sadscp_req_t* sadscp;
            size_t sadscp_size;

            if (remainingSize < (int)sizeof(pcp_sadscp_req_t)) {
                return PCP_RES_MALFORMED_REQUEST;
            }

            sadscp = (pcp_sadscp_req_t*)(req_help + processedSize);
            sadscp_size = print_SADSCP_opcode_ver2(sadscp, log_file);

            processedSize += sadscp_size;
            remainingSize -= sadscp_size;

            while (remainingSize > 0) {
                pcp_return_val = print_PCP_options(req, &remainingSize,
                        &processedSize, opt_occ, log_file);
            }

        }
    } else {
        return PCP_RES_UNSUPP_VERSION;
    }
    fflush(log_file);
    return pcp_return_val;
}

static int create_response(char *request, int pcp_result_code,
        const server_info_t *server_info)
{

    pcp_response_t *resp = (pcp_response_t*) request;
    resp->reserved = 0;
    resp->reserved1[0] = 0;
    resp->reserved1[1] = 0;
    resp->reserved1[2] = 0;
    resp->r_opcode |= 0x80;
    resp->result_code = (uint8_t)pcp_result_code;
    resp->epochtime = htonl((uint32_t) (time(NULL) - server_info->epoch_time_start));

    if (pcp_result_code == PCP_RES_UNSUPP_VERSION) {
        resp->ver = server_info->server_version;
        return 0;
    }

    if ((resp->r_opcode & 0x7f) == PCP_OPCODE_MAP) {
        if (resp->ver==1) {
            pcp_map_v1_t *m1 = (pcp_map_v1_t *)resp->next_data;
            memcpy(m1->ext_ip, &server_info->ext_ip, sizeof(m1->ext_ip));
        } else if (resp->ver==2) {
            pcp_map_v2_t *m2 = (pcp_map_v2_t *)resp->next_data;
            memcpy(m2->ext_ip, &server_info->ext_ip, sizeof(m2->ext_ip));
        }
    }

    if ((resp->r_opcode & 0x7f) == PCP_OPCODE_SADSCP) {
        pcp_sadscp_resp_t * r = (pcp_sadscp_resp_t*)resp->next_data;
        r->a_r_dscp = server_info->app_bit << 7;
        r->a_r_dscp |= server_info->ret_dscp;
    }

    return 0;
}

static int execPCPServer(const char* serverPort, const char* serverAddress,
        const server_info_t *server_info)
{
    PCP_SOCKET sockfd;
    int numbytes;
    int pcp_result_code;
    struct sockaddr_storage their_addr;
    int execute = 1;
    options_occur_t opt_occurence = { 0, 0 };

    char buf[PCP_MAX_LEN];
    socklen_t addr_len=0;
    char s[INET6_ADDRSTRLEN];

    struct timeval tod; //store current time of the day
    struct timeval timeout_time; // store expected timeout time

    memset(buf, 0, sizeof(buf));

#ifdef WIN32
    if (pcp_win_sock_startup()) {
        return 1;
    }
#endif //WIN32


    // if number of messages server will receive was set
    // use this number to control number of times while() cycle executes
    // if end_after_recv was not set, cycle will run with execute = 1
    if (server_info->end_after_recv != 0) {
        execute = server_info->end_after_recv;
    }

    sockfd = createPCPsocket(serverPort, serverAddress);

    gettimeofday(&tod, NULL);
    timeout_time.tv_sec = tod.tv_sec + server_info->tv.tv_sec;
    timeout_time.tv_usec = tod.tv_usec + server_info->tv.tv_usec;

    while (execute) {
        time_t recvtime;

        if (server_info->end_after_recv != 0) {
            execute--;
        }

        recvtime = 0;

        if (server_info->tv.tv_sec != 0 || server_info->tv.tv_usec != 0) {
            struct timeval end_time;
            PI_TIMEOUT_STRUCT pi_timeout;
            gettimeofday(&tod, NULL);

            if (timeval_subtract(&end_time, &timeout_time, &tod)) {//LCOV_EXCL_START
                end_time.tv_sec = 0;
                end_time.tv_usec = 0;
            }

            SET_PI_TIMEOUT(pi_timeout, end_time);
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                (char*)&pi_timeout, sizeof(pi_timeout));
        }

        printf("############################################\n");
        printf("###   PCP server: waiting to recvfrom... ###\n");
        printf("############################################\n");

        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, buf, PCP_MAX_LEN - 1, 0,
                (struct sockaddr *) &their_addr, &addr_len)) == PCP_SOCKET_ERROR) {//LCOV_EXCL_START
            perror("recvfrom");
            exit(1);
        }//LCOV_EXCL_STOP
        time(&recvtime);

        printf("PCP server: got packet at %s from %s\n", ctime(&recvtime),
                inet_ntop(their_addr.ss_family,
                        get_in_addr((struct sockaddr *) &their_addr), s,
                        sizeof s));

        printf("PCP server: packet is %d bytes long\n", numbytes);

        pcp_result_code = printPCPreq(buf, numbytes, &opt_occurence,
                server_info->server_version, server_info->log_file);

        // check if default result code should be returned
        // or the result code that was retrieved after message parsing
        create_response(buf,
                (server_info->default_result_code == 255) ?
                        pcp_result_code : server_info->default_result_code,
                server_info);

        // send response to client
        sendto(sockfd, buf, numbytes, 0, (struct sockaddr*) &their_addr,
                addr_len);

        reset_option_occur(&opt_occurence);
        printf("\n");
    }

    printf("Closing sockfd \n");
    CLOSE(sockfd);

#ifdef WIN32
    pcp_win_sock_cleanup();
#endif  //WIN32

    return 0;
}

static void print_usage(void)
{

    printf("\n");
    printf("Usage: \n");
    printf("-h, --help \t  Display this help \n");
    printf("-v \t\t  Set server version.\n");
    printf("   \t\t   Only versions 1 and 2 are supported for now.\n");
    printf("   \t\t   DEFAULT: By default server processes both versions.\n");
    printf("-p \t\t  Lets you set the port on which server listens.\n");
    printf("   \t\t   DEFAULT: 5351.\n");
    printf(
           "-r \t\t  Set result code that will be returned by server \n"
           "   \t\t   to every request no matter what.\n");
    printf("   \t\t   Possible values from range {0..13} \n");
    printf(
           "--ear #num \t  Terminates server after #num requests have been \n"
            "          \t   received and responded to. \n");
    printf("--ip \t\t  Sets IP address on which server is listening.\n");
    printf("     \t\t   DEFAULT: 0.0.0.0 (listening on all interfaces)\n");
    printf("--timeout \t  Set timeout time of the server in miliseconds.\n");
    printf("     \t\t   DEFAULT 0 (Running indefinitely)\n");
    printf("--app-bit \t  set application bit in SADSCP opcode response\n");
    printf("--ret-dscp\t  return DSCP value for SADSCP opcode\n");
    printf("--log-file \t  Log Requests to file \n");

}

#ifndef no_argument
#define no_argument 0
#endif

#ifndef required_argument
#define required_argument 1
#endif

int main(int argc, char *argv[])
{

    const char *port = PCP_PORT;
    int test_port;
    uint8_t pcp_version = PCP_TEST_MAX_VERSION;
    uint8_t end_after_recv = 0;
    uint8_t default_result_code = 255;
#ifdef WIN32
    long timeout_us = 0;
#else
    suseconds_t timeout_us = 0;
#endif
    server_info_t server_info_storage;
    const char *server_ip = "0.0.0.0";
    uint8_t app_bit = 0;
    uint8_t ret_dscp = 0;
    char *log_file = NULL;

    {
        int c;
        int option_index = 0;

        static struct option long_options[] = {
                { "ear", required_argument, 0, 0 },
                { "help", no_argument, 0, 0 },
                { "ip", required_argument, 0, 0 },
                { "ext-ip", required_argument, 0, 0 },
                { "timeout", required_argument, 0, 0 },
                { "app-bit", no_argument, 0, 0 },
                { "ret-dscp", required_argument, 0, 0 },
                { "log-file", required_argument, 0, 0},
                { 0, 0, 0, 0}
        };

        opterr = 0;
        while ((c = getopt_long(argc, argv, "hr:v:p:", long_options,
                &option_index)) != -1) {
            switch (c) {
            // assign values for long options
            case 0:
                if (!long_options[option_index].name) {
                    break;
                }

                if (!strcmp(long_options[option_index].name, "ear"))
                    end_after_recv = (uint8_t) atoi(optarg);

                if (!strcmp(long_options[option_index].name, "timeout")) {
                    int temp_timeout = atoi(optarg);
                    if (temp_timeout < 0) {
                        printf(
                                "Value provided for timeout was negative %d. "
                                "Please provide correct value.\n",
                                temp_timeout);
                        exit(1);
                    } else {
#ifdef WIN32
                        timeout_us = (long) atoi(optarg);
#else
                        timeout_us = (suseconds_t) atoi(optarg);
#endif
                    }
                }

                if (!strcmp(long_options[option_index].name, "app-bit")) {
                    app_bit = 1;
                }

                if (!strcmp(long_options[option_index].name, "ret-dscp")) {
                    ret_dscp = (uint8_t) atoi(optarg);
                }

                if (!strcmp(long_options[option_index].name, "log-file")) {
                    log_file = optarg;
                }

                if (!strcmp(long_options[option_index].name, "ip"))
                    server_ip = optarg;

                if (!strcmp(long_options[option_index].name, "ext-ip"))
                {
                    inet_pton(AF_INET6, optarg, &server_info_storage.ext_ip);
                }

                if (!strcmp(long_options[option_index].name, "help")) {
                    print_usage();
                    exit(1);
                }

                break;
            case 'h':
                print_usage();
                exit(1);
                break;
            case 'p':
                test_port = atoi(optarg);
                if ( (test_port < 1) || (test_port > 65535)) {
                    printf("Bad value for option -p %d \n", test_port);
                    printf("Port value can be in range 1-65535. \n");
                    printf("Default value will be used. \n");
                } else {
                    port = optarg;
                }
                break;
            case 'r':
                default_result_code = (uint8_t) atoi(optarg);
                if (default_result_code > 13
                                && default_result_code != 255) {
                    printf(
                            "Unsupported  RESULT CODE %d (acceptable values "
                            "0 <= result_code <= 13 or result_code == 255)\n",
                            default_result_code);
                    exit(1);
                }
                break;
            case 'v':
                pcp_version = (uint8_t) atoi(optarg);
                if (pcp_version < 1 || pcp_version > 2) {
                    printf("Version %d is not supported! \n", pcp_version);
                    exit(1);
                }
                break;
            case '?':
                if (isprint (optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Unknown option character `\\x%x'.\n",
                            optopt);
                print_usage();
                exit(1);
            default://LCOV_EXCL_START
                print_usage();
                exit(1);
                break;//LCOV_EXCL_STOP
            }
        }
    }

    server_info_storage.default_result_code = default_result_code;
    server_info_storage.end_after_recv = end_after_recv;
    server_info_storage.server_version = pcp_version;
    server_info_storage.tv.tv_sec = timeout_us / 1000;
    server_info_storage.tv.tv_usec = (timeout_us % 1000) * 1000;
    server_info_storage.epoch_time_start = time(NULL);
    server_info_storage.app_bit = app_bit;
    server_info_storage.ret_dscp = ret_dscp;
    if (log_file != NULL) {
        memcpy(&(server_info_storage.log_file[0]), log_file, min(strlen(log_file) + 1, MAX_LOG_FILE));
    } else {
        server_info_storage.log_file[0] = 0;
    }

    printf("Server listening on %s:%s \n", server_ip, port);
    return execPCPServer(port, server_ip, &server_info_storage);
}
