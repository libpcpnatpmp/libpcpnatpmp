#!/bin/sh
VERSION=${1:-2}

if [ $VERSION -ge 3 ]; then
    echo "Version number must be less than 3 for this test";
    exit 1;
fi

killall pcp-server

sleep 0.1

pcp-server -v 1 --ear 2 >/dev/null &
PCP_SERVER_PID=$!

sleep 0.1
$VALGRIND test_version_negotiation $VERSION
EXIT_STATUS=$?


# now kill the server, if its still running
kill $PCP_SERVER_PID
exit $EXIT_STATUS
