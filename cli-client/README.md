PCP/NAT-PMP CLI client
======================

The command-line interface client app supports sending MAP, PEER and
SADSCP Opcodes (for more information run `pcpnatpmpc -h`). Only one type of
request can be sent per CLI invocation and the type of message is determined
following way:

1.  The MAP message is sent if you provide at least
    internal port of the flow with option -i --internal.

2.  If peer IP or port are set (using -p --peer)
    then PCP client forms and sends PCP message with PEER Opcode.

3.  By providing jitter/loss/delay tolerances via CLI options
    the SADSCP opcode request (if built in) will be created and sent.

CLI options/help
----------------

    -e, --suggested-external
        Provide suggested external IP address and port in the PCP request.

    -t, -u, -n
        These three options are used to set the flow’s protocol type.

    -T, --timeout
        Sets the timeout, for how long should client wait on the response
        from server.

    -d, --disable-autodiscovery
        Disable auto-discovery, if this option is not present as command line
        argument, the client performs auto-discovery of the PCP server on all
        available interfaces.

    -P, --prefer-failure
        Adds the prefer failure to PCP message.

    -F, --filter
        Adds filter option to the PCP MAP message.

Further CLI options are available if built with additional features, see `./configure -h`.

Examples
--------

  Send MAP command for TCP port 1234 bound to all internal addresses with
  lifetime set to 1 hour:

    pcpnatpmpc -i :1234 -l 3600

  Send MAP command for TCP port 1234 only to PCP server at 10.0.0.1:

    pcpnatpmpc -d -s 10.0.0.1 -i :1234

  Send PEER for 5-tuple (source: 10.0.0.2:1234, dest 8.8.8.8:9, protocol UDP):

    pcpnatpmpc -i 10.0.0.2:1234 -p 8.8.8.8:9 -u
