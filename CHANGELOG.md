# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Added
- `pcp_flow_info_t.pcp_version`: the protocol version spoken with the flow's
  server, so a client can tell a PCP mapping from a NAT-PMP one (version 0).

## [1.0.0] - 2026-05-14

### Added
- First official public release of `libpcpnatpmp` under the current project name.
- CMake and autotools build support with cross-platform CI coverage (Linux, macOS, Windows).

### Changed
- Unified versioning baseline for a SemVer-based release process.

### Notes
- This is the first tagged release. Older historical development was not maintained as versioned release notes.
