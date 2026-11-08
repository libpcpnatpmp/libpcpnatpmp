/*
 Copyright (c) 2014 by Cisco Systems, Inc.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PCP_CLIENT_DB_H_
#define PCP_CLIENT_DB_H_

#include "pcpnatpmp.h"

#include "pcp_event_handler.h"
#include "pcp_msg_structs.h"
#include <stdint.h>
#ifdef WIN32
#include "pcp_win_defines.h"
#include "unp.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    optf_3rd_party = 1 << 0,
    optf_flowp = 1 << 1,
} opt_flags_e;

#define PCP_INV_SERVER (~0u)

#ifdef PCP_EXPERIMENTAL

#ifndef MD_VAL_MAX_LEN
#define MD_VAL_MAX_LEN 24
#endif

typedef struct {
    uint16_t md_id;
    uint16_t val_len;
    uint8_t val_buf[MD_VAL_MAX_LEN];
} md_val_t;
#endif

#define FLOW_HASH_BITS 5
#define FLOW_HASH_SIZE (2 << FLOW_HASH_BITS)

struct flow_key_data {
    uint8_t operation;
    struct in6_addr src_ip;
    struct in6_addr pcp_server_ip;
    uint32_t scope_id;
    struct pcp_nonce nonce;
    union {
        struct mp_keydata {
            uint8_t protocol;
            uint16_t src_port;
            struct in6_addr dst_ip;
            uint16_t dst_port;
        } map_peer;
    };
};

typedef struct pcp_recv_msg {
    opt_flags_e opt_flags;
    struct flow_key_data kd;
    uint32_t key_bucket;

    // response data
    struct in6_addr assigned_ext_ip;
    uint16_t assigned_ext_port;
    uint8_t recv_dscp;
    uint32_t recv_version;
    uint32_t recv_epoch;
    uint32_t recv_lifetime;
    uint32_t recv_result;
    time_t received_time;

    // control data
    uint32_t pcp_server_indx;
    struct sockaddr_storage rcvd_from_addr;
    struct sockaddr_in6 rcvd_to_addr;
    // msg buffer
    uint32_t pcp_msg_len;
    char pcp_msg_buffer[PCP_MAX_LEN];
} pcp_recv_msg_t;

struct pcp_ctx_s {
    PCP_SOCKET socket;
    struct pcp_client_db {
        size_t pcp_servers_length;
        pcp_server_t *pcp_servers;
        size_t flow_cnt;
        pcp_flow_t *flows[FLOW_HASH_SIZE];
    } pcp_db;
    pcp_flow_change_notify flow_change_cb_fun;
    void *flow_change_cb_arg;
    pcp_recv_msg_t msg;
    pcp_socket_vt_t *virt_socket_tb;
    // send pacing: requests sent in the current window, and when it ends
    unsigned pace_sent;
    unsigned pace_new;
    unsigned pace_inflight;
    struct timeval pace_window_end;
    // longest random wait before re-sending after a server restart
    uint32_t restart_delay_max_ms;
    // the most FILTER options a flow keeps, see pcp_set_max_filter_opts()
    uint16_t max_filter_opts;
};

/* where a FILTER option of a flow is: not yet sent to the server (or to be
 * sent again), in the last request built, accepted by the server, or not sent
 * because it is over the server's limit or never fits in a request */
typedef enum {
    PCP_FILTER_PENDING = 0,
    PCP_FILTER_IN_REQUEST,
    PCP_FILTER_CONFIRMED,
    PCP_FILTER_REFUSED
} pcp_filter_state_e;

/* a FILTER option of a flow, as it goes on the wire */
struct pcp_flow_filter {
    struct in6_addr ip;
    uint16_t port; /* network byte order */
    uint8_t prefix;
    uint8_t state; /* pcp_filter_state_e */
};

struct pcp_flow_s {
    // flow's data
    struct pcp_ctx_s *ctx;
    opt_flags_e opt_flags;
    struct flow_key_data kd;
    uint32_t key_bucket;

    uint32_t lifetime;
    union {
        struct {
            struct in6_addr ext_ip;
            uint16_t ext_port;
        } map_peer;
#ifdef PCP_SADSCP
        struct {
            uint8_t toler_fields;
            uint8_t app_name_length;
            uint8_t learned_dscp;
        } sadscp;
#endif
    };
#ifdef PCP_SADSCP
    char *sadscp_app_name;
#endif
    // response data
    time_t recv_lifetime;
    uint32_t recv_result;

    // control data
    struct pcp_flow_s *next;       // next flow with same key bucket
    struct pcp_flow_s *next_child; // next flow for MAP with 0.0.0.0 src ip
    uint32_t pcp_server_indx;
    pcp_flow_state_e state;
    uint32_t resend_timeout;
    // a retransmission was put off by send pacing; keep its backoff
    uint8_t resend_deferred;
    uint32_t retry_count;
    uint32_t to_send_count;
    struct timeval timeout;

#ifdef PCP_EXPERIMENTAL
    // Userid
    pcp_userid_option_t f_userid;

    // Location
    pcp_location_option_t f_location;

    // DeviceID
    pcp_deviceid_option_t f_deviceid;
#endif

#ifdef PCP_FLOW_PRIORITY
    // FLOW Priority Option
    uint8_t flowp_option_present;
    uint8_t flowp_dscp_up;
    uint8_t flowp_dscp_down;
#endif

    // PREFER FAILURE Option
    uint8_t pfailure_option_present;

