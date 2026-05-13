# Contributing

Thanks for contributing.

## Commit Message Convention

This repository uses Conventional Commits for new contributions.

Use the format:

`type(scope): short summary`

Examples:
- `feat(cli): add nonce parsing validation`
- `fix(tests): avoid hardcoded PCP port collisions`
- `chore(ci): run coverage job on push`

Supported types:
- `feat`: new functionality
- `fix`: bug fix
- `docs`: documentation-only changes
- `test`: test-only changes
- `refactor`: code change without behavior change
- `perf`: performance improvement
- `build`: build system or dependency updates
- `ci`: CI pipeline/workflow changes
- `chore`: maintenance tasks

For breaking changes, add `!` after type/scope and explain in the footer:
- `feat(api)!: remove deprecated flow setter`
- Footer: `BREAKING CHANGE: ...`

## Changelog Policy

- Changelog entries are maintained from `1.0.0` onward.
- Release notes should be generated from Conventional Commit history where possible.
