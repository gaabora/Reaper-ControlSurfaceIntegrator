# Zone Widget Modifier Validation

## Goal

Define a deterministic Zone binding grammar and validation contract for physical widgets, modifiers, state selectors, button gestures, directional input, value transforms, timing, and context changes.

This document describes proposed behavior. The current runtime does not implement this complete contract.

## Explicit Zone Syntax

The left side of a binding has three explicit token classes:

- Square brackets `[]` contain context selectors.
- Parentheses `()` contain input events, direction selectors, or value transforms.
- The last unwrapped name is an exact physical widget or a widget family with the `@CH` surface-channel qualifier.

Context selectors include:

- Standard modifiers: `[Shift]`, `[Option]`, `[Control]`, `[Alt]`, `[Flip]`, `[Global]`, `[Marker]`, `[Nudge]`, `[Zoom]`, and `[Scrub]`.
- Zone-local pseudo-modifiers such as `[SomeButton]`.
- Channel states: `[Touch]` and `[Toggle]`.

Input events and transforms include:

- Button events: `(Press)`, `(Tap)`, `(Release)`, `(Hold)`, `(LongHold)`, and `(DoublePress)`.
- Relative directions: `(Increase)` and `(Decrease)`.
- Value transforms: `(Invert)` and `(InvertFB)`.

Examples:

```text
[Shift]+(Hold)+Marker SomeAction
[Marker]+(DoublePress)+Play SomeAction
[Touch]+(Increase)+Rotary1 SomeAction
[Shift]+[SomeButton]+(Hold)+Play SomeAction
[Shift]+Rotary@CH TrackPan
```

An unwrapped name before the action is always a physical widget reference. A widget named `Marker`, `Touch`, or `Hold` remains valid because only wrapped tokens have special meaning. `@CH` is the exact case-sensitive postfix qualifier for expansion across physical surface channels. It is not a context selector and does not describe the zone's logical `Target`.

An unqualified button binding uses the effective `DefaultButtonTrigger` setting:

```text
Button SomeAction
```

The canonical form places context selectors first, then one input event, then one direction selector, then value transforms, and then the physical widget. Non-canonical order can have a quick fix, but it does not change binding identity.

## Selector Meanings

`[Touch]` selects an action context while the target widget's surface channel is touched. The touch state normally comes from a separate hardware touch message, such as the capacitive sensor on a motor fader. CSI receives only the device's touch on or off result. It does not detect bare skin itself and does not define which material the hardware accepts. Touch can also come from any control that reports a two-state touch event and has the same channel number as the target widget.

`[Touch]` is a selector, not a general safety filter. A binding without `[Touch]` remains available as the fallback when the channel is not touched. A surface can use this for different fader or encoder actions while the user touches the control, for touch-sensitive automation, or to suppress motor feedback through action-specific touch behavior.

`[Toggle]` selects a software-maintained per-channel state. The `ToggleChannel` action flips that state for the source widget's channel. It is useful for two banks of behavior on the same row of controls, even when the hardware has no physical toggle switch. It is not the feedback state of an arbitrary toggle action.

`[Global]` is the name of one standard modifier bit. It does not make an action, Zone, or widget globally visible. Its state follows the same Page, Surface-local, broadcaster, and listener routing rules as the other standard modifiers. The default configuration shares standard modifier state through the Page unless a Surface is configured to use or receive local modifier state.

`[Touch]` and `[Toggle]` are channel state selectors. `[Global]` is a standard modifier. Their shared bracket syntax does not make them the same type internally.

## Modifier Declarations

Physical widget names do not automatically create modifiers. Standard modifiers use an explicit declaration:

```text
ShiftButton Modifier Shift
MarkerButton Modifier Marker
LinkButton Modifier Control Mode=Momentary
```

Zone-local named modifiers use `PseudoModifier` as defined in [PSEUDO_MODIFIER_PLAN.md](PSEUDO_MODIFIER_PLAN.md):

```text
SomeButton PseudoModifier
OtherButton PseudoModifier Mode=Momentary
```

`Mode` is an optional declaration override. A declaration without `Mode` uses the effective default for its declaration type.

Do not resolve a modifier name as an alias for its physical widget. A Hold action on `LinkButton` must name `LinkButton`, not `Control`:

```text
(Hold)+LinkButton SomeAction
```

The parser must collect declarations before it validates bindings. Declaration order must not change behavior.

## Normalized Binding Model

Validation must parse every binding into a normalized record:

