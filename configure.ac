AC_INIT([pcp-client], [2.0], [aeon@cisco.com])
AC_CANONICAL_SYSTEM
AC_PREREQ([2.62])
AM_INIT_AUTOMAKE([-Wall -Werror foreign color-tests subdir-objects])
#AM_SUBDIR_OBJECTS([yes])
AM_PROG_AR
LT_INIT
AC_PROG_CC
AC_CONFIG_FILES([
 Makefile
 libpcp/Makefile
 libpcp/libpcp-client.pc
 pcp_server/Makefile
 pcp_app/Makefile
 tests/Makefile
])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_PROG_CC_C_O
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([arpa/inet.h malloc.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRERROR_R
AC_CHECK_FUNCS([gettimeofday memset select socket strdup strerror strndup])

case "$target" in
        *-*-mingw*|*-*-cygwin*|*-*-msys*)
                AC_DEFINE(PLATFORM_WIN32, 1, [Platform is Win32])
                PLATFORM_STRING="Win32"
                ;;
        *-*-linux*)
                AC_DEFINE(PLATFORM_LINUX, 1, [Platform is Linux])
                PLATFORM_STRING="Linux"
                ;;
        *-*-darwin*)
                AC_DEFINE(PLATFORM_APPLE, 1, [Platform is Apple])
                PLATFORM_STRING="Apple"
                ;;
        *)
                AC_MSG_WARN([*** Please add $host to configure.ac checks!])
                ;;
esac

AM_CONDITIONAL([PLATFORM_LINUX], [test "$PLATFORM_STRING" = "Linux"])
AM_CONDITIONAL([PLATFORM_WINDOWS], [test "$PLATFORM_STRING" = "Win32"])

AC_ARG_ENABLE(debug,
AS_HELP_STRING([--enable-debug],
               [enable debugging, default: no]),
[case "${enableval}" in
             yes) debug=true ;;
             no)  debug=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
esac],
[debug=false])

AM_CONDITIONAL(DEBUG, test x"$debug" = x"true")


AC_ARG_ENABLE(prof,
AS_HELP_STRING([--enable-prof],
               [enable profiling, default: no]),
[case "${enableval}" in
             yes) prof=true ;;
             no)  prof=false ;;
             *)   AC_MSG_ERROR([bad value ${enableval} for --enable-prof]) ;;
esac],
[prof=false])

AM_CONDITIONAL(PROF, test x"$prof" = x"true")

AC_CHECK_MEMBER([struct sockaddr.sa_len],
                AC_DEFINE(HAVE_SOCKADDR_SA_LEN, 1,
                  [Define if struct sockaddr has sa_len field]),,
                [#include <sys/types.h>
                 #include <sys/socket.h>])


AC_MSG_CHECKING([for struct sockaddr_storage])
AC_TRY_COMPILE([ #include <sys/types.h>
#include <sys/socket.h>], [struct sockaddr_storage s; s],
  have_sockaddr_storage=yes, have_sockaddr_storage=no)
if test "$have_sockaddr_storage" = "yes" ; then
  AC_DEFINE(HAVE_SOCKADDR_STORAGE,1,[Whether you have struct sockaddr_storage])
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE([gcov],
  [AS_HELP_STRING([--enable-gcov],
    [use Gcov to test the test suite])],
    [],
    [enable_gcov=no])
AM_CONDITIONAL([GCOV],[test '!' "$enable_gcov" = no])

AC_ARG_ENABLE([app],
              [AS_HELP_STRING([--enable-app],[build command-line client app])],
              [build_app=${enableval}],
              [build_app="yes"])
AM_CONDITIONAL([BUILD_APP],[test "x$build_app" = "xyes"])

AC_ARG_ENABLE([server],
              [AS_HELP_STRING([--enable-server],[build test server app])],
              [build_server=${enableval}],
              [build_server="yes"])
AM_CONDITIONAL([BUILD_SERVER],[test "x$build_server" = "xyes"])

AC_ARG_ENABLE([natpmp],
              [AS_HELP_STRING([--disable-natpmp],[disable NATPMP support])],
              [enable_natpmp=${enableval}],
              [enable_natpmp="yes"])

if test "x$enable_natpmp" != "xyes" ; then
AC_DEFINE([PCP_DISABLE_NATPMP_SUPPORT],1,[disable NATPMP support])
AC_DEFINE([PCP_MIN_SUPPORTED_VERSION],1,[Minimal PCP version to use])
else
AC_DEFINE([PCP_MIN_SUPPORTED_VERSION],0,[Minimal PCP version to use])
fi
AC_DEFINE([PCP_MAX_SUPPORTED_VERSION],2,[Maximal PCP version to use])

AC_ARG_ENABLE([flow-priority],
              [AS_HELP_STRING([--enable-flow-priority],[enable flow priority option support])],
              [enable_flowp=${enableval}],
              [enable_flowp="no"])

if test "x$enable_flowp" = "xyes" ; then
AC_DEFINE([PCP_FLOW_PRIORITY],1,[enable FLOW-PRIORITY option support])
fi

AC_ARG_ENABLE([learn-dscp],
              [AS_HELP_STRING([--enable-learn-dscp],[enable SADSCP operation support])],
              [enable_sadscp=${enableval}],
              [enable_sadscp="no"])

if test "x$enable_sadscp" = "xyes" ; then
AC_DEFINE([PCP_SADSCP],1,[enable SADSCP option support])
fi
AM_CONDITIONAL([PCP_SADSCP],[test "x$enable_sadscp" = "xyes"])

AC_ARG_ENABLE([experimental],
              [AS_HELP_STRING([--enable-experimental],[enable experimental PCP options support])],
              [enable_experimental=${enableval}],
              [enable_experimental="no"])

if test "x$enable_experimental" = "xyes" ; then
AC_DEFINE([PCP_EXPERIMENTAL],1,[enable experimental PCP options support])
fi
AM_CONDITIONAL([PCP_EXPERIMENTAL],[test "x$enable_experimental" = "xyes"])

AC_ARG_ENABLE([ipv6],
              [AS_HELP_STRING([--disable-ipv6],[disable use of IPv6 socket])],
              [enable_ipv6=${enableval}],
              [enable_ipv6="yes"])

if test "x$enable_ipv6" = "xyes" ; then
AC_DEFINE([PCP_USE_IPV6_SOCKET],1,[use IPv6 socket])
fi
AM_CONDITIONAL([PCP_USE_IPV6_SOCKET],[test "x$enable_ipv6" = "xyes"])

AC_DEFINE([PCP_SERVER_PORT], 5351, [Default PCP server port])
AC_DEFINE([PCP_MAX_PING_COUNT], 3, [Maximum number of ping attempts])
AC_DEFINE([PCP_SERVER_DISCOVERY_RETRY_DELAY], 3600, [Server discovery retry delay])
AC_DEFINE([PCP_RETX_IRT], 3000, [Initial retransmission time])
AC_DEFINE([PCP_RETX_MRC], 3, [Maximum retransmission count (0 indicates no maximum)])
AC_DEFINE([PCP_RETX_MRT], 1024000, [Maximum retransmission time])
AC_DEFINE([PCP_RETX_MRD], 0, [Maximum retransmission duration (0 indicates no maximum)])

AC_PROG_LIBTOOL

AC_ARG_ENABLE([silent-rules],
  [AS_HELP_STRING([--enable-silent-rules],
    [enable silent rules to be less verbose])],
    [],
    [enable_silent_rules=yes])
if test "$enable_silent_rules" = "yes" ; then
AM_SILENT_RULES([yes])
else
AM_SILENT_RULES([no])
fi
AC_OUTPUT
