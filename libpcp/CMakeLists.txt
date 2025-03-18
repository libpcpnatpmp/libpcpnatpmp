set(SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/libpcp/src
        )
set(INCLUDE_FILES
        ${CMAKE_SOURCE_DIR}/libpcp/include
        )
if (WIN32)
set(WIN_INCLUDE_FILES
        ${CMAKE_SOURCE_DIR}/libpcp/include/windows
		${CMAKE_SOURCE_DIR}/win_utils
        )
set(WIN_SOURCE_FILES
        ${CMAKE_SOURCE_DIR}/libpcp/src/windows
		${CMAKE_SOURCE_DIR}/libpcp/src/
        )
endif()

# include directories with source and header files
include_directories(${SOURCE_FILES} ${SOURCE_FILES}/net/ ${INCLUDE_FILES})

if (WIN32)
include_directories(${WIN_INCLUDE_FILES} ${WIN_SOURCE_FILES})
endif()

set(LIBPCP_SRC_FILES
    ${SOURCE_FILES}/net/gateway.c
    ${SOURCE_FILES}/net/findsaddr-udp.c

    ${SOURCE_FILES}/pcp_api.c
    ${SOURCE_FILES}/pcp_client_db.c
    ${SOURCE_FILES}/pcp_event_handler.c
    ${SOURCE_FILES}/pcp_logger.c
    ${SOURCE_FILES}/pcp_msg.c
    ${SOURCE_FILES}/pcp_server_discovery.c
    ${SOURCE_FILES}/net/sock_ntop.c
    ${SOURCE_FILES}/net/pcp_socket.c
    )

if (WIN32)
set(LIBPCP_SRC_FILES
    ${LIBPCP_SRC_FILES}
    ${WIN_SOURCE_FILES}/windows/pcp_gettimeofday.c
    )
endif()

# header files for building pcp library
set(LIBPCP_INC_FILES
    ${INCLUDE_FILES}/pcp.h
    ${SOURCE_FILES}/pcp_client_db.h
    ${SOURCE_FILES}/pcp_event_handler.h
    ${SOURCE_FILES}/pcp_logger.h
    ${SOURCE_FILES}/pcp_msg.h
    ${SOURCE_FILES}/pcp_server_discovery.h
    ${SOURCE_FILES}/net/unp.h
    ${SOURCE_FILES}/net/pcp_socket.h
    ${SOURCE_FILES}/net/gateway.h
    ${SOURCE_FILES}/net/findsaddr.h
    ${INCLUDE_FILES}
    )

# additional header file when compiling on Windows
if (WIN32)
set(LIBPCP_INC_FILES
    ${LIBPCP_INC_FILES}
    ${WIN_SOURCE_FILES}/windows/pcp_win_defines.h
    ${WIN_SOURCE_FILES}/windows/pcp_gettimeofday.h
    )
endif()

# create getopt library and link it against pcp library on win platform
#if (WIN32)
#add_library(libgetopt
#   STATIC
#   ${WIN_SOURCE_FILES}/getopt.c
#   ${WIN_SOURCE_FILES}/getopt_long.c
#   ${WIN_INCLUDE_FILES}/getopt.h
#   )
#endif()

add_library(${LIB_LIBPCP}
    STATIC
    ${LIBPCP_SRC_FILES}
    ${LIBPCP_INC_FILES}
    )

if (WIN32)
target_link_libraries(${LIB_LIBPCP})
endif()

