# Zone and Surface Format 2 Plan

## Goal

Replace the legacy zone and surface syntax and name-based runtime behavior with one explicit, validated model. Use the same lexical rules for `.zon`, surface `.txt`, `.fxzon`, and `.snippet` files. Keep legacy CSI parsing only in the Bun importer. The current runtime does not need backward compatibility after all bundled and user test data is converted.

## Current problems

- A normal file repeats its identity in `Zone <name>` and closes a single file object with `ZoneEnd`.
- Runtime parsing ignores `ZoneEnd`, while the editor treats it as required.
- File names and zone IDs can differ without a clear reason or safe rename workflow.
- Zone IDs are case-insensitive, but duplicate diagnostics have not always shown both declarations or respected profile boundaries.
- `GoZone` is navigation and may form a valid return path, but it has been checked like a recursive structural dependency.
- `Home`, `LastTouchedFXParam`, `FXRowLayout`, `FXWidgetLayout`, `FXPrologue`, and `FXEpilogue` have hidden runtime roles.
- `Track`, `SelectedTrack`, `SelectedTracks`, `MasterTrack`, `TrackSend`, `TrackReceive`, `TrackFXMenu`, `SelectedTrackSend`, `SelectedTrackReceive`, `SelectedTrackFXMenu`, and `MasterTrackFXMenu` can select navigation or bank behavior through their names.
- `GoZones` is deprecated but still has a special loader path.
- Learn FX templates pretend to be zones and use `#WidgetType`, `#DisplayRow`, `#RingStyle`, `#DisplayFont`, and `#SupportsColor` directives outside the `Zone` block.
- `FXRowLayout` manually lists modifier combinations and widget-name suffixes because the native Learn FX window cannot use the real surface layout.
- `IncludedZones` has no precise activation, priority, fallback, or cycle contract.
- The `|` character in `Fader|` is a current channel suffix placeholder, but it looks like a wildcard or pipe operator.
- Square brackets contain reset values, stepped values, ranges, acceleration data, and MIDI encoder direction ranges in different contexts without named fields.
- Anonymous action color blocks such as `{ 20 20 0 255 255 0 }` do not identify their purpose or separate their colors clearly.
- Custom blocks use `BlockName` and `BlockNameEnd` instead of normal matched braces.
- The current product configuration file keeps the old CSI line-record grammar and obsolete `Version=7.0` marker under a new name and path, and the native configuration dialog parses the same file again after the shared parser.
- C++ preprocesses zone metadata, bindings, Learn FX templates, and OSK edits through separate line readers.

## Current source references

- [zone_manager.cpp](../src/controls/zone_manager.cpp) preprocesses zone headers, selects Home, excludes Learn FX pseudo-zones, and applies name-based navigator behavior.
- [zone_parser.cpp](../src/controls/zone_parser.cpp) parses bindings and structural sections while ignoring `Zone` and `ZoneEnd` tokens.
- [learn_dialog.cpp](../src/ui/learn_dialog.cpp) reads Learn FX pseudo-zones and hash-prefixed directives separately.
- [action_context.cpp](../src/actions/action_context.cpp) interprets anonymous square-bracket action values and passes `RingStyle` properties to widget feedback.
- [integrator_config_parser.cpp](../src/controls/integrator_config_parser.cpp) requires the product version on physical line 1.
- [config_dialog.cpp](../src/ui/config_dialog.cpp) reparses the product configuration file after the shared parser has already parsed it.
- [osk.cpp](../src/controls/osk.cpp) locates and edits zone blocks through its own line scanning.
- [zone.ts](../tools/config-editor/src/zone.ts) contains the current Bun format 1 parser.
- [validation.ts](../tools/config-editor/src/validation.ts) validates active profile layers, duplicate IDs, references, and structural cycles.

## ✅ Confirmed format direction

### Common syntax

The new custom DSL files use these common lexical rules:

- `@Meta` is the first significant element in versioned zone, surface, Learn FX, and snippet documents and contains file metadata.
- `{` and `}` delimit structural blocks.
- `[` and `]` delimit a value list only after a named `Property=` or identify a defined selector or index in its grammar position.
- `//` starts a comment.
- `#` is not a comment marker and can be used in hexadecimal colors.
- Values containing spaces use double quotes.
- One file contains one logical document and ends at EOF.
- Anonymous square-bracket groups are not valid in format 2.
- The new product configuration file uses the same brace lexer but has its own unversioned document model.
- `product_identity.conf` and `settings_schema.conf` keep their existing formats unless a separate plan changes them.

A short metadata block can stay on one line:

```text
@Meta { Version=2 Role=Home }

Play Play
[Control]+ButtonB1 GoZone SelectedTrack
```

The formatter can expand a long metadata block:

```text
@Meta {
  Version=2
  Role=Home
}
```

Structural blocks use matched braces:

```text
ZoneLayers {
  Paning
  LinkLock
}
```

The same brace rule applies to the new surface format:

```text
Widget Fader {
  Input Fader14Bit { Status=0xE0 }
  Input Touch { On=[0x90, 0x68, 0x7F] Off=[0x90, 0x68, 0x00] }
  Feedback Fader14Bit { Status=0xE0 }
}

OSKLayout {
  Row {
    Widget Fader Shape=Fader Height=7.5
    Widget Solo Color=#FF9900
  }
}
```

Action state colors use a named list instead of an anonymous RGB block:

```text
Touch TrackAutoMode 2 StateColors=[#141400, #FFFF00] // Touch
```

The first color represents state `0`, the second color represents state `1`, and later colors represent later indexed states. `StateColors=[Track]` uses the current track color.

### Zone identity and FX matching

The Main zone ID is always the `.zon` filename stem. A rename operation in the editor must rename the file and update every typed reference in one transaction.

FX zones use the same `.zon` format and declare external plugin matching in metadata because a plugin identifier is not a safe file ID:

```text
@Meta { Version=2 MatchFX="VST3: ReaEQ (Cockos)" Alias="ReaEQ" }

Fader FXParam 0
```

Independent Main zones declare a user-facing `Target` when their actions need a track or FX context. The runtime derives its navigator, track set, lifetime, activation scope, and link routing. Zone layers inherit the parent context. FX zones get their target context when the runtime activates them.

Normal zones omit `Role`. A file name and the surface channel count must not silently select behavior.

### Multi-channel widget references

Legacy `Fader|` is not a wildcard. Current C++ creates one zone object per surface channel and replaces `|` with that object's numeric suffix. Format 2 does not expose or preserve this implementation model.

One format 2 zone document is loaded once. `@CH` after a widget family expands that binding for every surface channel and supplies the current surface-channel index to its widget selector, target resolution, bank target, validation, and action context. A binding without `@CH` creates one channel-neutral context.

Do not replace legacy `Fader|` with `Fader*`. A true `Fader*` wildcard matches real widget names and can also match an unintended widget family. For example, `Rotary*` can match both `Rotary1` and `RotaryPush1`.

The recommended explicit replacement is a channel placeholder:

```text
@Meta { Version=2 Target=Tracks }

Fader@CH TrackVolume
Rotary@CH TrackPan
RotaryPush@CH TrackVolume StepValues=[0.716]
ButtonA@CH TrackSolo
```

`Target=Tracks` resolves a separate track for each channel-qualified binding.

A repeated binding can use the same selected track while each channel selects a send from the current bank:

```text
@Meta { Version=2 Target=SelectedTrack BankTarget=Sends }

Fader@CH TrackSendVolume
```

On an eight-channel surface, this initially maps the faders to sends 1 through 8 of the selected track. After one eight-channel bank step, they map to sends 9 through 16. The zero-based target indexes remain an internal runtime detail.

`@CH` is not a glob and does not create several zone objects. Actual wildcard matching remains available only in schema fields that explicitly accept a widget pattern.

The token classes remain visually distinct:

- `Fader@CH` is a postfix surface-channel expansion on a widget family.
- `[Shift]+Fader1` is a prefix context selector defined by the binding grammar.
- `StepValues=[0.0, 0.5, 1.0]` is a named value list after `=`.
- A bare action suffix such as `[ 0.5 ]` is invalid in format 2.

### Named action values and MIDI encoder ranges

Legacy zone action brackets mix several unrelated values:

```text
RotaryPush| TrackPan [ 0.5 ]
RotaryC| TrackVolume [ (0.0002,0.001,0.005,0.01,0.05) ]
RotaryPushP1 FXParam 21 [ 0 1 ]
```

Format 2 converts them to named properties:

```text
RotaryPush@CH TrackPan StepValues=[0.5]
RotaryC@CH TrackVolume AccelerationDeltas=[0.0002, 0.001, 0.005, 0.01, 0.05]
RotaryPushP1 FXParam 21 StepValues=[0, 1]
```

A continuous generated FX parameter can declare a range, a normal delta, and acceleration deltas:

```text
Rotary1 FXParam 0 Range=[0.0, 1.0] Delta=0.025 AccelerationDeltas=[0.025, 0.05, 0.1]
```

A discrete parameter uses ordered values and can require a different number of input ticks at each acceleration level:

```text
Rotary1 FXParam 0 Range=[0.0, 1.0] StepValues=[0.0, 0.5, 1.0] TicksPerStep=[4, 2, 1]
```

Legacy MIDI encoder direction blocks such as `[ > 01-3f < 41-7f ]` belong to the Surface input protocol, not to action values. The Surface contract below converts the exact standard range to `Encoding=SignedBit` only when no WidgetClass supplied the current runtime behavior. When a WidgetClass exists, the importer converts its `AccelerationValues` to an EncoderProfile and removes the ignored inline range with a notice.

The importer must decode each legacy value type. It must not copy one old anonymous bracket group into one new generic list.

### ✅ Learn FX through OSK

The native Learn FX window is replaced by an OSK FX edit mode. OSK already knows the real surface layout and real widget names, so format 2 removes `FXRowLayout` instead of converting its modifier and suffix matrix.

The runtime supplies all available modifiers and their valid combinations. The Learn FX profile does not enumerate them.

One dedicated `LearnFX.fxzon` file belongs to each zone profile. It contains the editable FX widget whitelist and bindings added to generated FX zones:

```text
@Meta { Version=2 }

FXWidgets {
  Parameter Fader@CH
  Parameter RotaryBig@CH RingStyle=Dot
  Parameter RotaryBigPush@CH
  NameDisplay DisplayUpper@CH
  ValueDisplay DisplayLower@CH
}

GeneratedBindings {
  On ZoneActivation {
    ToggleUseLocalModifiers
  }

  On ZoneDeactivation {
    ToggleUseLocalModifiers
    HideFXSlot
  }

  Plugin ClearFXSlot
}
```

`@CH` in `FXWidgets` selects the exact numbered family for the configured surface channels. A true trailing `*` remains available when the author intentionally wants a broader widget-name pattern. Ring styles and other feedback capabilities come from the matched widget feedback processor instead of a repeated profile list.

`FXWidgets` is required exactly once and must resolve at least one `Parameter` widget on the selected surface. `NameDisplay` and `ValueDisplay` entries are optional. One resolved widget can have only one role. Parameter widgets require numeric, relative, absolute, or two-state input. Display roles require text feedback. A capability mismatch or overlap links the selector and the real Surface widget.

An entry can contain default binding properties supported by its resolved widget and generated action. For example, `RingStyle=Dot` on a `Parameter` entry becomes the initial property on a generated `FXParam` binding. Display font, margin, and color defaults can be declared on display entries when their feedback processors support those properties. Unsupported defaults are errors. The user can change or remove a copied default in the FX-zone draft.

