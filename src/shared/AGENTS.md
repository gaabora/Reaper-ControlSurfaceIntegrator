# Shared Integration Guide

## Purpose

- Provide common REAPER, platform, transport, type, and protocol facilities used by the plugin.

## Ownership

- REAPER SDK declarations and generated API function bindings.
- DAW wrappers for transport, tracks, FX, display, and utility operations.
- Shared types, string/file/logging helpers, SysEx construction, OSC packet code, and UDP transport.
- Generated product identity constants and typed product path resolution in `product_paths.h` and `product_paths.cpp`.

## Local Contracts

- `reaper_plugin_functions.h` mirrors the REAPER API and should be treated as generated/vendor-style code.
- `reaper_plugin.h`, `oscpkt.hh`, and `udp.hh` carry upstream or compatibility behavior; keep local edits minimal and well justified.
- DAW wrappers must preserve REAPER pointer validity, index conventions, and threading expectations.
- Publish OSD messages through the shared `ReaCtrlSurf_OSD` payload and event id keys documented in `docs/LUA_CPP_EXTSTATE_INTERFACE.md`.
- Shared headers have broad compile impact; avoid adding heavyweight dependencies without need.
- Resolve product-owned paths through `ProductPaths`; surface files use `Surfaces/Vendor/<surface-id>.txt` or `Surfaces/User/<surface-id>.txt`, zone profiles use matching `Zones/Vendor` and `Zones/User` roots, and stable surface, profile, and operation IDs use lowercase ASCII and must remain inside their typed roots.

## Work Guidance

- Prefer the typed DAW wrapper layer over scattered direct REAPER API calls when an existing wrapper covers the operation.
- Keep platform differences behind existing preprocessor boundaries.
- Validate pointers, sizes, and indices at external API and network boundaries.

## Verification

- Build the plugin on every platform affected by a shared or portability change.
- Exercise the specific REAPER API, OSC/UDP, or SysEx path that changed.

## Child DOX Index

- None.