    // FILTER Options (RFC 6887 section 13.3). A server adds the filters of
    // each request to the ones it has and keeps them when the mapping is
    // renewed, so a request carries only the filters the server does not
    // have yet (state PCP_FILTER_PENDING), as many as fit; the rest go in
    // the next requests. Only a FILTER with prefix length 0 removes filters.
    struct pcp_flow_filter *filters;
    uint16_t filter_count;
    uint16_t filter_cap; /* entries allocated in filters */
    uint8_t filter_mode; /* pcp_filter_mode_e */
    // filter_sent: a request with filters was built, so the server may have
    // some. filter_reset: a filter was removed after that, so the next request
    // starts with a prefix 0 FILTER (and then every filter of the flow) until
    // a success reply to a request that carried it (filter_reset_sent).
    uint8_t filter_sent;
    uint8_t filter_reset;
    uint8_t filter_reset_sent;
    // A server accepts a limited number of FILTER options per mapping and
    // answers a request that would go over it with EXCESSIVE_REMOTE_PEERS.
    // filter_last_total: the FILTER options the mapping would have after the
    // last request built.
    // filter_try_total: the most the next request may bring it to (0: no
    // limit), one less after each refusal. filter_server_limit: the most the
    // server accepted when it had refused one more (0: not known).
    uint16_t filter_last_total;
    uint16_t filter_try_total;
    uint16_t filter_server_limit;

    // THIRD_PARTY Option
    uint8_t third_party_option_present;
    struct in6_addr third_party_ip;

#ifdef PCP_EXPERIMENTAL
    // MD Option
    uint32_t md_val_count;
    md_val_t *md_vals;
#endif

    // msg buffer
    uint32_t pcp_msg_len;
    char *pcp_msg_buffer;
    void *user_data;
};

/* structure holding PCP server specific data */
struct pcp_server {
    pcp_ctx_t *ctx;
    uint32_t af;
    uint32_t pcp_ip[4];
    uint16_t pcp_port;
    uint32_t pcp_scope_id;
    uint32_t src_ip[4];
    char pcp_server_paddr[INET6_ADDRSTRLEN];
    struct sockaddr_storage pcp_server_saddr;
    uint8_t pcp_version;
    uint8_t next_version;
    pcp_server_state_e server_state;
    uint32_t epoch;
    time_t cepoch;
    struct pcp_nonce nonce;
    uint32_t index;
    pcp_flow_t *ping_flow_msg;
    pcp_flow_t *restart_flow_msg;
    uint32_t ping_count;
    struct timeval next_timeout;
    uint32_t natpmp_ext_addr;
    // the library's socket is not on port 5350: listening there on this one
    uint8_t announce_listening;
    PCP_SOCKET announce_socket;
    struct in6_addr announce_src_ip;
    uint32_t announce_scope_id;
    // re-sending after a restart is waiting for restart_resend_at
    uint8_t restart_pending;
    struct timeval restart_resend_at;
    void *app_data;
};

typedef int (*pcp_db_flow_iterate)(pcp_flow_t *f, void *data);

typedef int (*pcp_db_server_iterate)(pcp_server_t *f, void *data);

pcp_flow_t *pcp_create_flow(pcp_server_t *s, struct flow_key_data *fkd);

pcp_errno pcp_free_flow(pcp_flow_t *f);

pcp_flow_t *pcp_get_flow(struct flow_key_data *fkd, pcp_server_t *s);

pcp_errno pcp_db_add_flow(pcp_flow_t *f);

pcp_errno pcp_db_rem_flow(pcp_flow_t *f);

pcp_errno pcp_db_foreach_flow(pcp_ctx_t *ctx, pcp_db_flow_iterate f,
                              void *data);

void pcp_flow_clear_msg_buf(pcp_flow_t *f);

/* the server may not have the flow's filters any more (it restarted, or the
 * mapping failed or expired): send all of them again, also the refused ones */
void pcp_flow_filters_resend(pcp_flow_t *f);

/* the server restarted: send every filter again and learn its limit anew */
void pcp_flow_filters_server_restarted(pcp_flow_t *f);

/* the server answered EXCESSIVE_REMOTE_PEERS: the next request brings the
 * mapping to one filter fewer than the refused one. Returns 1 if there is
 * such a request to send */
int pcp_flow_filters_refused(pcp_flow_t *f);

/* how many of the flow's filters the server has accepted */
uint16_t pcp_flow_filters_accepted(const pcp_flow_t *f);

/* 1 if the last request built for the flow carried a FILTER option, the one
 * with prefix length 0 that removes the server's filters included */
int pcp_flow_filters_in_request(const pcp_flow_t *f);

/* a success reply arrived: the server has the filters of the last request.
 * Returns 1 if that request carried any FILTER option */
int pcp_flow_filters_confirmed(pcp_flow_t *f);

/* 1 if the flow has a FILTER option (or a reset) the server has not accepted
 * yet and that its next request would carry */
int pcp_flow_filters_pending(const pcp_flow_t *f);

#ifdef PCP_EXPERIMENTAL
void pcp_db_add_md(pcp_flow_t *f, uint16_t md_id, void *val, size_t val_len);
#endif

int pcp_new_server(pcp_ctx_t *ctx, struct in6_addr *ip, uint16_t port,
                   uint32_t scope_id);

pcp_errno pcp_db_foreach_server(pcp_ctx_t *ctx, pcp_db_server_iterate f,
                                void *data);

pcp_server_t *get_pcp_server(pcp_ctx_t *ctx, int pcp_server_index);

pcp_server_t *get_pcp_server_by_ip(pcp_ctx_t *ctx, struct in6_addr *ip,
                                   uint32_t scope_id);

void pcp_db_free_pcp_servers(pcp_ctx_t *ctx);

pcp_errno pcp_delete_flow_intern(pcp_flow_t *f);

#ifdef __cplusplus
}
#endif

#endif /* PCP_CLIENT_DB_H_ */
