AM_CPPFLAGS = -I$(top_srcdir)/lib/include -I$(top_srcdir)/lib/src/net -I$(top_srcdir)/lib/src
AM_CPPFLAGS += $(PCP_CPPFLAGS)
AM_CFLAGS = $(PCP_CFLAGS) -I$(top_srcdir)/lib/src/windows

noinst_PROGRAMS = pcp-server

pcp_server_SOURCES = pcp-server.c
pcp_server_CPPFLAGS = $(AM_CPPFLAGS)
pcp_server_LDADD = $(GCOVLIB)
if PLATFORM_WINDOWS
    pcp_server_LDADD += $(top_builddir)/lib/libpcpnatpmp.la -lws2_32 -liphlpapi
endif