```text
context key = zone + resolved physical widget + optional surface-channel index + standard modifier set + pseudo-modifier set + Touch state + Toggle state
event       = configured default | Press | Tap | Release | Hold | LongHold | DoublePress
direction   = Any | Increase | Decrease
transforms  = Invert + InvertFB
```

Selector order does not change identity. Several action lines can have the same normalized identity. These lines form one ordered action group and are not independent alternatives.

## Modifier Modes

Every modifier declaration has an effective mode. Standard and pseudo-modifier declarations use separate defaults:

```text
DefaultModifierMode=Latch
DefaultPseudoModifierMode=Latch
```

Resolve the effective mode from lowest to highest priority:

```text
compiled fallback Latch < product global default < Device override < declaration Mode override
```

`Mode=Momentary|Latch|Hybrid` on a declaration overrides only that declaration. Changing either configured default requires validation of every declaration that inherits it because the effective mode changes source-widget gesture validity.

### Momentary

`Mode=Momentary` engages the modifier on press and disengages it on release. A Hold action on the modifier source widget is invalid because normal chord use requires the user to keep that button pressed while reaching another control.

```text
ShiftButton Modifier Shift Mode=Momentary
(Hold)+ShiftButton SomeAction
```

The second line is an error.

### Latch

`Mode=Latch` uses tap-toggle behavior. One Tap engages the modifier and the next Tap disengages it. A Hold action on the source widget is allowed because Hold is not used by latch state management.

```text
ShiftButton Modifier Shift Mode=Latch
(Hold)+ShiftButton SomeAction
```

The Hold action is additional behavior and does not toggle the latch state.

### Hybrid

`Mode=Hybrid` uses tap to latch and a longer press for temporary engagement or unlock behavior. Hold on the source widget is invalid because the modifier state machine already owns press duration.

The current CSI modifier behavior is closest to Hybrid. The new grammar must not silently label current Hybrid behavior as Latch.

### Modifier Event Rules

- `(Hold)` and `(LongHold)` on the source widget are invalid for Momentary and Hybrid.
- `(Hold)` and `(LongHold)` on the source widget are allowed for tap-toggle Latch.
- `(DoublePress)` on a modifier source is additive and must report a warning unless the modifier state machine explicitly consumes DoublePress.
- A Hold on another widget while a modifier is active is valid, such as `[Shift]+(Hold)+Play`.
- A zone change must clear zone-local pseudo-modifier state before input routing continues.

## Button Event Semantics

Trigger selection belongs to the binding, not to the Action class. Do not add a `RunOn` property to actions.

### Press

`(Press)` runs immediately when the button is pressed. It is additive with later Hold, LongHold, and DoublePress events. An action that changes context on Press makes later events from the old context unreachable.

### Release

`(Release)` runs on every physical release, including a release after Hold or LongHold. Use it only when raw release behavior is required.

### Tap

`(Tap)` runs on release only when Hold and LongHold did not fire. Tap is the correct event for an exclusive short-press and long-press pair:

```text
(Tap)+Button  GoZone Zone1
(Hold)+Button GoZone Zone2
```

### Hold and LongHold

`(Hold)` fires when `HoldDelayMs` is reached. `(LongHold)` fires later when `LongHoldDelayMs` is reached.

Hold and LongHold are milestones, not exclusive alternatives. If the button remains pressed, Hold fires first and LongHold fires later.

```text
(Hold)+Button     Action1
(LongHold)+Button Action2
```

Use a numeric binding override for an exceptional delay:

```text
(LongHold)+Button Action DelayMs=5000
```

Do not use symbolic numeric values such as `HoldDelay=long`.

### DoublePress

DoublePress behavior is controlled by `DoublePressPolicy`:

- `Additive` allows the first Tap or Press action to run before DoublePress.
- `Exclusive` delays Tap until the DoublePress window closes. A recognized DoublePress suppresses Tap.

Press and raw Release remain additive even when DoublePress is Exclusive.

Hold and DoublePress can share one context:

```text
(Hold)+Metronome        GoZone Zone1
(DoublePress)+Metronome GoZone Zone2
```

A short single press does nothing. A long first press opens `Zone1`. A fast double press opens `Zone2`.

## Behavior and Timing Configuration

The behavior schema includes:

- `DefaultModifierMode=Momentary|Latch|Hybrid`, with `Latch` as the compiled and initial product default.
- `DefaultPseudoModifierMode=Momentary|Latch|Hybrid`, with `Latch` as the compiled and initial product default.
- `DefaultButtonTrigger=Press|Tap`, with `Press` as the compiled and initial product default.
- `DoublePressPolicy=Additive|Exclusive`, with `Exclusive` as the compiled and initial product default.

Timing settings use this precedence, from lowest to highest:

```text
compiled fallback < product global settings < Device override < binding override
```

Default behavior and user timing preferences belong to root product settings and configured Devices. They do not belong in a Vendor Surface hardware template or in a Page Surface assignment.

Product defaults use the root `Settings` block in the unversioned product `.conf`:

```text
Settings {
  DefaultModifierMode=Latch
  HoldDelayMs=1000
  LongHoldDelayMs=2000
  DoublePressWindowMs=400
  ModifierTapWindowMs=100
}
```

A Device can override settings for every Page Surface assignment that uses that physical device:

```text
Device fp2 {
  Type=MIDI
  Channels=1
  Input=0
  Output=0

  Settings {
    HoldDelayMs=750
    LongHoldDelayMs=1500
  }
}
```

`Page.Surface.Settings` is not valid. An omitted Product value inherits the compiled default. An omitted Device value inherits the effective Product value. A setting can appear only once in one scope. An invalid root Settings block is rejected as one unit and runtime uses compiled defaults for that block. An invalid Device Settings block is rejected as one unit and that Device uses the resolved Product values. Other valid Devices continue to load.

The timing schema includes:

- `HoldDelayMs=1000`, range 50 to 10000.
- `LongHoldDelayMs=2000`, range 100 to 30000 and greater than the effective `HoldDelayMs`.
- `DoublePressWindowMs=400`, range 100 to 2000.
- `ModifierTapWindowMs=100`, range 50 to 5000, for Hybrid modifier behavior.
- `HoldRepeatIntervalMs=100`, range 25 to 5000.

`HoldRepeatIntervalMs` defines the default interval only. It does not enable repetition for every Hold action. A binding must explicitly request repetition. A positive binding `RepeatIntervalMs` both requests repetition and overrides the configured default interval.

Binding overrides use generic event properties:

```text
(Hold)+Button Action DelayMs=900 RepeatIntervalMs=100
(LongHold)+Button Action DelayMs=3000
```

The current `SetLatchTime`, `SetHoldTime`, and `SetDoublePressTime` zone actions change surface state in zone activation order. Replace them with persistent configuration and remove them after migration. Backward compatibility is not required before release.

## Current Runtime Gaps

The proposed implementation must replace or account for these current behaviors:

- [`ZoneManager::GetWidgetNameAndModifiers()`](../src/controls/zone_manager.cpp) recognizes unwrapped fixed keywords and silently ignores unknown modifier tokens.
- [`ZoneFileParser::ParseFile()`](../src/controls/zone_parser.cpp) creates modifier widget aliases and can make Hold validity depend on source or zone load order.
- [`ActionContext::DoAction()`](../src/actions/action_context.cpp) owns Hold and DoublePress timing independently in each action context.
- [`Widget`](../src/controls/widget.h) stores modifier, Hold, DoublePress, and Hold-fired state on the physical widget instead of the selected zone and normalized context.
- `Action::IgnoresRelease()` is a fixed action trait. There is no current per-binding Press, Tap, or Release selector.
- A parent or active zone layer can consume a widget before an IncludedZone receives it, even when the parent has no action for the exact normalized binding event.

The new runtime should use one deterministic button gesture recognizer per active physical widget context. Action contexts should consume recognized events instead of running independent timers.

## Diagnostic Levels

Use an error when a binding cannot work, has ambiguous meaning, violates the declared modifier mode, or can leave an active context dependent on source order.

Use a warning when behavior is valid but additive, delayed, or likely to surprise the user.

## Expression and Combination Errors

Report an error for:

- An unknown or incorrectly cased selector, event, direction, transform, modifier mode, or property.
- An empty expression part, missing physical widget, or more than one unwrapped widget name.
- A repeated selector or operator.
- More than one button event in one binding.
- Both `(Increase)` and `(Decrease)` in one binding.
- A button event together with a relative direction event.
- More than one of `[Marker]`, `[Nudge]`, `[Zoom]`, and `[Scrub]` in one context because those standard modes are mutually exclusive.
- A selector written without `[]` or an input operator written without `()`.
- An invalid `Mode` override or effective default modifier mode.
- Use of a modifier alias as a physical widget name.
- Press, Tap, Release, Hold, LongHold, DoublePress, Increase, Decrease, Touch, or Toggle semantics in an explicit lifecycle block such as `On ZoneActivation`.

