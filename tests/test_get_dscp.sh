#!/bin/bash
echo "########################################"
echo "####   ***********************     ####"
echo "####   * Begin get DSCP test *     ####"
echo "####   ***********************     ####"
echo "#######################################"
killall pcp-server
pcp-server --ret-dscp 10 --ear 1 --timeout 1000 >/dev/null &
sleep 0.1
$VALGRIND pcpnatpmpc --server 127.0.0.1 -J 1 -d