`GeneratedBindings` is optional and appears at most once. It accepts normal bindings plus the explicit lifecycle blocks. These entries are copied into each new FX-zone draft; they are not a hidden runtime include and do not change an existing saved FX zone later. The block uses normal action, widget, property, and capability validation.

FX edit mode works as follows:

1. The user focuses an FX and opens OSK FX edit mode from one configured surface assignment. That assignment supplies the surface, zone profile, channel count, and resolved `LearnFX.fxzon`.
2. The runtime finds an active FX zone with the same exact `MatchFX`. It opens the User file, offers a User override for a Vendor file, or creates a new unsaved User draft when no match exists.
3. Widgets selected by `FXWidgets.Parameter` entries are assignment targets. Modifier buttons remain usable as mode controls but are not FX assignment targets. Widgets outside the whitelist are disabled and shown with a muted style.
4. A physical or OSK parameter-widget click opens an FX-specific editor. It shows a searchable dropdown of the focused plugin's parameters, selects the current mapping, and offers only value and feedback properties supported by that action and widget.
5. The user can remove a parameter mapping. A valid parameter can be assigned to more than one physical widget, but one normalized widget and modifier context cannot map two different parameters unless normal multi-action rules allow that exact group.
6. Name and value displays default to a same-channel display family when one unambiguous `@CH` pairing exists. The user can select a different eligible display or no display. The draft writes normal `FXParamNameDisplay` and `FXParamValueDisplay` bindings with the same FX parameter index.
7. Every accepted edit updates one in-memory draft and its live preview. It does not write the file. Closing or cancelling restores the pre-edit runtime mapping unless the user saved.
8. Save validates the complete draft, target ID, `MatchFX`, User/Vendor collision, and surface capabilities, then writes one normal User `.zon` FX file atomically. The saved file contains no Learn FX directives or hidden template reference.

A new draft starts with normal metadata and copied defaults:

```text
@Meta { Version=2 MatchFX="VST3: ReaEQ (Cockos)" Alias="ReaEQ" }

On ZoneActivation {
  ToggleUseLocalModifiers
}

Fader1 FXParam 0
DisplayUpper1 FXParamNameDisplay 0
DisplayLower1 FXParamValueDisplay 0
```

The suggested filename is a valid stable ID derived from the plugin identity, but the user can change it before first save. An existing case-insensitive FX ID collision is an error unless the operation is the explicit User override of that same Vendor ID. A second active FX zone with another ID and the same `MatchFX` is also an error.

During legacy import:

- `#WidgetType` entries become `FXWidgets.Parameter` selectors.
- `#DisplayRow` entries become name or value display selectors according to their old binding role.
- A `RingStyle` attached to a legacy `#WidgetType` becomes the matching `Parameter` entry default when it is supported.
- Standalone `#RingStyle` choice lists and `#SupportsColor` are checked against generated Surface capabilities and then dropped; format 2 does not repeat capability lists.
- `#DisplayFont` becomes a supported display-entry default when its old target can be identified. An ambiguous target requires user resolution.
- `FXPrologue` and `FXEpilogue` active bindings are merged into `GeneratedBindings` while preserving their relative order.
- `FXRowLayout` is not imported because OSK uses real widgets and runtime modifier combinations.
- Conflicts between generated parameter bindings and `GeneratedBindings` are reported in the draft instead of preserving hidden before/after ordering. The user can keep one binding, move it to another widget context, or keep an ordered multi-action group when normal action metadata permits it.

Learn FX does not reference functional snippets. The small profile-specific generated bindings remain directly visible in `LearnFX.fxzon`.

### ✅ Ring style behavior

Legacy `#RingStyle` entries only populate the native Learn FX ring-style dropdown. They do not set runtime feedback and do not infer a style from an action.

Legacy `#WidgetType Rotary RingStyle=Dot` provides the initial `RingStyle` property when the native Learn FX window assigns a parameter to that widget type. The generated FX binding then stores the selected property directly:

```text
Rotary1 FXParam 0 RingStyle=Dot
```

At runtime, the MIDI feedback processor reads `RingStyle` from that binding and converts the normalized action value to the device-specific LED ring mode. Current actions do not select a ring style themselves. Existing normal zones therefore declare styles explicitly, commonly `Fill` for volume, `Dot` or `BoostCut` for pan and balance, and `Spread` for width-like feedback.

Format 2 keeps these responsibilities separate:

- One global `RingStyle` enum defines `Dot`, `Fill`, `BoostCut`, and `Spread` once.
- Each ring feedback processor reports the subset that it supports and its default style. A widget without ring feedback reports no styles.
- OSK and the Bun editor get the available choices from the selected widget instead of `LearnFX.fxzon`.
- `RingStyle=...` on a zone binding remains an explicit override.
- Each known action can declare a semantic `FeedbackShape` trait in generated action metadata. This is explicit shared metadata, not an action-name comparison in the parser.

The complete format 2 semantic shape enum is:

| Feedback shape | Preferred ring result |
|---|---|
| `Level` | `Fill` |
| `Centered` | `BoostCut`, with `Dot` as a supported fallback |
| `Spread` | `Spread`, with the processor-defined fallback |
| `Position` | `Dot` |

Omitting `FeedbackShape` is valid and means that the widget processor default is used. `FeedbackShape` is generated action metadata, not a public binding property or a value inferred from the action name.

The initial processor mappings are complete for the current ring processors:

| Processor | Supported styles | Default | `Level` | `Centered` | `Spread` | `Position` |
|---|---|---|---|---|---|---|
| Generic encoder | `Dot`, `BoostCut`, `Fill`, `Spread` | `Dot` | `Fill` | `BoostCut` | `Spread` | `Dot` |
| SCE24 encoder | `Dot`, `BoostCut`, `Fill`, `Spread` | `Dot` | `Fill` | `BoostCut` | `Spread` | `Dot` |
| Asparion encoder | `Dot`, `Fill` | `Dot` | `Fill` | `Dot` | `Dot` | `Dot` |

A new ring processor must declare its supported styles, default, and mapping for every `FeedbackShape` before it can expose automatic ring feedback. A fallback such as Asparion `Centered -> Dot` is normal processor metadata, not a warning or parser special case.

The initial reliable action assignments are:

- `Level`: `TrackVolume`, `TrackSendVolume`, `TrackReceiveVolume`, normalized output-meter families, normalized volume-with-meter families, and `FXGainReductionMeter`.
- `Centered`: `TrackPan`, `TrackPanL`, `TrackPanR`, `TrackPanAutoLeft`, `TrackSendPan`, and `TrackReceivePan`.
- `Spread`: `TrackPanWidth`.
- `Position`: only actions whose normalized value represents one position rather than level or bipolar amount; each assignment must be explicit in the generated action catalog.

`TrackPanAutoRight` has no automatic shape because it controls width in a normal pan mode and right pan in dual-pan mode. `FXParam` also has no automatic shape because plugin parameters expose no reliable common meaning. Both use the processor default unless the binding declares an explicit `RingStyle`.

Ring style resolution uses this precedence:

1. Explicit `RingStyle=...` on the binding.
2. The action `FeedbackShape` mapped through the widget processor capabilities.
3. The widget processor default.

An explicit `RingStyle` must be supported by the selected widget's ring processor. It is an error on a widget without ring feedback or on a processor that does not support that style. A binding cannot set `FeedbackShape`; `RingStyle` is the public override.

Normal zones therefore do not repeat standard ring styles:

```text
Rotary@CH TrackVolume
[Shift]+Rotary@CH TrackPan
[Option]+Rotary@CH TrackPanWidth
```

An explicit override remains available:

```text
[Shift]+Rotary@CH TrackPan RingStyle=Dot
```

OSK uses the widget processor default for `FXParam`, lets the user select a supported style, and stores a non-default choice as an explicit FX-zone binding property.

### Simultaneous zones, zone layers, and snippets

`IncludedZones` keeps its existing name but gets an explicit contract:

```text
@Meta { Version=2 Role=Home }

IncludedZones {
  Track
  MasterTrack
}
```

`IncludedZones` means that each referenced independent zone remains active at the same time as the current zone. Each referenced zone keeps its own explicit `Target` and channel-qualified bindings. For example, `Home` can own global transport bindings while a `Target=Tracks` zone expands only its `@CH` bindings for the surface channels.

A functional snippet is different. The editor inserts snippet bindings into the current zone draft, and those bindings use the current zone context. A snippet does not create or activate an independent runtime zone.

Legacy `IncludedZones` sections are converted directly to brace-based `IncludedZones`. The importer does not try to classify or flatten them into snippets.

Legacy subzones become reusable zone layers:

```text
ZoneLayers {
  Paning
  LinkLock
}
```

Zone layers use `Role=Layer`, inherit the active parent's derived target, channel, and bank context, and can be declared by several parents. An active zone layer temporarily has priority over its current parent. `EnterZoneLayer` activates a declared layer, and `ExitZoneLayer` returns to the current parent.

### ✅ New product configuration format

The current `ReaControlSurface.ini` grammar is a renamed continuation of `CSI.ini`. Format 2 replaces it instead of carrying its `Version=7.0`, order-dependent page records, and `Broadcaster` state into the new product.

The canonical product identity changes `PRODUCT_CONFIG_FILENAME` to a `.conf` filename because the new document is not INI. The new file lives at the existing product configuration root. Old `CSI.ini` files remain read-only sources for the Bun importer.

The new product configuration has no embedded `Version` property. The product-owned path and the current parser identify its contract. The app is not released, so runtime backward compatibility with the current development file is not required.

The configuration uses the shared brace lexer and a product-specific typed model:

```text
Settings {
  DefaultModifierMode=Latch
  HoldDelayMs=1000
}

Device fp2 {
  Type=MIDI
  Channels=1
  Input=0
  Output=0
  RefreshRate=15
  MaxMessagesPerRun=200

  Settings {
    DefaultModifierMode=Latch
    HoldDelayMs=750
  }
}

Page Home {
  FollowMCP=false
  SyncPages=false
  ScrollLink=false
  ScrollSync=false

  Surface fp2 {
    Device=fp2
    Template=faderportv2
    MainProfile=faderportv2
    FXProfile=faderportv2
    StartChannel=0
  }

  Link {
    From=fp2
    To=xtouch
    Share=[Home, Modifiers, FXMenu]
  }
}
```

`Device` defines one MIDI or OSC endpoint. `Page.Surface` assigns that device to a surface template and zone profiles. Device `Settings` override root product settings for every use of that device. `Page.Surface.Settings` is not valid. `Link` replaces the order-dependent `Broadcaster` and following `Listener` records with one complete relationship.

The root accepts zero or one `Settings` block, one or more `Device` blocks, and one or more `Page` blocks. Block order is not semantic and references can point forward. `@Meta`, `Version`, legacy line records, and `#` comments are invalid. Unknown blocks, properties, enum values, and duplicate singleton properties are errors.

Public boolean values use the exact lowercase literals `true` and `false`.

Settings resolve in this order:

```text
compiled default < root Settings < Device.Settings
```

`settings_schema.conf` declares whether each setting supports `Product`, `Device`, or both scopes. Existing behavior and timing settings that currently use the `Surface` scope move to `Device`; product-only logging settings remain product-only. A setting is valid only in a matching block. If a manually edited root Settings block is invalid, runtime reports it and uses compiled defaults for that block. If one Device Settings block is invalid, that device inherits the resolved root settings. Other valid configuration blocks still load.

