PCP client library
===================

PCP client library implements client side of PCP 
([RFC 6887](http://datatracker.ietf.org/doc/rfc6887/)) and 
NATPMP([RFC 6886](http://tools.ietf.org/html/rfc6886)) protocols. 
Switch to NATPMP is done automatically by version negotiation. This library 
enables any network application to manage network edge device (e.g. to create
NAT mapping or ask router for specific flow treatment).

Supported platforms are 
Linux, MS Windows (Vista and later), OS X.

### Components ###

  - libpcp     - PCP client library
  - pcp_app    - PCP client CLI app
  - pcp_server - mock PCP server
  - scapy      - PCP layer for Scapy

Build instructions are located in INSTALL.md file. More information about
components are in each subdirectory's README.md file.

