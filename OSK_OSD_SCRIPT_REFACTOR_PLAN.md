# OSK and OSD Script Refactor Plan

## Purpose

This plan covers cleanup of the Lua OSK/OSD scripts after the current configuration editor and label replacement work. The goal is to fix known correctness risks first, then extract reusable UI and parsing components so the scripts stay smaller, clearer, and easier to evolve.

## Current Baseline

Primary files:

- `Scripts/CSI/CSI OSK on-screen keyboard.lua`
- `Scripts/CSI/CSI OSD on-screen display.lua`
- `Scripts/CSI/osk_data.lua`
- `Scripts/CSI/osk_render.lua`
- `Scripts/CSI/osk_config.lua`
- `Scripts/CSI/osd_ui.lua`

Current strengths:

- OSK entry point is already split from data, rendering, config editing, and OSD UI.
- C++/Lua communication is mostly centralized through current `ReaCtrlSurf_*` ExtState sections.
- OSK config editor has structured editing, dirty tracking, live apply, save, and revert.
- OSD display logic is already shared between standalone OSD and OSK in part.

Known review findings to address:

- `osk_config.lua` calls `BeginChild` without guaranteed matching `EndChild` when the child is not visible.
- `osk_data.lua` layout label parsing can consume trailing `,Color=...`.
- `osk_render.lua` press and tooltip state keys use only widget name, so multiple surfaces can collide.
- `osk_config.lua` quoted action properties can serialize escaped quotes but parse them incorrectly.
- `osd_ui.lua` ignores repeated identical OSD messages, so the timeout may not refresh.
- Standalone OSD settings are difficult to open when no OSD message is visible.
- `osd_ui.RenderOSDBar` is unused by OSK and unsafe if reused without a height argument.
- GUI helper code is scattered across entry scripts, `osk_config.lua`, and `osd_ui.lua`.

## Phase 1: Correctness Fixes

Goal: remove behavior risks before moving code around.

### Work

1. Ensure every ImGui `Begin*` call has a guaranteed matching `End*`.
   - Fix `BeginChild` / `EndChild` in `osk_config.lua`.
   - Audit table, popup, tooltip, child, and style push/pop pairs.
2. Fix layout property parsing in `osk_data.lua`.
   - Parse layout metadata as key/value pairs instead of ad hoc `Label=(.+)$`.
   - Preserve labels, colors, group names, shape, size, and top offset independently.
3. Scope OSK transient interaction state by surface and widget.
   - Replace `pressedWidgets[widgetName]` with `pressedWidgets[surfaceName .. "|" .. widgetName]`.
   - Do the same for tooltip hover timing and wheel state where needed.
4. Make action-line quoting round-trip safe.
   - Teach tokenization to handle escaped quotes.
   - Add one helper for quote/unquote behavior and use it for OSD, KeyLabel, parameters, and unknown properties.
5. Make repeated OSD messages refresh display lifetime.
   - Prefer deleting or acknowledging consumed ExtState messages if compatible.
   - If not enough, add a sequence/timestamp field to the C++ OSD payload and update docs.
6. Make standalone OSD settings reachable when no message is visible.
   - Add a tiny always-available settings hit area/window, or provide a toolbar/menu command path.
7. Remove or repair unused `osd_ui.RenderOSDBar`.
   - Either delete it or make it the one shared renderer used by OSK.

### Acceptance

- Debug build passes.
- No ImGui stack imbalance warnings in REAPER.
- Two OSK surfaces with same widget names do not cross-release or share tooltip timing.
- Labels with `Label=...` and `Color=...` display correctly.
- Repeated identical OSD messages extend the visible timeout.
- OSD settings can be opened from an idle state.

## Phase 2: Shared UI Components

Goal: centralize common ReaImGui widget patterns and style behavior.

### New Module: `Scripts/CSI/ui_components.lua`

Candidate helpers:

- `Tooltip(ctx, text, wrapped)`
- `HelpTooltip(ctx, text)`
- `InputTextWithClear(ctx, id, value, placeholder)`
- `DirtyActionButton(ctx, label, enabled, onClick)`
- `Disabled(ctx, disabled, renderFn)`
- `SliderWithInput(ctx, label, value, min, max, step, options)`
- `ComboEnum(ctx, label, value, options)`
- `Toolbar(ctx, leftFn, rightFn)`
- `ColorSwatch(ctx, id, color, tooltip)`
- `BeginPinnedToolbarBody(ctx, toolbarFn, bodyFn)`

### Work

1. Move tooltip helpers from OSK entry, config editor, and render modules.
2. Move the dirty red Apply/Save/Revert button styling from `osk_config.lua`.
3. Move slider/input and combo helpers from `osd_ui.lua`.
4. Move label replacement input UI from OSK entry into a component-like function.
5. Use the same disabled styling and help tooltip behavior everywhere.

### Acceptance

