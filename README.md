Port Control Protocol (PCP) and NAT-PMP client library
======================================================

Library implements client side of PCP
([RFC 6887](https://datatracker.ietf.org/doc/html/rfc6887)) and
NAT-PMP ([RFC 6886](https://datatracker.ietf.org/doc/html/rfc6886)) protocols.
Switch to NAT-PMP is done automatically by version negotiation. This library
enables any network application to manage network edge device (e.g. to create
NAT mapping or ask router for specific flow treatment).

Supported platforms are 
Linux, Microsoft Windows (Vista and later) and macOS.

Components
----------

  - [lib](lib)                 - Client library
  - [cli-client](cli-client)   - Command-line interface client
  - [test-server](test-server) - Test server
  - [scapy](scapy)             - PCP layer for Scapy

Build instructions are located in [INSTALL.md](INSTALL.md) file.
More information about components are in each subdirectory's README.md file.

Versioning follows [Semantic Versioning](https://semver.org/).
Release notes are tracked in [CHANGELOG.md](CHANGELOG.md).

Using the C library
-------------------

The API reference is in the comments in
[`lib/include/pcpnatpmp.h`](lib/include/pcpnatpmp.h). See
[`example/pcp-example.c`](example/pcp-example.c) for a usage example.

Initialize a context with `pcp_init()`, discover servers automatically or add
one with `pcp_add_server()`, then create a mapping with `pcp_new_flow()`.
Use `pcp_wait()` to wait for a result, or integrate `pcp_pulse()` and
`pcp_get_socket()` into your event loop. Keep calling `pcp_pulse()` and honoring
its next timeout while mappings are needed, so the library can renew them and
handle server restarts.

Read mapping results with `pcp_flow_get_info()` and free the returned array
when finished. To remove a mapping, call `pcp_close_flow()` and continue
processing events to send the request, then release the flow with
`pcp_delete_flow()`. Finally, call `pcp_terminate()` to release the context.
