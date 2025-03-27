Installation Instructions
=========================

Basic installation
------------------

    ./autogen.sh
    ./configure
    make
    sudo make install

Configuration options
---------------------

For all available configuration option check `./configure -h` output.

### Examples ###

*Set compile time log level:*

    ./configure CPPFLAGS="-DPCP_MAX_LOG_LEVEL=5"

*Compile library only:*

    ./configure --disable-server --disable-cli-client