- OSK and OSD settings use the same tooltip, slider, combo, and disabled-button behavior.
- `osk_config.lua` and `osd_ui.lua` shrink without changing visible behavior.
- Styling constants are in one place.

## Phase 3: Script Host Boilerplate

Goal: remove duplicated entry-script setup code.

### New Module: `Scripts/CSI/script_host.lua`

Candidate responsibilities:

- ReaImGui dependency check and ReaPack prompt.
- `package.path` setup.
- ImGui context creation.
- Font creation and attachment.
- Toolbar toggle state management.
- Context validity check.
- Common `atexit` cleanup wrapper.

### Work

1. Extract duplicate ReaImGui dependency checks from OSK and OSD entry scripts.
2. Extract duplicate toolbar toggle state code.
3. Provide a small host API:
   - `RequireImGui(scriptDir)`
   - `CreateContext(name, fonts)`
   - `SetToolbarState(value)`
   - `IsContextValid(ctx)`
   - `OnExit(cleanupFn)`
4. Keep entry scripts responsible only for script-specific modules and main loop.

### Acceptance

- OSK and OSD entry scripts become short orchestration files.
- Toolbar state still turns on while running and off on exit/error.
- Missing ReaImGui behavior remains unchanged.

## Phase 4: Parser and Contract Modules

Goal: keep UI files from owning serialization formats.

### New Modules

- `Scripts/CSI/action_line.lua`
- `Scripts/CSI/layout_parser.lua`
- `Scripts/CSI/label_replacements.lua`

### Work

1. Move action-line parse/build/color logic out of `osk_config.lua`.
2. Move layout string parsing out of `osk_data.lua`.
3. Move label replacement merge, priority ordering, and help syntax out of `osk_data.lua`.
4. Add lightweight pure-Lua self-check helpers if no test runner exists.
5. Keep all exported functions compatible with current call sites during the first extraction.

### Acceptance

- `osk_config.lua` no longer contains raw action-line tokenizer/parser code.
- `osk_data.lua` focuses on ExtState polling and data storage.
- Parser modules can be manually loaded in REAPER without OSK windows.

## Phase 5: OSK Input Runtime Extraction

Goal: separate input semantics from drawing.

### New Module: `Scripts/CSI/osk_input.lua`

Candidate responsibilities:

- Press-down and press-up tracking.
- Surface-scoped transient state keys.
- Mouse-wheel acceleration state.
- Rate-limited ExtState command sending.
- Context-menu suppression timing.

### Work

1. Move `pressedWidgets`, `wheelStates`, and wheel acceleration helpers out of `osk_render.lua`.
2. Expose simple functions:
   - `HandlePressDown(surfaceName, cell)`
   - `HandlePressUp(surfaceName, cell)`
   - `HandleWheel(ctx, surfaceName, cell)`
   - `FlushWheelCommands()`
3. Keep drawing functions in `osk_render.lua` focused on geometry and visual output.

### Acceptance

- OSK press, release, hold, double-press, and wheel behavior remains unchanged.
- `osk_render.lua` becomes easier to scan by shape drawing responsibility.

## Phase 6: Unified OSD Drawing

Goal: render standalone OSD and embedded OSK OSD bar through one drawing path.

### Work

1. Create a shared function that draws an OSD rectangle and centered text from explicit geometry.
2. Use it from standalone `RenderOSDWindow`.
3. Use it from OSK embedded OSD bar.
4. Keep positioning logic separate from drawing.
5. Remove duplicate contrast, alpha, and centering code.

### Acceptance

- Standalone OSD and OSK embedded OSD use identical color/contrast logic.
- OSD bar still respects OSK layout and configured top/bottom placement.
- Standalone OSD still respects screen alignment, margins, size, and font settings.

## Phase 7: Documentation and Verification

Goal: make the new structure easy to maintain.

### Work

1. Update `Scripts/CSI/AGENTS.md` with any new module ownership boundaries.
2. Update `LUA_CPP_EXTSTATE_INTERFACE.md` if any ExtState payload changes.
3. Add a small manual REAPER verification checklist.
4. Keep root user docs in sync only for user-visible behavior changes.

### Manual Verification Checklist

- Launch OSK and OSD scripts with ReaImGui installed.
- Open OSK context menu and change settings.
- Verify label replacements with empty user input, user override, and longer phrase override.
- Open widget config, edit action, search action, apply live, save, and revert.
- Test two surfaces with the same widget names.
- Test press/release, hold, double-press, and rotary wheel input.
- Send repeated identical OSD messages and confirm timeout refresh.
- Open OSD settings while no message is visible.

## Suggested Execution Order

1. Phase 1 only, no major extraction.
2. Phase 2 UI components.
3. Phase 3 entry-script host cleanup.
4. Phase 4 parser extraction.
5. Phase 5 input extraction.
6. Phase 6 OSD drawing unification.
7. Phase 7 docs and verification pass.

Each phase should be buildable and manually testable before the next phase starts.
