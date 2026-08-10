# ReaControlSurface Conf Editor Guide

## Purpose

- Define and validate product configuration, surface, zone, and functional snippet formats.
- Provide the lossless core, local browser editor, safe file service, and standalone compile workflow.

## Ownership

- Bun package and TypeScript configuration in this directory.
- Lossless source model, tokenization, parsers, semantic views, and diagnostics under `src/`.
- Runtime action catalog generation from C++ registry and documentation source.
- REAPER data path discovery, contained configuration access, safe saves, transactions, backups, rollback, and operation reports.
- Read-only legacy CSI discovery, dependency preview, conflict resolution, and transactional import.
- Loopback HTTP API and embedded browser UI under `src/`.
- Standalone executable generation under `scripts/`.
- Format examples and malformed input under `fixtures/`.
- Parser and validator checks under `tests/`.

## Local Contracts

- Preserve original line text, line endings, comments, whitespace, unsupported properties, and unknown lines.
- Unknown data produces a warning and remains serializable. Syntax or unsafe identifiers produce errors.
- Product config version is the runtime `Version` value. Surface and zone version markers use `// @format <type> <version>` and remain safe for the current C++ parsers.
- Functional snippets use semantic bindings and must not store fixed hardware widget names.
- Read action names from `src/shared/types.h` `ACTION_TYPE_LIST`. Do not add a manual action-name list.
- Keep parsers independent from the browser UI and file-writing service.
- Bind the editor server only to `127.0.0.1` and require a random session token for every API request.
- Let the user select only the REAPER `Data` directory. Derive the internal product configuration folder from product identity before file access.
- Reject absolute child paths, `..`, unsupported locations, symbolic links, and paths outside the internal configuration folder.
- Keep all fixed user-interface text in the typed English catalog in `src/i18n.ts`.
- Write only the product config and user-owned surface, zone, and snippet paths. Clone vendor content before editing.
- Keep a selected legacy CSI source separate from the writable product-root guard. Accept the legacy `CSI/` directory or its parent, reject source symbolic links, and never modify source files.
- Import legacy `Surface.txt` and exact `Zones/**/*.zon` files into user-owned paths. Ignore backup names that do not end in `.zon`, add missing surface and zone format markers, and preserve zone subdirectories.
- Require explicit `Rename`, `Replace`, or `Skip` decisions for existing import targets. Recheck source and target hashes before one validated multi-file transaction.
- Check the open-file SHA-256 before save. Stage and validate complete files before atomic rename.
- Back up and roll back multi-file operations and keep their manifest below the product `Backups` directory.

## Work Guidance

- Use Bun-compatible TypeScript and Node standard-library APIs. Do not add a package dependency when the standard library is sufficient.
- Keep parsers line-oriented until a format feature requires a more complex grammar.
- Add a valid and malformed fixture when a public grammar rule changes.
- Keep browser assets embedded so compiled executables do not need adjacent UI files.
- Generate standalone identity and action data from repository contracts during compile. Do not source-control generated compile input or `dist/` output.

## Verification

- Run `bun test` from this directory.
- Run `bun run validate -- fixtures/valid` and confirm there are no errors.
- Run `bun run validate -- fixtures/invalid` and confirm the command reports errors.
- Run `bun run actions` and compare the catalog count with `ACTION_TYPE_LIST`.
- Run focused store tests for conflicts, rollback, cloning, and symlink rejection.
- Run focused legacy import tests for source discovery, backup-file filtering, dependency selection, repeat conflicts, source immutability, and transactional writes.
- Start the server with `--no-open` and confirm unauthenticated API access fails.
- Build and launch each standalone target in its matching operating system before release.

## Child DOX Index

- None.