`Device` IDs use the lowercase stable-ID grammar and are unique in the file. Common properties are:

| Property | Required | Value and default |
|---|---|---|
| `Type` | Yes | `MIDI` or `OSC` |
| `Channels` | Yes | Positive integer surface-channel count |

A MIDI device accepts:

| Property | Required | Value and default |
|---|---|---|
| `Input` | Yes | Non-negative REAPER MIDI input index |
| `Output` | Yes | Non-negative REAPER MIDI output index |
| `RefreshRate` | No | Positive integer; default `15` |
| `MaxMessagesPerRun` | No | Positive integer; default `200` |

An OSC device accepts:

```text
Device x32 {
  Type=OSC
  Protocol=X32
  Channels=32
  ReceivePort=8000
  TransmitPort=9000
  Address="192.168.1.20"
  MaxPacketsPerRun=200
}
```

| Property | Required | Value and default |
|---|---|---|
| `Protocol` | No | `Generic` or `X32`; default `Generic` |
| `ReceivePort` | Yes | Integer from `1` through `65535` |
| `TransmitPort` | Yes | Integer from `1` through `65535` |
| `Address` | Yes | Non-empty quoted host name or IP address |
| `MaxPacketsPerRun` | No | Positive integer; default `200` |

MIDI-only properties on OSC and OSC-only properties on MIDI are errors. A valid configured port that is not currently available is a runtime warning, not a syntax error; only that device and its dependent Surface instances are skipped.

`Page` uses a required quoted display name. Page names are non-empty and unique case-insensitively. Page properties are optional booleans with these defaults:

| Property | Default |
|---|---|
| `FollowMCP` | `true` |
| `SyncPages` | `true` |
| `ScrollLink` | `false` |
| `ScrollSync` | `false` |

Each Page contains at least one `Surface` block and zero or more `Link` blocks. A Surface block ID uses the lowercase stable-ID grammar and is unique inside that Page. It accepts:

| Property | Required | Value and default |
|---|---|---|
| `Device` | Yes | Existing Device ID |
| `Template` | Yes | Existing Surface template stable ID |
| `MainProfile` | No | Zone profile stable ID; defaults to `Template` |
| `FXProfile` | No | Zone profile stable ID; defaults to `MainProfile` |
| `StartChannel` | No | Non-negative integer; default `0` |

The same Device can be assigned on more than one Page. `StartChannel` is the zero-based starting channel of this Surface inside the Page. It does not change the surface-local numbering used by `@CH`. A missing template or required Main profile skips only that Surface instance. A missing or empty FX profile is valid.

A Link block contains exactly `From`, `To`, and `Share`. `From` and `To` reference distinct Surface IDs in the same Page. `Share` is a non-empty list from the closed enum `Home`, `Modifiers`, `FXMenu`, `SelectedTrackFX`, `SelectedTrackSends`, and `SelectedTrackReceives`. One pair uses one Link block. Duplicate pairs, duplicate categories, more than one incoming source for the same target and category, and a directed cycle for any shared category are errors linked to all involved blocks.

Product configuration validation is local where the block boundaries remain known. An invalid Device skips that Device and dependent Surface instances. An invalid Surface or Link skips only that block. An invalid Page-level property skips that Page. A structural lexer error that prevents reliable block recovery stops the document. The parser collects every independent diagnostic before applying the valid model.

All C++ consumers use one parsed `IntegratorConfig` model. The native configuration dialog must not open and interpret the same file a second time. The Bun editor and C++ must use the same field names, nesting, defaults, and validation rules.

### ✅ Surface structure and encoder input

A Surface template describes hardware or an OSC endpoint layout. It does not select REAPER ports, set a surface-channel count, or contain user behavior settings. Those values belong to the product `Device` and Page `Surface` assignment.

The selected Device `Type` must equal the Surface metadata `Protocol`. A mismatch skips that Page Surface assignment and links both declarations in the diagnostic.

A Surface document accepts these order-independent top-level blocks after `@Meta`:

- zero or more `EncoderProfile` blocks;
- zero or one `ColorCalibration` block;
- one or more `Widget` blocks;
- zero or one `OSKLayout` block when `Protocol=MIDI`.

Block IDs and references are case-sensitive. Duplicate Widget or EncoderProfile IDs are errors linked to every declaration. Unknown blocks and properties are errors. The parser builds one typed Surface model; hardware input, feedback capabilities, validation, Learn FX, and OSK all consume that model.

The normal MIDI form is:

```text
@Meta { Version=2 Protocol=MIDI Name="FaderPort V2" }

EncoderProfile Rotary {
  Delta=0.003
  Increase=[0x01, 0x02, 0x03]
  Decrease=[0x41, 0x42, 0x43]
  AccelerationDeltas=[0.005, 0.01, 0.02]
}

Widget RotaryBig {
  Input Encoder {
    Message=[0xB0, 0x10]
    Profile=Rotary
  }
}

Widget RotaryBigPush {
  Input Press {
    On=[0x90, 0x20, 0x7F]
    Off=[0x90, 0x20, 0x00]
  }
}

Widget Solo {
  Input Press {
    On=[0x90, 0x08, 0x7F]
    Off=[0x90, 0x08, 0x00]
  }
  Feedback TwoState {
    On=[0x90, 0x08, 0x7F]
    Off=[0x90, 0x08, 0x00]
  }
}
```

`Widget` contains zero or one quoted `Alias` property and one or more `Input` or `Feedback` blocks. It can contain several typed blocks, such as fader input, touch input, motor feedback, and color feedback. `Input Type` and `Feedback Type` use separate closed namespaces, so the same short type can exist in both. The public Feedback type drops the redundant legacy `FB_` prefix.

Every registered Input and Feedback type has one shared schema entry that defines its protocol, allowed named properties, required properties, message matching rule, runtime decoder or processor, and derived capabilities. C++, Bun, Lua metadata, Learn FX, and OSK must use this catalog. They must not infer capabilities by searching type-name text. Adding a processor without adding its schema entry is a build-generation error.

The initial Input catalog replaces the current public runtime names as follows:

| Protocol | Public Input type | Named properties | Derived input capability | Legacy input |
|---|---|---|---|---|
| MIDI | `Press` | required three-byte `On`; optional three-byte `Off` | press; release only when Off exists | `Press` |
| MIDI | `AnyPress` | required two-byte `Message` prefix | press without release | `AnyPress` |
| MIDI | `Fader14Bit` | required `Status` byte | absolute | `Fader14Bit` |
| MIDI | `FaderportClassicFader14Bit` | required three-byte `MSBMessage` and `LSBMessage` | absolute | same name |
| MIDI | `Fader7Bit` | required two-byte `Message` prefix | absolute | `Fader7Bit` |
| MIDI | `Encoder` | required two-byte `Message` prefix and exactly one Profile or Encoding | relative | `Encoder`, `MFTEncoder`, `EncoderPlain`, `Encoder7Bit` |
| MIDI | `Touch` | required three-byte `On` and `Off` | touch | `Touch` |
| OSC | `Control` | required `Address` | numeric value | `Control` |
| OSC | `AnyPress` | required `Address` | press without release | `AnyPress` |
| OSC | `Touch` | required `Address` | touch | `Touch` |
| OSC | `X32Fader` | required `Address` | absolute | `X32Fader` |
| OSC | `X32RotaryToEncoder` | required `Address` | relative | `X32RotaryToEncoder` |

`EncoderPlain` converts to `Encoding=SignedBitFixed`; `Encoder7Bit` converts to `Encoding=Relative7Bit`; and `MFTEncoder` converts to a generated local EncoderProfile. These legacy names do not remain in the runtime catalog.

The initial Feedback catalog groups processors by their public property shape. Types in one row share the listed syntax but keep their own processor behavior and capability entry:

| Property shape | Public Feedback types |
|---|---|
| `On` and `Off` MIDI messages | `TwoState` |
| one `Message` | `NovationLaunchpadMiniRGB7Bit`, `MFT_RGB`, `AsparionRGB`, `FaderportRGB`, `FaderportTwoStateRGB`, `Fader7Bit`, `Encoder`, `AsparionEncoder`, `ConsoleOneVUMeter`, `ConsoleOneGainReductionMeter`, `SCE24LEDButton`, `SCE24Encoder` |
| `Status` | `Fader14Bit` |
| `MSBMessage` and `LSBMessage` | `FaderportClassicFader14Bit` |
| `Channel` | `FaderportValueBar`, `FPVUMeter`, `QConProXMasterVUMeter`, `MCUVUMeter`, `MCUXTVUMeter`, `AsparionVUMeterL`, `AsparionVUMeterR`, `MCUDisplayUpper`, `MCUDisplayLower`, `MCUXTDisplayUpper`, `MCUXTDisplayLower`, `IconDisplay1Upper`, `IconDisplay1Lower`, `IconDisplay2Upper`, `IconDisplay2Lower`, `AsparionDisplayUpper`, `AsparionDisplayLower`, `AsparionDisplayEncoder`, `XTouchDisplayUpper`, `XTouchDisplayLower`, `XTouchXTDisplayUpper`, `XTouchXTDisplayLower`, `FP8ScribbleLine1`, `FP8ScribbleLine2`, `FP8ScribbleLine3`, `FP8ScribbleLine4`, `FP16ScribbleLine1`, `FP16ScribbleLine2`, `FP16ScribbleLine3`, `FP16ScribbleLine4`, `FP8ScribbleStripMode`, `FP16ScribbleStripMode`, `QConLiteDisplayUpper`, `QConLiteDisplayUpperMid`, `QConLiteDisplayLowerMid`, `QConLiteDisplayLower` |
| `Row` and `Channel` | `C4DisplayUpper`, `C4DisplayLower` |
| `Message`, `TopMargin`, `BottomMargin`, and `Font` | `SCE24OLEDButton`, `SCE24EncoderText` |
| no properties | `MCUTimeDisplay`, `MCUAssignmentDisplay` |
| `Address` | OSC `Value`, `Integer`, `X32`, `X32Integer`, `X32Fader`, `X32RotaryToEncoder` |

These names come from the current registered C++ processors, not from every public example file. A legacy Surface line whose type is misspelled, stale, or not registered produces an unknown-type diagnostic with close catalog suggestions. The importer does not preserve a non-working type only because it exists in a public example.

MIDI bytes use `0x00` through `0xFF`. MIDI data bytes must also be at most `0x7F`; status-byte positions require a valid status value. `Message`, `On`, and `Off` are typed lists with exact lengths defined by the selected Input or Feedback type. A diagnostic points to the exact byte or missing property instead of reporting only an invalid Widget block.

Incoming MIDI match keys must be unique after each Input type applies its declared status, data-byte, and value matching mask. Two Inputs that can consume the same physical message are an error linked to both blocks. Several Feedback blocks can send to the same address only when their catalog entries declare that combination compatible.

#### Encoder profiles

`EncoderProfile` replaces `StepSize`, `AccelerationValues`, and `WidgetClass`. It is a reusable local lookup table, not a widget capability or runtime class name.

| Property | Required | Value and rule |
|---|---|---|
| `Increase` | Yes | Non-empty list of unique MIDI data bytes |
| `Decrease` | Yes | Non-empty list of unique MIDI data bytes |
| `Delta` | No | Positive finite default delta for a binding without its own `Delta` |
| `AccelerationDeltas` | No | Non-empty list of positive finite defaults for a binding without its own `AccelerationDeltas` |

