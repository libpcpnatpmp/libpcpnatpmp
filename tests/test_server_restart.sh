#!/bin/sh

killall pcp-server
# first server
pcp-server --ear 1 >/dev/null && pcp-server --ear 2 > /dev/null &
sleep 4
$VALGRIND test_server_restart

EXIT_STATUS=$?

killall pcp-server

[ $EXIT_STATUS -eq 0 ] || exit 1

pcp-server --ear 1 >/dev/null && pcp-server --ear 1 > /dev/null &

sleep 4

$VALGRIND test_server_restart fail

EXIT_STATUS=$?

killall pcp-server

exit $EXIT_STATUS
