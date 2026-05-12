#!/bin/sh

pcp-server --ear 1 >/dev/null &
PCP_SERVER_PID=$!

sleep 0.1
$VALGRIND test_pcp_client_peer_opcode
EXIT_STATUS=$?

#clean up after test in case server was left active
#ps -p $PCP_SERVER_PID
#VAL=$?
#if [[ $VAL -ne 0 ]]; then
#	echo "DEBUG"
#	kill -9 $PCP_SERVER_PID
#fi
kill $PCP_SERVER_PID 2> /dev/null
exit $EXIT_STATUS
