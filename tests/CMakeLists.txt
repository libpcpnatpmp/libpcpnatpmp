include_directories (../lib ../lib/src ../lib/src/net ../test-server)
if (WIN32)
    include_directories (../lib/src/windows ../win_utils)
endif ()

find_package (Threads REQUIRED)

function (register_pcp_test target)
    add_test (NAME ${target} COMMAND ${target})
endfunction ()

if (NOT WIN32)
    find_program (SH_PROGRAM sh REQUIRED)

    function (register_pcp_shell_test name script)
        add_test (
            NAME ${name}
            COMMAND ${CMAKE_COMMAND} -E env
                "PATH=$<TARGET_FILE_DIR:pcpnatpmpc>:$<TARGET_FILE_DIR:pcp-server>:${CMAKE_CURRENT_BINARY_DIR}:$ENV{PATH}"
                ${SH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/${script}
        )
        set_tests_properties (${name} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endfunction ()
endif ()

set (TEST_PCP_SERVER_HELPER_SOURCES
    test_pcp_server_helper.c
    ../test-server/pcp-server.c
)
set (TEST_PROCESS_HELPER_SOURCES
    test_process_helper.c
)

set (TEST_FLOW_NOTIFY_SOURCES
    test_flow_notify.c
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_flow_notify ${TEST_FLOW_NOTIFY_SOURCES})
add_executable (test_event_handler test_event_handler.c)
add_executable (test_gateway test_gateway.c)
set (TEST_LIFETIME_RENEWAL_SOURCES
    test_lifetime_renewal.c
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_lifetime_renewal ${TEST_LIFETIME_RENEWAL_SOURCES})
add_executable (test_pcp_api test_pcp_api.c)
set (TEST_PCP_CLI_NONCE_SRC
    test_pcp_cli_nonce.c
    ${TEST_PROCESS_HELPER_SOURCES}
    ../cli-client/pcp_cli_nonce.c
)
add_executable (test_pcp_cli_nonce ${TEST_PCP_CLI_NONCE_SRC})
add_dependencies (test_pcp_cli_nonce pcpnatpmpc)
set (TEST_PCP_CLI_CLIENT_SOURCES
    test_pcp_cli_client.c
    ${TEST_PROCESS_HELPER_SOURCES}
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_pcp_cli_client ${TEST_PCP_CLI_CLIENT_SOURCES})
add_dependencies (test_pcp_cli_client pcpnatpmpc)
add_dependencies (test_pcp_cli_client pcp-server)
add_executable (test_pcp_client_db test_pcp_client_db.c)
set (TEST_PCP_CLIENT_MAP_OPCODE_SOURCES
    test_pcp_client_map_opcode.c
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_pcp_client_map_opcode
                ${TEST_PCP_CLIENT_MAP_OPCODE_SOURCES})
set (TEST_PCP_CLIENT_PEER_OPCODE_SOURCES
    test_pcp_client_peer_opcode.c
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_pcp_client_peer_opcode
                ${TEST_PCP_CLIENT_PEER_OPCODE_SOURCES})
add_executable (test_pcp_logger test_pcp_logger.c)
add_executable (test_pcp_msg test_pcp_msg.c)
add_executable (test_ping_gws test_server_discovery.c)
set (TEST_PCP_SERVER_SOURCES
    test_pcp_server.c
    ${TEST_PROCESS_HELPER_SOURCES}
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_pcp_server ${TEST_PCP_SERVER_SOURCES})
add_dependencies (test_pcp_server pcp-server)
set (TEST_SERVER_REPING_SOURCES
    test_server_reping.c
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_server_reping ${TEST_SERVER_REPING_SOURCES})
set (TEST_SERVER_RESTART_SOURCES
    test_server_restart.c
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_server_restart ${TEST_SERVER_RESTART_SOURCES})
add_executable (test_sock_ntop test_sock_ntop.c)
set (TEST_VERSION_NEGOTIATION_SOURCES
    test_version_negotiation.c
    ${TEST_PCP_SERVER_HELPER_SOURCES}
)
add_executable (test_version_negotiation ${TEST_VERSION_NEGOTIATION_SOURCES})

target_link_libraries (test_flow_notify pcpnatpmp)
target_link_libraries (test_event_handler pcpnatpmp)
target_link_libraries (test_gateway pcpnatpmp)
target_link_libraries (test_lifetime_renewal pcpnatpmp)
target_link_libraries (test_pcp_api pcpnatpmp)
target_link_libraries (test_pcp_cli_client pcpnatpmp)
target_link_libraries (test_pcp_cli_nonce pcpnatpmp)
target_link_libraries (test_pcp_client_db pcpnatpmp)
target_link_libraries (test_pcp_client_map_opcode pcpnatpmp)
target_link_libraries (test_pcp_client_peer_opcode pcpnatpmp)
target_link_libraries (test_pcp_logger pcpnatpmp)
target_link_libraries (test_pcp_msg pcpnatpmp)
target_link_libraries (test_pcp_server pcpnatpmp)
target_link_libraries (test_ping_gws pcpnatpmp)
target_link_libraries (test_server_reping pcpnatpmp)
target_link_libraries (test_server_restart pcpnatpmp Threads::Threads)
target_link_libraries (test_sock_ntop pcpnatpmp)
target_link_libraries (test_version_negotiation pcpnatpmp)

target_compile_definitions (test_pcp_cli_client PRIVATE
    PCP_CLI_CLIENT_EXE=\"$<TARGET_FILE:pcpnatpmpc>\"
    PCP_TEST_SERVER_EXE=\"$<TARGET_FILE:pcp-server>\"
)
target_compile_definitions (test_pcp_cli_nonce PRIVATE
    PCP_CLI_CLIENT_EXE=\"$<TARGET_FILE:pcpnatpmpc>\"
)
target_compile_definitions (test_pcp_server PRIVATE
    PCP_TEST_SERVER_EXE=\"$<TARGET_FILE:pcp-server>\"
)

register_pcp_test (test_event_handler)
register_pcp_test (test_flow_notify)
register_pcp_test (test_gateway)
register_pcp_test (test_lifetime_renewal)
register_pcp_test (test_pcp_api)
register_pcp_test (test_pcp_cli_client)
register_pcp_test (test_pcp_cli_nonce)
register_pcp_test (test_pcp_client_db)
register_pcp_test (test_pcp_client_map_opcode)
register_pcp_test (test_pcp_client_peer_opcode)
register_pcp_test (test_pcp_logger)
register_pcp_test (test_pcp_msg)
register_pcp_test (test_pcp_server)
register_pcp_test (test_ping_gws)
register_pcp_test (test_server_reping)
register_pcp_test (test_server_restart)
register_pcp_test (test_sock_ntop)
register_pcp_test (test_version_negotiation)

if (NOT WIN32)
    if (WITH_EXPERIMENTAL AND TARGET pcpnatpmpc AND TARGET pcp-server)
        register_pcp_shell_test (test_get_dscp_sh test_get_dscp.sh)
        register_pcp_shell_test (test_flow_md_sh test_flow_md.sh)
    endif ()
endif ()
