# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Fixed
- A NAT-PMP reply, which carries no nonce, never matched a flow whose nonce
  was set with `pcp_flow_set_nonce()`: it is looked up under the server's
  nonce. A flow to a server that speaks NAT-PMP now keeps the server's nonce.

## [1.0.0] - 2026-05-14

### Added
- First official public release of `libpcpnatpmp` under the current project name.
- CMake and autotools build support with cross-platform CI coverage (Linux, macOS, Windows).

### Changed
- Unified versioning baseline for a SemVer-based release process.

### Notes
- This is the first tagged release. Older historical development was not maintained as versioned release notes.
