# Configuration Resources Guide

## Purpose

- Store configuration files that can be linked into a local REAPER installation during development.

## Ownership

- Surface templates in `Surfaces/Vendor` and `Surfaces/User`.
- Vendor and user zone profiles in `Zones/Vendor` and `Zones/User`.
- Built-in and user snippets in `Snippets/BuiltIn` and `Snippets/User`.

## Local Contracts

- Surface files use `Surfaces/Vendor/<surface-id>.txt` or `Surfaces/User/<surface-id>.txt`.
- Format 2 Surface metadata declares the positive hardware channel count as `Channels=N`; product I/O definitions do not duplicate it.
- A user surface with the same stable ID overrides its vendor file.
- Zone profiles use `Zones/Vendor/<profile-id>/Main`, `Zones/Vendor/<profile-id>/FX`, and matching paths under `Zones/User`.
- Use `User/<profile-id>/Main` when that directory exists. Otherwise use `Vendor/<profile-id>/Main`.
- Load Vendor and User FX zones together. A User FX zone overrides a Vendor FX zone only when both have the same exact `Zone` name. Duplicate zone names in one layer are invalid.
- Stable IDs match `[a-z0-9][a-z0-9_-]*`.
- Use `//` for comments in Surface and Zone files. Do not use a single leading `/` or `#` to disable a configuration line.
- Write Learn template directives as the exact `#WidgetType`, `#DisplayRow`, `#RingStyle`, `#DisplayFont`, and `#SupportsColor` metadata keywords.
- Built-in snippets use semantic bindings with explicit `Role`, `Input`, `Feedback`, and `Required` values. They must not contain fixed surface widget names.
- ReaPack installs these resources below `REAPER/Data/<ProductResourceDirectory>`, owns vendor surfaces, vendor zone profiles, and built-in snippets, and must not overwrite user surfaces, user zone profiles, or user snippets.

## Work Guidance

- Keep user-owned configuration changes separate from package-owned files.

## Verification

- Load linked resources in REAPER and verify each changed surface, zone profile, or snippet.

## Child DOX Index

- None.
