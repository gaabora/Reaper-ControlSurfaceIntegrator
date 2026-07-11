# CSI Scripts Guide

## Purpose

- Implement the installed CSI on-screen keyboard and on-screen display tools.

## Ownership

- `CSI OSK on-screen keyboard.lua` and `CSI OSD on-screen display.lua` entry scripts.
- `osk_data.lua`, `osk_render.lua`, `osk_input.lua`, `osk_config.lua`, `osd_ui.lua`, `ui_components.lua`, `script_host.lua`, `action_line.lua`, `layout_parser.lua`, and `label_replacements.lua` shared modules.

## Local Contracts

- ExtState payloads must remain compatible with `CSurfIntegrator` and `ControlSurface` command handling.
- The OSK layout, state, label, binding, and action-list formats are serialized contracts; change both ends together.
- Module loading must continue to work from the installed REAPER Scripts path.
- Track unapplied editor changes separately from live unsaved changes, and request C++ revert when an editor with live changes closes.
- Keep the binding editor's structured columns, pseudo-modifiers, generated titles, and color controls backed by the same serialized raw action line used by `ConfigApplyLive`.
- Keep action search beside the Action field, apply clicked results directly, match all space-separated partial terms across titles plus numeric and named REAPER command IDs, and leave the raw action line visible at the end of the form.
- Keep the config window undockable and its dirty-state Apply, Save, and Revert controls in the top toolbar so they remain visible.
- Show direction pseudo-modifiers only for relative controls identified by layout metadata; normal buttons must not expose Increase or Decrease controls.
- Show the resolved binding title as the editable OSD default, and when KeyLabel is empty present OSD as its effective default without serializing duplicate properties until the user edits them.
- Always apply built-in OSK label replacements; merge user replacements on top with user rules taking priority and longer phrases evaluated before shorter phrases.
- Send OSK wheel input as rate-limited semantic acceleration packets; do not generate MIDI messages or expose device IDs from Lua.
- Treat the OSK/OSD Lua interface as pre-release: use only current `ReaCtrlSurf_*` sections and settings, with no legacy aliases unless publication changes that requirement.
- Keep one OSK window per surface and persist each surface position independently without writing persistent ExtState on every movement frame.

## Work Guidance

- Keep data acquisition/parsing in `osk_data.lua`, drawing in `osk_render.lua`, input dispatch state in `osk_input.lua`, binding editing in `osk_config.lua`, and OSD behavior in `osd_ui.lua`.
- Keep reusable ReaImGui widget helpers in `ui_components.lua` so entry scripts and feature modules do not re-implement the same UI patterns.
- Keep shared script startup, context creation, toolbar state, and shutdown boilerplate in `script_host.lua` so the entry scripts stay thin orchestration layers.
- Keep serialized action-line, layout, and label-replacement contracts in `action_line.lua`, `layout_parser.lua`, and `label_replacements.lua` so UI modules do not own those formats.
- Avoid duplicating shared UI behavior in the entry scripts.
- Keep action edits reversible until the user explicitly saves them.

## Verification

- In REAPER, launch both entry scripts and verify the affected UI path.
- For OSK changes, test press, release, scroll, live apply, save, and revert as relevant.
- For OSD changes, test timeout, placement, styling, and settings persistence as relevant.

## Child DOX Index

- None.
