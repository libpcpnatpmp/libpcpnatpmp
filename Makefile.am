ACLOCAL_AMFLAGS = -I m4
if BUILD_APP
  APP_DIR = pcp_app
endif

if BUILD_SERVER
  SERVER_DIR = pcp_server
endif

PCP_CPPFLAGS = -I$(top_srcdir)/libpcp/include -I$(top_srcdir)/libpcp/src/net -I$(top_srcdir)/libpcp/src
PCP_CFLAGS = -Wall -Werror -Wno-error=strict-aliasing
GCOVLIB =

if GCOV
PCP_CFLAGS +=  -ggdb3 -O0 --coverage --no-inline
GCOVLIB += -lgcov
endif

if PROF
PCP_CFLAGS +=  -ggdb3 -O0 -pg
endif

if DEBUG
PCP_CFLAGS += -ggdb3 -O0
PCP_CPPFLAGS += -DDEBUG -DPCP_MAX_LOG_LEVEL=5
else
PCP_CPPFLAGS += -DNDEBUG
endif

export GCOVLIB
export PCP_CFLAGS
export PCP_CPPFLAGS

SUBDIRS = libpcp \
          tests \
          $(APP_DIR) \
          $(SERVER_DIR)


EXTRA_DIST = tests/test_flow_notify.sh \
             tests/test_version_negotiation.sh \
             tests/test_pcp_client_map_opcode.sh \
             tests/test_pcp_client_peer_opcode.sh \
             tests/test_server_restart.sh \
             tests/test_get_dscp.sh \
             tests/test_lifetime_renewal.sh \
             autogen.sh \
             tests/test_pcp_app.sh \
             tests/test_pcp_server.sh \
             tests/test_flow_md.sh \
             tests/test_server_reping.sh \
             INSTALL.md \
             README.md \
             pcp_app/README.md \
             pcp_server/README.md \
             scapy/README.md \
             COPYING

if PCP_USE_IPV6_SOCKET
DPCP_USE_IPV6_SOCKET=1
endif

TESTS_ENVIRONMENT = PATH=pcp_app/:pcp_server/:libpcp/:tests/:$(PATH)\
                    PCP_USE_IPV6_SOCKET=$(DPCP_USE_IPV6_SOCKET)

if PCP_EXPERIMENTAL
PCP_EXPERIMENTAL_TESTS = tests/test_flow_md.sh
else
PCP_EXPERIMENTAL_TESTS =
endif

if PCP_SADSCP
PCP_SADSCP_TESTS = tests/test_get_dscp.sh
else
PCP_SADSCP_TESTS =
endif

TESTS = tests/test_flow_notify.sh \
        tests/test_version_negotiation.sh \
        tests/test_pcp_client_map_opcode.sh \
        tests/test_pcp_client_peer_opcode.sh \
        tests/test_server_restart.sh \
        tests/test_lifetime_renewal.sh \
        tests/test_event_handler \
        tests/test_gw \
        tests/test_ping_gws \
        tests/test_pcp_client_db \
        tests/test_pcp_api \
        tests/test_pcp_app.sh \
        tests/test_pcp_server.sh \
        tests/test_sock_ntop \
        tests/test_pcp_logger \
        tests/test_pcp_msg \
        tests/test_server_reping.sh \
        $(PCP_SADSCP_TESTS) \
        $(PCP_EXPERIMENTAL_TESTS)


