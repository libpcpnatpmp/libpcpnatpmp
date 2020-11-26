AM_CPPFLAGS = $(PCP_CPPFLAGS)
AM_CFLAGS = $(PCP_CFLAGS)

bin_PROGRAMS = pcp

pcp_SOURCES = pcp_app.c
pcp_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/libpcp/include -I$(top_srcdir)/libpcp/src/net -I$(top_srcdir)/libpcp/src -I$(top_srcdir)/libpcp/src/windows

pcp_LDADD = $(top_builddir)/libpcp/libpcp-client.la
if PLATFORM_WINDOWS
    pcp_LDADD += -lws2_32 -liphlpapi
endif
pcp_LDFLAGS = -static

