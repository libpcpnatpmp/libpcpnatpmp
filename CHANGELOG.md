# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Added

- `pcp_flow_info_t.pcp_version`: the protocol version spoken with the flow's
  server, so a client can tell a PCP mapping from a NAT-PMP one (version 0).

### Fixed

- A flow from a global IPv6 address through a link-local server (the usual
  default router) got no matching replies, since its key kept the source's
  scope while each reply's key has none. Only a link-local source now keeps a
  scope, and it must still be the server's.
- Replies to a flow whose nonce was set with `pcp_flow_set_nonce()` never
  matched it: the flow stayed filed under its old nonce. It is now filed again
  under the new one.
- The test server, listening on all addresses, answered IPv6 requests from
  whatever address routing picked rather than the one asked, so a client that
  asked its link-local address dropped every answer. It now answers from the
  address each request was sent to.

## [1.0.0] - 2026-05-14

### Added
- First official public release of `libpcpnatpmp` under the current project name.
- CMake and autotools build support with cross-platform CI coverage (Linux, macOS, Windows).

### Changed
- Unified versioning baseline for a SemVer-based release process.

### Notes
- This is the first tagged release. Older historical development was not maintained as versioned release notes.
