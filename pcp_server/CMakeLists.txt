set (PCP_SERVER_INC_DIRS
    ../libpcp
    ../libpcp/src
    ../libpcp/include
    ../libpcp/src/net
)
    
set (PCP_SERVER_SOURCES
    pcp_server.c
)

if (WIN32)
    list (APPEND PCP_SERVER_SOURCES
        ../win_utils/getopt.c
        ../win_utils/getopt_long.c
        ../libpcp/src/net/pcp_socket.c
        ../libpcp/src/windows/pcp_gettimeofday.c
        ../libpcp/src/pcp_logger.c
    )
    list (APPEND PCP_SERVER_INC_DIRS
        ../libpcp/src/windows
        ../win_utils
    )
endif ()

add_executable(pcp-server ${PCP_SERVER_SOURCES})
target_include_directories(pcp-server PRIVATE ${PCP_SERVER_INC_DIRS})

if (WIN32)
    target_link_libraries (pcp-server PRIVATE ws2_32.lib Iphlpapi.lib)
endif()
