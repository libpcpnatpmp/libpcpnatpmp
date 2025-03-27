#!/bin/bash

function echo_exit {
    echo $1
    rm test.log
    exit 1
}

echo "#######################################"
echo "####     Test pcp mock server      ####"
echo "#######################################"

killall pcp-server &>/dev/null

pcp-server -h >/tmp/pcp-server-help.out &
sleep 0.1
grep -q Usage: /tmp/pcp-server-help.out > /dev/null || echo_exit "Failed printing command line usage #1"
pcp-server --help | diff -q - /tmp/pcp-server-help.out || echo_exit "Failed printing command line usage #2"
pcp-server -q &>/dev/null && echo_exit "Unknown option test failed #1"
pcp-server --bad-argument &>/dev/null && echo_exit "Unknown option test failed #2"


#Test timeout option because it is used latger on as part of other tests
killall pcp-server &>/dev/null
pcp-server --timeout 500 &>/dev/null &
sleep 1
killall pcp-server && echo_exit "Failed starting server with timeout option #1"
pcp-server --timeout -500 >/tmp/pcp_server_timeout_test.txt &
sleep 0.1
grep -q "Value provided for timeout was negative" /tmp/pcp_server_timeout_test.txt || echo_exit "Failed starting server with timeout option #2"

# Setting default return code tests
pcp-server -r 1 &>/dev/null &
sleep 0.1
killall pcp-server || echo_exit "Failed setting default return code #1"
pcp-server -r 13 &>/dev/null &
sleep 0.1
killall pcp-server || echo_exit "Failed setting default return code #2"
pcp-server -r 100 &>/dev/null &
sleep 0.1
killall pcp-server &>/dev/null && echo_exit "Failed setting default return code #3"
pcp-server -r 255 &>/dev/null &
sleep 0.1
killall pcp-server &>/dev/null || echo_exit "Failed setting default return code #4"

#PCP server version tests
pcp-server -v 1 &>/dev/null &
sleep 0.1
killall pcp-server &>/dev/null || echo_exit "Failed setting pcp-server version #1"
pcp-server -v 2 &>/dev/null &
sleep 0.1
killall pcp-server &>/dev/null || echo_exit "Failed setting pcp-server version #2"
pcp-server -v 3 > /dev/null &
sleep 0.1
killall pcp-server &>/dev/null && echo_exit "Failed setting pcp-server version #3"
pcp-server -v 0 &>/dev/null &
sleep 0.1
killall pcp-server &>/dev/null && echo_exit "Failed setting pcp-server version #4"

# Log file tests
# Tests assumes the log file is being overwritten (current implementation)
pcp-server --log-file test.log --timeout 1000 &>/dev/null &
sleep 0.1
pcpnatpmpc -v 1 -s 127.0.0.1 -i :1234 &>/dev/null &
sleep 1.6
grep -q "PCP protocol VERSION 1." test.log || echo_exit "Failed log file test #1.1"
grep -q "MAP protocol:" test.log || echo_exit "Failed log ile test #1.2"

killall pcp-server &>/dev/null
pcp-server --log-file test.log --timeout 1000 &>/dev/null &
sleep 0.1
pcpnatpmpc -v 2 --server 127.0.0.1 --internal :1234 &>/dev/null &
sleep 1.6
grep -q "PCP protocol VERSION 2." test.log || echo_exit "Failed log file test #2.1"
grep -q "MAP protocol:" test.log || echo_exit "Failed log ile test #2.2"

killall pcp-server &>/dev/null
pcp-server --log-file test.log --timeout 1000 &>/dev/null &
sleep 0.1
pcpnatpmpc -v 1 --server 127.0.0.1 -i :1234 -p 127.0.0.1:8888 &>/dev/null &
sleep 1.6
grep -q "PCP protocol VERSION 1." test.log || echo_exit "Failed log file test #3.1"
grep -q "PEER Opcode specific information." test.log || echo_exit "Failed log ile test #3.2"

killall pcp-server &>/dev/null
pcp-server --log-file test.log --timeout 1000 &>/dev/null &
sleep 0.1
pcpnatpmpc -s 127.0.0.1 -i :1234 -p 127.0.0.1:8888 &>/dev/null &
sleep 1.6
grep -q "PCP protocol VERSION 2." test.log || echo_exit "Failed log file test #4.1"
grep -q "PEER Opcode specific information." test.log || echo_exit "Failed log ile test #4.2"
killall pcp-server &>/dev/null

# Options tests
#pcp-server --log-file test.log --timeout 1000 &>/dev/null &
#sleep 0.1
#pcpnatpmpc -s 127.0.0.1 -i :1234 -p 127.0.0.1:8888 --user-id user@example.com --location --device-id &>/dev/null &
#sleep 1.6
#grep -q "PCP protocol VERSION 2." test.log || echo_exit "Failed log file test #5.1"
#grep -q "PEER Opcode specific information." test.log || echo_exit "Failed log ile test #5.2"
#grep -q "OPTION: 	 Device ID" test.log || echo_exit "Failed log ile test #5.3"
#grep -q "OPTION: 	 User ID" test.log || echo_exit "Failed log ile test #5.4"
#grep -q "OPTION: 	 Location" test.log || echo_exit "Failed log ile test #5.5"
#grep -q "" test.log || echo_exit "Failed log ile test #5.6"
#grep -q "" test.log || echo_exit "Failed log ile test #5.7"
#killall pcp-server &>/dev/null

