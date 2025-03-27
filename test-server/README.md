PCP Server
==========

Simple mock PCP server for testing purposes. It doesn't create any real
mappings, it just displays the contents of received PCP message and returns PCP
response back to the client.

The pcp_server CLI help:

    $ ./pcp_server -h

    Usage:
    -h, --help          Display this help
    -v                  Set server version.
                        Only versions 1 and 2 are supported for now.
                        Default: By default server processes both versions.
    -p                  Lets you set the port on which server listens.
                        Default: 5351.
    -r                  Set result code that will be returned by server
                        to every request no matter what.
                        Possible values from range {0..13}
    --ear #num          Terminates server after #num requests have been received
                        and responded to.
    --ip                Sets IP address on which server is listening.
                        Default: 0.0.0.0 (listening on all interfaces)
    --timeout           Set timeout time of the server in miliseconds.
                        Default 0.0.0.0 (listening on all interfaces)
    --app-bit           set application bit in SADSCP opcode response
    --ret-dscp          return DSCP value for SADSCP opcode
    --log-file          Log Requests to file


Running pcp_server
------------------

Running pcp_server executable without any arguments will start PCP server
with default settings.

    $ ./pcp_server

Examples
--------

Following command will return UNSUPP_VERSION result code to every request it
receives:

    $ ./pcp_server –r 1

Following command will set version to 1 then wait for 5 seconds or receive
5 messages before it ends, whichever happens first:

    $ ./pcp_server –v 1 --timeout 5000 --ear 5

--app-bit
This flag sets the bit in SADSCP opcode response indicating , that the
application name from the request was matched.

--ret-dscp
Sets DSCP value to be returned in the response to any SADSCP request.

--log-file
Output of the pcp-server is not only printed to stdout, but also logged into
file. The file is being overwritten every time new message arrives.

