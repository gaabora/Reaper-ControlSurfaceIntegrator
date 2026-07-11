# ReaControlSurface Agent Guide

Read `DOX.md` before editing. This file is the repository-wide contract.

## Purpose

- Build and maintain the cross-platform Control Surface Integrator (CSI) extension for REAPER.
- Keep the C++ plugin, bundled ReaScripts, and user documentation aligned with the configuration and ExtState contracts they share.

## Ownership

- Root build and packaging files: `CMakeLists.txt`, `config.h.in`, `CMakeSettings.json`, and `Makefile`.
- Repository-wide architecture, configuration, action reference, migration, planning, and project documentation in root Markdown files.
- Project-wide engineering rules and the top-level DOX hierarchy.

## Local Contracts

- CMake is the supported build system; `Makefile` is legacy compatibility material unless a task explicitly targets it.
- The plugin target is `ControlSurfaceIntegrator`, emitted as `reaper_csurf_integrator`.
- Treat REAPER APIs, WDL/SWELL, MIDI/OSC protocols, zone files, surface templates, and Lua ExtState keys as external or cross-component contracts.
- Do not index or create DOX files under `.git`, `.github`, `.vscode`, `build`, `cmake`, or `lib`.
- Preserve user changes in the working tree and keep generated build output out of source ownership docs.

## Work Guidance

### C++

- No newlines between params in methods and conditions if line <200 symbols.
- Never use one-letter variable names. Use `idx` for index variables.
- Use `this->` when accessing class members from class methods.
- Keep the project on C++17 and follow the existing WDL/SWELL portability patterns.
- Maintain the precompiled-header exceptions for `src/main.cpp` and `src/controls/integrator.cpp`.

### Documentation

- Verify behavioral claims against current source before updating reference documentation.
- Keep root reference documents and `Wiki/` navigation consistent when public behavior changes.

## Verification

- Configure and build with the repository CMake presets/settings appropriate to the platform.
- On Windows, the existing local build can be checked with `cmake --build build --config Debug`.
- There is no automated unit-test suite in the included source tree; runtime changes require focused manual verification in REAPER.
- Documentation-only changes should at minimum check links, paths, and DOX index coverage.

## Child DOX Index

- `Scripts/AGENTS.md` - REAPER Lua scripts and their shared runtime contracts.
- `src/AGENTS.md` - C++ plugin source, resources, and component boundaries.
- `Wiki/AGENTS.md` - User-facing wiki pages, navigation, and documentation roadmap.
