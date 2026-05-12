#!/bin/sh

Get_Status() {
if [ "$1" -ne 0 ]
then
echo "$2 FAILED! Exit Status: $1" >> "$CURRENT_DIR/test_results.txt"
printf '%s \033[00;31mFAILED\033[00m Exit Status: %s\n' "$2" "$1" >> "$CURRENT_DIR/TEMP.tmp"
printf '\033[00;31m---------------------------------------------------\033[00m \n'
printf '\n\n %s \033[00;31mFAILED\033[00m Exit Status: %s \n\n \n' "$2" "$1"
printf '\033[00;31m---------------------------------------------------\033[00m \n'
else
echo "$2 PASSED! Exit Status: $1" >> "$CURRENT_DIR/test_results.txt"
printf '%s \033[00;32mPASSED\033[00m Exit Status: %s\n' "$2" "$1" >> "$CURRENT_DIR/TEMP.tmp"
printf '\033[00;32m---------------------------------------------------\033[00m \n'
printf '\n\n %s \033[00;32mPASSED\033[00m Exit Status: %s \n\n \n' "$2" "$1"
printf '\033[00;32m---------------------------------------------------\033[00m \n'
fi
return 0
}
#############################################################################

# BIN_PATH describes the path to scripts required by executables.

# this script is expected to be in /lib/tests, else you have to change $BIN_PATH
# variable, which directs script to executables.
# paths to scripts for pcpnatpmpc and pcp-server must be changed manually

OS=$(uname)
CURRENT_DIR=$(pwd)
case "$OS" in
	Linux) BIN_PATH=$CURRENT_DIR/cli-client:$CURRENT_DIR/test-server:$CURRENT_DIR/tests ;;
	*) BIN_PATH=$CURRENT_DIR/build/bin/Debug ;;
esac
PATH_SCRIPT=tests
PATH=$PATH:$BIN_PATH:$CURRENT_DIR/win_utils

echo "" > "$CURRENT_DIR/test_results.txt"
echo "" > "$CURRENT_DIR/TEMP.tmp"

export PCP_USE_IPV6_SOCKET=0

test_flow_notify
Get_Status $? "test_flow_notify           "

test_version_negotiation 2
Get_Status $? "test_version_negotiation   "

test_pcp_client_map_opcode
Get_Status $? "test_pcp_client_map_opcode "

test_pcp_client_peer_opcode
Get_Status $? "test_pcp_client_peer_opcode"

$PATH_SCRIPT/test_get_dscp.sh
Get_Status $? "test_get_dscp              "

$PATH_SCRIPT/test_flow_md.sh
Get_Status $? "test_flow_md               "

test_lifetime_renewal
Get_Status $? "test_lifetime_renewal      "

test_server_reping
Get_Status $? "test_server_reping         "

test_event_handler
Get_Status $? "test_event_handler         "

test_gateway || test_gw
Get_Status $? "test_gateway               "

test_pcp_client_db
Get_Status $? "test_pcp_client_db         "

test_pcp_api
Get_Status $? "test_pcp_api               "

test_sock_ntop
Get_Status $? "test_sock_ntop             "

test_pcp_logger
Get_Status $? "test_pcp_logger            "

test_pcp_msg
Get_Status $? "test_pcp_msg               "

test_server_restart
Get_Status $? "test_server_restart        "

test_pcp_cli_client
Get_Status $? "test_pcp_cli_client        "

test_pcp_server
Get_Status $? "test_pcp_server            "

test_ping_gws
Get_Status $? "test_ping_gws              "

cat "$CURRENT_DIR/TEMP.tmp"
rm "$CURRENT_DIR/TEMP.tmp"

printf "Testing ended, results in '%s/test_results.txt'\n" "$CURRENT_DIR"

exit
