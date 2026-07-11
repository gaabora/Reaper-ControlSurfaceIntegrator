# Scripts Guide

## Purpose

- Provide REAPER ReaScripts that expose CSI on-screen display and on-screen keyboard workflows.

## Ownership

- Standalone Lua script prototypes and entry points directly under `Scripts/`.
- Installation/runtime assumptions shared by the scripts in this subtree.

## Local Contracts

- Scripts execute inside REAPER and depend on the `reaper` API and ReaImGui.
- Keep ExtState section names, keys, payload delimiters, and command lifecycles compatible with the C++ bridge in `src/controls`.
- Do not silently persist settings that are intended to remain session-only.

## Work Guidance

- Keep Lua modules small enough to separate data parsing, configuration editing, and rendering responsibilities.
- Preserve REAPER defer-loop cleanup and ImGui context lifetime behavior.
- When changing shared protocol data, update the C++ producer/consumer and relevant documentation in the same task.

## Verification

- Load the affected script in REAPER with ReaImGui available.
- Exercise opening, closing, settings persistence, and the affected ExtState command path.

## Child DOX Index

- `CSI/AGENTS.md` - Installed CSI OSK/OSD scripts and shared Lua modules.
