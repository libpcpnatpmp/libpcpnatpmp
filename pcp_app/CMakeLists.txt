set (PCP_CLIENT_SRC
    pcp_app.c
)

if (WIN32)
    list (APPEND PCP_CLIENT_SRC
        ../win_utils/getopt.c
        ../win_utils/getopt_long.c
        ../libpcp/src/windows/pcp_gettimeofday.c
    )
endif ()

add_executable (pcpnatpmpc ${PCP_CLIENT_SRC})

target_link_libraries (pcpnatpmpc pcpnatpmp)
target_include_directories (pcpnatpmpc PRIVATE
    ../libpcp
    ../libpcp/src
    ../libpcp/src/net
)

if (WIN32)
    target_include_directories (pcpnatpmpc PRIVATE
        ../libpcp
        ../libpcp/src
        ../libpcp/src/net
        ../libpcp/src/windows
        ../win_utils
    )
endif ()
