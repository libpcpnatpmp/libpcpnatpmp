#!/bin/sh

killall pcp-server
sleep 0.1
pcp-server --ear 1 > /dev/null &
PCP_SERVER_PID=$!
sleep 0.1
$VALGRIND test_pcp_client_map_opcode
EXIT_STATUS=$?

kill $PCP_SERVER_PID 2>/dev/null

exit $EXIT_STATUS
