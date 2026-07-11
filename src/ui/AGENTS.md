# Native UI Guide

## Purpose

- Implement CSI's native REAPER configuration and learn dialogs.

## Ownership

- Integrator UI dispatch in `integrator_ui.cpp`.
- Configuration dialog behavior in `config_dialog.cpp`.
- Learn dialog behavior in `learn_dialog.cpp`.

## Local Contracts

- Dialog and control identifiers must match `src/resource.h` and `src/res.rc`.
- `integrator_ui.cpp` includes the dialog implementation files directly; do not also compile them as independent translation units without restructuring the build.
- UI callbacks operate under REAPER/WDL/SWELL conventions and must preserve dialog lifetime and localization behavior.

## Work Guidance

- Keep persistent CSI behavior in controls/actions code and use UI files to collect, validate, and dispatch user intent.
- Maintain Windows and SWELL-compatible control usage.
- Validate user-entered paths, names, indices, and numeric values before applying them.

## Verification

- Build the plugin.
- Open every affected dialog in REAPER and test apply, cancel, close, invalid input, and repeated-open behavior.
- Verify resource layout on each affected platform.

## Child DOX Index

- None.
