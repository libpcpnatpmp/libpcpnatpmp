#!/bin/sh

killall pcp-server
rm -f /tmp/test_lf_rn
sleep 0.1
pcp-server --ear 4 >/dev/null && echo "1" > /tmp/test_lf_rn && pcp-server --ear 1 >/dev/null &
PCP_SERVER_PID=$!
echo "PID=${PCP_SERVER_PID}"
sleep 0.1
$VALGRIND test_lifetime_renewal 2
rm /tmp/test_lf_rn

EXIT_STATUS=$?
kill $PCP_SERVER_PID
if [ $EXIT_STATUS -eq 0 ] && [ $? -eq 0 ]
then
    EXIT_STATUS=0
else
    EXIT_STATUS=1
fi

echo "EXIT_STATUS=${EXIT_STATUS}"
killall pcp-server
exit $EXIT_STATUS