Increase and Decrease values cannot overlap. Their list position is the zero-based acceleration level. The lists can have different lengths. Runtime clamps a level beyond the resolved `AccelerationDeltas` list to its last value, matching the format 2 action-value rule. A binding property overrides the corresponding EncoderProfile default. A profile does not define action range or discrete `StepValues`.

`Input Encoder` requires a two-byte MIDI `Message` prefix and exactly one of:

- `Profile=ProfileId`, which maps the incoming third byte through that EncoderProfile;
- `Encoding=SignedBit`, which uses bit 6 as direction and bits 0 through 5 as magnitude;
- `Encoding=SignedBitFixed`, which uses bit 6 as direction and emits one fixed tick;
- `Encoding=Relative7Bit`, which compares consecutive values and emits one directional tick.

Unknown third-byte values in a profile do not produce input. Duplicate profile values and an unknown Profile reference are errors. The old special `MFTEncoder` table becomes a normal EncoderProfile during import.

Legacy inline encoder text such as `[ > 01-3f < 41-7f ]` is not copied. With a legacy WidgetClass, the importer uses the class `AccelerationValues` table and reports that the ignored inline text was removed. Without a WidgetClass, the exact standard range converts to `Encoding=SignedBit`. Any other inline range that does not describe current runtime behavior remains unresolved and produces a preview diagnostic instead of a guessed mapping.

#### OSC widgets

OSC uses the same Widget, Input, and Feedback structure with OSC-specific catalog types and a named `Address` property:

```text
@Meta { Version=2 Protocol=OSC Name="X32" }

Widget ChannelFader1 {
  Input X32Fader { Address="/ch/01/mix/fader" }
  Feedback X32Fader { Address="/ch/01/mix/fader" }
}
```

An OSC address is a non-empty quoted string that starts with `/`. A slash never starts a comment. Only `//` starts a comment. Duplicate incoming OSC address and decoder combinations are errors. Feedback address reuse follows the same catalog compatibility rule as MIDI feedback.

OSC Surface templates cannot contain `OSKLayout`. This preserves the current product decision that tablet OSC interfaces are not mirrored in the desktop OSK.

#### OSK layout

`OSKLayout` is part of the MIDI Surface document and does not have a separate version:

```text
OSKLayout {
  Row {
    Spacer Width=0.25
    Widget RotaryBig Shape=Round Width=1.5 Height=1.5 PressTarget=RotaryBigPush
    Spacer Width=0.25
  }
}
```

Each Row contains one or more `Widget` or `Spacer` entries. A layout Widget references one declared hardware Widget. A visible Widget can occur only once. A referenced press, scroll, value, or touch target does not need its own visible cell.

The initial layout properties remain `Shape`, `Width`, `Height`, `Top`, `Group`, `Label`, `Color`, `Role`, `PressTarget`, `ScrollTarget`, `ValueTarget`, `TouchTarget`, and `RotaryStyle`. Colors use `#RRGGBB` or `#RRGGBBAA`. Positive Width and Height default to `1`; Top defaults to `0`; Spacer Width is positive and defaults to `0.5`. Input and Feedback capability strings are derived from the typed Widget blocks and are not editable OSK layout properties.

Target properties must reference a Widget with the matching derived capability. For example, `PressTarget` requires press input and `ScrollTarget` requires relative input. `Role`, when omitted, is derived from capabilities. Explicit Role can change presentation but cannot add a missing hardware capability.

`ColorCalibration` keeps device color conversion separate from the visual OSK layout:

```text
ColorCalibration {
  OutputMax=127
  NeutralTolerancePercent=5
  NeutralRedScale=0.80
  NeutralCurve=2.0
}
```

The block is active when present. Its exact properties, defaults, and ranges must be stored in the same Surface schema catalog as color-capable Feedback types. It is invalid when no Widget uses color feedback.

## Remaining design decisions

- ✅ Expose only `Role`, `Target`, and optional `BankTarget` in Main zone metadata. Derive navigator, track set, lifetime, activation scope, link routing, and internal FX context in the typed runtime model.
- ✅ Load each zone document once, use `@CH` to expand only channel-qualified bindings, and reject bare anonymous square-bracket groups in format 2.
- ✅ Confirm the zone action property names `Range`, `Delta`, `StepValues`, `AccelerationDeltas`, and `TicksPerStep`, and resolve legacy MIDI encoder direction ranges in the typed Surface contract.
- ✅ Define `@CH` surface-channel expansion plus wildcard case handling, escaping, match ordering, overlap handling, and no-match diagnostics.
- ✅ Store `LearnFX.fxzon` in the zone profile root beside `Main/` and `FX/` because a zone profile can be selected independently from a surface file.
- ✅ Confirm the complete global `FeedbackShape` set, current ring-processor mappings, and actions that intentionally have no automatic shape.

## ✅ Phase 1: Runtime behavior inventory

The replacement column below names the required semantic field. Phase 2 confirms its exact spelling and grammar. It must not collapse every special string into `Role`.

### Zone construction and lifecycle branches

| Current name or construct | Current hidden behavior | Required explicit replacement | Source |
|---|---|---|---|
| `Home` | Required single zone, uses the selected-track navigator, activates after initialization, and remains the fallback after other zones | `Role=Home`; runtime derives the selected-track context, startup, fallback, and required-profile behavior | [ZoneManager::Initialize()](../src/controls/zone_manager.cpp) |
| `LastTouchedFXParam` zone | Optional single high-priority mapping loaded during initialization with the focused-FX navigator | `Role=LastTouchedFXParam`; runtime derives the focused-FX context and high-priority behavior | [ZoneManager::Initialize()](../src/controls/zone_manager.cpp), [zone_parser.cpp](../src/controls/zone_parser.cpp) |
| Legacy `NavType` | Preprocessing accepts short values and stores one in `ZoneInfo.navigator`, but automatic loading adds only the zone ID to `zoneList`, so the stored value is discarded. Zone construction then compares stale `TrackNavigator`, `MasterTrackNavigator`, and `FocusedFXNavigator` strings from the deprecated `GoZones` second token | Parse public `Target` once, derive one typed internal navigator, and pass it to zone construction. Keep `FixedTrackNavigator` internal | [NavigatorType](../src/shared/types.h), [ZoneManager::PreProcessZoneFile(), Initialize(), LoadZones(), and GetNavigatorsForZone()](../src/controls/zone_manager.cpp) |
| Lua and native zone creators | Publish stale long navigator names plus unsupported `VCANavigator` and `FolderNavigator` values | Generate only public format 2 `Role`, `Target`, and `BankTarget` values from one metadata contract | [osk_zone_create.lua](../Scripts/osk_zone_create.lua), [zone_file_creator.cpp](../src/controls/zone_file_creator.cpp) |
| `MasterTrack` | Selects the master-track navigator and one instance | `Target=MasterTrack`; runtime derives the master-track navigator | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp) |
| `Track` | Creates one track-navigator instance per surface channel and selects the normal track bank | `Target=Tracks`; each `@CH` binding resolves the matching channel track | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Page::AdjustBank()](../src/controls/page.h) |
| `VCA`, `Folder`, `SelectedTracks` | Create track-navigator instances, select a special track-list mode while active, and select the matching bank | `Target=VCA`, `Folder`, or `SelectedTracks`; runtime derives the track navigator, Page scope, track set, and bank | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::Activate()](../src/controls/zone.cpp), [Page::AdjustBank()](../src/controls/page.h) |
| `TrackSend`, `TrackReceive`, `TrackFXMenu` | Create track-navigator instances and derive each zone slot from a separate bank offset | `Target=Tracks` plus `BankTarget=Sends`, `Receives`, or `FX`; `@CH` resolves tracks and runtime derives Page routing | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::GetSlotIndex()](../src/controls/zone.cpp), [ZoneManager::AdjustBank()](../src/controls/zone_manager.h) |
| `SelectedTrack` | Creates one selected-track instance per surface channel, uses the instance slot, selects the selected-track bank, and deactivates on track deselection | `Target=SelectedTrack`; runtime derives navigator, banking, and lifetime | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::GetSlotIndex()](../src/controls/zone.cpp), [ZoneManager::OnTrackDeselection()](../src/controls/zone_manager.h) |
| `SelectedTrackSend`, `SelectedTrackReceive`, `SelectedTrackFXMenu` | Create selected-track instances, add a dedicated bank offset to the instance slot, and deactivate on track deselection | `Target=SelectedTrack` plus `BankTarget=Sends`, `Receives`, or `FX`; runtime derives navigator, lifetime, and link category | [zone_manager.cpp](../src/controls/zone_manager.cpp), [zone.cpp](../src/controls/zone.cpp), [zone_manager.h](../src/controls/zone_manager.h) |
| `MasterTrackFXMenu` | Creates one master-track instance per surface channel and derives each slot from the master FX-menu offset | `Target=MasterTrack BankTarget=FX`; runtime derives the master-track navigator and Page routing | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::GetSlotIndex()](../src/controls/zone.cpp) |
| `SelectedTrackFX` target | Normal `GoZone` activation also creates and activates matching FX zones for every FX on the selected track | Remove the pseudo-zone and convert `GoZone SelectedTrackFX` to the typed `ToggleSelectedTrackFX` action | [ZoneManager::GoZone()](../src/controls/page.h), [ZoneManager::GoSelectedTrackFX()](../src/controls/zone_manager.cpp) |
| FX plugin title used as the zone map key | Focused, selected-track, and slot FX loading finds a zone by the current REAPER plugin title | Build a separate `MatchFX` index; the zone ID remains the filename stem | [zone_manager.cpp](../src/controls/zone_manager.cpp) |

### Navigation, propagation, and internal command branches

| Current name or construct | Current hidden behavior | Required explicit replacement | Source |
|---|---|---|---|
| `GoZone Folder`, `VCA`, `TrackSend`, `TrackReceive`, `TrackFXMenu`, or `MasterTrackFXMenu` | Routes activation through the Page and all its surfaces; other targets use the local surface or configured listeners | Target zone declares page or surface navigation scope; `GoZone` resolves the target and does not compare its ID | [GoZone::Do()](../src/actions/manager_actions.h) |
| `SelectedTrackSend`, `SelectedTrackReceive`, `SelectedTrackFX`, `SelectedTrackFXMenu` listener targets | Select one listener category by target name | Target zone declares its listener event category | [ZoneManager::ListenToGoZone()](../src/controls/zone_manager.h) |
| `TrackFXMenu`, `SelectedTrackFXMenu` | Reactivate the active FX-menu zone after an FX-slot mapping closes | Explicit FX-menu context or return target | [ZoneManager::ReactivateFXMenuZone()](../src/controls/zone_manager.h) |
| `LastTouchedFXParam`, `FocusedFX`, `SelectedTrackFX`, `FXSlot` passed by clear actions | Select one hard-coded clear operation | Keep separate typed clear actions or a typed FX context enum; these strings are commands, not zone roles | [ZoneManager::DeclareClearFXZone()](../src/controls/zone_manager.h), [manager_actions.h](../src/actions/manager_actions.h) |
| `GoZones` | Uses a deprecated file as the top-level zone list | Remove the runtime path; the importer reads it and emits normal format 2 references | [ZoneManager::Initialize()](../src/controls/zone_manager.cpp) |
| `IncludedZones`, `SubZones` | Load simultaneously active zones or declared subzones | `IncludedZones` remains composition; `SubZones` becomes the reusable `ZoneLayers` relation | [zone_parser.cpp](../src/controls/zone_parser.cpp), [zone.cpp](../src/controls/zone.cpp) |
| `GoZone`, `GoSubZone` action names | Mark their string parameter as a reference when the parser recognizes the action name | Action metadata declares typed independent-zone and zone-layer references; dependency extraction does not compare action text | [zone_parser.cpp](../src/controls/zone_parser.cpp) |
| Lifecycle pseudo-widget names | Run bindings during surface, page, transport, track-selection, or zone events | Explicit `On EventName { ... }` action blocks, not physical widget names | [widget.h](../src/controls/widget.h), [control_surface.h](../src/controls/control_surface.h), [zone.cpp](../src/controls/zone.cpp) |

