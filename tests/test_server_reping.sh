#!/bin/sh

killall pcp-server
pcp-server --ear 4 >/dev/null 2>&1 &
sleep 1
test_server_reping
