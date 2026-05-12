include_directories (../lib ../lib/src ../lib/src/net)
if (WIN32)
    include_directories (../lib/src/windows ../win_utils)
endif ()

function (register_pcp_test target)
    add_test (NAME ${target} COMMAND ${target})
endfunction ()

if (NOT WIN32)
    find_program (BASH_PROGRAM bash REQUIRED)

    function (register_pcp_shell_test name script)
        add_test (
            NAME ${name}
            COMMAND ${CMAKE_COMMAND} -E env
                "PATH=$<TARGET_FILE_DIR:pcpnatpmpc>:$<TARGET_FILE_DIR:pcp-server>:${CMAKE_CURRENT_BINARY_DIR}:$ENV{PATH}"
                ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/${script}
        )
        set_tests_properties (${name} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endfunction ()
endif ()

add_executable (test_flow_notify test_flow_notify.c)
add_executable (test_event_handler test_event_handler.c)
add_executable (test_gateway test_gateway.c)
add_executable (test_lifetime_renewal test_lifetime_renewal.c)
add_executable (test_pcp_api test_pcp_api.c)
set (TEST_PCP_CLI_NONCE_SRC
    test_pcp_cli_nonce.c
)
if (WIN32)
    list (APPEND TEST_PCP_CLI_NONCE_SRC
        ../win_utils/getopt.c
        ../win_utils/getopt_long.c
        ../lib/src/windows/pcp_gettimeofday.c
    )
endif ()
add_executable (test_pcp_cli_nonce ${TEST_PCP_CLI_NONCE_SRC})
add_executable (test_pcp_client_db test_pcp_client_db.c)
add_executable (test_pcp_client_map_opcode test_pcp_client_map_opcode.c)
add_executable (test_pcp_client_peer_opcode test_pcp_client_peer_opcode.c)
add_executable (test_pcp_logger test_pcp_logger.c)
add_executable (test_pcp_msg test_pcp_msg.c)
add_executable (test_ping_gws test_server_discovery.c)
add_executable (test_server_reping test_server_reping.c)
add_executable (test_server_restart test_server_restart.c)
add_executable (test_sock_ntop test_sock_ntop.c)
add_executable (test_version_negotiation test_version_negotiation.c)

target_link_libraries (test_flow_notify pcpnatpmp)
target_link_libraries (test_event_handler pcpnatpmp)
target_link_libraries (test_gateway pcpnatpmp)
target_link_libraries (test_lifetime_renewal pcpnatpmp)
target_link_libraries (test_pcp_api pcpnatpmp)
target_link_libraries (test_pcp_cli_nonce pcpnatpmp)
target_link_libraries (test_pcp_client_db pcpnatpmp)
target_link_libraries (test_pcp_client_map_opcode pcpnatpmp)
target_link_libraries (test_pcp_client_peer_opcode pcpnatpmp)
target_link_libraries (test_pcp_logger pcpnatpmp)
target_link_libraries (test_pcp_msg pcpnatpmp)
target_link_libraries (test_ping_gws pcpnatpmp)
target_link_libraries (test_server_reping pcpnatpmp)
target_link_libraries (test_server_restart pcpnatpmp)
target_link_libraries (test_sock_ntop pcpnatpmp)
target_link_libraries (test_version_negotiation pcpnatpmp)

register_pcp_test (test_event_handler)
register_pcp_test (test_gateway)
register_pcp_test (test_pcp_api)
register_pcp_test (test_pcp_cli_nonce)
register_pcp_test (test_pcp_client_db)
register_pcp_test (test_pcp_logger)
register_pcp_test (test_pcp_msg)
register_pcp_test (test_ping_gws)
register_pcp_test (test_sock_ntop)

if (NOT WIN32)
    register_pcp_shell_test (test_flow_notify_sh test_flow_notify.sh)
    register_pcp_shell_test (test_version_negotiation_sh
                             test_version_negotiation.sh)
    register_pcp_shell_test (test_pcp_client_map_opcode_sh
                             test_pcp_client_map_opcode.sh)
    register_pcp_shell_test (test_pcp_client_peer_opcode_sh
                             test_pcp_client_peer_opcode.sh)
    register_pcp_shell_test (test_server_restart_sh test_server_restart.sh)
    register_pcp_shell_test (test_lifetime_renewal_sh test_lifetime_renewal.sh)
    register_pcp_shell_test (test_pcp_cli_client_sh test_pcp_cli_client.sh)
    register_pcp_shell_test (test_pcp_server_sh test_pcp_server.sh)
    register_pcp_shell_test (test_server_reping_sh test_server_reping.sh)

    if (WITH_EXPERIMENTAL)
        register_pcp_shell_test (test_get_dscp_sh test_get_dscp.sh)
        register_pcp_shell_test (test_flow_md_sh test_flow_md.sh)
    endif ()
endif ()
