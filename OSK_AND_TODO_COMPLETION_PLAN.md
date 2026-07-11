# OSK and Remaining TODO Completion Plan

## Purpose

This document consolidates the work that is still relevant from:

- `tmp/FEATURE3_PLAN.md`
- `tmp/FIXME_TODO_PLAN.md`
- `tmp/LUA_CPP_EXTSTATE_INTERFACE.md`
- `tmp/OSK_MIDI_SIMULATION_PLAN.md`
- `tmp/OSK_REFACTORING_PLAN.md`
- `tmp/OSK_DESIGN.md`

The older documents remain useful design history, but several of their "future" items
are already implemented. This plan should be used as the current execution order.

## Current Baseline

Already implemented:

- OSK layout parsing, widget shapes, state/color feedback, labels, and label maps.
- `ToggleOSK`, multi-surface display, combined/separate windows, and persistent settings.
- Lua module split into entry point, data, rendering, configuration, and OSD modules.
- Semantic press-down, press-up, and scroll commands from Lua to C++.
- C++ hold and double-press timing through the normal action path.
- Right-click widget configuration query, live apply, save, timestamped backup, revert,
  CSI action list export, and REAPER action search.
- Named constants and most DRY work listed as completed in `FIXME_TODO_PLAN.md`.

## Phase 1: Configuration Editor Correctness

Goal: make editing safe and predictable before expanding the UI.

**Status:** Implemented on June 14, 2026. Debug build passes; manual REAPER acceptance testing remains.

### Work

1. [x] Add explicit dirty-state tracking in `osk_config.lua`.
2. [x] Automatically revert live unsaved changes when the editor or script closes.
3. [x] Keep the last confirmed binding snapshot in Lua.
4. [x] Validate complete C++ binding batches before replacing active contexts.
5. [x] Return structured, widget-scoped status for query, apply, save, and revert.
6. [x] Preserve quoted parameters, unknown properties, colors, inline comments,
   pseudo-modifiers, inversion, directional bindings, and unrelated zone lines.
7. [x] Write through a temporary file and recover from replacement failure.
8. [x] Avoid backups for no-op saves and remove temporary backup artifacts after failed commits.

### Acceptance

- Closing after Apply Live cannot leave accidental in-memory changes behind.
- A malformed binding produces a visible error and leaves the previous binding active.
- Save changes only the selected widget lines in the selected zone.
- Failed writes leave both the original file and active in-memory configuration usable.

## Phase 2: Complete the Configuration Editor UI

Goal: replace the raw-line-oriented prototype with the intended binding editor.

**Status:** Implemented on June 15, 2026. Static checks and Debug build pass; manual REAPER acceptance testing remains.

### Work

1. [x] Render bindings as columns: Modifier, Action, Colors, and Other.
2. [x] Display `Hold` and `DoublePress` as explicit pseudo-modifiers rather than hiding
   them inside timing properties.
3. [x] Show a generated action title using this order:
   - explicit `OSD` or `KeyLabel`
   - REAPER action name
   - CSI action name processed through label replacements
   - raw action line
4. [x] Add inactive and active color swatches for `{ r g b r g b }` action colors.
5. [x] Add a color picker with a small reusable palette and a clear/default option.
6. [x] Keep raw-line editing as an advanced fallback.
7. [x] Make Add, Remove, Clone, Move, and action-search application update dirty state.
8. [x] Keep batch `ConfigApplyLive` as the only write protocol. Dedicated
   `ConfigAddBinding`, `ConfigRemoveBinding`, and `ConfigUpdateBinding` commands were
   never implemented and are not needed.

### Acceptance

- Normal, modified, Hold, and DoublePress bindings can be created without raw editing.
- Both action colors round-trip through query, live apply, save, and reload.
- Generated titles remain understandable when no explicit label is present.

## Phase 3: OSK Input Parity

Goal: make mouse interaction behave consistently with physical controls.

**Status:** Implemented on June 15, 2026. Debug build passes; manual REAPER acceptance testing remains.

### Work

1. [x] Add mouse-wheel acceleration based on wheel event timing and accumulated wheel delta.
2. [x] Route accelerated wheel events through the indexed
   `ZoneManager::DoRelativeAction(widget, accelerationIndex, delta)` overload.
3. [x] Add debounce/rate limiting so high-resolution wheels cannot flood ExtState commands.
4. [x] Keep semantic input as the default because it supports MIDI and OSC surfaces.
5. [x] Do not add MIDI-message injection without a real device-specific behavior that
   cannot be represented semantically. A future implementation would:
   - store parsed per-widget MIDI press/release/increase/decrease messages
   - inject through `Midi_ControlSurface::ProcessMidiMessage()`
   - fall back to semantic dispatch when no mapping exists
