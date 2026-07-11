# Plugin Source Guide

## Purpose

- Build the CSI REAPER control-surface extension and its native resources.

## Ownership

- Plugin entry point in `main.cpp`.
- Windows/SWELL resources in `res.rc` and `resource.h`.
- Source target composition in `CMakeLists.txt`.
- Component boundaries for actions, controls, shared integration code, and native UI.

## Local Contracts

- `main.cpp` owns `REAPERAPI_IMPLEMENT` and must not use the precompiled header.
- `controls/integrator.cpp` has localization include-order requirements and must not use the precompiled header.
- Resource identifiers must stay synchronized between `resource.h`, `res.rc`, and UI code.
- Source files are collected recursively by `src/CMakeLists.txt`; new implementation files become part of the plugin target automatically.

## Work Guidance

- Follow the repository C++ conventions from the root guide.
- Keep REAPER entry-point work minimal and move behavior into the owning component.
- Avoid introducing platform-specific behavior without an equivalent WDL/SWELL path or an explicit platform guard.

## Verification

- Build the `ControlSurfaceIntegrator` target for the affected platform.
- For resource changes, open each affected dialog in REAPER on Windows and, when relevant, a SWELL platform.

## Child DOX Index

- `actions/AGENTS.md` - Action types, contexts, values, timing, colors, and REAPER command behavior.
- `controls/AGENTS.md` - CSI runtime model, parsing, surfaces, zones, widgets, and navigation.
- `shared/AGENTS.md` - REAPER API wrappers, common types/utilities, OSC packets, UDP, and SysEx helpers.
- `ui/AGENTS.md` - Native configuration and learn dialogs.
