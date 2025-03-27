set (PCP_SERVER_INC_DIRS
    ../lib
    ../lib/src
    ../lib/include
    ../lib/src/net
)
    
set (PCP_SERVER_SOURCES
    pcp-server.c
)

if (WIN32)
    list (APPEND PCP_SERVER_SOURCES
        ../win_utils/getopt.c
        ../win_utils/getopt_long.c
        ../lib/src/net/pcp_socket.c
        ../lib/src/windows/pcp_gettimeofday.c
        ../lib/src/pcp_logger.c
    )
    list (APPEND PCP_SERVER_INC_DIRS
        ../lib/src/windows
        ../win_utils
    )
endif ()

add_executable(pcp-server ${PCP_SERVER_SOURCES})
target_include_directories(pcp-server PRIVATE ${PCP_SERVER_INC_DIRS})

if (WIN32)
    target_link_libraries (pcp-server PRIVATE ws2_32.lib Iphlpapi.lib)
endif()