### Learn FX name branches

| Current name or construct | Current hidden behavior | Required explicit replacement | Source |
|---|---|---|---|
| `FXRowLayout` | Lists modifier and row combinations for the native Learn FX dialog | Do not import it; OSK uses real widgets and runtime modifier combinations | [learn_dialog.cpp](../src/ui/learn_dialog.cpp) |
| `FXWidgetLayout` | Supplies eligible widgets, display rows, ring choices, fonts, and color flags | Convert eligible widget and display selectors to `LearnFX.fxzon`; derive capabilities from matched widgets and feedback processors | [learn_dialog.cpp](../src/ui/learn_dialog.cpp) |
| `FXPrologue`, `FXEpilogue` | Inject bindings before and after generated FX parameter bindings | Merge active bindings into ordered `GeneratedBindings`; diagnose conflicts instead of keeping hidden ordering | [learn_dialog.cpp](../src/ui/learn_dialog.cpp) |
| `#WidgetType`, `#DisplayRow`, `#RingStyle`, `#DisplayFont`, `#SupportsColor` | Parsed only by the native Learn FX dialog and skipped by the normal zone parser | Legacy importer input only; format 2 emits selectors and typed capabilities without hash directives | [learn_dialog.cpp](../src/ui/learn_dialog.cpp), [zone_parser.cpp](../src/controls/zone_parser.cpp) |
| Standalone `FocusedFXNavigator` and other navigator-name lines inside a zone body | Parsed as an invalid or unknown binding and do not select the zone navigator | Legacy importer removes exact known standalone navigator lines; format 2 reports any remaining unknown line | [zone_parser.cpp](../src/controls/zone_parser.cpp) |
| Surface name `SCE24` in Learn FX | Enables device-specific display behavior by exact surface name | Widget and feedback processor capability metadata | [learn_dialog.cpp](../src/ui/learn_dialog.cpp) |

### Reference diagnostics and active layers

| Condition | Format 2 result |
|---|---|
| Assigned Main profile does not exist | Error on the surface assignment; skip only that surface |
| No zone has `Role=Home`, or more than one active Main zone has it | Profile error; do not initialize that surface profile |
| Duplicate zone ID inside one Vendor or User Main/FX source | Error that links both files |
| Exact case-insensitive User Main or FX ID matches a Vendor ID in the same profile and collection | Valid per-zone override, not a duplicate |
| `IncludedZones`, `ZoneLayers`, `GoZone`, or `EnterZoneLayer` target is missing from the complete active profile | Error on the reference |
| A partial editor validation set does not contain the target | Defer the diagnostic and resolve it from the profile index; do not report it as missing |
| Main zone is valid but no structural or navigation reference reaches it | Warning for an orphan zone |
| Vendor or User Main/FX directory is absent or empty | Valid empty source; it does not hide the other source |
| `MatchFX` names a plugin that is not installed | Valid external matcher; runtime simply has no matching active FX |

The current layer model is inconsistent: [ProductPaths::FindMainZones()](../src/shared/product_paths.cpp) selects the complete User Main directory when it exists, while [ZoneManager::PreProcessZones()](../src/controls/zone_manager.cpp) reads Vendor and User FX directories together. Format 2 replaces both behaviors with the same per-zone overlay contract defined in Phase 2.

### Product config readers and writers

| Consumer | Current path | Format 2 ownership |
|---|---|---|
| Runtime initialization | `ParseIntegratorConfig()` produces `IntegratorConfig`; [config_parser.cpp](../src/controls/config_parser.cpp) applies it | One C++ parser and typed model |
| Native Settings and Devices protocols | Read source, call `ParseIntegratorConfigSource()`, then use `WriteSettingsConfigAtomically()` | Edit and validate the same typed document model through one transaction writer |
| Native configuration dialog | Calls `ParseIntegratorConfig()` again and writes directly with `fopenUTF8()` | Consume the existing model and use the same transaction writer; remove its duplicate semantic path |
| Bun editor | Uses `parseProductConfig()` and the editor store | Independent TypeScript implementation of the same normative grammar, diagnostics, and fixtures |

The relevant implementations are [integrator_config_parser.cpp](../src/controls/integrator_config_parser.cpp), [settings_protocol.cpp](../src/controls/settings_protocol.cpp), [devices_protocol.cpp](../src/controls/devices_protocol.cpp), [config_dialog.cpp](../src/ui/config_dialog.cpp), and [product-config.ts](../tools/config-editor/src/product-config.ts).

### Ring feedback and action meaning

`PropertyType_RingStyle` exists, but there is no global typed `RingStyle` enum. Current actions do not choose a style. Each processor reads an optional string property:

| Processor | Accepted styles | Current default |
|---|---|---|
| Generic encoder | `Dot`, `BoostCut`, `Fill`, `Spread` | `Dot` |
| SCE24 encoder | `Dot`, `BoostCut`, `Fill`, `Spread` | `Dot` |
| Asparion encoder | `Dot`, `Fill` | `Dot` |

Sources: [fb_generic.h](../src/controls/midi/fb_generic.h), [fb_sce24.h](../src/controls/midi/fb_sce24.h), and [fb_asparion.h](../src/controls/midi/fb_asparion.h).

The initial reliable semantic action groups are:

| Proposed `FeedbackShape` | Normalized action families |
|---|---|
| `Level` | `TrackVolume`, `TrackSendVolume`, `TrackReceiveVolume`, `TrackOutputMeter*`, `TrackVolumeWithMeter*`, `FXGainReductionMeter` |
| `Centered` | `TrackPan`, `TrackPanL`, `TrackPanR`, `TrackPanAutoLeft`, `TrackSendPan`, `TrackReceivePan` |
| `Spread` | `TrackPanWidth` |

`FXParam` has no reliable global meaning. `TrackPanAutoRight` changes between width and right-pan meaning at runtime. Raw dB, percentage, text, toggle, and color actions are not normalized ring-shape candidates. Phase 2 must define dynamic or absent shape behavior instead of guessing from action names.

✅ Every current exact zone-name branch has a source-linked semantic replacement. Zone IDs, action names, parser directives, listener events, and internal FX commands remain separate concepts.

Ready when every current special case has a source-linked explicit replacement and no behavior depends on an undocumented name. ✅

## [ ] Phase 2: Format 2 specification

The confirmed format direction above is input to this phase, not the complete grammar. Main zone identity remains the `.zon` filename stem. `@Meta` carries format and behavior metadata. `MatchFX` identifies an external plugin match, not the zone itself. Format 2 uses `@CH`; legacy `Widget|` is accepted only by the importer and converted during migration.

### ✅ Common lexical grammar

The following rules are normative for format 2 `.zon`, surface `.txt`, `.fxzon`, and `.snippet` files:

- A document is UTF-8 text. UTF-8 BOM is accepted and ignored. `LF` and `CRLF` line endings have the same meaning.
- Spaces and tabs separate tokens. A newline ends a binding or other line declaration, except inside a brace block whose schema accepts several `Name=Value` entries on one line, such as `@Meta`.
- `//` starts a comment outside a quoted string and discards text through the end of that physical line.
- A single `/` is normal data. OSC addresses such as `/ch/01/mix/fader` are not comments.
- `#` is normal data only where the document schema expects a hexadecimal color such as `#FF9900`. It is not a comment marker or directive prefix.
- `{` opens a named block and `}` closes it. Every opened block must close before EOF. Legacy `BlockNameEnd` tokens are invalid.
- `[` and `]` contain a comma-separated list after `=` or a selector/index in a grammar position that defines one. A bare anonymous value list after an action is invalid.
- `(` and `)` contain an input event, direction selector, or value transform as defined by the binding grammar. They are not generic grouping characters.
- `@Meta` is the document metadata marker. `@CH` is the exact case-sensitive postfix surface-channel qualifier in a widget reference. No other unquoted `@` qualifier is defined.
- `=` separates a property name from its value. `+` joins explicit binding selectors. `,` separates list items.
- A bare token continues until whitespace, a structural delimiter, or the start of `//`. It can contain protocol characters such as `/`, `:`, `-`, `.`, `*`, and hexadecimal byte text.
- An identifier starts with an ASCII letter or `_` and then uses ASCII letters, digits, `_`, or `-`. Document schemas can apply stricter stable-ID and case rules.
- An integer uses decimal digits with an optional leading `-`. A decimal uses a dot and must include digits on both sides, such as `0.5`. Scientific notation is not accepted.
- A quoted string starts and ends with `"`. It supports `\\`, `\"`, `\n`, `\r`, and `\t`. A raw newline or unknown escape inside a string is an error.
- Boolean and enum spelling is schema-specific. Parsers must not silently convert an unknown word to `false`, zero, or an empty value.
- A list uses `[Value1, Value2]`. Empty lists and a trailing comma are invalid unless a specific property explicitly permits them.
- Semicolons are not statement terminators. EOF ends the document but never repairs an open string, list, selector, parenthesis, or block.

The shared lexer recognizes structure but does not assign one universal meaning to every bare token. The document schema determines whether a token is a widget, action, protocol byte, enum, reference, or property value.

The common document shape is:

```text
document        = spacing, metadata, spacing, { statement, spacing }, EOF ;
metadata        = "@Meta", spacing, "{", spacing, metadata-entry, { spacing, metadata-entry }, spacing, "}" ;
metadata-entry  = identifier, spacing, "=", spacing, value ;
block           = identifier, [ spacing, value ], spacing, "{", spacing, { statement, spacing }, "}" ;
property        = identifier, spacing, "=", spacing, value ;
list            = "[", spacing, value, { spacing, ",", spacing, value }, spacing, "]" ;
value           = identifier | integer | decimal | quoted-string | bare-token | list ;
widget-reference = identifier, [ "@CH" ] ;
widget-pattern  = identifier, "*" ;
comment         = "//", { character except line-end }, line-end | EOF ;
```

This EBNF describes token boundaries only. Each document schema defines its valid statements and values. A parser must keep source file, line, and column locations for all tokens and diagnostics.

`@Meta` rules are:

- It is required in every format 2 `.zon`, surface `.txt`, `.fxzon`, and `.snippet` document.
- It is the first significant element after an optional BOM, whitespace, and comments.
- `Version=2` is required exactly once.
- Duplicate metadata keys are errors, including duplicates that differ only by case.
- Unknown metadata keys are errors. A newer document therefore cannot be misread with missing behavior by an older runtime.
- Each metadata entry must fit on one physical line. The expanded `@Meta { ... }` block can place different entries on different lines. Quoted strings cannot span lines.
- The product `.conf` file is not a versioned format 2 document and does not use `@Meta`.

Valid lexical examples:

```text
// A comment can precede metadata.
@Meta { Version=2 Target=Tracks }

[Shift]+Rotary@CH TrackPan StepValues=[0.0, 0.5, 1.0]
```

```text
@Meta { Version=2 }

Widget Encoder1 {
  OSC /ch/01/mix/fader
  Color #FF9900
}
```

Invalid lexical examples:

```text
@Meta { Version=2
Play Play
```