`[Touch]` and `[Toggle]` can be used together because they are independent channel states.

## Surface Capability Errors

Resolve the binding against the selected surface and report an error when:

- The physical widget does not exist.
- An `@CH` family is missing any expected numbered widget for the configured surface channel count.
- A normal binding uses `*`; wildcard widget patterns are valid only in schema fields that explicitly accept them.
- A button event targets a widget without two-state press and release input.
- `(Increase)` or `(Decrease)` targets a widget without relative input.
- `[Touch]` or `[Toggle]` targets a widget without a valid channel number.
- `[Touch]` has no matching touch-state source for that channel.
- `(Invert)` is used without numeric input.
- `(InvertFB)` is used without numeric or toggle feedback.
- An input action is assigned to a display-only widget.
- A modifier is assigned to a widget without two-state press and release input.

Typed Surface `Input` and `Feedback` catalog entries are the only authority for widget capabilities. `OSKLayout` can change presentation and connect a visible control to compatible target Widgets, but it cannot add or override a hardware capability.

## Action Value Properties

Action metadata declares whether a binding accepts `Range`, `Delta`, `AccelerationDeltas`, `StepValues`, `TicksPerStep`, or `StateColors`. Unknown or unsupported properties are errors.

Report an error when:

- `Range` does not contain exactly two finite numbers or its minimum is not less than its maximum.
- `Delta` is not positive.
- `AccelerationDeltas` is empty or contains a non-positive or non-finite value.
- `StepValues` is empty, contains a non-finite value, or repeats a value.
- A `StepValues` entry is outside the explicit or action-defined effective range.
- `StepValues` is combined with `Delta` or `AccelerationDeltas`.
- `TicksPerStep` is used without `StepValues`, is empty, or contains a non-positive integer.
- A color is not exact `#RRGGBB` or `#RRGGBBAA` syntax.
- `StateColors=[Track]` also contains an explicit color.
- The action or widget feedback does not support the requested state colors.
- `RingStyle` is used on a widget without ring feedback or names a style that its ring processor does not support.

Without `TicksPerStep`, each input tick advances one discrete step. If an acceleration level is higher than the available `AccelerationDeltas` or `TicksPerStep` entries, reuse the final entry. Report a warning for decreasing `AccelerationDeltas` because faster input then produces a smaller change.

Ring feedback resolves in this order: explicit binding `RingStyle`, action-catalog `FeedbackShape` mapped by the selected processor, then processor default. Bindings cannot set `FeedbackShape`. Actions without a reliable shape, including `FXParam` and `TrackPanAutoRight`, use the processor default unless `RingStyle` is explicit.

## Gesture Reachability Rules

Apply these rules to bindings with the same normalized context key.

Report an error when:

- A context-changing `(Press)` action has Hold, LongHold, DoublePress, Tap, or Release actions in the old context.
- A context-changing `(Release)` action prevents a later DoublePress from reaching the old context.
- A context-changing `(Tap)` is combined with DoublePress under `DoublePressPolicy=Additive`.
- `(Hold)` changes context while the same context also has `(LongHold)`.
- A context-changing Hold shares a DoublePress context and its effective `DelayMs` is not greater than `DoublePressWindowMs`.
- A repeated Hold or LongHold action changes context.
- One event group has more than one context-changing action.

Allow:

- Context-changing `(Tap)` with `(Hold)` because Hold suppresses Tap.
- Context-changing `(Tap)` with `(DoublePress)` under Exclusive policy because DoublePress suppresses Tap.
- Context-changing `(Hold)` with `(DoublePress)` when their timing is valid and no earlier event changes context.

Report a warning when:

- `(Press)` is combined with a non-context-changing Hold or LongHold because the actions are additive.
- Tap is delayed by Exclusive DoublePress recognition.
- Hold and LongHold both exist because LongHold also runs after Hold.
- `(Release)` runs after a Hold or LongHold action.
- DoublePress is additive with Press or Release.

## Context-Changing Actions

The action catalog must expose a `changesContext` or `invalidatesContext` trait. The editor must not infer this behavior only from action names.

The initial trait set includes:

- `NextPage`
- `GoPage`
- `GoHome`
- `AllSurfacesGoHome`
- `GoZone`
- `EnterZoneLayer`
- `ExitZoneLayer`
- `GoFXSlot`
- `ClearLastTouchedFXParam`
- `ClearFocusedFX`
- `ClearSelectedTrackFX`
- `ClearFXSlot`

