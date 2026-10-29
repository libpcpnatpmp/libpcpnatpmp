# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Fixed
- A flow failed by a long lifetime error, such as NOT_AUTHORIZED, dropped
  the error's lifetime, which says how long the server will keep refusing.
  `pcp_flow_get_info()` now reports it as `recv_lifetime_end`.

## [1.0.0] - 2026-05-14

### Added
- First official public release of `libpcpnatpmp` under the current project name.
- CMake and autotools build support with cross-platform CI coverage (Linux, macOS, Windows).

### Changed
- Unified versioning baseline for a SemVer-based release process.

### Notes
- This is the first tagged release. Older historical development was not maintained as versioned release notes.
