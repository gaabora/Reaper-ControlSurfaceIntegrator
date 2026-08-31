# Shared Integration Guide

## Purpose

- Provide common REAPER, platform, transport, type, and protocol facilities used by the plugin.

## Ownership

- REAPER SDK declarations and generated API function bindings.
- DAW wrappers for transport, tracks, FX, display, and utility operations.
- Shared types, string/file/logging helpers, SysEx construction, OSC packet code, and UDP transport.
- Generated product identity constants and typed product path resolution in `product_paths.h` and `product_paths.cpp`.
- `product_log.*` daily temporary log selection, file and optional REAPER-console writes, ExtState path publication, and native file/folder opening.
- Generated C++ setting metadata from `Scripts/settings_schema.conf`.
- `settings_values.*` schema-driven defaults, scope checks, value validation, and atomic override resolution.
- `reascript_action.*` registration and unique command resolution for installed product ReaScripts.

## Local Contracts

- `reaper_plugin_functions.h` mirrors the REAPER API and should be treated as generated/vendor-style code.
- `reaper_plugin.h`, `oscpkt.hh`, and `udp.hh` carry upstream or compatibility behavior; keep local edits minimal and well justified.
- DAW wrappers must preserve REAPER pointer validity, index conventions, and threading expectations.
- Publish OSD messages through the shared `ReaCtrlSurf_OSD` payload and event id keys documented in `docs/LUA_CPP_EXTSTATE_INTERFACE.md`.
- Publish a new OSD event id for every accepted request, including an identical payload, so Lua can refresh the timeout and re-evaluate runtime templates.
- Keep the generic explicit-message flag in the OSD payload. Do not put Lua OSD template names or resolver logic in shared C++ code.
- Route plugin logs through `ProductLog`. Respect the canonical Product settings that independently enable the daily file and optional REAPER-console output. The Notifications ReaScript remains the normal presentation for NOTICE, WARNING, and ERROR records.
- Create one shared daily file below the operating system temporary directory in `<product-id>/logs/<extstate-prefix>_logs_YYYY-MM/<extstate-prefix>_YYYY-MM-DD.log`. Publish its resolved daily ID, directory, active file, initial reader offset, and output states through the Log ExtState section so Lua does not build the platform path.
- Prefix each product log record with local time in `[HH:MM:SS]` format without a calendar date.
- Open the active product log file and monthly directory through the system default association. Do not force Notepad, Explorer, or another platform-specific application.
- Shared headers have broad compile impact; avoid adding heavyweight dependencies without need.
- Keep runtime Surface and Zone parsing compatible with OSC address tokens that start with `/`. In these formats, only `//` starts a comment, including after another token. `IsCommentedOrEmpty` must not classify a single leading `/` or `#` as a comment.
- Resolve product-owned paths through `ProductPaths`; the product root is `REAPER/Data/<ProductResourceDirectory>`, the optional standalone editor uses `Tools/config-editor-<ProductId>[.exe]`, surface files use `Surfaces/Vendor/<surface-id>.txt` or `Surfaces/User/<surface-id>.txt`, zone profiles use matching `Zones/Vendor` and `Zones/User` roots, and stable surface, profile, and operation IDs use lowercase ASCII and must remain inside their typed roots.
- Resolve bundled ReaScript command IDs through `ReaScriptAction::ResolveCommandId`. Reuse only one unique matching Main-section action when REAPER reports that the script is already registered.

## Work Guidance

- Prefer the typed DAW wrapper layer over scattered direct REAPER API calls when an existing wrapper covers the operation.
- Keep platform differences behind existing preprocessor boundaries.
- Validate pointers, sizes, and indices at external API and network boundaries.

## Verification

- Build the plugin on every platform affected by a shared or portability change.
- Exercise the specific REAPER API, OSC/UDP, or SysEx path that changed.

## Child DOX Index

- None.