The metadata block is not closed.

```text
@Meta { Version=2 }
Rotary1 TrackPan [ 0.5 ]
```

The list has no named property and is invalid in format 2.

```text
/ old single-slash comment
@Meta { Version=2 }
```

The first line is data before `@Meta`, not a comment, so the document is invalid. The legacy importer converts recognized old single-slash comment lines to `//`.

### ✅ Surface-channel expansion and widget patterns

`@CH` is a structural widget-family qualifier. It always means surface channel and never means track, selected track, send, receive, or FX:

```text
@Meta { Version=2 Target=Tracks }

Fader@CH TrackVolume
[Shift]+Rotary@CH TrackPan
```

The `@CH` contract is:

- `@CH` follows one non-empty widget-family identifier without whitespace and is valid only in a schema position that accepts a widget reference.
- The spelling is exactly `@CH`. Bare `@`, `@Channel`, `@Track`, different casing, multiple qualifiers, and text after the qualifier are errors.
- For a surface with channel count `N`, `Fader@CH` resolves in numeric channel order to the exact real widget names `Fader1` through `FaderN`.
- Every expected real widget must exist on the selected surface. A missing family member is an error that names each missing widget. A family that resolves no widgets uses the same error code with the complete expected set.
- One zone document and one source binding remain in the parsed model. Expansion creates channel-specific runtime action contexts; it does not clone the zone or its channel-neutral bindings.
- `Target`, `BankTarget`, and action metadata decide how the supplied surface-channel index resolves a track, send, receive, FX, or other action target. The qualifier itself does not name that logical target.
- A widget reference without `@CH` names one exact widget and creates a channel-neutral binding unless the concrete widget itself supplies channel state.

A true wildcard is a different feature. It is accepted only by schema fields declared as `WidgetPattern`. The initial format 2 `WidgetPattern` fields are the selectors inside `FXWidgets`; normal binding left sides do not accept wildcards.

Wildcard rules are intentionally small:

- A pattern is a non-empty widget identifier prefix followed by one terminal `*`, such as `DisplayLower*`.
- `*` matches zero or more remaining characters in one real widget name. Bare `*`, embedded `*`, multiple `*`, `?`, character ranges, and escape syntax are not supported.
- Widget identifiers cannot contain a literal `*`, so no wildcard escaping is needed.
- Matching is case-sensitive. A case-only near match is an error with a quick-fix suggestion that uses the declared widget spelling.
- Matching uses the real widgets of the selected resolved surface after Vendor/User selection and keeps their surface declaration order.
- A pattern that matches no widgets is an error on the pattern. The diagnostic names the surface and offers nearby widget-family names when available.
- If two entries in the same block select the same real widget, validation reports both entries as a duplicate selection instead of relying on source order.
- `@CH` and `*` cannot appear in the same widget reference. Use `@CH` for a complete numbered channel family and `*` only for an intentional broader name match.

For example, `Rotary@CH` resolves only `Rotary1` through `RotaryN`, while `Rotary*` can also match `RotaryPush1`. The editor offers a safe `Pattern -> @CH` quick fix only when the pattern result is exactly one complete numbered family for the selected surface.

### ✅ Action value and state-color properties

Numeric action behavior uses only named properties. Action metadata declares which properties each action accepts.

| Property | Value | Rules |
|---|---|---|
| `Range` | Two-number list `[Minimum, Maximum]` | Both values are finite, `Minimum` is less than `Maximum`, and action output is clamped to this range |
| `Delta` | Positive number | Amount applied by one relative input event when no acceleration level is supplied |
| `AccelerationDeltas` | Non-empty list of positive numbers | Entry zero is the slowest acceleration level; a higher input level uses the last available entry when the list is shorter |
| `StepValues` | Non-empty list of finite numbers | Defines an ordered discrete sequence; source order is behavior and is preserved |
| `TicksPerStep` | Non-empty list of positive integers | Valid only with `StepValues`; entry zero is the slowest acceleration level; omitted higher levels reuse the final entry |
| `StateColors` | Non-empty list of colors or the single value `[Track]` | Entry zero is state zero, entry one is state one, and later entries are later indexed states |

`Range` is optional. Without it, action metadata supplies the action's valid range. Every `StepValues` entry must be inside the effective range. Continuous changes produced by `Delta` or `AccelerationDeltas` are clamped to that range.

`StepValues` selects discrete mode and cannot be combined with `Delta` or `AccelerationDeltas`. Duplicate adjacent or non-adjacent step values are errors because they create distinct positions with the same output. Without `TicksPerStep`, every input tick advances one step. `TicksPerStep` values map by acceleration level, not by `StepValues` position.

`Delta` and `AccelerationDeltas` can be used together. `Delta` handles relative input without an acceleration level; `AccelerationDeltas` handles indexed acceleration input. A decreasing acceleration-delta list is valid but produces a warning because faster input then changes the value by a smaller amount.

Colors use exact `#RRGGBB` or `#RRGGBBAA` syntax. Six-digit colors get alpha `FF`. `[Track]` cannot be combined with explicit colors. The property is valid only when action and widget feedback metadata support indexed or track color. Unsupported colors, too few colors for a fixed known state count, and extra unreachable colors are diagnostics.

Examples:

```text
Rotary FXParam 0 Range=[0.0, 1.0] Delta=0.005 AccelerationDeltas=[0.005, 0.02, 0.1]
RotaryPush TrackPan StepValues=[0.5]
Rotary TrackAutoMode StepValues=[0, 1, 2, 3, 4] TicksPerStep=[4, 2, 1]
Touch TrackAutoMode 2 StateColors=[#141400, #FFFF00]
```

Legacy import identifies the old anonymous values by their parsed type, not only their punctuation. Decimal parenthesis lists become `AccelerationDeltas`, one decimal parenthesis value becomes `Delta`, integer parenthesis lists become `TicksPerStep`, `Minimum>Maximum` becomes `Range`, remaining numbers become `StepValues`, and anonymous RGB or `Track` blocks become `StateColors`. If one old group is ambiguous or contains a combination rejected by format 2, preview reports it and leaves that binding unresolved instead of guessing.

### ✅ Simple document metadata and derived runtime context

Document identity comes from its profile-relative filename. Metadata does not repeat a stable ID.

| Document | Allowed `@Meta` keys |
|---|---|
| Main `.zon` | `Version`, `Role`, `Target`, `BankTarget`, `Alias` |
| FX `.zon` | `Version`, `MatchFX`, `Alias` |
| Surface `.txt` | `Version`, `Protocol`, `Name`, `Description` |
| `LearnFX.fxzon` | `Version` |
| `.snippet` | `Version`, `Name`, `Description` |

`Version=2` is required for every document in this table. `Protocol=MIDI|OSC` is required in a Surface document. `Name`, `Description`, `MatchFX`, and `Alias` are quoted strings. All other values are closed enums. An omitted optional key uses the documented default and does not produce a warning.

Surface and snippet IDs are their lowercase filename stems. A Main zone ID is its filename stem and is compared case-insensitively inside the active Main profile. An FX zone ID is its filename stem, while `MatchFX` is its external plugin lookup key. `LearnFX.fxzon` has a fixed filename and no separate ID.

Main zone metadata uses only these user-facing properties:

| Property | Values | Default and rules |
|---|---|---|
| `Role` | `Home`, `LastTouchedFXParam`, `Layer` | Omitted for normal independent zones. Exactly one active Main zone uses `Home`; zero or one uses `LastTouchedFXParam`; `Layer` marks a reusable zone layer declared by at least one parent's `ZoneLayers` block |
| `Target` | `Tracks`, `SelectedTrack`, `MasterTrack`, `FocusedFX`, `VCA`, `Folder`, `SelectedTracks` | Omitted for global-only zones and zone layers. It states what the zone controls |
| `BankTarget` | `Sends`, `Receives`, `FX` | Omitted for normal track banking. It states which child collection is shown and moved by bank actions |
| `Alias` | Quoted display text | Optional display name. It never changes the document ID or `MatchFX` key |

`Role=Layer` cannot use `Target` or `BankTarget`; it inherits them from the active parent. The same zone-layer file can be declared by several parents and gets a separate runtime context for each active parent. It cannot be an `IncludedZones`, `GoZone`, or Home target.

`Role` and `Target` are mutually exclusive. `Role=Home` derives selected-track context plus startup and fallback behavior. `Role=LastTouchedFXParam` derives focused-FX context plus its special high-priority mapping behavior. `Role=Layer` derives its target context from the current parent. File names never supply a role.

`BankTarget=Sends` or `Receives` is valid with `Target=Tracks` or `SelectedTrack`. `BankTarget=FX` is also valid with `Target=MasterTrack`. Other combinations are errors. FX `.zon` files and zone layers do not declare these fields because they inherit their activation context.

The public values derive these internal behaviors:

- `Target=Tracks` uses channel tracks and the normal track bank.
- `Target=VCA`, `Folder`, or `SelectedTracks` uses channel tracks from that collection and activates at Page scope because the collection is shared by all surfaces on the Page.
- `Target=SelectedTrack` uses the selected-track navigator and deactivates when that track context is lost.
- `Target=MasterTrack` uses the master-track navigator.
- `Target=FocusedFX` uses the focused-FX navigator.
- `Target=Tracks` with a child `BankTarget` keeps `@CH` mapped to tracks and applies one shared banked send, receive, or FX index.
- `Target=SelectedTrack` or `MasterTrack` with a child `BankTarget` keeps one track target and maps `@CH` to consecutive items in that bank.
- Link routing is derived from `Target` and `BankTarget`, then filtered by the Page link configuration. It is not zone metadata.

The runtime stores navigator, track set, lifetime, activation scope, link category, bank offset, and zero-based REAPER indexes in its typed model. These fields are not part of the file syntax.

Action metadata declares whether an action needs a track, send, receive, or FX. Validation reports a direct error when that requirement does not fit the zone's `Target` and `BankTarget`.

Common recipes are:

| User task | Metadata |
|---|---|
| Home and global controls | `Role=Home` |
| Last-touched FX parameter mapping | `Role=LastTouchedFXParam` |
| Channel tracks | `Target=Tracks` |
| Selected track | `Target=SelectedTrack` |
| Master track | `Target=MasterTrack` |
| Focused FX context | `Target=FocusedFX` |
| Selected-track sends | `Target=SelectedTrack BankTarget=Sends` |
| Sends for channel tracks | `Target=Tracks BankTarget=Sends` |
| Selected-track receives | `Target=SelectedTrack BankTarget=Receives` |
| Selected-track FX | `Target=SelectedTrack BankTarget=FX` |
| Master-track FX | `Target=MasterTrack BankTarget=FX` |
| VCA, folder, or selected-track collection | `Target=VCA`, `Folder`, or `SelectedTracks` |
| Reusable overlay of its current parent | `Role=Layer` |

The Bun editor does not present raw runtime fields. Zone creation first asks what the user wants to control. It shows the optional bank-target choice only for compatible targets and writes the metadata. Ready-made task choices such as Selected-track sends can preselect both values. OSK zone creation uses the current live context and asks for confirmation before writing the same metadata.

### ✅ Profile layers, uniqueness, and safe rename

Main and FX are separate collections inside one zone profile. Each collection resolves its Vendor and User files by case-insensitive zone ID:

1. Load every Vendor zone under the selected profile and collection.
2. Load every User zone under the same profile and collection.
3. A User zone replaces only the Vendor zone with the same canonical ID.
4. Keep every other valid Vendor and User zone active.

