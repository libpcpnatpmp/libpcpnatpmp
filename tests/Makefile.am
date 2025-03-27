AM_CPPFLAGS = -I$(top_srcdir)/lib/include -I$(top_srcdir)/lib/src/net -I$(top_srcdir)/lib/src
AM_CPPFLAGS += $(PCP_CPPFLAGS)
AM_CFLAGS = $(PCP_CFLAGS)

check_PROGRAMS = test_server_restart \
                 test_version_negotiation \
                 test_pcp_client_map_opcode \
                 test_pcp_client_peer_opcode \
                 test_flow_notify \
                 test_ping_gws \
                 test_gw \
                 test_event_handler \
                 test_lifetime_renewal \
                 test_pcp_client_db \
                 test_pcp_api \
                 test_sock_ntop \
                 test_pcp_logger \
                 test_pcp_msg \
                 test_server_reping

noinst_HEADERS = test_macro.h

test_flow_notify_SOURCES = test_flow_notify.c
test_flow_notify_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_flow_notify_LDFLAGS = -static

test_gw_SOURCES = test_gateway.c
#src/net/gateway.c src/pcp_logger.c src/net/sock_ntop.c
test_gw_LDADD = $(top_builddir)/lib/$(top_builddir)/lib/libpcpnatpmp.la
test_gw_LDFLAGS = -static
#test_gw_CFLAGS = $(AM_CFLAGS)
#test_gw_LDADD = $(GCOVLIB)

test_ping_gws_SOURCES = test_server_discovery.c
test_ping_gws_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_ping_gws_LDFLAGS = -static

test_server_restart_SOURCES = test_server_restart.c
test_server_restart_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_server_restart_LDFLAGS = -static

test_version_negotiation_SOURCES = test_version_negotiation.c
test_version_negotiation_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_version_negotiation_LDFLAGS = -static

test_pcp_client_map_opcode_SOURCES = test_pcp_client_map_opcode.c
test_pcp_client_map_opcode_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_pcp_client_map_opcode_LDFLAGS = -static

test_pcp_client_peer_opcode_SOURCES = test_pcp_client_peer_opcode.c
test_pcp_client_peer_opcode_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_pcp_client_peer_opcode_LDFLAGS = -static

test_event_handler_SOURCES = test_event_handler.c
test_event_handler_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_event_handler_LDFLAGS = -static

test_lifetime_renewal_SOURCES = test_lifetime_renewal.c
test_lifetime_renewal_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_lifetime_renewal_LDFLAGS = -static

test_pcp_client_db_SOURCES = test_pcp_client_db.c
test_pcp_client_db_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_pcp_client_db_LDFLAGS = -static

test_pcp_api_SOURCES = test_pcp_api.c
test_pcp_api_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_pcp_api_LDFLAGS = -static

test_sock_ntop_SOURCES = test_sock_ntop.c
test_sock_ntop_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_sock_ntop_LDFLAGS = -static

test_pcp_logger_SOURCES = test_pcp_logger.c
test_pcp_logger_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_pcp_logger_LDFLAGS = -static

test_pcp_msg_SOURCES = test_pcp_msg.c
test_pcp_msg_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_pcp_msg_LDFLAGS = -static

test_server_reping_SOURCES = test_server_reping.c
test_server_reping_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
test_server_reping_LDFLAGS = -static



