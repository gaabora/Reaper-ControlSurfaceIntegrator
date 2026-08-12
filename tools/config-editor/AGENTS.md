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
- Loopback HTTP API under `src/`, browser asset sources under `src/ui/`, and their small text-import loader in `src/ui.ts`.
- Standalone executable generation under `scripts/`.
- Format examples and malformed input under `fixtures/`.
- Parser and validator checks under `tests/`.

## Local Contracts

- Preserve original line text, line endings, comments, whitespace, unsupported properties, and unknown lines.
- Unknown data produces a warning and remains serializable. Syntax or unsafe identifiers produce errors.
- Product config version is the runtime `Version` value. Surface and zone version markers use `// @format <type> <version>` and remain safe for the current C++ parsers.
- Functional snippets use semantic bindings and must not store fixed hardware widget names.
- Functional snippet `Role`, `Input`, and `Feedback` requirements use the same capability rules as runtime OSK metadata. Formal layout metadata overrides inferred surface metadata when present.
- Resolve an exact compatible semantic-slot and widget-name match automatically. Require explicit confirmation for a different manual choice. Allow an incompatible choice only after showing its role or capability mismatch and receiving an explicit override.
- Keep the browser editor task-based. Show one main workflow at a time and keep file paths, application IDs, and generated source below an advanced-details control when they are not needed for the normal workflow.
- Apply resolved snippets only to writable user zones. Use stable application IDs and the `// @snippet Application=<id> Source=<snippet-id>` and matching `// @snippet-end Application=<id>` markers.
- Require `Replace`, `Rename`, or `Skip` when an application marker or snippet import target already exists. Recheck all preview hashes before a validated transaction.
- Import snippet files only below `Snippets/User`. Keep browser export independent from file writes so checked unsaved source can be downloaded.
- Read action names from `src/shared/types.h` `ACTION_TYPE_LIST`. Do not add a manual action-name list.
- Keep parsers independent from the browser UI and file-writing service.
- Bind the editor server only to `127.0.0.1` and require a random session token for every API request.
- Let the user select only the REAPER `Data` directory. Derive the internal product configuration folder from product identity before file access.
- Reject absolute child paths, `..`, and unsupported logical locations. Follow configuration file and directory links while applying ownership and write permissions to their logical paths.
- Keep all fixed user-interface text in the typed English catalog in `src/i18n.ts`.
- Write only the product config and user-owned surface, zone, and snippet paths. Clone vendor content before editing.
- Keep a selected legacy CSI source separate from the writable product-root guard. Accept the legacy `CSI/` directory or its parent, follow source links, and never modify source files.
- Import legacy `Surface.txt` into `Surfaces/User`, `Zones/**/*.zon` into the matching `Zones/User/<profile>/Main` tree, and `FXZones/**/*.zon` into its `FX` tree. Ignore backup names that do not end in `.zon`, add missing surface and zone format markers, and preserve zone subdirectories.
- Resolve missing or incompatible legacy zone widgets through capability-filtered target widget mappings. Preserve `Widget|` channel-family semantics and map them only to another compatible channel family. Apply mappings to preview source before source hashes and transactional saves are calculated.
- Require explicit `Rename`, `Replace`, or `Skip` decisions for existing import targets. Recheck source and target hashes before one validated multi-file transaction.
- Show the complete legacy import operation report with changed, created, failed, restored, and skipped paths.
- Check the open-file SHA-256 before save. Stage and validate complete files before atomic rename.
- Back up and roll back multi-file operations and keep their manifest below the product `Backups` directory.

## Work Guidance

- Use Bun-compatible TypeScript and Node standard-library APIs. Do not add a package dependency when the standard library is sufficient.
- Keep parsers line-oriented until a format feature requires a more complex grammar.
- Add a valid and malformed fixture when a public grammar rule changes.
- Keep browser assets embedded so compiled executables do not need adjacent UI files.
- Keep HTML, CSS, and browser JavaScript in their native file types under `src/ui/`. Import them as text from `src/ui.ts` so standalone builds still embed them.
- Generate standalone identity and action data from repository contracts during compile. Do not source-control generated compile input or `dist/` output.

## Verification

- Run `bun test` from this directory.
- Run `bun run validate -- fixtures/valid` and confirm there are no errors.
- Run `bun run validate -- fixtures/invalid` and confirm the command reports errors.
- Run `bun run actions` and compare the catalog count with `ACTION_TYPE_LIST`.
- Run focused store tests for conflicts, rollback, cloning, symlink traversal, and directory link cycles.
- Run focused legacy import tests for source discovery, `Zones` and `FXZones` mapping, backup-file filtering, dependency selection, semantic widget mapping, repeat conflicts, source immutability, and transactional writes.
- Run focused snippet workflow tests for semantic widget filtering, explicit confirmation, mismatch override, repeat application conflicts, import conflicts, hash conflicts, and transactional writes.
- Start the server with `--no-open` and confirm unauthenticated API access fails.
- Build and launch each standalone target in its matching operating system before release.

## Child DOX Index

- None.
