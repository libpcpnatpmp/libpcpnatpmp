# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Added

- A flow keeps several FILTER options (RFC 6887 section 13.3), by default up to `PCP_MAX_FILTER_OPTIONS` (500); `pcp_set_max_filter_opts()` changes the limit. A request carries the filters the server does not have yet, as many as fit, and the next requests carry the rest; a renewal carries none, and all of them go out again when the server restarts or the mapping is lost. `pcp_flow_clear_filter_opts()` removes them, and `pcp_flow_set_filter_mode()` chooses between adding filters (`PCP_FILTER_MODE_MULTI`) and one filter that each call replaces (`PCP_FILTER_MODE_ONE`).
- `pcp_flow_info_t.pcp_version`: the protocol version spoken with the flow's
  server, so a client can tell a PCP mapping from a NAT-PMP one (version 0).
- `pcp_set_restart_delay()`: the longest random wait before re-sending after a server restart (default 5000 ms, 0 turns it off).

### Changed

- `pcp_flow_set_filter_opt()` now adds a filter to the flow instead of replacing the one it had. A caller that calls it again to switch to another remote host keeps both allowed, unless it calls `pcp_flow_set_filter_mode(flow, PCP_FILTER_MODE_ONE)` first or `pcp_flow_clear_filter_opts()` before adding the new filter.
- A filter with prefix length 0 ("no filter") removes the flow's filters instead of being stored as a filter.
- `pcp_flow_set_filter_opt()` returns `int` instead of `void`: `PCP_ERR_SUCCESS`, or `PCP_ERR_BAD_ARGS`, `PCP_ERR_MAX_SIZE` or `PCP_ERR_NO_MEM`, in which case none of the flow's server flows is changed. Callers that ignore the result build unchanged. `pcpnatpmpc` exits with an error when the filter is refused.

### Fixed

- A flow sent one FILTER option per request, so a client that allowed several remote hosts had to set a filter, wait for its request to go out, and only then set the next; a filter set before that silently replaced the previous one, and the request re-sent after a server restart carried only the last filter. All the filters a flow has outstanding now go out together, as many as fit in one request and the rest in the next ones, and every one of them is sent again after a restart.
- A server adds the filters of each request to the ones it has, so replacing a flow's filter left the old one allowed. When a filter is removed (replaced, cleared or dropped by a mode switch), the next requests now start with a FILTER option with prefix length 0, which removes the server's filters, until the server accepts one.
- Deleting a mapping that has FILTER options sent them with the delete, which a server refuses with `MALFORMED_OPTION` (RFC 6887 section 13.3), so the mapping was never deleted. A delete now carries no FILTER option, and neither does a PEER request.
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
- A NAT-PMP reply, which carries no nonce, never matched a flow whose nonce
  was set with `pcp_flow_set_nonce()`: it is looked up under the server's
  nonce. A flow to a server that speaks NAT-PMP now keeps the server's nonce.
- PCP version 1 replies also carry no nonce. Flows now keep the server's nonce
  when negotiating down to version 1, and `pcp_flow_set_nonce()` leaves those
  flows unchanged, including builds without NAT-PMP support.
- A flow failed by a long lifetime error, such as NOT_AUTHORIZED, dropped
  the error's lifetime, which says how long the server will keep refusing.
  `pcp_flow_get_info()` now reports it as `recv_lifetime_end`. Expiry
  calculations use the range of `time_t`, avoiding premature truncation at
  2038 on Windows.
- A short lifetime error, such as NO_RESOURCES, answered with lifetime 0 was
  resent at once, over and over. The flow now waits at least
  `PCP_MIN_ERROR_WAIT` seconds (30 by default, the lifetime RFC 6887
  recommends for a short lifetime error) before resending, including renewal
  errors. The existing mapping address, port, and expiry are retained while
  waiting.
- Restart announcements an IPv4 PCP server sends to 224.0.0.1 port 5350 never reached the client on Linux: its IPv6 socket had not joined the group. The library now joins 224.0.0.1 and ff02::1 on the interface of each server's source address, also for servers found by autodiscovery, when it made the socket itself, and logs a warning when it cannot.
- A client that could not have port 5350, because another client on the host had it, never received restart announcements, which servers send to 5350 only. It now listens on 5350 beside the other client, on a socket bound to the group address, and the client holding 5350 lets other programs share the port.
- After a server restart every client of it re-sent its mappings at once. A client now waits a random time of up to 5 seconds first (RFC 6887, 14.2.1).

## [1.0.0] - 2026-05-14

### Added
- First official public release of `libpcpnatpmp` under the current project name.
- CMake and autotools build support with cross-platform CI coverage (Linux, macOS, Windows).

### Changed
- Unified versioning baseline for a SemVer-based release process.

### Notes
- This is the first tagged release. Older historical development was not maintained as versioned release notes.
