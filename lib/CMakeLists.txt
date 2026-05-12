set (LIBPCP_SRC_FILES
    src/net/gateway.c
    src/net/findsaddr-udp.c
    src/pcp_api.c
    src/pcp_client_db.c
    src/pcp_event_handler.c
    src/pcp_logger.c
    src/pcp_msg.c
    src/pcp_server_discovery.c
    src/net/sock_ntop.c
    src/net/pcp_socket.c
)

if (WIN32)
    list (APPEND LIBPCP_SRC_FILES
         src/windows/pcp_gettimeofday.c
    )
endif ()

include (GNUInstallDirs)

# header files for building PCP/NAT-PMP client library
set (LIBPCP_INC_FILES
    include/pcpnatpmp.h
    src/pcp_client_db.h
    src/pcp_event_handler.h
    src/pcp_logger.h
    src/pcp_msg.h
    src/pcp_server_discovery.h
    src/net/unp.h
    src/net/pcp_socket.h
    src/net/gateway.h
    src/net/findsaddr.h
)

# additional header file when compiling on Windows
if (WIN32)
    list (APPEND LIBPCP_INC_FILES
        src/windows/pcp_win_defines.h
        src/windows/pcp_gettimeofday.h
    )
endif ()

add_library (pcpnatpmp
    ${LIBPCP_SRC_FILES}
    ${LIBPCP_INC_FILES}
)

set_target_properties (pcpnatpmp PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

if (WIN32)
    target_link_libraries (pcpnatpmp PUBLIC ws2_32.lib Iphlpapi.lib)
endif ()

# include directories with source and header files
target_include_directories (pcpnatpmp PRIVATE src/ src/net/ .)

# include directories with source and header files
target_include_directories (pcpnatpmp PUBLIC include)

if (WIN32)
    target_include_directories (pcpnatpmp PRIVATE src/windows ../win_utils)
endif ()

configure_file (
    ${CMAKE_CURRENT_SOURCE_DIR}/libpcpnatpmp.pc.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/libpcpnatpmp.pc
    @ONLY
)

install (TARGETS pcpnatpmp
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/include/pcpnatpmp.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install (FILES ${CMAKE_CURRENT_BINARY_DIR}/libpcpnatpmp.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