A `Reaper` command can also reload a project or plugin. Treat it as opaque unless action metadata identifies its context effect.

## Multi-Action and Feedback Rules

Several actions on one event are allowed as an ordered macro.

Report an error for:

- An exact duplicate with the same action, parameters, and properties.
- `NoAction` together with another action in the same event group.
- More than one context-changing action.
- An immediate action after a context-changing action.
- `RunCount` greater than one on a modifier or context-changing action.
- `RepeatIntervalMs` on Press, Tap, Release, DoublePress, a modifier action, or a context-changing action.

Report a warning for:

- More than one action that changes modifier state.
- A modifier action and a normal action on the same raw event.
- More than one action that provides feedback to the same widget.
- An unqualified relative action together with `(Increase)` or `(Decrease)` actions.
- Feedback properties on DoublePress when runtime does not request DoublePress feedback.

`NoAction` is valid as the only Hold or LongHold action. It can consume that gesture and suppress Tap.

## Timing Validation

Parse timing values as complete integer tokens and validate the resolved effective values.

Report an error when:

- Any timing value is negative or outside its supported range.
- `LongHoldDelayMs` is not greater than `HoldDelayMs`.
- `RepeatIntervalMs` is not positive.
- `RepeatIntervalMs` is used outside Hold or LongHold.
- A binding uses `DelayMs` outside Hold or LongHold.
- A Hybrid modifier lacks a valid `ModifierTapWindowMs`.
- One configured Device resolves contradictory timing settings.

Report a warning when a timing value is below the practical input polling interval or makes two gestures difficult to distinguish.

## Zone and Profile Rules

Resolve each profile before validating bindings and dependencies. Main and FX use separate case-insensitive ID namespaces. In each collection, one User zone overrides only the Vendor zone with the same canonical filename stem; all other valid files from both sources remain active. Duplicate IDs inside one source are errors. An invalid User override keeps that ID invalid and does not silently expose Vendor behavior.

Duplicate and reference diagnostics must include complete relative paths and links to every related declaration. Nested directories do not change zone identity. Partial-file validation defers missing-target diagnostics until the complete resolved profile index is available.

Resolve `LearnFX.fxzon` against the Surface assignment that opened FX edit mode. `FXWidgets` must select at least one input-capable `Parameter` widget. Name and value display entries must resolve text-feedback widgets. Overlapping roles, missing `@CH` family members, unsupported entry defaults, ambiguous display pairing, duplicate active `MatchFX` values, and a conflict between copied generated bindings and a parameter assignment are errors linked to both sources. The saved FX-zone draft is then validated as a normal FX `.zon` file with no hidden Learn FX context.

Report an error for:

- `GoZone` or `EnterZoneLayer` without a target.
- A missing target zone.
- `GoZone` targeting a zone that is available only as a zone layer.
- `EnterZoneLayer` targeting a zone that is not declared in the enclosing zone's `ZoneLayers` section.
- One zone listed in both `IncludedZones` and `ZoneLayers` of the same zone.
- A duplicate dependency entry.
- A structural cycle through `IncludedZones` or `ZoneLayers`. Navigation return paths through `GoZone` are not structural cycles.
- A zone-local pseudo-modifier that can remain active after its owning zone deactivates.

Report a warning for:

- `GoHome` or `ExitZoneLayer` in Home.
- A self-target `GoZone` transition that has no effect.
- A parent binding that shadows an `IncludedZones` binding.
- A zone-layer binding that makes a parent binding unreachable.

Run profile-level checks after applying the same User and Vendor layer override rules as runtime. Do not store zone-local modifier or gesture state permanently on the shared physical `Widget`.

## Cross-Component Contract

C++ is the authority for effective timing values, ranges, gesture semantics, and runtime validation.

