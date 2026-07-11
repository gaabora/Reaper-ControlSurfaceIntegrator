# Surface Template Extraction Plan

## Summary

This plan extends the new surface-level `ColorCalibration` approach into a broader pattern for extracting device-specific behavior from C++ into optional `Surface.txt` configuration.

The goal is to move configurable policy out of hardcoded surface checks and into reusable surface-template features, while leaving protocol-specific message encoding and decoding in C++.

Success criteria:

- new surface-specific tweaks can be enabled from `Surface.txt` without adding new `if (surface == "...")` logic
- existing surfaces keep current behavior when new blocks are absent
- device processors continue to own protocol framing, but not optional behavior policy

## Current Direction

`ColorCalibration` is now the reference pattern:

- parsed as an optional surface-level block
- stored on `ControlSurface`
- consumed by a generic helper (`GetDeviceFeedbackColor()`)
- opt-in per surface

The same extraction pattern should be reused for other surface-specific behaviors that are:

- user-tunable
- surface-template dependent
- currently hardcoded in general runtime or UI code

This pattern should not be used for protocol behavior that is inherently tied to a device message format and offers no useful template-level variation.

## Extraction Targets

### 1. Learn-dialog surface filtering

Current issue:

- `src/ui/learn_dialog.cpp` contains hardcoded `SCE24` checks that change how learned display widgets are written out

Plan:

- add a surface capability block to `Surface.txt`
- replace name-based checks with capability-based behavior

Recommended `Surface.txt` shape:

```txt
SurfaceCapabilities
  LearnWidgetFilterMode ParamNameValueMatch
SurfaceCapabilitiesEnd
```

Behavior:

- when absent, keep current default behavior
- when `ParamNameValueMatch` is enabled, only emit the display widgets that match the selected param/name/value targets

### 2. Surface initialization SysEx/messages

Current issue:

- MCU and MCUXT startup/reconnect initialization lives in hardcoded `InitializeMCU()` / `InitializeMCUXT()` paths in `Midi_ControlSurface`

Plan:

- move initialization message sets into optional `Surface.txt` global blocks
- use one generic runtime path to send initialization messages on startup and reconnect

Recommended `Surface.txt` shape:

```txt
InitMessages
  F0 7E 00 06 01 F7
  F0 00 00 66 14 00 F7
  F0 00 00 66 14 21 01 F7
InitMessagesEnd
```

Behavior:

- if `InitMessages` is absent, preserve existing behavior
- if present, use the configured message set instead of device-specific init branches

### 3. Display color override / palette control

Current issue:

- X-Touch display-color override support is exposed through X-Touch-specific actions and feedback hooks
- this is a useful capability pattern, but it is locked to one device family

Plan:

- generalize surface display-color override support behind a surface-level palette/config mechanism
- keep existing X-Touch actions as compatibility wrappers in the first pass

Recommended direction:

- add a named display-color palette block to `Surface.txt`
- add a generic runtime path for applying and restoring surface display colors
- map names to surface-specific color codes inside the surface implementation

Recommended `Surface.txt` concept:

```txt
DisplayColorPalette
  Black 0
  Red 1
  Green 2
  Yellow 3
  Blue 4
  Magenta 5
  Cyan 6
  White 7
DisplayColorPaletteEnd
```

Behavior:

- surfaces that do not support this simply omit the block
- X-Touch can migrate to the shared path without changing user-facing behavior

### 4. Surface capability flags for UI/runtime quirks

Current issue:

- some behavior differences belong neither to widget syntax nor to protocol framing, but are still currently buried in code

Plan:

- add a small set of generic surface capability flags stored on `ControlSurface`
- use them for optional behavior branches in shared code

Initial capabilities worth supporting:

- `LearnWidgetFilterMode`
- `TrackColorDisplayOverride`
- `DisplayColorOverrideFormat`
- `InitMessageSet`

These should be represented as optional, backward-compatible capabilities, not mandatory schema.

## Non-Goals

The first extraction pass should **not** attempt to externalize:

- MFT indexed palette lookup in `fb_generic.h`
- `rgbToColor()` palette reduction for X-Touch / OSC X32
- MCU meter curves already driven by `MeterMode`
- device-family message framing in `fb_mcu.h`, `fb_icon.h`, `fb_qcon.h`, `fb_faderport.h`, or similar files
- a full declarative replacement for widget registration or message-generator construction

Those areas are protocol-heavy and should stay in C++ unless there is a clear surface-template use case.

## Implementation Plan

### Phase 1. Surface capability plumbing

- extend the current surface-level block parser in `ControlSurface::ProcessValues()`
- add support for optional global blocks beyond `StepSize`, `AccelerationValues`, and `ColorCalibration`
- store parsed values in a dedicated surface capability/config structure

### Phase 2. Learn-dialog extraction

- replace `SCE24` name checks in the learn dialog with a capability lookup on the owning surface
- preserve current default behavior for all surfaces that do not opt in

### Phase 3. Init message extraction

- add generic support for `InitMessages`
- route MIDI startup and reconnect initialization through the configured surface message set
- keep current MCU/MCUXT behavior as the fallback until all built-in templates are migrated

### Phase 4. Display color override generalization

- define a generic display-color override abstraction on the feedback/surface side
- make X-Touch use the shared mechanism internally
- retain existing X-Touch-specific actions as wrappers for compatibility

### Phase 5. Follow-up review

- audit remaining surface-name branches and hardcoded policy decisions
- only extract cases that clearly belong to template configuration rather than protocol implementation

## Validation

### Build

- `cmake --build build --config Debug`

### Behavior checks

- FaderPort neutral-gray calibration still behaves correctly with `ColorCalibration`
- SCE24 learn output matches current filtered behavior when the new capability is enabled
- non-SCE24 surfaces keep current learn output when the capability is absent
- MCU / MCUXT initialization still runs on startup and MIDI reconnect
- X-Touch display color overrides still work with existing actions after the generic layer is introduced

### Regression checks

- surfaces without any new blocks behave exactly as before
- no `CSI.ini` changes are required
- no existing `Surface.txt` file must be updated unless it opts into a new capability

## Assumptions

- the repository wants a pragmatic extraction layer, not a full declarative hardware protocol system
- backward compatibility for existing surfaces and actions is required
- new surface-template blocks should be optional and additive
- protocol-specific encoders remain in C++ even when high-level behavior becomes template-driven
