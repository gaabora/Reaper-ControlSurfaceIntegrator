# Configuration Resources Guide

## Purpose

- Store configuration files that can be linked into a local REAPER installation during development.

## Ownership

- Surface templates in `Surfaces/Vendor` and `Surfaces/User`.
- Vendor and user zone profiles in `Zones/Vendor` and `Zones/User`.
- Built-in and user snippets in `Snippets/BuiltIn` and `Snippets/User`.

## Local Contracts

- Surface files use `Surfaces/Vendor/<surface-id>.txt` or `Surfaces/User/<surface-id>.txt`.
- A user surface with the same stable ID overrides its vendor file.
- Zone profiles use `Zones/Vendor/<profile-id>/Main`, `Zones/Vendor/<profile-id>/FX`, and matching paths under `Zones/User`.
- A user zone profile with the same stable ID fully overrides its vendor profile.
- Stable IDs match `[a-z0-9][a-z0-9_-]*`.
- ReaPack owns vendor surfaces, vendor zone profiles, and built-in snippets. It must not overwrite user surfaces, user zone profiles, or user snippets.

## Work Guidance

- Keep user-owned configuration changes separate from package-owned files.

## Verification

- Load linked resources in REAPER and verify each changed surface, zone profile, or snippet.

## Child DOX Index

- None.