- [`settings_schema.conf`](../Scripts/settings_schema.conf) is the canonical metadata source for setting names, types, compiled defaults, ranges, scopes, categories, and cross-setting constraints. It currently defines input behavior and timing settings and can later define other product configuration metadata. It does not store user values.
- The generated Surface schema catalog is the canonical source for Input and Feedback type names, protocols, named properties, MIDI or OSC matching rules, compatible output sharing, and derived widget capabilities.
- Product configuration stores root values and Device overrides.
- The Bun editor reads schema metadata and user-selected values from the product `.conf`.
- Lua reads schema metadata for its settings interface, but it queries and changes effective values through structured C++ ExtState commands. Lua must not read or write the product `.conf` directly or keep an independent timing source of truth.
- A Lua settings change is validated, applied live, and saved atomically by C++. A manual or Bun editor file change becomes active only after explicit Apply or Reload, or after CSI restarts.
- C++ provides `Query`, `Apply`, and `Reload` through the generated `ReaCtrlSurf_SETTINGS_CMD` and `ReaCtrlSurf_SETTINGS` sections. Query reports each effective value, its `Compiled`, `Product`, or `Device` source, and the value that would be inherited after removing the current override.
- C++ validates product configuration, Zone loading, and OSK live apply before changing runtime state.
- The Bun editor validates the same grammar, settings, catalog-derived surface capabilities, action traits, and cross-file relationships.
- The legacy converter translates old unwrapped expressions and timing actions into the explicit grammar and persistent settings.
- OSK serialization, labels, tooltips, and editing preserve bracketed selectors, parenthesized events, `@CH` widget-family qualifiers, and effective overrides.
- One generated metadata schema must keep C++, Lua, and TypeScript setting names, ranges, enums, and action traits aligned.

## Test Contract

Extract a deterministic C++ button gesture recognizer with an injected clock. Cover:

- Press and raw Release.
- Tap with and without Hold.
- Hold and LongHold milestones.
- Additive and Exclusive DoublePress.
- Hold and DoublePress on the same widget.
- Hold repeat start, interval, release, and cancellation.
- Lost release recovery.
- Momentary, Latch, and Hybrid modifier state.
- Zone deactivation during every gesture phase.
- Product global, Device, declaration mode, and binding timing overrides.
- Multiple actions and context-changing action ordering.

Add Bun tests for parsing, normalization, validation, quick fixes, conversion, Vendor compatibility, and complete configuration sets. Add Lua serialization and settings-command checks. Keep focused manual REAPER verification for real MIDI and OSC timing.

## Acceptance Criteria

- Selectors, input events, transforms, exact physical widget names, and `@CH` widget-family qualifiers are visually and grammatically distinct.
- Physical widget names never create modifiers implicitly.
- Diagnostics use normalized identity and do not depend on selector order, line order, or zone load order.
- Momentary, Latch, and Hybrid source-widget Hold rules are enforced.
- Tap, Hold, LongHold, and DoublePress reachability is deterministic before actions run.
- Product global, Device, declaration mode, and binding timing overrides resolve to one C++-owned effective value set.
- Every conflicting diagnostic identifies all related lines and paths when possible.
- Lua, C++, the Bun editor, and the converter use the same names, values, and validation rules.
- Existing Vendor zones are audited and converted before the new grammar becomes required.

## Short Implementation Plan

- ✅ Define the shared Product and Device behavior and timing metadata source, defaults, ranges, scopes, cross-setting constraints, C++ generation input, Lua loader, and TypeScript loader.
- [ ] Define the explicit binding grammar, normalized binding types, declaration and binding overrides, modifier modes, action traits, and remaining generated cross-component metadata.
- [ ] Implement the deterministic C++ gesture recognizer, scoped modifier state, configuration precedence, runtime validation, and persistent settings.
  - ✅ Implement the current Product and Surface setting behavior. Phase 3 moves the assignment-level scope to Device while preserving atomic scope validation, effective precedence, runtime timing application, atomic persistence, and reload without replacing valid runtime state on error.
- [ ] Update Zone parsing, OSK live apply, ExtState settings commands, Lua settings UI, labels, tooltips, and serialization.
  - ✅ Implement structured settings Query, Apply, and Reload commands plus the schema-driven OSK settings window. Phase 3 changes its editable override scope from Surface to Device.
- [ ] Update the Bun editor, complete-set validator, quick fixes, snippets, legacy converter, and Vendor configuration migration.
  - ✅ Validate the current Product settings and Surface overrides in the Bun parser and CLI from the shared schema. Phase 4 changes this validation to Product and Device scopes.
- [ ] Add C++, Bun, and Lua tests for parsing, timing, state transitions, overrides, conversion, and invalid combinations.
  - ✅ Add focused C++ value-resolution tests, Bun Product and Surface setting tests, and Lua settings protocol self-checks. Update their scope fixtures to Product and Device with the format 2 product `.conf`.
- [ ] Audit all Vendor surfaces and zones, then perform focused manual MIDI, OSC, motor-fader, touch, and multi-surface verification.
