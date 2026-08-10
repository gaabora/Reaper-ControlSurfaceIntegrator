# Configuration Editor Core Guide

## Purpose

- Define and validate product configuration, surface, zone, and functional snippet formats.
- Provide the lossless document core for the future local browser editor.

## Ownership

- Bun package and TypeScript configuration in this directory.
- Lossless source model, tokenization, parsers, semantic views, and diagnostics under `src/`.
- Runtime action catalog generation from C++ registry and documentation source.
- Format examples and malformed input under `fixtures/`.
- Parser and validator checks under `tests/`.

## Local Contracts

- Preserve original line text, line endings, comments, whitespace, unsupported properties, and unknown lines.
- Unknown data produces a warning and remains serializable. Syntax or unsafe identifiers produce errors.
- Product config version is the runtime `Version` value. Surface and zone version markers use `// @format <type> <version>` and remain safe for the current C++ parsers.
- Functional snippets use semantic bindings and must not store fixed hardware widget names.
- Read action names from `src/shared/types.h` `ACTION_TYPE_LIST`. Do not add a manual action-name list.
- Keep the core independent from the browser UI and file-writing workflows owned by later phases.

## Work Guidance

- Use Bun-compatible TypeScript and Node standard-library APIs. Do not add a package dependency when the standard library is sufficient.
- Keep parsers line-oriented until a format feature requires a more complex grammar.
- Add a valid and malformed fixture when a public grammar rule changes.

## Verification

- Run `bun test` from this directory.
- Run `bun run validate -- fixtures/valid` and confirm there are no errors.
- Run `bun run validate -- fixtures/invalid` and confirm the command reports errors.
- Run `bun run actions` and compare the catalog count with `ACTION_TYPE_LIST`.

## Child DOX Index

- None.
