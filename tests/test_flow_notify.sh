#!/bin/sh
killall pcp-server
sleep 0.1
pcp-server -r 0 --ext-ip ::ffff:10.20.30.40 --ear 1 && pcp-server --ip ::1 -r 3 --ext-ip ::ffff:10.20.30.40 --ear 1 &
sleep 1
$VALGRIND test_flow_notify
