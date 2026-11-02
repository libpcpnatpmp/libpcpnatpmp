# Changelog

All notable changes to this project will be documented in this file.

The format is inspired by Keep a Changelog and this project follows Semantic Versioning.

## [Unreleased]

### Fixed
- A short lifetime error, such as NO_RESOURCES, answered with lifetime 0 was
  resent at once, over and over. The flow now waits at least
  `PCP_MIN_ERROR_WAIT` seconds (30 by default, the lifetime RFC 6887
  recommends for a short lifetime error) before resending.

## [1.0.0] - 2026-05-14

### Added
- First official public release of `libpcpnatpmp` under the current project name.
- CMake and autotools build support with cross-platform CI coverage (Linux, macOS, Windows).

### Changed
- Unified versioning baseline for a SemVer-based release process.

### Notes
- This is the first tagged release. Older historical development was not maintained as versioned release notes.