6. [x] Do not add Lua `StuffMIDIMessage` support unless a concrete host/device requirement
   appears; it requires exposing device IDs and is more fragile than the C++ bridge.

### Acceptance

- Slow and fast wheel movement produce distinguishable accelerated behavior.
- Press, release, Hold, and DoublePress match physical button timing.
- MIDI and OSC OSK surfaces continue to work without Lua device IDs.

## Phase 4: Windows, Settings, and ExtState Cleanup

Goal: simplify the runtime model without breaking existing user settings.

### Work

1. Measure and fix separate-window movement lag before removing combined mode.
2. If separate-only mode is selected:
   - migrate existing `window_mode`, `surface_pos`, and `show_all_surfaces` settings
   - store each surface position independently
   - remove dead combined/tab code after one compatibility release
3. Rename the UI setting `clickable` to `interactive`, while reading the old key as a
   migration fallback.
4. Document the implemented Feature 3 keys in `LUA_CPP_EXTSTATE_INTERFACE.md`:
   `ConfigQuery`, `ConfigApplyLive`, `ConfigSave`, `ConfigRevert`, response keys,
   `ConfigStatus`, and `ActionList`.
5. Remove obsolete proposed keys such as `ZoneInfo_*`, `ReloadZones`, and
   `ActiveZone_*` unless they gain a real consumer.
6. Move OSD from `CSI_TMP` only as a versioned protocol change.
7. Treat a `CSI_` to `ReaCtrlSurf_` prefix rename as a migration project, not a simple
   search-and-replace. Read old keys during a compatibility period.

### Acceptance

- Existing users retain window positions and interaction settings after migration.
- The interface document matches every key currently produced or consumed.
- No protocol rename causes an OSK/OSD version mismatch to fail silently.

## Phase 5: Outstanding Core Correctness Work

Goal: resolve the still-valid items from `FIXME_TODO_PLAN.md`.

### Safety First

1. Guard the learn-dialog `GetDlgItemText` call against a missing or destroyed dialog.
2. Fix or formally define the SubZone modifier inheritance behavior.
3. Decide whether `ForceClearTrack` must clear one or multiple widgets; add `break` only
   if uniqueness is guaranteed.
4. Add CSI.ini version-mismatch backup and a user-visible migration message.
5. Validate referenced `GoZone` and `GoSubZone` targets during zone preprocessing.

### Contract Reviews

1. Resolve or document `FixedTrackNavigator` handling.
2. Document `GetRestrictedLengthText` return ownership or always return the caller buffer.
3. Document the lifetime of non-owning track-color feedback pointers.
4. Design a replacement for `BUTTON_RELEASE_MESSAGE_VALUE == 0.0` before changing it;
   zero is also a legitimate continuous-control value.

### Lower-Priority Improvements

1. Move FX alias prefixes to configuration with compiled defaults.
2. Add a compatibility alias before renaming `TrackInvertPolarity`.
3. Support explicit OSC argument types and multiple arguments in `SendOSCMessage`.
4. Remove the `reaper_actions.h` compatibility shim after all includes are migrated.
5. Add integer `1-3` stepped-range syntax.
6. Detect selected-track reordering, not only selected-track count changes.

### Acceptance

- The identified crash/assert paths are guarded.
- Invalid zone references are reported with file and zone context.
- Compatibility-sensitive action names and release semantics are migrated rather than
  changed abruptly.

## Phase 6: Documentation Consolidation

Goal: leave one accurate description of the implemented system.

### Work

1. Update each source plan with a short status banner:
   `Implemented`, `Partially implemented`, or `Superseded by this plan`.
2. Update `LUA_CPP_EXTSTATE_INTERFACE.md` from the current C++ and Lua code.
3. Keep `OSK_DESIGN.md` as the architecture reference, but remove obsolete phase status.
4. Keep `FIXME_TODO_PLAN.md` focused on unresolved core issues and remove completed items.
5. Verify external Surface.txt and zone annotation rollout in an actual REAPER CSI
   resource directory; those files are not present in this repository.

## Recommended Execution Order

1. Phase 1: editor correctness and transactional safety.
2. Phase 2: colors, virtual modifiers, and usable binding presentation.
3. Phase 3: wheel acceleration and input parity.
4. Phase 5 safety items.
5. Phase 4 protocol/settings cleanup.
6. Phase 5 lower-priority improvements.
7. Phase 6 documentation consolidation.

## Verification Strategy

There is no automated unit-test suite in this repository. Each implementation phase
must include:

- A Debug CMake build.
- Manual REAPER testing with at least one MIDI surface configuration.
- OSK press, release, Hold, DoublePress, and wheel checks.
- Config query, Apply Live, Save, Revert, close-with-dirty-state, and malformed-input checks.
- Zone-file diff inspection and backup recovery testing.
- OSC testing when protocol-independent input or `SendOSCMessage` changes.