The zone ID is the `.zon` filename stem. Nested directories organize files but do not change identity, reference syntax, or override behavior. IDs use the format 2 identifier grammar and compare with ASCII case folding. `Mixer`, `mixer`, and `MIXER` are one canonical ID.

The uniqueness and override rules are:

- Two Vendor files with the same canonical ID in one profile and collection are an error that links both files.
- Two User files with the same canonical ID in one profile and collection are the same error.
- One User file and one Vendor file with the same canonical ID form one valid User override. The editor labels both files and shows which one is active.
- An invalid User override does not silently reactivate the Vendor file. That ID remains unavailable until the User error is fixed or the override is removed.
- Main and FX have separate ID namespaces. The same ID can exist once in each collection.
- Two active FX zones with different IDs but the same exact `MatchFX` key are an error that links both files because plugin lookup would be ambiguous.
- An empty or missing User directory never hides Vendor data. Creating one User override never copies the rest of the Vendor profile.
- `LearnFX.fxzon` is a fixed profile file: User replaces Vendor when both exist. Surface and snippet documents keep their existing whole-file User-by-ID override model because each ID already represents one complete document.
- Profile validation, dependency resolution, orphan checks, Home-role counting, OSK labels, and runtime loading all use the same resolved active set.

Every duplicate diagnostic contains the canonical ID, both complete relative paths, links to both declarations, and the profile plus Main/FX scope. A case-only spelling difference is shown explicitly. A missing dependency is reported only after the complete resolved profile index is available.

Zone rename is one editor transaction and is available only for User zones. Renaming a Vendor zone is not an in-place operation; the editor offers `Create User override` for the same ID or `Copy as new User zone` for a new ID.

A User-zone rename transaction:

1. Validate the new filename stem and destination path in the same profile and Main/FX collection.
2. Reject a case-insensitive collision with another active User or Vendor ID. Replacing a Vendor zone is a separate explicit override operation, not a rename side effect.
3. Load the complete profile index and find every typed reference, including `IncludedZones`, `ZoneLayers`, `GoZone`, `EnterZoneLayer`, and future action parameters declared as zone references.
4. Update writable User referrers. When a Vendor referrer must change, preview the exact file and create one matching User override for that file after confirmation. Never copy the complete Vendor profile.
5. Rename the file and write every changed or newly created User file atomically. A case-only filesystem rename uses an internal temporary path.
6. Move the editor draft, open-file route, diagnostics, and undo state to the new path only after the file transaction succeeds.

The transaction checks current file hashes immediately before writing. A changed source, stale reference index, destination collision, failed User override creation, or validation error aborts the complete transaction and leaves every original file and draft unchanged. Moving a User file between organizational subdirectories without changing its filename stem does not change its ID and does not rewrite references.

### ✅ Bindings and zone relations

A normal binding is one line:

```text
[context selectors]+(input event)+Widget@CH Action positional-parameters NamedProperty=Value
```

Only the parts required by that binding are present. Examples:

```text
Fader@CH TrackVolume
[Shift]+Rotary@CH TrackPan RingStyle=Dot
(Hold)+Play GoZone Mixer
```

The detailed selector, gesture, modifier, timing, and multi-action rules remain in [ZONE_WIDGET_MODIFIER_VALIDATION.md](ZONE_WIDGET_MODIFIER_VALIDATION.md). Format 2 adds these shared line rules:

- The physical widget selector comes before the action.
- Positional action parameters come before named properties.
- Action metadata defines the number and type of positional parameters, named properties, target requirements, feedback traits, and whether a parameter is a zone reference.
- Unknown actions, extra parameters, missing parameters, duplicate properties, and properties that the action does not support are errors.
- Several lines with the same normalized widget event form one ordered action group. The runtime preserves their source order.
- Modifier and pseudo-modifier declarations are declarations, not normal action bindings.

`IncludedZones` lists independent zones that remain active together with the owner:

```text
IncludedZones {
  Channel
  Master
}
```

The owner has input priority. Referenced zones follow in listed order. If an earlier zone consumes a widget event, later zones do not receive it. A duplicate entry is an error. A binding hidden by an earlier zone produces a warning with links to both declarations.

`ZoneLayers` declares reusable overlays that temporarily replace part of their current parent behavior:

```text
ZoneLayers {
  Pan
  LinkLock
}
```

A zone layer:

- inherits the parent target, current channel, bank state, and modifier context;
- has input priority over its parent while active;
- declares `Role=Layer` and can be listed by several parents;
- gets a separate runtime context for each active parent but is parsed from one shared document;
- cannot also be an independent `IncludedZones`, `GoZone`, Home, or FX target;
- can declare its own `ZoneLayers`, but cannot declare `IncludedZones`;
- returns to its current parent when `ExitZoneLayer` runs or when that parent deactivates.

Navigation bindings remain simple:

```text
ButtonA GoZone Mixer
ButtonB EnterZoneLayer Pan
ButtonC ExitZoneLayer
```

- `GoZone ZoneId` activates an independent Main zone. Calling it for the already active target does not toggle that zone off. `GoHome` returns to the Home role.
- `EnterZoneLayer ZoneId` activates one layer listed directly in the current zone's `ZoneLayers` block and deactivates its active sibling.
- `ExitZoneLayer` takes no parameter and returns to the direct runtime parent. It is valid only in `Role=Layer` files.
- `GoZone` cannot target Home, an FX zone, or a zone layer. The editor offers `GoHome` or the matching typed FX action instead.
- Action metadata marks the target parameter as `ZoneRef` or `ZoneLayerRef`. Parsers do not identify references by comparing action-name strings.

Home is the persistent base. One independently navigated zone can be active above it, and an active zone layer can be active above its current parent. Fallback is decided for the exact normalized binding event, not only by widget name. If the upper zone has no binding for that exact event, the event continues to its parent, then through that owner's `IncludedZones` in listed order, and finally to Home. `NoAction` explicitly consumes the exact event and blocks fallback.

The normal priority is active FX mappings, active zone layer, its current parent, then Home. `IncludedZones` are checked at their declared owner level in listed order. The runtime and editor use the same priority model for input, feedback ownership, shadow warnings, and OSK labels.

`IncludedZones` and `ZoneLayers` are structural edges because they create simultaneously loaded or parent-owned runtime structure. Their combined graph must not contain a cycle. `GoZone` and `EnterZoneLayer` are navigation edges and are not part of structural cycle detection. A valid return workflow can therefore contain `GoZone Mixer` in one zone and `GoZone Edit` in another.

Every structural or navigation reference resolves against the complete active Main profile. Partial-file validation defers a missing-reference result until the profile index is available. Diagnostics link the source reference and target declaration.

Zone lifecycle actions do not use fake physical widget names. They use explicit blocks:

```text
On ZoneActivation {
  ToggleUseLocalModifiers
}

On ZoneDeactivation {
  ToggleUseLocalModifiers
}
```

The supported event blocks are `On SurfaceInitialization`, `On TrackSelection`, `On PageEnter`, `On PageExit`, `On PlaybackStart`, `On PlaybackStop`, `On RecordStart`, `On RecordStop`, `On ZoneActivation`, and `On ZoneDeactivation`.

Lifecycle action lines contain an action, its positional parameters, and its named properties. They cannot use widget feedback properties or input-event selectors.

Legacy `SelectedTrackFX` is not converted to a format 2 zone. `GoZone SelectedTrackFX` becomes the typed `ToggleSelectedTrackFX` action. Its action metadata supplies the linked-surface category and prevents a normal zone reference from being created.

Normal FX `.zon` files are activated with one internal typed context: `FocusedFX`, `SelectedTrackChain`, `FXSlot`, or `LearnFX`. These values are runtime state, not `@Meta` values. The same FX file can therefore be reused in every activation context.

Validation reports an error for an unknown key, unknown enum, invalid property combination, missing required target context, invalid role count, or metadata that is not valid for that document type. The diagnostic points to the metadata key or to the binding that requires missing context.

- ✅ Write the normative lexical grammar for `@Meta`, brace blocks, identifiers, selectors, lists, properties, strings, comments, whitespace, and EOF behavior.
- ✅ Define the allowed metadata keys, value types, defaults, combinations, and diagnostics for zones, surfaces, snippets, and `LearnFX.fxzon`.
- ✅ Expose only `Role`, `Target`, and optional `BankTarget`; derive navigator, track set, lifetime, activation scope, link category, banking state, and internal FX context from the completed Phase 1 inventory.
- ✅ Load each zone document once. Expand `@CH` bindings across the surface, keep bindings without `@CH` channel-neutral, let zone layers inherit the activating context, and let the runtime activation context provide navigator and target-index data for FX zones.
- ✅ Define binding, `IncludedZones`, reusable `Role=Layer` overlays, `GoZone`, `GoHome`, `EnterZoneLayer`, `ExitZoneLayer`, exact-event fallback, and lifecycle schemas without treating navigation references as structural recursion.
- ✅ Specify true wildcard matching, case handling, escaping, match order, and no-match diagnostics separately from `@CH`.
- ✅ Specify the valid prefix selector, postfix `@CH`, and named property list positions. Reject anonymous bracket groups.
- ✅ Confirm the names and value rules for `Range`, `Delta`, `StepValues`, `AccelerationDeltas`, `TicksPerStep`, and `StateColors`, and keep raw MIDI encoder decoding in the typed Surface schema.
- ✅ Define case-insensitive per-zone Main/FX overlays, uniqueness scopes, complete duplicate diagnostics, and distinct whole-file Surface, snippet, and Learn FX override rules.
- ✅ Define transactional User-zone rename, collision checks, Vendor-referrer overrides, stale-reference handling, and updates of every typed reference.
- ✅ Define `LearnFX.fxzon`, OSK FX edit mode, widget eligibility, display pairing, generated-binding copy semantics, live draft preview, explicit save, and legacy directive handling.
- ✅ Define the global `RingStyle` type, processor capabilities and defaults, the complete action `FeedbackShape` set, current processor mappings, actions without automatic shape, and explicit binding overrides.
- ✅ Specify the unversioned product `.conf` block schemas, identifiers, required and optional fields, defaults, Product and Device settings scopes, links, diagnostics, and shared semantic model ownership.
- ✅ Specify the Surface document structure, protocol identity, Widget/Input/Feedback blocks, EncoderProfile behavior, MIDI and OSC address rules, OSK layout, color calibration placement, and legacy encoder-range handling.
- ✅ Inventory the currently registered Input and Feedback processors, remove obsolete encoder input types, and group their format 2 public names by named property shape.
- [ ] Define the exact message matching rule, compatible output sharing, and derived capability set for every Feedback catalog entry.
- [ ] Confirm ColorCalibration property defaults, ranges, and compatibility with color-capable Feedback entries.
- [ ] Specify the brace-based snippet schema using the common lexical grammar.
- [ ] Add representative valid and invalid fixture files for every top-level format before runtime implementation starts.
- [ ] Update the Phase 4 conversion matrix and add golden legacy-input/output fixtures for every remaining Phase 2 syntax or action decision before marking that decision complete.

Ready when the normative specification and fixtures let C++, Bun, Lua, and documentation implement the same grammar without interpretation differences.

## [ ] Phase 3: One C++ parser model

