Installation Instructions
=========================

Preferred build system: CMake
-----------------------------

Basic build and install:

    cmake -S . -B build
    cmake --build build
    sudo cmake --install build

Install to a custom prefix:

    cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build
    sudo cmake --install build

Common CMake options
--------------------

Enable optional components:

    -DBUILD_CLI_CLIENT=ON|OFF
    -DBUILD_SERVER=ON|OFF
    -DBUILD_TESTS=ON|OFF

Enable/disable protocol and feature flags:

    -DENABLE_NATPMP_SUPPORT=ON|OFF
    -DUSE_IPV6_SOCKET=ON|OFF
    -DENABLE_EXPERIMENTAL=ON|OFF
    -DENABLE_FLOW_PRIORITY=ON|OFF
    -DENABLE_SADSCP=ON|OFF

Build modes and instrumentation:

    -DENABLE_DEBUG=ON|OFF
    -DENABLE_PROFILING=ON|OFF
    -DENABLE_GCOV=ON|OFF

Example:

    cmake -S . -B build -DBUILD_SERVER=OFF -DBUILD_CLI_CLIENT=OFF
    cmake --build build

Alternative build system: autotools
-----------------------------------

    ./autogen.sh
    ./configure
    make
    sudo make install

Autotools configuration examples:

    ./configure CPPFLAGS="-DPCP_MAX_LOG_LEVEL=5"

Build library-only style setup:

    ./configure --disable-server --disable-cli-client
