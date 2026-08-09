# CSI Scripts Guide

## Purpose

- Implement the installed CSI on-screen keyboard and on-screen display tools.

## Ownership

- `CSI OSK on-screen keyboard.lua` and `CSI OSD on-screen display.lua` entry scripts.
- `osk_data.lua`, `osk_render.lua`, `osk_input.lua`, `osk_config.lua`, `osd_ui.lua`, `ui_components.lua`, `script_host.lua`, `action_line.lua`, `layout_parser.lua`, `label_replacements.lua`, `self_checks.lua`, `theme_settings.lua`, `font_cache.lua`, `settings_store.lua`, `osk_settings_ui.lua`, `osk_widget_math.lua`, `osk_draw_primitives.lua`, `osk_widget_drawers.lua`, `osk_config_model.lua`, `osk_config_protocol.lua`, `osk_config_view.lua`, and `osk_color_picker.lua` shared modules.

## Local Contracts

- ExtState payloads must remain compatible with `CSurfIntegrator` and `ControlSurface` command handling.
- The OSK layout, state, label, binding, and action-list formats are serialized contracts; change both ends together.
- Module loading must continue to work from the installed REAPER Scripts path.
- Load public display names and ExtState sections from the CMake-generated `product_identity.lua`; do not duplicate those values in Lua modules.
- Track unapplied editor changes separately from live unsaved changes, and request C++ revert when an editor with live changes closes.
- Keep the binding editor's structured columns, pseudo-modifiers, generated titles, and color controls backed by the same serialized raw action line used by `ConfigApplyLive`.
- Keep config color-picker swatches compact: empty saved/recent slots use checker/transparent swatches, left-click uses a stored color, and right-clicking a saved slot overwrites it with the current picker color.
- Keep config color live preview separate from toolbar Apply Live: preview uses `ConfigApplyLive`, coalesces in-flight color changes to the latest serialized binding state, and does not request a follow-up `ConfigQuery`.
- Keep OSK Inactive LED Boost display-only: apply it only to inactive state/action colors for OSK button widgets that do not have a fixed layout color; fixed layout colors are read-only in the config table, render active at full layout color and inactive at HSV/value -50, and must never alter serialized action-line colors, CSI state payloads, OSD, faders, or rotaries.
- Keep action search beside the Action field, apply clicked results directly, match all space-separated partial terms across titles plus numeric and named REAPER command IDs, and leave the raw action line visible at the end of the form.
- Keep the config window undockable and its dirty-state Apply, Save, and Revert controls in the top toolbar so they remain visible.
- Show direction pseudo-modifiers only for relative controls identified by layout metadata; normal buttons must not expose Increase or Decrease controls.
- Show the resolved binding title as the editable OSD default, and when KeyLabel is empty present OSD as its effective default without serializing duplicate properties until the user edits them.
- Always apply built-in OSK label replacements; merge user replacements on top with user rules taking priority and longer phrases evaluated before shorter phrases.
- Send OSK wheel input as rate-limited semantic acceleration packets; do not generate MIDI messages or expose device IDs from Lua.
- Send OSK fader drag and wheel input as absolute normalized `WidgetValue` packets; when fader feedback is dB-valued, convert it for display and send matching dB command values for DB actions.
- Prefer OSK layout `Role`, `Input`, `Feedback`, and semantic target metadata over `Shape`, widget name, or group heuristics when choosing widget behavior.
- Treat the OSK/OSD Lua interface as pre-release: use only current `ReaCtrlSurf_*` sections and settings, with no legacy aliases unless publication changes that requirement.
- Keep one OSK window per surface and persist each surface position independently without writing persistent ExtState on every movement frame.
- Keep OSK surface enabled/hidden state persisted per surface and mirrored to C++ through `SurfaceEnabled` when a window closes.
- Keep OSK widget config window geometry persistent, keep its font independent from OSK button font settings, and keep embedded OSD bar position scoped per surface.
- Keep standalone OSD settings reachable by right-clicking a visible OSD overlay; do not add an idle launcher window.
- Keep OSK font size, font family, wrapped-label line-height, and label-case controls in the OSK context menu near zoom.
- Keep OSK wheel inversion in the context menu with interactive-control settings; ReaImGui exposes wheel delta but not reliable mouse-wheel versus trackpad source.
- Show OSK hover tooltips with the default binding first and `+ `-prefixed alternate bindings for modifiers, Hold, DoublePress, and combined pseudo-modifier entries.

## Work Guidance

- Keep data acquisition/parsing in `osk_data.lua`, drawing coordination in `osk_render.lua`, input dispatch state in `osk_input.lua`, binding editing coordination in `osk_config.lua`, and OSD behavior in `osd_ui.lua`.
- Keep reusable ReaImGui widget helpers in `ui_components.lua` so entry scripts and feature modules do not re-implement the same UI patterns.
- Keep shared script startup, context creation, toolbar state, and shutdown boilerplate in `script_host.lua` so the entry scripts stay thin orchestration layers.
- Keep serialized action-line, layout, and label-replacement contracts in `action_line.lua`, `layout_parser.lua`, and `label_replacements.lua` so UI modules do not own those formats.
- Keep pure Lua parser checks registered through `self_checks.lua` when adding or changing parser module self-checks.
- Keep OSD color, alpha, contrast, and centered text drawing in `osd_ui.lua` so standalone OSD and the embedded OSK bar share one renderer.
- Keep visual defaults, reusable style tokens, and color/font helpers in `theme_settings.lua`; keep per-context font attachment/caching in `font_cache.lua`; keep typed ExtState setting coercion in `settings_store.lua`.
- Keep OSK context-menu settings UI in `osk_settings_ui.lua`, low-level widget math in `osk_widget_math.lua`, reusable draw primitives in `osk_draw_primitives.lua`, and widget shape drawing/interaction plumbing in `osk_widget_drawers.lua`.
- Keep config binding parsing/state helpers in `osk_config_model.lua`, ExtState request/response handling in `osk_config_protocol.lua`, config-window rendering in `osk_config_view.lua`, and persistent color-picker/swatches behavior in `osk_color_picker.lua`.
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
- Verify standalone OSD settings stay open after the message timeout when opened by right-clicking the visible overlay.

## Child DOX Index

- None.
