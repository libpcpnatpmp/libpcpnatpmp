#!/bin/bash

function echo_exit {
    echo $1
    exit 1
}

echo "########################################"
echo "####        Test cli client         ####"
echo "########################################"
killall pcp-server

pcpnatpmpc >/tmp/pcp-help.out 2>/dev/null
grep -q Usage: /tmp/pcp-help.out || echo_exit "Failed test of printing command line usage #1"

pcpnatpmpc --help 2>/dev/null | diff -q - /tmp/pcp-help.out ||echo_exit "Failed test of printing command line usage #2"

pcpnatpmpc -h 2>/dev/null | diff -q - /tmp/pcp-help.out || echo_exit "Failed test of printing command line usage #3"

pcpnatpmpc --bad-argument-input 2>/dev/null &>/dev/null &&  echo_exit "Failed test of bad arguments on input #1"

pcpnatpmpc --peer ::0bad:ip6 &>/dev/null &&  echo_exit "Failed test of bad arguments on input #2"

pcpnatpmpc --external ::0bad:ip6 &>/dev/null &&  echo_exit "Failed test of bad arguments on input #3"

pcpnatpmpc --external [::2]:99999  &>/dev/null &&  echo_exit "Failed test of bad arguments on input #4"

pcpnatpmpc --internal [::0bad:ip6]:1234 &>/dev/null &&  echo_exit "Failed test of bad arguments on input #5"

pcpnatpmpc --disable-autodiscovery --server 1.2.3.4:1234 --peer 1.2.3.4:1234 --internal 0.0.0.0:4321 --external 1.1.1.1:4321 --protocol 17 --lifetime 120 --pcp-version 1 --timeout 0 &>/dev/null

[ $? -eq 2 ] || echo_exit "Failed long-opt test"

pcpnatpmpc -v 3 --internal :1234 &>/dev/null && echo_exit "Failed in bad pcp-version test #1"

pcpnatpmpc -d &> /dev/null  && echo_exit "Failed in not enough params test"

pcpnatpmpc --pcp-version 3 --internal :1234 &>/dev/null && echo_exit "Failed in bad version test #2"

pcpnatpmpc -v 1 -q &>/dev/null && echo_exit "Failed in bad arg test #1"
pcpnatpmpc - &>/dev/null && echo_exit "Failed in bad argument test #2"

pcpnatpmpc --internal :1234 --jitter-tolerance 1 &>/dev/null && echo_exit "Failed in test of mutual exclusivity of MAP_PEER and SADSCP parameters"

pcp-server --ear 1 -r 8 &>/dev/null &
sleep 0.1

pcpnatpmpc --server 127.0.0.1 -f --disable-autodiscovery --pcp-version 1 --peer 127.0.0.1:1234 --internal :1234 &>/dev/null

[ $? -eq 3 ] || echo_exit "Failed in recieved short lifetime error"

killall pcp-server &>/dev/null && sleep 0.1

pcp-server --ear 1 -r 3 &>/dev/null &
sleep 0.1

pcpnatpmpc --server 127.0.0.1 -d --pcp-version 1 --peer 127.0.0.1:1111 --fast-return --internal :1234 &>/dev/null

[ $? -eq 4 ] || echo_exit "Failed in recieved error test"

killall pcp-server &>/dev/null && sleep 0.1

pcp-server --ear 1 -r 8 &>/dev/null &

sleep 0.1
pcpnatpmpc --server 127.0.0.1 -f --pcp-version 1 --internal=:1234 &>/dev/null

[ $? -eq 2 -o $? -eq 3 -o $? -eq 4 ] || echo_exit "Failed in partial short lifetime error test."

killall pcp-server &>/dev/null && sleep 0.1

pcp-server --ear 1 -r 3 &>/dev/null &

sleep 0.1
pcpnatpmpc --server 127.0.0.1 --pcp-version 1 --fast-return -i :1234 &>/dev/null

[ $? -eq 2 ] || echo_exit "Failed in partial error test."

killall pcp-server 2>/dev/null &>/dev/null && sleep 0.1

pcp-server --ear 1 &>/dev/null &

sleep 0.1 && pcpnatpmpc -s 127.0.0.1 --pcp-version 2 --fast-return -i :1234 &>/dev/null

[ $? -eq 0 ] || echo_exit "Failed in partial success test."

sleep 0.1
pcpnatpmpc -s 8.8.8.2 --pcp-version 2 --fast-return -i :1234 --timeout 0 &>/dev/null

[ $? -eq 2 ] || echo_exit "Failed in timeout test."

if [ "$PCP_USE_IPV6_SOCKET" == "1" ] ; then
  echo "Testing IPv6 support"
  pcp-server --ip ::1 -v 1 --ear 2 &>/dev/null &

  sleep 0.1
  pcpnatpmpc --pcp-version 2 --server ::1 --internal [::]:1234 --peer [::]:4321 --fast-return &>/dev/null

  [ $? -eq 0 ] || echo_exit "Failed in IPv6 MAP success test."

  pcp-server --ip ::1 -v 1 --ear 2 &>/dev/null &

  sleep 0.1
  pcpnatpmpc --pcp-version 2 -s ::1 -i [::]:1234 -p [::1]:1234 &>/dev/null

  [ $? -eq 0 ] || echo_exit "Failed in IPv6 PEER success test."

  sleep 0.1
  pcpnatpmpc --pcp-version 2 -i [::]:1234 -p [::1]:4321 &>/dev/null

  [ $? -gt 0 ] || echo_exit "Failed in IPv6 PEER failure test."
fi

killall pcp-server &>/dev/null
pcp-server --ear 1 &>/dev/null &
sleep 0.1
pcpnatpmpc -d -s 127.0.0.1 -i :1234 -P &>/dev/null
[ $? -eq 0 ] || echo_exit "Failed sending MAP with PREFER FAILURE option."

pcp-server --ear 1 &>/dev/null &
sleep 0.1
pcpnatpmpc -d -s 127.0.0.1 -i :1234 -p 8.8.8.8:3333 -P &>/dev/null
[ $? -eq 1 ] || echo_exit "Failed testing PEER with PREFER FAILURE option."

pcp-server --ear 1 &>/dev/null &
sleep 0.1
pcpnatpmpc -d -s 127.0.0.1 -i :1234 -F [8.8.8.8/12]:4444 &>/dev/null
[ $? -eq 0 ] || echo_exit "Failed testing MAP with FILTER option."

pcp-server --ear 1 &>/dev/null &
sleep 0.1
pcpnatpmpc -s 127.0.0.1 -i :1234 -p 8.8.8.8:3333 -F [8.8.8.8/12]:4444 &>/dev/null
[ $? -eq 1 ] || echo_exit "Failed testing PEER with FILTER option."

pcp-server --ear 1 &>/dev/null &
pcp-server --ear 1 --ip 127.0.0.2 -r 7 &>/dev/null &
pcp-server --ear 1 --ip 127.0.0.3 -r 11 &>/dev/null &
sleep 0.1
pcpnatpmpc -s 127.0.0.1 -s 127.0.0.2 -s 127.0.0.3 -i :1234 -d &>/dev/null &
[ $? -eq 0 ] || echo_exit "Failed testing different result codes."

exit 0
