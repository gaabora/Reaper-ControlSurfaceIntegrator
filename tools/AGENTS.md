# Development Tools Guide

## Purpose

- Store durable developer and release tools that are not part of the runtime plugin.

## Ownership

- ReaPack staging, package metadata, checksum, and index verification tools in `reapack/`.
- Bun and TypeScript configuration parsing, validation, local browser editing, safe-save, and standalone tooling in `config-editor/`.

## Local Contracts

- Read public product names, stable action IDs, and repository names from `Scripts/product_identity.conf`.
- Write generated ReaPack work only below `.reapack-build/` or another explicit staging directory.
- ReaPack packages may own the extension, shared scripts, vendor surfaces, vendor zone profiles, and built-in snippets.
- ReaPack packages must not target user surfaces, user zone profiles, user snippets, backups, or generated user data.

## Work Guidance

- Keep tools independent from the C++ build when they only process source files and release assets.
- Use standard library dependencies when they are sufficient.

## Verification

- Validate generated package metadata with official `reapack-index` strict mode.
- Verify every generated index source has a SHA-256 multihash and one known local source.

## Child DOX Index

- `config-editor/AGENTS.md` - Configuration contracts, lossless editor core, local browser app, safe saves, fixtures, and CLI validation.