- [ ] Implement one lexer for metadata, brace blocks, lists, properties, quotes, comments, wildcard selectors, and `@CH` qualifiers.
- [ ] Parse each zone, surface, Learn FX, and snippet file once into a typed document model with source locations.
- [ ] Generate the C++, TypeScript, and Lua Surface Input/Feedback catalog from one schema and reject runtime processor registrations without matching metadata.
- [ ] Compile each `@CH` binding into channel-specific action contexts without cloning the containing zone document or its channel-neutral bindings.
- [ ] Resolve Vendor and User Main/FX sources into one per-zone active set before roles, references, dependencies, or runtime objects are validated.
- [ ] Parse the new product `.conf` into `IntegratorConfig` and let every C++ consumer use that one semantic model.
- [ ] Change generated setting scope metadata and effective-value resolution from `Surface` to `Device`. Reject settings inside Page Surface assignments.
- [ ] Expose ring feedback capabilities and resolved action feedback shapes through runtime and generated editor metadata.
- [ ] Replace the native Learn FX dialog with OSK FX edit mode, one in-memory live-preview draft, and atomic User FX-zone save through the shared validated model.
- [ ] Validate the complete active profile before runtime objects are created.
- [ ] Let an invalid non-required zone be skipped with focused diagnostics instead of disabling unrelated zones.
- [ ] Treat an invalid or missing Home role as a profile-level initialization error.
- [ ] Replace the metadata preprocessor, binding parser, surface block readers, Learn FX line readers, and OSK line edits with the shared model where they overlap.

Ready when runtime behavior consumes validated documents and no feature reparses the same zone with a different grammar.

## [ ] Phase 4: Bun editor and migration

Migration is a required part of every format 2 decision, not a later best-effort cleanup. A Phase 2 decision is ready for implementation only when this plan also states its legacy input, format 2 output, ambiguity behavior, and fixture requirement. Future syntax or action renames must add or update a row in the conversion matrix below.

The initial conversion matrix is:

| Legacy CSI input | Format 2 output |
|---|---|
| `Version=7.0` | Dropped; the product `.conf` is unversioned |
| Legacy global `Settings` records | One root `Settings { ... }` block |
| Legacy `SurfaceType` record | One `Device id { ... }` block with typed MIDI or OSC properties |
| Legacy Page record and its Surface assignments | One `Page "name" { ... }` block with nested `Surface id { ... }` blocks |
| Legacy `Broadcaster` and following `Listener` records | Explicit same-Page `Link { From=... To=... Share=[...] }` blocks |
| `Zone Name ... ZoneEnd` | One file document whose ID is the filename stem and whose behavior uses `Role`, `Target`, and optional `BankTarget` |
| Magic zone names such as `Home`, `Track`, `SelectedTrackSend`, and `MasterTrackFXMenu` | Explicit metadata according to the Phase 1 behavior inventory |
| `IncludedZones ... IncludedZonesEnd` | `IncludedZones { ... }` |
| `SubZones ... SubZonesEnd` | `ZoneLayers { ... }`; every referenced file gets `Role=Layer`. Several parents can reference the same file |
| Old `GoZone SelectedTrackFX` | `ToggleSelectedTrackFX` |
| Other old `GoZone ZoneId` | `GoZone ZoneId`; format 2 does not preserve an implicit self-target toggle |
| `GoSubZone ZoneId` | `EnterZoneLayer ZoneId` |
| `LeaveSubZone` | `ExitZoneLayer` |
| `OnInitialization` | `On SurfaceInitialization { ... }` |
| `OnTrackSelection` | `On TrackSelection { ... }` |
| `OnPageEnter`, `OnPageLeave` | `On PageEnter { ... }`, `On PageExit { ... }` |
| `OnPlayStart`, `OnPlayStop` | `On PlaybackStart { ... }`, `On PlaybackStop { ... }` |
| `OnRecordStart`, `OnRecordStop` | `On RecordStart { ... }`, `On RecordStop { ... }` |
| `OnZoneActivation`, `OnZoneDeactivation` | `On ZoneActivation { ... }`, `On ZoneDeactivation { ... }` |
| `Widget|` | `Widget@CH` |
| Decimal parenthesis list inside an anonymous action value group | `AccelerationDeltas=[...]` |
| One decimal parenthesis value inside an anonymous action value group | `Delta=...` |
| Integer parenthesis list inside an anonymous action value group | `TicksPerStep=[...]` when the binding also has discrete step values |
| `Minimum>Maximum` inside an anonymous action value group | `Range=[Minimum, Maximum]` with normalized ascending bounds |
| Remaining numeric values inside an anonymous action value group | `StepValues=[...]` in original order |
| Anonymous RGB, hexadecimal, or `Track` color block | `StateColors=[...]` |
| Legacy `StepSize` and `AccelerationValues` entries for one WidgetClass | One local `EncoderProfile` with `Delta`, `Increase`, `Decrease`, and optional `AccelerationDeltas` |
| Legacy WidgetClass on an encoder Widget | `Profile=ProfileId` on its typed `Input Encoder` block; the runtime class name is removed |
| Exact inline encoder range `[ > 01-3f < 41-7f ]` without a WidgetClass | `Encoding=SignedBit` |
| Inline encoder range on a Widget that already uses a WidgetClass | Removed with an import notice because current runtime ignores it and uses the class lookup table |
| Other inline encoder direction range | Unresolved import diagnostic; do not infer behavior that the current runtime did not implement |
| `Widget Name ... WidgetEnd` and positional hardware processor lines | `Widget Name { ... }` with catalog-driven `Input Type { ... }` and `Feedback Type { ... }` blocks whose message bytes or OSC address use named properties |
| Legacy `OSKLayout Version=1`, Row blocks, and bare hexadecimal colors | Versionless `OSKLayout { Row { ... } }` inside the format 2 MIDI Surface; colors gain the required `#` prefix |
| Legacy `ColorCalibration ... ColorCalibrationEnd` | Typed `ColorCalibration { ... }` block |
| `BlockName ... BlockNameEnd` | `BlockName { ... }` |
| Hash-prefixed Learn FX directives and Learn FX pseudo-zones | `LearnFX.fxzon` plus normal generated FX bindings; `FXRowLayout` is dropped |
| Legacy single-slash comment lines | `//` comments when the line is recognized as a legacy comment; OSC address tokens remain data |
| Any renamed action | New action name and transformed parameters from the Bun-only action rename registry |

Action renames live in one declarative Bun-only registry separate from the general importer. Each entry identifies the legacy action, the new action, and any parameter transformation or context restriction. Runtime C++ does not keep old action aliases. The importer validates that every registry destination exists in the current generated action catalog.

Every conversion-matrix row requires at least one golden legacy-input and format-2-output fixture. Context-sensitive conversions require fixtures for every branch and for ambiguous input. Ambiguous input produces a focused diagnostic and remains unchanged until the user resolves it. The import preview lists every renamed action and structural conversion before files are written.

If one legacy file is referenced both as a SubZone and as an independent zone, the importer cannot silently assign one format 2 `Role`. The preview reports both reference locations and asks the user to keep one role or create a renamed copy for one use. A file referenced as a SubZone by several parents is not ambiguous and converts to one reusable `Role=Layer` document.

- [ ] Add lossless format 2 parsing, validation, syntax highlighting, quick fixes, and cross-file references.
- [ ] Create the declarative Bun-only action rename registry and validate its destinations against the current action catalog.
- [ ] Keep the conversion matrix and golden fixture pairs synchronized with every later format or action rename.
- [ ] Show a Zone Layer badge, all current parent references, and context-valid navigation actions in the editor. Offer `ExitZoneLayer` only for zone layers.
- [ ] Add safe User-zone rename with complete-profile reference updates, case-only filesystem handling, hash checks, and focused User overrides for Vendor referrers.
- [ ] Replace whole-profile Main cloning with `Create User override` for one zone. Show a clear override confirmation when import or copy selects an existing Vendor ID.
- [ ] Convert old `Zone ... ZoneEnd`, `BlockName ... BlockNameEnd`, and surface blocks during legacy import.
- [ ] Convert legacy `Widget|` channel placeholders to `Widget@CH` and include exact, missing-family, and non-channel wildcard golden fixtures.
- [ ] Convert every legacy anonymous zone value group to `Range`, `Delta`, `StepValues`, `AccelerationDeltas`, and `TicksPerStep` according to the conversion matrix.
- [ ] Convert legacy Surface Widget blocks through the completed Input and Feedback catalog. Convert WidgetClass, `StepSize`, and `AccelerationValues` to EncoderProfile references. Convert the exact unclassified standard signed-bit range, remove ignored redundant ranges with a notice, and report other ranges as unresolved.
- [ ] Convert MIDI `OSKLayout Version=1` and ColorCalibration blocks to their format 2 Surface blocks. Do not create OSKLayout for OSC templates.
- [ ] Convert legacy anonymous RGB groups to `StateColors` hexadecimal lists.
- [ ] Convert name-based navigator behavior into public `Role`, `Target`, and `BankTarget` metadata.
- [ ] Remove exact standalone legacy navigator-name lines from zone bodies and report other unknown lines.
- [ ] Convert Learn FX pseudo-zones into `LearnFX.fxzon`, derive supported entry defaults, report ambiguous display/default targets, and do not convert `FXRowLayout`.
- [ ] Convert brace-based `IncludedZones`, convert `SubZones` to the reusable `ZoneLayers` relation, and mark every referenced file with `Role=Layer`.
- [ ] Convert `GoZone SelectedTrackFX`, `GoSubZone`, `LeaveSubZone`, and all lifecycle pseudo-widgets to their explicit format 2 actions or blocks.
- [ ] Replace current development product INI parsing, serialization, diagnostics, fixtures, drafts, and editor support with the new unversioned `.conf` model and Product plus Device settings scopes.
- [ ] Convert old `CSI.ini` only through the Bun legacy importer and do not add runtime fallback.
- [ ] Report ambiguous legacy content before import and keep the source unchanged.

Ready when old public CSI examples can be imported into format 2 without runtime legacy parsing.

## [ ] Phase 5: Bundled data and runtime cutover

- [ ] Convert all files under `resources/Zones`, `resources/Surfaces`, `resources/Snippets`, and related Learn FX data.
- [ ] Change `PRODUCT_CONFIG_FILENAME` to the selected `.conf` filename and update every generated consumer and package path.
- [ ] Remove duplicated ring-style lists from Learn FX data and remove repeated standard `RingStyle` properties where action metadata produces the same result.
- [ ] Remove duplicate bundled zone IDs and verify User and Vendor layer behavior.
- [ ] Update OSK zone creation to write only format 2.
- [ ] Remove format 1 support from runtime after the importer and bundled data are complete.
- [ ] Update the Wiki and durable developer contracts.
- [ ] Perform focused C++ build, Bun checks, legacy import checks, and manual REAPER verification after explicit approval.

Ready when the plugin, editor, OSK, bundled resources, and documentation use format 2 as the only current runtime contract.

## [ ] Deferred design

### Track pinning

- [ ] Confirm a real user workflow and sufficient priority before designing track pinning.
- [ ] If track pinning is later required, evaluate it as runtime state on an existing navigator. Do not add a public `PinnedTrack` navigator by default.
- [ ] Decide actions, persistence, banking, and deleted-track behavior only after the workflow is accepted.

### Live binding reuse

- [ ] Confirm that repeated bindings cause enough maintenance cost to require live reuse instead of editor snippets.
- [ ] Use functional snippets as editor-time copied templates while live dependencies are not required.
- [ ] If live reuse is later required, define its document model, dependency rules, overrides, validation, and storage as a separate design task.

## Non-goals

- Runtime loading from legacy CSI folders.
- Multiple zones in one `.zon` file.
- Keeping magic zone names for compatibility.
- Moving full configuration editing into Lua OSK.