#pcp-server --log-file test.log --timeout 1000 &>/dev/null &
#sleep 0.1
#pcpnatpmpc -s 127.0.0.1 -i :1234 --metadata-id 96 --metadata-value "testing" &>/dev/null &
#sleep 1.6
#grep -q "PCP protocol VERSION 2." test.log || echo_exit "Failed log file test #6.1"
#grep -q "MAP" test.log || echo_exit "Failed log ile test #6"
#grep -q "OPTION: 	 Metadata" test.log || echo_exit "Failed log ile test #6.2"
#grep -q "METADATA ID" test.log || echo_exit "Failed log ile test #6.3"
#grep -q "METADATA 	 testing" test.log || echo_exit "Failed log ile test #6.4"
#killall pcp-server &>/dev/null

#pcp-server --log-file test.log --timeout 1000 &>/dev/null &
#sleep 0.1
#pcpnatpmpc -s 127.0.0.1 -i :1234 --dscp-up 10 --dscp-down 10 &>/dev/null &
#sleep 1.6
#grep -q "PCP protocol VERSION 2." test.log || echo_exit "Failed log file test #7.1"
#grep -q "MAP" test.log || echo_exit "Failed log file test #7.2"
#grep -q "OPTION: 	 Flow priority" test.log || echo_exit "Failed log file test #7.3"
#grep -q "DSCP UP: 	 10" test.log || echo_exit "Failed log file test #7.4"
#grep -q "DSCP DOWN: 	 10" test.log || echo_exit "Failed log file test #7.5"
#killall pcp-server &>/dev/null

#pcp-server --log-file test.log --timeout 1000 &>/dev/null &
#sleep 0.1
#pcpnatpmpc -s 127.0.0.1 -J 0 -E 1 -L 2 -A Webex &>/dev/null &
#sleep 1.6
#grep -q "PCP protocol VERSION 2." test.log || echo_exit "Failed log file test #8.1"
#grep -q "SADSCP Opcode specific information." test.log || echo_exit "Failed log file test #8.2"
#grep -q "Jitter tolerance:	 0" test.log || echo_exit "Failed log file test #8.3"
#grep -q "Delay tolerance:  	 1" test.log || echo_exit "Failed log file test #8.4"
#grep -q "Loss tolerance: 	 2" test.log || echo_exit "Failed log file test #8.5"
#grep -q "App name:        	 Webex" test.log || echo_exit "Failed log file test #8.6"
#killall pcp-server &>/dev/null

pcp-server --log-file test.log --timeout 1000 &>/dev/null &
sleep 0.1
pcpnatpmpc -s 127.0.0.1 -i :1234 -P &>/dev/null
sleep 1.6
grep -q "PCP protocol VERSION 2." test.log || echo_exit "Failed log file test #9.1"
grep -q "MAP" test.log || echo_exit "Failed log file test #9.2"
grep -q "OPTION: 	 Prefer fail" test.log || echo_exit "Failed log file test #9.3"

pcp-server --log-file test.log --timeout 1000 &>/dev/null &
sleep 0.1
pcpnatpmpc -d -s 127.0.0.1 -i :1234 -F [8.8.8.8/12]:4444 &>/dev/null
sleep 1.6
grep -q "PCP protocol VERSION 2." test.log || echo_exit "Failed log file test #10.1"
grep -q "MAP" test.log || echo_exit "Failed log file test #10.2"
grep -q "OPTION:.*Filter" test.log || echo_exit "Failed log file test #10.3"
grep -q "FILTER PORT:" test.log || echo_exit "Failed log file test #10.4"
grep -q "FILTER IP:" test.log || echo_exit "Failed log file test #10.5"



#Setting port tests
pcp-server -p -1 --timeout 500 | grep -q "Bad value for option -p" || echo_exit "Failed starting pcp server with nondefault port #1"

pcp-server -p 5351 --timeout 500 | grep -q "Server listening" || echo_exit "Failed starting pcp server with nondefault port #2"

pcp-server -p 5555555555 --timeout 500 | grep -q "Bad value for option -p" || echo_exit "Failed starting pcp server with nondefault port #3"
killall pcp-server &>/dev/null

#Setting default DSCP value tests
pcp-server --ret-dscp 10 --timeout 500 | grep -q "Server listening" || echo_exit "Failed starting server with DSCP value #1"
pcp-server --app-bit --ret-dscp 10 --timeout 500 | grep -q "Server listening" || echo_exit "Failed starting server with DSCP value #2"

# Test malformed requests, responses are not checked
# this is test should increase code coverage
nc -h > /dev/null
if [$? -eq 0]
then
pcp-server --ear 1 >/tmp/pcp_server_bad_packet.test &
sleep 0.1
echo -ne "\x00\x00\x00" | nc -w 1 -u 0.0.0.0 5351 &>/dev/null &
sleep 1
killall pcp-server &>/dev/null
grep -q "Size of PCP packet is either smaller" /tmp/pcp_server_bad_packet.test || echo_exit "Malformed packet test failed #1"
fi

#Well formed packet unsuported version
#pcp-server --ear 1 >/tmp/pcp_server_bad_packet.test &
#echo -ne "\x03\x01\x00\x00\x00\x00\x03\x85\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\x7f\x00\x00\x01\x5f\x61\x4e\x21\x04\x58\x7f\x87\x2d\xb0\x47\x38\x06\x00\x00\x00\x04\xd2\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" | nc -w 1 -u 0.0.0.0 5351 > /dev/null &
#sleep 1
#grep -q "MAP" /tmp/pcp_server_bad_packet.test || echo_exit "Malformed packet test failed #4"
#killall pcp-server && echo_exit "Malformed packet test failed #4"

#pcp-server --timeout 2000 &>/dev/null &
#sudo python ./test_pcp_server.py
#sleep 2
#[ $? -eq 0 ] || exit 1
rm test.log
exit 0
