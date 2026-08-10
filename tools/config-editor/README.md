# Configuration Editor Core

This Bun and TypeScript package parses and validates configuration files without changing them. It is the non-UI core for the local editor planned in Phase 4.

## Current scope

- Lossless line model and serializer.
- Product config, surface, zone, and functional snippet parsers.
- Semantic views and syntax diagnostics.
- Case-insensitive duplicate checks and zone dependency checks.
- Runtime action catalog loaded from C++ `ACTION_TYPE_LIST` and `//!` metadata, including short descriptions where available.
- CLI validation and JSON action-catalog generation.

The core does not select a product root, save files, create backups, import legacy data, or run a browser server. Those operations belong to later phases.

## Commands

Run these commands from `tools/config-editor/`:

```sh
bun run validate -- fixtures/valid
bun run validate -- --json fixtures/invalid
bun run actions
bun run actions:generate
bun test
```

`actions:generate` writes `generated/action-catalog.json`. Generated output is not source-controlled. The catalog is rebuilt from `src/shared/types.h` and action documentation, so action names are not duplicated in TypeScript.

The repository rule requires explicit permission before running tests or build commands. The commands above describe the available checks; they are not automatic.

## Lossless model

Every parsed document keeps each original line and its exact line ending. The serializer joins those stored values without normalization. Semantic parsers attach meaning and diagnostics but do not remove unknown data.

Unknown lines and unsupported properties produce warnings when the surrounding syntax remains safe. Missing block ends, invalid versions, unsafe stable IDs, and case-insensitive duplicates produce errors.

See [docs/FORMATS.md](docs/FORMATS.md) for the grammar contracts.
