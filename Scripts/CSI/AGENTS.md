# CSI Scripts Guide

## Purpose

- Implement the installed CSI on-screen keyboard and on-screen display tools.

## Ownership

- `CSI OSK on-screen keyboard.lua` and `CSI OSD on-screen display.lua` entry scripts.
- `osk_data.lua`, `osk_render.lua`, `osk_input.lua`, `osk_config.lua`, `osd_ui.lua`, `ui_components.lua`, `script_host.lua`, `action_line.lua`, `layout_parser.lua`, `label_replacements.lua`, and `self_checks.lua` shared modules.

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
- Send OSK fader drag and wheel input as absolute normalized `WidgetValue` packets; when fader feedback is dB-valued, convert it for display and send matching dB command values for DB actions.
- Treat the OSK/OSD Lua interface as pre-release: use only current `ReaCtrlSurf_*` sections and settings, with no legacy aliases unless publication changes that requirement.
- Keep one OSK window per surface and persist each surface position independently without writing persistent ExtState on every movement frame.

## Work Guidance

- Keep data acquisition/parsing in `osk_data.lua`, drawing in `osk_render.lua`, input dispatch state in `osk_input.lua`, binding editing in `osk_config.lua`, and OSD behavior in `osd_ui.lua`.
- Keep reusable ReaImGui widget helpers in `ui_components.lua` so entry scripts and feature modules do not re-implement the same UI patterns.
- Keep shared script startup, context creation, toolbar state, and shutdown boilerplate in `script_host.lua` so the entry scripts stay thin orchestration layers.
- Keep serialized action-line, layout, and label-replacement contracts in `action_line.lua`, `layout_parser.lua`, and `label_replacements.lua` so UI modules do not own those formats.
- Keep pure Lua parser checks registered through `self_checks.lua` when adding or changing parser module self-checks.
- Keep OSD color, alpha, contrast, and centered text drawing in `osd_ui.lua` so standalone OSD and the embedded OSK bar share one renderer.
- Avoid duplicating shared UI behavior in the entry scripts.
- Keep action edits reversible until the user explicitly saves them.

## Verification

- Build the plugin with `cmake --build build --config Debug` before runtime verification.
- Run parser self-checks with `dofile("Scripts/CSI/self_checks.lua").RunAndReport()` from REAPER, or equivalent Lua with `Scripts/CSI` on `package.path`.
- In REAPER with ReaImGui installed, launch both `CSI OSK on-screen keyboard.lua` and `CSI OSD on-screen display.lua`.
- Open the OSK context menu and change settings, including the embedded OSD bar position.
- Verify label replacements with empty user input, a user override, and a longer phrase override.
- Open widget config, edit an action, search actions, apply live, save, and revert.
- Test two surfaces with the same widget names to confirm surface-scoped OSK state.
- Test OSK press, release, hold, double-press, and rotary wheel input.
- Test OSK fader drag, wheel scroll, value feedback, and touch/release behavior on touch-aware actions.
- Send repeated identical OSD messages and confirm the visible timeout refreshes.
- Open OSD settings while no OSD message is visible.
- Check standalone OSD top/bottom placement, left/center/right alignment, margins, size, font size, styling, Save, and Cancel.

## Child DOX Index

- None.
