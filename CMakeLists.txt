cmake_minimum_required(VERSION 3.14)

project (pcpnatpmp-client)

set (WITH_EXPERIMENTAL ON CACHE BOOL "Experimental extensions suppport")
set (USE_IPV6_SOCKET ON CACHE BOOL "Use IPv6 socket")

if (WITH_EXPERIMENTAL)
add_definitions (-DPCP_SADSCP -DPCP_EXPERIMENTAL -DPCP_FLOW_PRIORITY)
endif ()

if  (USE_IPV6_SOCKET)
add_definitions (-DPCP_USE_IPV6_SOCKET)
endif ()

if (WIN32)
	add_definitions (-D_CRT_SECURE_NO_WARNINGS)
endif ()

add_subdirectory (lib)
add_subdirectory (cli-client)
add_subdirectory (test-server)
add_subdirectory (tests)
