# CSI Scripts Guide

## Purpose

- Implement the installed CSI on-screen keyboard and on-screen display tools.

## Ownership

- `CSI OSK on-screen keyboard.lua` and `CSI OSD on-screen display.lua` entry scripts.
- `osk_data.lua`, `osk_render.lua`, `osk_config.lua`, and `osd_ui.lua` shared modules.

## Local Contracts

- ExtState payloads must remain compatible with `CSurfIntegrator` and `ControlSurface` command handling.
- The OSK layout, state, label, binding, and action-list formats are serialized contracts; change both ends together.
- Module loading must continue to work from the installed REAPER Scripts path.
- Track unapplied editor changes separately from live unsaved changes, and request C++ revert when an editor with live changes closes.
- Keep the binding editor's structured columns, pseudo-modifiers, generated titles, and color controls backed by the same serialized raw action line used by `ConfigApplyLive`.
- Send OSK wheel input as rate-limited semantic acceleration packets; do not generate MIDI messages or expose device IDs from Lua.

## Work Guidance

- Keep data acquisition/parsing in `osk_data.lua`, drawing in `osk_render.lua`, binding editing in `osk_config.lua`, and OSD behavior in `osd_ui.lua`.
- Avoid duplicating shared UI behavior in the entry scripts.
- Keep action edits reversible until the user explicitly saves them.

## Verification

- In REAPER, launch both entry scripts and verify the affected UI path.
- For OSK changes, test press, release, scroll, live apply, save, and revert as relevant.
- For OSD changes, test timeout, placement, styling, and settings persistence as relevant.

## Child DOX Index

- None.
