cmake_minimum_required(VERSION 3.14)

project (pcpnatpmp-client VERSION 2.0.0)

include (CheckIncludeFile)
include (CheckStructHasMember)
include (CheckSymbolExists)
include (CMakePackageConfigHelpers)
include (GNUInstallDirs)

enable_testing ()

option (ENABLE_DEBUG "Enable debug build flags" OFF)
option (ENABLE_PROFILING "Enable profiling flags" OFF)
option (ENABLE_GCOV "Enable gcov/coverage flags" OFF)
option (BUILD_CLI_CLIENT "Build command-line interface client" ON)
option (BUILD_SERVER "Build test server" ON)
option (BUILD_TESTS "Build test targets" ON)
option (ENABLE_NATPMP_SUPPORT "Enable NAT-PMP support" ON)
option (ENABLE_EXPERIMENTAL "Enable experimental PCP options support" OFF)
option (ENABLE_FLOW_PRIORITY "Enable FLOW-PRIORITY option support" OFF)
option (ENABLE_SADSCP "Enable SADSCP option support" OFF)
set (USE_IPV6_SOCKET ON CACHE BOOL "Use IPv6 socket")

set (WITH_EXPERIMENTAL ${ENABLE_EXPERIMENTAL})

check_symbol_exists (strndup "string.h" HAVE_STRNDUP)

if (NOT WIN32)
	check_symbol_exists (gettimeofday "sys/time.h" HAVE_GETTIMEOFDAY)
endif ()

check_struct_has_member (
	"struct sockaddr"
	sa_len
	"sys/types.h;sys/socket.h"
	HAVE_SOCKADDR_SA_LEN
)

check_include_file (net/if_dl.h HAVE_NET_IF_DL_H)

if (HAVE_STRNDUP)
	add_definitions (-DHAVE_STRNDUP)
endif ()

if (HAVE_GETTIMEOFDAY)
	add_definitions (-DHAVE_GETTIMEOFDAY)
endif ()

if (HAVE_SOCKADDR_SA_LEN)
	add_definitions (-DHAVE_SOCKADDR_SA_LEN)
endif ()

if (HAVE_NET_IF_DL_H)
	add_definitions (-DHAVE_SOCKADDR_DL_STRUCT)
endif ()

if (WIN32)
	add_definitions (-DPLATFORM_WIN32)
elseif (APPLE)
	add_definitions (-DPLATFORM_APPLE)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
	add_definitions (-DPLATFORM_LINUX)
endif ()

if (WITH_EXPERIMENTAL)
add_definitions (-DPCP_EXPERIMENTAL)
endif ()

if (ENABLE_FLOW_PRIORITY)
add_definitions (-DPCP_FLOW_PRIORITY)
endif ()

if (ENABLE_SADSCP)
add_definitions (-DPCP_SADSCP)
endif ()

if (ENABLE_NATPMP_SUPPORT)
add_definitions (-DPCP_MIN_SUPPORTED_VERSION=0)
else ()
add_definitions (-DPCP_DISABLE_NATPMP_SUPPORT -DPCP_MIN_SUPPORTED_VERSION=1)
endif ()

add_definitions (-DPCP_MAX_SUPPORTED_VERSION=2)

if (ENABLE_DEBUG)
if (MSVC)
add_compile_options (/Zi /Od)
else ()
add_compile_options (-g -O0)
endif ()
endif ()

if (ENABLE_PROFILING)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
add_compile_options (-pg)
add_link_options (-pg)
endif ()
endif ()

if (ENABLE_GCOV)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
add_compile_options (--coverage)
add_link_options (--coverage)
endif ()
endif ()

if  (USE_IPV6_SOCKET)
add_definitions (-DPCP_USE_IPV6_SOCKET)
endif ()

if (WIN32)
	add_definitions (-D_CRT_SECURE_NO_WARNINGS)
endif ()

add_subdirectory (lib)

if (BUILD_CLI_CLIENT)
add_subdirectory (cli-client)
endif ()

if (BUILD_SERVER)
add_subdirectory (test-server)
endif ()

if (BUILD_TESTS)
add_subdirectory (tests)
endif ()

set (PCPNATPMP_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/pcpnatpmp")

configure_package_config_file (
	${CMAKE_CURRENT_SOURCE_DIR}/pcpnatpmpConfig.cmake.template
	${CMAKE_CURRENT_BINARY_DIR}/pcpnatpmpConfig.cmake
	INSTALL_DESTINATION ${PCPNATPMP_CMAKE_INSTALL_DIR}
)

write_basic_package_version_file (
	${CMAKE_CURRENT_BINARY_DIR}/pcpnatpmpConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)

install (EXPORT pcpnatpmpTargets
	FILE pcpnatpmpTargets.cmake
	NAMESPACE pcpnatpmp::
	DESTINATION ${PCPNATPMP_CMAKE_INSTALL_DIR}
)

install (FILES
	${CMAKE_CURRENT_BINARY_DIR}/pcpnatpmpConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/pcpnatpmpConfigVersion.cmake
	DESTINATION ${PCPNATPMP_CMAKE_INSTALL_DIR}
)
