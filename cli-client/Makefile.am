AM_CPPFLAGS = $(PCP_CPPFLAGS)
AM_CFLAGS = $(PCP_CFLAGS)

bin_PROGRAMS = pcpnatpmpc

pcpnatpmpc_SOURCES = pcpnatpmpc.c
pcpnatpmpc_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)/lib/include -I$(top_srcdir)/lib/src/net -I$(top_srcdir)/lib/src -I$(top_srcdir)/lib/src/windows

pcpnatpmpc_LDADD = $(top_builddir)/lib/libpcpnatpmp.la
if PLATFORM_WINDOWS
    pcpnatpmpc_LDADD += -lws2_32 -liphlpapi
endif
pcpnatpmpc_LDFLAGS = -static

