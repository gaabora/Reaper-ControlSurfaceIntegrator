# Developer Docs Guide

## Purpose

- Document the current developer-facing architecture, protocols, and reference contracts for CSI.

## Ownership

- `README.md` developer-doc index.
- `ARCHITECTURE.md` current runtime/component overview.
- `LUA_CPP_EXTSTATE_INTERFACE.md` current C++/Lua ExtState contract.
- `PRODUCT_IDENTITY.md` canonical public-name generation and product-owned path contract.

## Local Contracts

- Keep this folder focused on implemented behavior and current reference material.
- Move proposals, deferred refactors, and unfinished work to `../todo/`.
- Verify behavior claims against current source before updating architecture or protocol text.
- Keep links relative inside `docs/` and point to `../Wiki/` or `../todo/` when crossing folders.

## Work Guidance

- Describe the system that exists today before describing extension points or caveats.
- Treat protocol keys, payload formats, and lifecycle expectations as cross-component contracts.
- Remove stale status notes once the behavior is implemented or removed.

## Verification

- Check links and referenced filenames.
- Re-read the affected source paths when updating behavior or protocol claims.

## Child DOX Index

- None.
