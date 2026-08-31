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
- The product configuration now uses the unversioned brace grammar. REAPER's native configuration callback opens the shared Control Panel and does not parse or write the file.
- C++ preprocesses zone metadata, bindings, Learn FX templates, and OSK edits through separate line readers.

## Current source references

- [zone_manager.cpp](../src/controls/zone_manager.cpp) preprocesses zone headers, selects Home, excludes Learn FX pseudo-zones, and applies name-based navigator behavior.
- [zone_parser.cpp](../src/controls/zone_parser.cpp) parses bindings and structural sections while ignoring `Zone` and `ZoneEnd` tokens.
- [learn_dialog.cpp](../src/ui/learn_dialog.cpp) reads Learn FX pseudo-zones and hash-prefixed directives separately.
- [action_context.cpp](../src/actions/action_context.cpp) interprets anonymous square-bracket action values and passes `RingStyle` properties to widget feedback.
- [format2_integrator_config_parser.cpp](../src/controls/format2_integrator_config_parser.cpp) parses the product configuration into the shared runtime model.
- [config_dialog.cpp](../src/ui/config_dialog.cpp) opens the Control Panel without a second configuration parser or writer.
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
  Input Value { Encoding=MIDI14 Status=0xE0 }
  Input Touch { On=[0x90, 0x68, 0x7F] Off=[0x90, 0x68, 0x00] }
  Feedback Value { Encoding=MIDI14 Status=0xE0 }
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
Touch TrackAutoMode 2 StateColors=[ #141400, #FFFF00 ] // Touch
```

The first color represents state `0`, the second color represents state `1`, and later colors represent later indexed states. `StateColors=[ Track ]` uses the current track color.

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

One format 2 zone document is loaded once. `#` after a widget family expands that binding for every surface channel and supplies the current surface-channel index to its widget selector, target resolution, bank target, validation, and action context. A binding without `#` creates one channel-neutral context.

Legacy `Fader|` converts directly to `Fader#`. Format 2 does not support free Widget wildcards, so `Fader*` is invalid instead of matching an unintended family.

The recommended explicit replacement is a channel placeholder:

```text
@Meta { Version=2 Target=Tracks }

Fader# TrackVolume
Rotary# TrackPan
RotaryPush# TrackVolume StepValues=[0.716]
ButtonA# TrackSolo
```

`Target=Tracks` resolves a separate track for each channel-qualified binding.

A repeated binding can use the same selected track while each channel selects a send from the current bank:

```text
@Meta { Version=2 Target=SelectedTrack BankTarget=Sends }

Fader# TrackSendVolume
```

On an eight-channel surface, this initially maps the faders to sends 1 through 8 of the selected track. After one eight-channel bank step, they map to sends 9 through 16. The zero-based target indexes remain an internal runtime detail.

`#` is a numeric channel placeholder, not a glob, and does not create several zone objects. Format 2 does not support free Widget wildcard matching.

The token classes remain visually distinct:

- `Fader#` contains a terminal numeric channel placeholder for a widget family.
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
RotaryPush# TrackPan StepValues=[0.5]
RotaryC# TrackVolume AccelerationDeltas=[0.0002, 0.001, 0.005, 0.01, 0.05]
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

Legacy MIDI encoder direction blocks such as `[ > 01-3f < 41-7f ]` belong to the Surface input protocol, not to action values. The Surface contract below converts the exact standard range to `Encoding=MIDI7 Mode=SignedBit` only when no WidgetClass supplied the current runtime behavior. When a WidgetClass exists, the importer converts its `AccelerationValues` to an EncoderProfile and removes the ignored inline range with a notice.

The importer must decode each legacy value type. It must not copy one old anonymous bracket group into one new generic list.

### ✅ Learn FX through OSK

The native Learn FX window is replaced by an OSK FX edit mode. OSK already knows the real surface layout and real widget names, so format 2 removes `FXRowLayout` instead of converting its modifier and suffix matrix.

The runtime supplies all available modifiers and their valid combinations. The Learn FX profile does not enumerate them.

One dedicated `LearnFX.fxzon` file belongs to each zone profile. It contains the editable FX widget whitelist and bindings added to generated FX zones:

```text
@Meta { Version=2 }

FXWidgets {
  Parameter Fader#
  Parameter RotaryBig# RingStyle=Dot
  Parameter RotaryBigPush#
  NameDisplay DisplayUpper#
  ValueDisplay DisplayLower#
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

`#` in `FXWidgets` selects the exact numbered family for the configured surface channels. Free Widget wildcard matching is not supported. Ring styles and other feedback capabilities come from the matched widget feedback processor instead of a repeated profile list.

`FXWidgets` is required exactly once and must resolve at least one `Parameter` widget on the selected surface. `NameDisplay` and `ValueDisplay` entries are optional. One resolved widget can have only one role. Parameter widgets require numeric, relative, absolute, or two-state input. Display roles require text feedback. A capability mismatch or overlap links the selector and the real Surface widget.

An entry can contain default binding properties supported by its resolved widget and generated action. For example, `RingStyle=Dot` on a `Parameter` entry becomes the initial property on a generated `FXParam` binding. Display font, margin, and color defaults can be declared on display entries when their feedback processors support those properties. Unsupported defaults are errors. The user can change or remove a copied default in the FX-zone draft.

`GeneratedBindings` is optional and appears at most once. It accepts normal bindings plus the explicit lifecycle blocks. These entries are copied into each new FX-zone draft; they are not a hidden runtime include and do not change an existing saved FX zone later. The block uses normal action, widget, property, and capability validation.

FX edit mode works as follows:

1. The user focuses an FX and opens OSK FX edit mode from one configured surface assignment. That assignment supplies the surface, zone profile, channel count, and resolved `LearnFX.fxzon`.
2. The runtime finds an active FX zone with the same exact `MatchFX`. It opens the User file, offers a User override for a Vendor file, or creates a new unsaved User draft when no match exists.
3. Widgets selected by `FXWidgets.Parameter` entries are assignment targets. Modifier buttons remain usable as mode controls but are not FX assignment targets. Widgets outside the whitelist are disabled and shown with a muted style.
4. A physical or OSK parameter-widget click opens an FX-specific editor. It shows a searchable dropdown of the focused plugin's parameters, selects the current mapping, and offers only value and feedback properties supported by that action and widget.
5. The user can remove a parameter mapping. A valid parameter can be assigned to more than one physical widget, but one normalized widget and modifier context cannot map two different parameters unless normal multi-action rules allow that exact group.
6. Name and value displays default to a same-channel display family when one unambiguous `#` pairing exists. The user can select a different eligible display or no display. The draft writes normal `FXParamNameDisplay` and `FXParamValueDisplay` bindings with the same FX parameter index.
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

The complete format 2 semantic shape enum is shared by ring and bar feedback:

| Feedback shape | Preferred ring result | Preferred bar result |
|---|---|---|
| `Level` | `Fill` | `Fill` |
| `Centered` | `BoostCut`, with `Dot` as a supported fallback | `Bipolar` |
| `Spread` | `Spread`, with the processor-defined fallback | `Spread` |
| `Position` | `Dot` | `Normal` |

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

Bar feedback uses the same precedence with the separate global `BarStyle` enum: `Normal`, `Bipolar`, `Fill`, `Spread`, and `Off`. An explicit `BarStyle` is valid only on a Widget with Bar feedback. The initial standard bar mapping is the preferred bar-result column above and defaults to Off when neither an override nor an action FeedbackShape is available.

Normal zones therefore do not repeat standard ring styles:

```text
Rotary# TrackVolume
[Shift]+Rotary# TrackPan
[Option]+Rotary# TrackPanWidth
```

An explicit override remains available:

```text
[Shift]+Rotary# TrackPan RingStyle=Dot
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

`IncludedZones` means that each referenced independent zone remains active at the same time as the current zone. Each referenced zone keeps its own explicit `Target` and channel-qualified bindings. For example, `Home` can own global transport bindings while a `Target=Tracks` zone expands only its `#` bindings for the surface channels.

A functional snippet is different. It is an editor-only fragment that uses normal zone-body syntax. The editor inserts its resolved statements into the current zone draft, and those statements use the destination zone context. A snippet does not create or activate an independent runtime zone.

A format 2 snippet has no semantic Slot, Role, Requires, Binding wrapper, application ID, or generated marker comments:

```text
@Meta { Version=2 Name="Transport controls" }

PlayButton Play
[Shift]+PlayButton Reaper 40044
StopButton Stop
RecordButton Record
```

The `.snippet` extension identifies a zone fragment. Its body uses the same statements and binding grammar as a normal zone body. `Name` and `Description` are optional display metadata; the filename stem remains the snippet ID. The runtime never loads `.snippet` files.

Each physical widget token is a source mapping name. Repeated use of the same exact name is one mapping unit, and one `Name#` family is one channel-family mapping unit. The snippet can therefore be written and edited like normal zone text without `$Widget` placeholders. Before insertion, the Bun editor:

- validates syntax, action names, action parameters, selectors, events, and named binding properties without pretending that source mapping names exist on the target Surface;
- derives required input and feedback capabilities from action metadata, the binding expression, modifier declarations, and named properties such as `StateColors` and `RingStyle`;
- selects an exact compatible target widget or widget family automatically and shows a dropdown when there is no unique compatible match;
- treats each standard or pseudo-modifier selector as a separate token-aware mapping and lets the user keep it, replace it with another valid modifier, or remove it;
- applies one confirmed mapping consistently to every occurrence, including a mapped physical modifier declaration and its pseudo-modifier selector;
- lets the user explicitly omit one source widget and all statements that depend on it;
- validates the complete resolved fragment against the selected Surface and destination zone context before insertion.

An incompatible widget or modifier choice is not accepted. Removing a selector or omitting a source widget can expose duplicates or unreachable bindings, which the normal destination-zone validator reports in the preview. The editor inserts plain resolved zone statements after the current cursor line, or at the normal end-of-body position when the cursor is on the metadata line. It changes only the unsaved draft. Applying the same snippet twice has no special replace behavior; any duplicate is an ordinary zone diagnostic. No snippet provenance remains in the saved zone.

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

The former `ReaControlSurface.ini` grammar was a renamed continuation of `CSI.ini`. Format 2 replaces it instead of carrying its `Version=7.0`, order-dependent page records, and `Broadcaster` state into the new product.

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

`Page`, `Device`, and Page-local `Surface` IDs use unquoted ASCII identifiers. An ID starts with a letter and then contains only letters, digits, or `_`. IDs preserve their source spelling for display, compare case-insensitively in their scope, and cannot contain spaces or quoted text. Users write `_` where a visible separator is needed, such as `Surface FP_v2`. Separate display-name properties are not used.

`Device` IDs are unique in the file. Common properties are:

| Property | Required | Value and default |
|---|---|---|
| `Type` | Yes | `MIDI` or `OSC` |

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
  ReceivePort=8000
  TransmitPort=9000
  Address=192.168.1.20
  MaxPacketsPerRun=200
}
```

| Property | Required | Value and default |
|---|---|---|
| `Protocol` | No | `Generic` or `X32`; default `Generic` |
| `ReceivePort` | Yes | Integer from `1` through `65535` |
| `TransmitPort` | Yes | Integer from `1` through `65535` |
| `Address` | Yes | Non-empty unquoted host name or IP address |
| `MaxPacketsPerRun` | No | Positive integer; default `200` |

MIDI-only properties on OSC and OSC-only properties on MIDI are errors. A valid configured port that is not currently available is a runtime warning, not a syntax error; only that device and its dependent Surface instances are skipped.

`Page` uses its required block ID as its displayed name. Page IDs are unique case-insensitively. Page properties are optional booleans with these defaults:

| Property | Default |
|---|---|
| `FollowMCP` | `true` |
| `SyncPages` | `true` |
| `ScrollLink` | `false` |
| `ScrollSync` | `false` |

Each Page contains at least one `Surface` block and zero or more `Link` blocks. A Surface block ID uses the common unquoted identifier grammar and is unique case-insensitively inside that Page. It accepts:

| Property | Required | Value and default |
|---|---|---|
| `Device` | Yes | Existing Device ID |
| `Template` | Yes | Existing Surface template stable ID |
| `MainProfile` | No | Zone profile stable ID; defaults to `Template` |
| `FXProfile` | No | Zone profile stable ID; defaults to `MainProfile` |
| `StartChannel` | No | Non-negative integer; default `0` |

The same Device can be assigned on more than one Page. `StartChannel` is the zero-based starting channel of this Surface inside the Page. It does not change the surface-local numbering used by `#`. A missing template or required Main profile skips only that Surface instance. A missing or empty FX profile is valid.

A Link block contains exactly `From`, `To`, and `Share`. `From` and `To` reference distinct Surface IDs in the same Page. `Share` is a non-empty list from the closed enum `Home`, `Modifiers`, `FXMenu`, `SelectedTrackFX`, `SelectedTrackSends`, and `SelectedTrackReceives`. One pair uses one Link block. Duplicate pairs, duplicate categories, more than one incoming source for the same target and category, and a directed cycle for any shared category are errors linked to all involved blocks.

Product configuration validation is local where the block boundaries remain known. An invalid Device skips that Device and dependent Surface instances. An invalid Surface or Link skips only that block. An invalid Page-level property skips that Page. A structural lexer error that prevents reliable block recovery stops the document. The parser collects every independent diagnostic before applying the valid model.

All C++ consumers use one parsed `IntegratorConfig` model. The native configuration dialog must not open and interpret the same file a second time. The Bun editor and C++ must use the same field names, nesting, defaults, and validation rules.

### ✅ Surface structure and encoder input

A Surface template describes hardware or an OSC endpoint layout. It owns its positive `Channels` count and does not select REAPER ports or contain user behavior settings. Port selection belongs to the product `Device`; Page placement belongs to the Page `Surface` assignment.

One Device can be reused on several Pages only when every assigned Surface template declares the same `Channels` value. An unassigned Device needs no channel count because no runtime I/O object is created for it.

The selected Device `Type` must equal the Surface metadata `Protocol`. A mismatch skips that Page Surface assignment and links both declarations in the diagnostic.

A Surface document accepts these order-independent top-level blocks after `@Meta`:

- zero or more `EncoderProfile` blocks;
- zero or more `ValueProfile`, `ColorProfile`, `RingProfile`, `BarProfile`, `MeterProfile`, and `TextProfile` blocks;
- zero or more `FeedbackGroup` blocks;
- zero or one `ColorCalibration` block;
- one or more `Widget` blocks;
- zero or one `OSKLayout` block for either protocol.

Block IDs and references are case-sensitive. Duplicate Widget IDs, FeedbackGroup IDs, or profile IDs inside one profile kind are errors linked to every declaration. Unknown blocks and properties are errors. The parser builds one typed Surface model; hardware input, feedback capabilities, validation, Learn FX, and OSK all consume that model.

The normal MIDI form is:

```text
@Meta { Version=2 Protocol=MIDI Channels=1 Name="FaderPort V2" }

EncoderProfile Rotary {
  Delta=0.003
  Increase=[0x01, 0x02, 0x03]
  Decrease=[0x41, 0x42, 0x43]
  AccelerationDeltas=[0.005, 0.01, 0.02]
}

Widget RotaryBig {
  Input Encoder {
    Encoding=MIDI7
    Message=[0xB0, 0x10]
    Profile=Rotary
  }
}

Widget RotaryBigPush {
  Input Press {
    Encoding=MIDIExact
    On=[0x90, 0x20, 0x7F]
    Off=[0x90, 0x20, 0x00]
  }
}

Widget Solo {
  Input Press {
    Encoding=MIDIExact
    On=[0x90, 0x08, 0x7F]
    Off=[0x90, 0x08, 0x00]
  }
  Feedback State {
    Encoding=MIDIExact
    On=[0x90, 0x08, 0x7F]
    Off=[0x90, 0x08, 0x00]
  }
}
```

`Widget` contains zero or one quoted `Alias`, zero or one positive integer `Channel`, and one or more `Input` or `Feedback` blocks. It can contain several typed blocks, such as fader input, touch input, motor feedback, and color feedback. Channel is explicit format 2 metadata and is never inferred from the Widget ID. Several different Widgets can share one Channel, such as its fader, touch sensor, display, and meter. `Input Type` and `Feedback Type` use separate closed namespaces, so the same short type can exist in both. Format 2 uses universal primitive names. The legacy `FB_` prefix and device-model names are not part of these public namespaces.

Every primitive and Encoding has one shared schema entry that defines its protocol, allowed named properties, required properties, message matching or output-ownership rule, reusable runtime codec, and derived capabilities. C++, Bun, Lua metadata, Learn FX, and OSK must use this catalog. They must not infer capabilities by searching type-name text. Adding a codec without adding its schema entry is a build-generation error.

The current Input inventory used for migration is:

| Protocol | Current registered type | Named properties | Derived input capability | Legacy input |
|---|---|---|---|---|
| MIDI | `Press` | required three-byte `On`; optional three-byte `Off` | press; release only when Off exists | `Press` |
| MIDI | `AnyPress` | required two-byte `Message` prefix | press without release | `AnyPress` |
| MIDI | `Fader14Bit` | required `Status` byte | absolute | `Fader14Bit` |
| MIDI | `FaderportClassicFader14Bit` | required three-byte `MSBMessage` and `LSBMessage` | absolute | same name |
| MIDI | `Fader7Bit` | required two-byte `Message` prefix | absolute | `Fader7Bit` |
| MIDI | `Encoder` | required two-byte `Message` prefix and exactly one Profile or Mode | relative | `Encoder`, `MFTEncoder`, `EncoderPlain`, `Encoder7Bit` |
| MIDI | `Touch` | required three-byte `On` and `Off` | touch | `Touch` |
| OSC | `Control` | required `Address` | numeric value | `Control` |
| OSC | `AnyPress` | required `Address` | press without release | `AnyPress` |
| OSC | `Touch` | required `Address` | touch | `Touch` |
| OSC | `X32Fader` | required `Address` | absolute | `X32Fader` |
| OSC | `X32RotaryToEncoder` | required `Address` | relative | `X32RotaryToEncoder` |

`EncoderPlain` converts to `Encoding=MIDI7 Mode=SignedBitFixed`; `Encoder7Bit` converts to `Encoding=MIDI7 Mode=Relative7Bit`; and `MFTEncoder` converts to `Encoding=MIDI7` plus a generated local EncoderProfile. These legacy names do not remain in the runtime catalog.

`FaderportClassicFader14Bit`, `X32Fader`, and `X32RotaryToEncoder` are current behavior labels, not accepted final public type names. Their byte assembly, value curve, and relative conversion must move into generic Input primitives and Surface metadata during the universalization stage below.

The current Feedback special-case inventory groups registered C++ processors by their legacy property shape:

| Property shape | Current registered Feedback types |
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

These are migration inputs, not approved format 2 public type names. They come from the current registered C++ processors, not from every public example file. A legacy Surface line whose type is misspelled, stale, or not registered produces an unknown-type diagnostic with close suggestions. The importer does not preserve a non-working type only because it exists in a public example.

The following table records the current behavior that universal format 2 Feedback must preserve:

| Capabilities | Current registered Feedback types |
|---|---|
| `Toggle` | `TwoState`, `SCE24LEDButton` |
| `Color` | `NovationLaunchpadMiniRGB7Bit`, `MFT_RGB`, `FaderportRGB` |
| `Color`, `TrackColor` | `AsparionRGB` |
| `Toggle`, `Color` | `FaderportTwoStateRGB` |
| `Toggle`, `Color`, `Text` | `SCE24OLEDButton` |
| `Value` | `Fader14Bit`, `FaderportClassicFader14Bit`, `Fader7Bit`, `FaderportValueBar`, `FP8ScribbleStripMode`, `FP16ScribbleStripMode`, `MCUTimeDisplay`, `MCUAssignmentDisplay` |
| `Value`, `Ring` | `Encoder`, `AsparionEncoder` |
| `Value`, `Ring`, `Color` | `SCE24Encoder` |
| `Value`, `Meter` | `ConsoleOneVUMeter`, `ConsoleOneGainReductionMeter`, `FPVUMeter`, `QConProXMasterVUMeter`, `MCUVUMeter`, `MCUXTVUMeter`, `AsparionVUMeterL`, `AsparionVUMeterR` |
| `Text` | all `MCUDisplay*`, `MCUXTDisplay*`, `IconDisplay*`, `AsparionDisplay*`, `C4Display*`, `FP8ScribbleLine*`, `FP16ScribbleLine*`, `QConLiteDisplay*`, and `SCE24EncoderText` types |
| `Text`, `TrackColor` | `XTouchDisplayUpper`, `XTouchDisplayLower`, `XTouchXTDisplayUpper`, `XTouchXTDisplayLower` |
| OSC `Value` | `Value`, `Integer`, `X32Integer`, `X32Fader`, `X32RotaryToEncoder` |
| OSC `Value`, `Color` | `X32` |

`Meter` and `Ring` include numeric value feedback. `TrackColor` is separate from general `Color`: it allows automatic track-color updates but does not imply that `StateColors` can drive the processor. A replacement gets only its declared capabilities, even if its C++ class inherits unused base overloads.

Format 2 replaces device-named processors with this closed semantic primitive catalog:

| Direction | Primitive | Runtime meaning and derived capabilities |
|---|---|---|
| Input | `Press` | Button event input. It derives `Press`; an explicit Off message also derives `Release` |
| Input | `Value` | Absolute numeric input normalized for an Action after its declared value conversion. It derives `Absolute` |
| Input | `Encoder` | Relative signed delta input with a declared encoding or EncoderProfile. It derives `Relative` |
| Input | `Touch` | Touch-state input routed to the channel touch context, not to a normal value Action. It derives `Touch` |
| Feedback | `State` | Discrete state output. It derives `Toggle` |
| Feedback | `Value` | Numeric output. It derives `Value`. Motor faders use this primitive with echo or touch suppression metadata instead of a separate device or motor type |
| Feedback | `Color` | RGB or palette output. It derives `Color`; `TrackColor=true` also derives `TrackColor`; an optional state brightness or state palette input also derives `Toggle` |
| Feedback | `Ring` | Numeric ring output with a closed supported RingStyle map. It derives `Value` and `Ring`; optional segment color metadata also derives `Color` |
| Feedback | `Bar` | Numeric bar output with a closed supported BarStyle map. It derives `Value` and `Bar` |
| Feedback | `Meter` | Numeric level or gain-reduction output with its own mapping and refresh policy. It derives `Value` and `Meter` |
| Feedback | `Text` | Text output with width, encoding, clear value, and optional layout fields. It derives `Text`; optional state and color fields can also derive `Toggle` or `Color` |

The public capability enum is closed to `Press`, `Release`, `Absolute`, `Relative`, `Touch`, `Toggle`, `Value`, `Color`, `TrackColor`, `Ring`, `Bar`, `Meter`, and `Text`. Each primitive always derives only the capabilities in the table above. Conditional capabilities are also exact:

- Input Press derives Release only when its encoding declares an Off event.
- Feedback Color derives TrackColor only with `TrackColor=true`, which requires an explicit Widget Channel. It derives Toggle only when active and inactive state metadata are both present.
- Feedback Ring derives Color only when it contains Configure.
- Feedback Text derives Color when it supports constant or state-indexed text or background colors, and derives Toggle when it supports state-indexed colors.
- FeedbackGroup with `Capability=TrackColor` derives TrackColor for its declared Members and nothing else.

No file can declare a capability directly. OSK, Learn FX, snippets, zone validation, and runtime dispatch consume this derived set from the shared Surface catalog.

`AnyPress`, `Fader7Bit`, `Fader14Bit`, split FaderPort values, OSC Control, and X32 value types are encodings of these primitives, not public primitive names. `TwoState` becomes `State`. A physical widget can contain several primitive blocks when they own different output keys. One combined hardware packet uses one owning primitive block with additional declared input fields instead of several blocks that compete for the same output key.

Primitive and encoding are separate. The primitive selects runtime meaning and capability validation. An optional `Encoding` selects one closed reusable protocol shape. A format 2 Encoding name describes data representation, such as `MIDI7`, `MIDI14`, `MIDISplit`, `MIDIExact`, `OSCFloat`, `OSCInt`, or `OSCString`; it cannot name a product or device. The default is inferred only when one representation is possible from the declared properties. Otherwise `Encoding` is required.

Surface metadata supplies the device data needed by those primitives and encodings:

- MIDI messages, OSC addresses, and SysEx prefix and suffix bytes;
- input and output value width, byte order, scaling curve, lookup table, and clamp range;
- display channel, row, width, margins, font, and field positions;
- color encoding, palette, channel scaling, and track-color support;
- ring-style and bar-style codes plus their supported style subsets;
- all derived physical output keys and widget capabilities.

Public format 2 type names cannot contain a product or device model such as `Asparion`, `Faderport`, `XTouch`, `SCE24`, or `QCon`. The design must not create an unrestricted scripting or bytecode language inside a Surface file. If one behavior cannot be expressed by the shared primitives and declarative data, the plan must first extract the smallest reusable codec and document why it is not device-specific. A new device that uses an existing protocol shape must require only a Surface file change, not a new C++ class.

Current processors that generate REAPER-specific content themselves, such as MCU time and assignment displays, must be split. A normal Action supplies the value or formatted text, and a universal Feedback primitive only encodes and sends it to the device.

The source inventory identifies these required reusable encoding cases. They must be resolved before the catalog task is complete:

| Current special case | Required format 2 representation |
|---|---|
| FaderPort Classic split fader messages | `Value` with one split-value encoding, declared bit width, MSB message, and LSB message |
| X32 fader piecewise dB conversion | Reversible ValueProfile metadata referenced by `Input Value` and `Feedback Value` |
| X32 rotary-to-encoder acknowledgement | `Input Encoder` acknowledgement metadata that sends a declared constant after accepted input; it is not normal Action feedback |
| FaderPort state-sensitive RGB brightness | One `Feedback Color` block with RGB messages plus declarative active and inactive brightness scales |
| SCE24 OLED button | One `Feedback Text` block with state, fixed text, foreground color, background color, margins, font, and SysEx encoding fields |
| Generic, SCE24, and Asparion encoder rings | One `Feedback Ring` block with RingProfile, declarative RingStyle byte placement, and optional segment color configuration |
| FaderPort value bar | One `Feedback Bar` block with BarProfile plus separate value and style MIDI messages |
| MIDI Fighter Twister palette color | One `Feedback Color` block with MIDIPalette, ColorProfile, and one constant Companion message |
| FaderPort scribble-strip mode | Feedback Value with MIDISysEx Value7 and one static InitialValue when every legacy zone uses the same mode |
| MCU, XTouch, ICON, QCon, Asparion, and FaderPort text variants | Shared text encodings with declared SysEx prefix, suffix, text width, padding, row, channel, and offset data |
| MCU, QCon, FaderPort, Asparion, and Console One meters | Shared MeterProfile threshold or linear metadata plus declared MIDI output placement and refresh policy |
| XTouch track-color packet shared by several displays | One Surface-level feedback group that owns the packet and references its member display widgets; per-widget duplicate output ownership remains invalid |
| MCU time and assignment displays | Normal Actions produce text or state; `Feedback Text` or `Feedback State` only encodes and sends it |

The current FPVUMeter calculation can produce `0x80..0xA0` in a MIDI data-byte position. Format 2 cannot silently preserve bytes that violate the MIDI seven-bit data rule. Migration must report this case, and the final MeterProfile needs device documentation or hardware verification before the bundled FaderPort meter is converted.

`MFT_RGB` also has a legacy branch that treats selected RGB bytes as arbitrary MIDI commands. This is not Color feedback and does not remain in the universal Color primitive. The importer reports any actual use of that branch instead of hiding commands inside colors.

Value conversion, color palette, ring map, bar map, meter map, and text encoding data use reusable top-level profiles inside one Surface document when more than one Widget uses the same data. A profile contains declarative constants, ordered points, lookup entries, or thresholds. It cannot execute actions, inspect REAPER state, branch on device names, or contain general arithmetic.

#### Reusable Surface profiles

All profile IDs are case-sensitive and local to one Surface document. Duplicate IDs within one profile kind are errors. Primitive blocks use the explicit `ValueProfile=`, `ColorProfile=`, `RingProfile=`, `BarProfile=`, `MeterProfile=`, or `TextProfile=` property, so a combined primitive can reference more than one kind without an overloaded Profile field. A reference to the wrong kind is an error. `Input Encoder` keeps its existing `Profile=` property because it can reference only EncoderProfile. An unreferenced profile is a warning. Device names are allowed in local profile IDs because the ID is data owned by that Surface, not a public runtime type.

`ValueProfile` converts a scalar before Action input and performs the inverse conversion for feedback when `Direction=Both`:

```text
ValueProfile FaderCurve {
  InputUnit=Normalized
  OutputUnit=Decibels
  Direction=Both
  Interpolation=Linear
  Point Input=0.0 Output=-90.0
  Point Input=0.0625 Output=-60.0
  Point Input=0.25 Output=-30.0
  Point Input=0.5 Output=-10.0
  Point Input=1.0 Output=10.0
}
```

`InputUnit` and `OutputUnit` use `Normalized`, `Integer`, or `Decibels`. Point Input is always the device-side value and Point Output is always the product-side value. `Direction=Decode` uses the listed direction, `Encode` uses its inverse, and `Both` permits both. An Input reference requires Decode or Both; a Feedback reference requires Encode or Both. `Interpolation` is `Linear` or `Step`. At least two Point entries are required. Input values must increase strictly. Encode and Both require Linear interpolation plus strictly monotonic Output values so the inverse is unique. Values outside the first and last point clamp to the nearest endpoint. Decibels are converted through the product's one shared amplitude conversion instead of a device codec.

`ColorProfile` maps RGB colors to a device palette:

```text
ColorProfile BasicPalette {
  Match=Nearest
  Default=0
  Entry Color=#000000 Value=0
  Entry Color=#FF0000 Value=1
  Entry Color=#00FF00 Value=2
  Entry Color=#FFFFFF Value=7
}
```

`Match` is `Nearest`, `Exact`, or `HueRanges`. Default is a non-negative integer and defaults to zero. `Nearest` and `Exact` require Entry lines whose colors and non-negative integer Values are each unique. `Nearest` uses the smallest squared distance in calibrated RGB space and resolves an equal-distance tie by source order. `Exact` uses Default when no Entry matches.

`HueRanges` is the universal representation for a small hue-based device palette:

```text
ColorProfile HuePalette {
  Match=HueRanges
  Default=7
  MinimumBrightness=0.10
  MaximumNeutralSaturation=0.10
  HueRange Minimum=330 Maximum=20 Value=1
  HueRange Minimum=20 Maximum=80 Value=3
  HueRange Minimum=80 Maximum=160 Value=2
  HueRange Minimum=160 Maximum=210 Value=6
  HueRange Minimum=210 Maximum=250 Value=4
  HueRange Minimum=250 Maximum=330 Value=5
}
```

It rejects Entry lines and requires finite `MinimumBrightness` and `MaximumNeutralSaturation` values from zero through one. A calibrated RGB color is converted to HSV. Brightness less than or equal to MinimumBrightness and saturation less than or equal to MaximumNeutralSaturation select Default. Other colors select the one HueRange whose degree interval contains their hue. Minimum is inclusive and Maximum is exclusive. Minimum greater than Maximum declares the one wraparound interval through zero degrees. Hue values use zero inclusive through 360 exclusive. Ranges must cover that complete interval exactly once without gaps or overlap, and each non-negative Value is unique. ColorCalibration runs before every ColorProfile lookup. Direct RGB output does not require a ColorProfile.

`RingProfile` converts normalized value and RingStyle into separate reusable output fields:

```text
RingProfile StandardRing {
  Segments=11
  DefaultColor=#000000
  Quantize=Floor
  ValueOffset=1
  Style Dot Code=0 Steps=11
  Style BoostCut Code=1 Steps=11
  Style Fill Code=2 Steps=11
  Style Spread Code=3 Steps=6
}
```

Segments is an optional positive integer physical segment count and is required when a Ring uses color configuration. `DefaultColor` is optional, defaults to `#000000`, and is valid only when Segments is present. `Quantize` is `Floor` or `Round`. ValueOffset is an integer. Each Style name is one global RingStyle, each non-negative Code is unique, and Steps is a positive integer count of output positions not greater than Segments when Segments is present. A normalized value is clamped to zero through one and quantized across zero through `Steps - 1`, then ValueOffset is added. The profile produces `RingValue` and `RingStyleCode`; the Widget output encoding decides which MIDI bytes or OSC argument receive them. Only listed styles are supported by that feedback block.

`BarProfile` converts the resolved BarStyle to a device code:

```text
BarProfile StandardBar {
  Default=Off
  Style Normal Code=0
  Style Bipolar Code=1
  Style Fill Code=2
  Style Spread Code=3
  Style Off Code=4
}
```

Default and each Style name use the global BarStyle enum. Default must have a Style entry. Every supported Style occurs once and every Code is a unique non-negative integer. The profile produces BarStyleCode. It does not map the numeric bar value, which remains a normal zero-through-one Feedback value encoded by the Bar block.

`MeterProfile` is either linear or threshold-based:

```text
MeterProfile LinearMeter {
  Mode=Linear
  InputUnit=Normalized
  InputRange=[0.0, 1.0]
  OutputRange=[0, 127]
  Quantize=Floor
}

MeterProfile SteppedMeter {
  Mode=Steps
  InputUnit=Decibels
  Default=0
  Step Minimum=-60.3 Output=1
  Step Minimum=-54.1 Output=2
  Step Minimum=0.1 Output=14
}
```

`InputUnit` is `Normalized` or `Decibels`. Linear mode requires an increasing two-value InputRange, a two-value OutputRange with distinct non-negative integer endpoints, and `Quantize=Floor|Round`; it rejects Step and Default. OutputRange can descend for gain-reduction displays. Steps mode requires Default plus one or more Steps in strictly increasing Minimum order and rejects the ranges and Quantize properties. It selects the last Step whose Minimum is not greater than the input. Output values are non-negative integers.

`TextProfile` defines text conversion without defining its MIDI or OSC destination:

```text
TextProfile SevenCharacterDisplay {
  Encoding=ASCII7
  Width=7
  Padding=Space
  ClearText=""
  SilenceAsEmpty=true
}
```

Encoding is `ASCII7` or `UTF8`; MIDI text requires `ASCII7`. Width is a positive integer when present. Padding is `Space` or `None`; `Space` requires Width. ClearText is a quoted string and defaults to empty. SilenceAsEmpty defaults to false and converts the product's negative-infinity display text to empty before width and padding are applied.

A text device with alignment or inversion adds only its supported presentation codes:

```text
TextProfile StyledDisplay {
  Encoding=ASCII7
  Width=30
  Padding=Space
  DefaultAlignment=Center
  Alignment Center Code=0
  Alignment Left Code=1
  Alignment Right Code=2
  InvertCode=4
  PresentationCombine=BitOr
}
```

Alignment names use `Left`, `Center`, and `Right`. When Alignment entries exist, DefaultAlignment is required, must have an entry, and is used when the binding omits TextAlign. Codes are unique non-negative integers. InvertCode is optional and enables `TextInvert=true`; false contributes zero. PresentationCombine is `Add` or `BitOr` and defaults to BitOr. It combines the resolved alignment and inversion into one TextPresentationCode. Every result must fit one MIDI data byte, and BitOr requires disjoint set bits. TextAlign or TextInvert on a binding is an error when its TextProfile does not declare the required code.

#### Protocol encodings

`Encoding` is a closed protocol-shape enum. It does not select runtime meaning and cannot add a capability. The selected primitive, Surface Protocol, and Encoding must be one of these combinations:

| Encoding | Protocol | Allowed primitives | Required transport properties |
|---|---|---|---|
| `MIDIExact` | MIDI | Input `Press`, Input `Touch`, Feedback `State` | three-byte `On`; `Off` is optional for Press and required for Touch and State |
| `MIDIPrefix` | MIDI | Input `Press` | two-byte `Message`; any third data byte produces Press and no Release |
| `MIDI7` | MIDI | Input or Feedback `Value`, Input `Encoder`, Feedback `Ring`, Feedback `Bar`, Feedback `Meter` | one- or two-byte Message prefix as restricted below |
| `MIDI14` | MIDI | Input or Feedback `Value` | one Status byte; incoming or outgoing data byte 1 is LSB and data byte 2 is MSB |
| `MIDISplit` | MIDI | Input or Feedback `Value` | two-byte `MSBMessage` and `LSBMessage` prefixes, `Bits`, and `Commit` |
| `MIDIRGB` | MIDI | Feedback `Color` | optional three-byte `Enable`; two-byte `Red`, `Green`, and `Blue` prefixes |
| `MIDIPalette` | MIDI | Feedback `Color` | two-byte `Message` prefix and `ColorProfile` |
| `MIDISysEx` | MIDI | any Feedback primitive | one non-empty `Payload` list |
| `MIDICharacters` | MIDI | Feedback `Text` | `Status`, `StartData`, `Direction`, and `TextProfile` |
| `OSCFloat` | OSC | Input `Press`, `Touch`, `Value`, or `Encoder`; Feedback `State`, `Value`, `Ring`, or `Meter` | `Address` |
| `OSCInt` | OSC | Input `Press`, `Touch`, `Value`, or `Encoder`; Feedback `State`, `Value`, `Color`, `Ring`, or `Meter` | `Address` |
| `OSCString` | OSC | Feedback `Text` or `Color` | `Address`; Color also requires `Format=HexRGBA` |

Numeric primitives are `Value`, `Encoder`, `State`, `Ring`, `Bar`, and `Meter` where that direction exists. `Press` and `Touch` can also use OSCFloat or OSCInt. `Match=Any` emits Press for every message and is invalid for Touch. `Match=NonZero` treats non-zero as On and zero as Off. `Match=Exact` requires OnValue; OffValue is optional for Press and required for Touch. State feedback defaults to OffValue zero and OnValue one. Value and Encoder default to no ValueProfile. Color with OSCInt requires ColorProfile.

For MIDI7 Input Value and Encoder, Message is exactly two bytes and the incoming third byte is the value. For MIDI7 Feedback Value, Ring, Bar, and Meter, Message is one or two bytes and the encoded value is appended as the final MIDI data byte. All produced bytes must fit zero through `0x7F`. A Ring block requires RingProfile and emits its RingValue; a Bar block requires BarProfile and emits a normalized value from zero through `0x7F`; a Meter block requires MeterProfile and emits its mapped value. `ValueBase` is an optional data byte combined with the mapped value through `Combine=Replace|Add|BitOr`, defaulting to Replace with base zero. Add must not overflow `0x7F`; BitOr requires disjoint set bits between every possible mapped value and ValueBase.

MIDI7 Feedback Ring also requires `StyleTarget=Status|Data1|Value`. Status and Data1 require a two-byte Message; Value selects the appended value byte. `StyleShift` is an integer from zero through six and defaults to zero. `StyleCombine` is `Add` or `BitOr` and defaults to BitOr. The RingStyleCode is shifted, then combined into the selected target after ValueBase and RingValue are resolved. Every possible result must remain a valid status or data byte, and BitOr requires disjoint bits. For example, a common encoder ring uses `StyleTarget=Value StyleShift=4 StyleCombine=BitOr`; an Asparion-style output uses `StyleTarget=Status StyleShift=0 StyleCombine=Add`. This is byte placement metadata, not a device-specific encoding.

MIDI7 Feedback Bar uses two explicit outputs:

```text
Widget ValueBar1 {
  Channel=1
  Feedback Bar {
    Encoding=MIDI7
    Message=[0xB0, 0x30]
    StyleMessage=[0xB0, 0x38]
    BarProfile=StandardBar
  }
}
```

StyleMessage is exactly two bytes. The numeric value is appended to Message and BarStyleCode is appended to StyleMessage. The Bar sends both messages when it becomes the active feedback owner, sends Message when its value changes, and sends StyleMessage when its resolved BarStyle changes. Clear sends value zero and the profile's Off style. A BarProfile without an Off entry is invalid. Message and StyleMessage must derive different output keys.

MIDI14 always represents a normalized value with 14 data bits. Status must be a pitch-bend status from `0xE0` through `0xEF`. Input reconstructs `(MSB << 7) | LSB`; Feedback sends LSB and then MSB in the same message. A ValueProfile is applied outside this packing.

MIDISplit supports `Bits=8..14`. MSBMessage and LSBMessage are distinct two-byte MIDI prefixes. Commit is `MSB` or `LSB` and identifies which arriving part publishes a complete Input value. Feedback sends the non-commit part first and the commit part second. Unused high bits are zero. The two parts use up to seven bits each, with the low part holding the least significant bits. This replaces the FaderPort Classic two-message codec without naming that device.

MIDIRGB appends one calibrated seven-bit channel value to each Red, Green, and Blue prefix and sends the optional Enable message first. `InactiveBrightness` and `ActiveBrightness` are finite values from zero through one. When either is present, both are required, state input is enabled for the Color primitive, and it also derives Toggle. TrackColor defaults to false. Direct RGB uses ColorCalibration but no ColorProfile.

MIDIPalette appends the selected ColorProfile integer value to Message. The value must fit one MIDI data byte. TrackColor defaults to false. It accepts one optional exact three-byte `Companion` message and `CompanionOrder=Before|After`, defaulting to After. The companion is sent with every accepted color output, including clear, and owns its own output key. It contains no dynamic field and cannot be selected or changed from a zone binding. This expresses hardware that needs one fixed mode message beside its palette value without adding general output scripting.

MIDISysEx automatically adds leading `0xF0` and trailing `0xF7`. Payload is an ordered list containing MIDI data-byte constants and fields from this closed set:

- `State7`, `Value7`, `ValueLSB7`, `ValueMSB7`;
- `Red7`, `Green7`, `Blue7`, `PaletteValue`;
- `RingValue`, `RingStyleCode`, `BarValue`, `BarStyleCode`, `MeterValue`;
- `TopMargin7`, `BottomMargin7`, `Font7`;
- `TextPresentationCode`;
- `BackgroundRed7`, `BackgroundGreen7`, `BackgroundBlue7`;
- `TextRed7`, `TextGreen7`, `TextBlue7`;
- `SegmentMasks`, `SegmentRed7`, `SegmentGreen7`, `SegmentBlue7`;
- `SlotColors`;
- `Text`.

Each field requires the matching primitive input or profile. Text requires TextProfile and must be the final Payload entry because its encoded length can vary. A Text Feedback block whose Payload contains `TopMargin7`, `BottomMargin7`, or `Font7` must declare the matching `TopMargin`, `BottomMargin`, or `Font` default on that block. A Payload that contains background or text RGB fields must declare the matching `BackgroundColor` or `TextColor` default. The presence of these Payload fields permits the same named binding overrides; no separate supported-properties list is needed. State-specific background, text color, or brightness properties enable state input and derive Toggle. No field can contain an expression, offset, condition, loop, action, or arbitrary property lookup. Device and channel constants are resolved into literal Payload bytes in each Widget declaration.

MIDICharacters sends one three-byte message per encoded character. Status is a MIDI status byte, StartData is a data byte, Direction is `Ascending` or `Descending`, and TextProfile supplies Width and encoded text. The second byte starts at StartData and changes by one per character; the third byte is the character. TextProfile Width is required. This supports character-addressed time or assignment displays after a normal Action supplies their text.

OSCFloat sends or accepts one 32-bit float argument, OSCInt one 32-bit integer argument, and OSCString one string argument. Address is explicit and starts with `/`; no encoding adds `/Color` or rewrites an address. ValueProfile, ColorProfile, RingProfile, MeterProfile, and TextProfile run before output or after input as applicable. OSCString Feedback Color requires `Format=HexRGBA` and sends lower-case `#RRGGBBAA`; this is a transport representation for compatible OSC clients, not configurable hardware transparency.

Input Encoder with OSCFloat or OSCInt requires either ValueProfile or `Scale`, where Scale is a non-zero finite multiplier applied to the signed received value. It can contain:

```text
Acknowledge {
  Encoding=OSCInt
  Address="/control/ack"
  Value=64
}
```

Acknowledge is optional, sends one declared constant only after accepted input, and owns its output Address. It cannot reference Action state or normal feedback. This replaces the X32 rotary acknowledgement behavior.

Feedback Value accepts `EchoGuardMs=0..10000` and `SuppressWhileTouched=true|false`, both defaulting to zero or false. EchoGuardMs suppresses output for that many milliseconds after the same Widget accepts Value input. SuppressWhileTouched requires Touch input on the same channel. These properties describe motor and bidirectional controls without a Motor primitive.

Feedback Value also accepts optional `InitialValue`, a finite product-side value validated through its ValueProfile or normal zero-through-one range. Its presence sends that value once when the Surface is initialized, even when no zone binding targets the Widget. It is static Surface configuration and cannot vary by zone.

Feedback Meter accepts `Refresh=OnChange|Continuous`, defaulting to OnChange. Continuous requires `RefreshIntervalMs=10..5000` and resends the current mapped value even when it has not changed. OnChange rejects RefreshIntervalMs. Clear sends the MeterProfile Default or the lower Linear OutputRange endpoint.

#### Ring color configuration

One Ring feedback can own its normal value output and an optional color-configuration output:

```text
Widget Rotary1 {
  Channel=1
  Feedback Ring {
    Encoding=MIDI7
    Message=[0xB0, 0x30]
    RingProfile=StandardRing

    Configure {
      Encoding=MIDISysEx
      Payload=[0x00, 0x02, 0x38, 0x01, 0x30, SegmentMasks, SegmentRed7, SegmentGreen7, SegmentBlue7]
    }
  }
}
```

`Configure` is valid only inside Feedback Ring, uses `Encoding=MIDISysEx`, and requires a RingProfile with Segments. Its Payload contains each of `SegmentMasks`, `SegmentRed7`, `SegmentGreen7`, and `SegmentBlue7` exactly once and in that order. SegmentMasks expands to `ceil(Segments / 7)` MIDI data bytes. Bit zero of its first byte selects segment zero, and remaining bits and bytes continue in segment order. The three color fields each produce one calibrated seven-bit component.

A zone binding can set `RingColors=[ #FF0000 ]` to use one color for every segment or provide exactly Segments colors in physical segment order. If RingColors is omitted, every segment uses the RingProfile DefaultColor. The runtime groups equal calibrated colors in first-appearance order and sends one Configure packet per group. SegmentMasks selects that group's segments. Configure packets are sent when the resolved RingColors value changes or when the binding becomes the Ring feedback owner, not for every Ring value update.

RingColors is valid only when the target Widget has a Ring Configure block. Every action in one ordered multi-action binding that supplies RingColors must resolve the same list. Different lists are an error because one Ring cannot have several simultaneous color owners. Configure remains part of its containing Ring feedback and does not create a second capability or independent owner.

#### Surface feedback groups

A `FeedbackGroup` is a Surface-level owner for one output packet assembled from several Widgets. The initial format supports only track-color groups:

```text
FeedbackGroup TrackColors {
  Capability=TrackColor
  Encoding=MIDISysEx
  ColorProfile=BasicPalette
  EmptyColor=#FFFFFF
  UseTrackColorWhen=SourceTextPresent
  Payload=[0x00, 0x00, 0x66, 0x14, 0x72, SlotColors]

  Slot Source=DisplayUpper1 Members=[DisplayUpper1, DisplayLower1]
  Slot Source=DisplayUpper2 Members=[DisplayUpper2, DisplayLower2]
}
```

The initial FeedbackGroup schema requires `Capability=TrackColor`, `Encoding=MIDISysEx`, ColorProfile, Payload, and one or more Slot lines. Payload contains `SlotColors` exactly once. SlotColors expands to one palette data byte per Slot in declaration order. No other dynamic MIDISysEx field is valid in this block. EmptyColor defaults to `#000000`. `UseTrackColorWhen` is `Always` or `SourceTextPresent` and defaults to Always. SourceTextPresent uses EmptyColor while the Source Widget's most recent Text feedback is empty.

Each Slot Source and Member references a declared Widget with an explicit positive Channel. A Source occurs in its own non-empty Members list. All Members in one Slot have the same Channel as Source, and different Slots have different channels. A Widget can belong to only one TrackColor FeedbackGroup. Group membership derives TrackColor capability for every Member but does not derive Color, Text, Toggle, or any action feedback property.

The group updates when a slot's resolved track color changes and, with SourceTextPresent, when its source text changes between empty and non-empty. It does not inspect action names or configuration state. The FeedbackGroup owns the complete SysEx output. Its member Widgets do not own parts of that packet. Two groups with the same fixed Payload prefix before SlotColors, or any other output with that key, are an error.

MIDI bytes use `0x00` through `0xFF`. MIDI data bytes must also be at most `0x7F`; status-byte positions require a valid status value. `Message`, `On`, and `Off` are typed lists with exact lengths defined by the selected Input or Feedback type. A diagnostic points to the exact byte or missing property instead of reporting only an invalid Widget block.

#### Input matching and output ownership

Input and Feedback use separate namespaces. One physical MIDI message or OSC address can be both an Input and a Feedback destination on the same Widget. Ownership must be unique inside each direction.

An Input match pattern is the complete set of messages that one block can consume:

- MIDIExact uses its complete On and Off messages.
- MIDIPrefix and MIDI7 Input use the fixed two-byte prefix with any valid final data byte. EncoderProfile or Mode filters the accepted value after ownership is resolved and therefore cannot make two overlapping prefixes valid.
- MIDI14 Input uses its Status with any two data bytes.
- MIDISplit Input owns both declared two-byte prefixes with any valid final data byte.
- OSC Input uses its exact Address independent of the received numeric type.

Two Input patterns conflict when at least one valid physical message can match both. The diagnostic links both declarations and shows the intersecting message or address. On and Off inside one MIDIExact block must be different, and two prefixes inside one MIDISplit block must be different.

A Feedback output key identifies the physical destination independently from its changing value:

- MIDIExact Feedback State derives one key from the status and first data byte of each On and Off message. Equal On and Off keys collapse to one owned key.
- MIDI7 derives its fixed one- or two-byte Message key. Feedback Ring derives every possible key after StyleTarget changes Status or Data1. Feedback Bar also derives its two-byte StyleMessage key.
- MIDI14 derives its Status key. MIDISplit derives one key for each declared prefix.
- MIDIRGB derives keys for Red, Green, Blue, and optional Enable. MIDIPalette derives Message and optional Companion keys. A complete constant three-byte message uses its status and first data byte as the destination key.
- MIDICharacters derives one key for each status and data-address pair from StartData through its TextProfile Width.
- MIDISysEx requires one or more constant Payload bytes before its first dynamic field. All remaining Payload entries are dynamic fields; a constant after a field is invalid. Its output key is the complete constant prefix. Two equal SysEx prefixes conflict.
- OSC Feedback derives its exact Address key.

Ring Configure and FeedbackGroup are owners under the same MIDISysEx rule. Configure belongs to its containing Ring block. A FeedbackGroup owns its key directly, not through its member Widgets.

One output key has one owning Feedback block or FeedbackGroup. Any overlap is an error linked to both owners. Several Feedback blocks on one Widget are valid only when all their derived keys differ. There is no output-sharing exception and no source-order winner.

#### Encoder profiles

`EncoderProfile` replaces `StepSize`, `AccelerationValues`, and `WidgetClass`. It is a reusable local lookup table, not a widget capability or runtime class name.

| Property | Required | Value and rule |
|---|---|---|
| `Increase` | Yes | Non-empty list of unique MIDI data bytes |
| `Decrease` | Yes | Non-empty list of unique MIDI data bytes |
| `Delta` | No | Positive finite default delta for a binding without its own `Delta` |
| `AccelerationDeltas` | No | Non-empty list of positive finite defaults for a binding without its own `AccelerationDeltas` |

Increase and Decrease values cannot overlap. Their list position is the zero-based acceleration level. The lists can have different lengths. Runtime clamps a level beyond the resolved `AccelerationDeltas` list to its last value, matching the format 2 action-value rule. A binding property overrides the corresponding EncoderProfile default. A profile does not define action range or discrete `StepValues`.

`Input Encoder` with `Encoding=MIDI7` requires a two-byte MIDI `Message` prefix and exactly one of:

- `Profile=ProfileId`, which maps the incoming third byte through that EncoderProfile;
- `Mode=SignedBit`, which uses bit 6 as direction and bits 0 through 5 as magnitude;
- `Mode=SignedBitFixed`, which uses bit 6 as direction and emits one fixed tick;
- `Mode=Relative7Bit`, which compares consecutive values and emits one directional tick.

Unknown third-byte values in a profile do not produce input. Duplicate profile values and an unknown Profile reference are errors. The old special `MFTEncoder` table becomes a normal EncoderProfile during import.

Legacy inline encoder text such as `[ > 01-3f < 41-7f ]` is not copied. With a legacy WidgetClass, the importer uses the class `AccelerationValues` table and reports that the ignored inline text was removed. Without a WidgetClass, the exact standard range converts to `Encoding=MIDI7 Mode=SignedBit`. Any other inline range that does not describe current runtime behavior remains unresolved and produces a preview diagnostic instead of a guessed mapping.

#### OSC widgets

OSC uses the same Widget, Input, and Feedback structure with OSC-specific universal catalog types and a named `Address` property:

```text
@Meta { Version=2 Protocol=OSC Channels=32 Name="OSC Hardware Surface" }

Widget ChannelFader1 {
  Input Value { Encoding=OSCFloat Address="/control/1" }
  Feedback Value { Encoding=OSCFloat Address="/control/1" }
}
```

Device-specific value conversion, such as the current X32 fader curve, uses `ValueProfile=FaderCurve` on both universal `Value` blocks. A bidirectional profile keeps input and feedback conversion symmetric.

An OSC address is a non-empty quoted string that starts with `/`. A slash never starts a comment. Only `//` starts a comment. One incoming OSC address has one owning Input block and one outgoing OSC address has one owning Feedback block. Duplicate ownership is an error linked to both blocks.

An OSC Surface can contain `OSKLayout`. This is required for hardware OSC devices such as Behringer X32. A tablet application that already provides its own GUI can omit the block and is then not offered as a desktop OSK surface.

#### OSK layout

`OSKLayout` is part of a MIDI or OSC Surface document and does not have a separate version:

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

The initial layout properties remain `Shape`, `Width`, `Height`, `Top`, `Group`, `Label`, `Color`, `Role`, `PressTarget`, `ScrollTarget`, `ValueTarget`, `TouchTarget`, and `RotaryStyle`. OSK layout colors use opaque `#RRGGBB`; alpha is not part of the OSK visual contract. Positive Width and Height default to `1`; Top defaults to `0`; Spacer Width is positive and defaults to `0.5`. Input and Feedback capability strings are derived from the typed Widget blocks and are not editable OSK layout properties.

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

The block is active when present and accepts:

| Property | Default | Value and rule |
|---|---|---|
| `InputMax` | `255` | Integer from `1` through `255` |
| `OutputMax` | Feedback processor default | Integer from `1` through `255` when present |
| `NeutralTolerancePercent` | `0` | Integer from `0` through `100`; zero disables neutral-color detection |
| `RedScale`, `GreenScale`, `BlueScale` | `1.0` | Positive finite channel scale |
| `NeutralRedScale`, `NeutralGreenScale`, `NeutralBlueScale` | `1.0` | Positive finite scale applied to near-neutral colors |
| `NeutralCurve` | `1.0` | Positive finite neutral-color brightness curve |

Block presence replaces the old `Enabled` property. An omitted OutputMax keeps each Feedback processor's native maximum, such as `127` for a seven-bit device. The final channel value is clamped from zero through the effective OutputMax. `ColorCalibration` is invalid when the Surface has no `Color` or `TrackColor` Feedback capability.

## Remaining design decisions

- ✅ Expose only `Role`, `Target`, and optional `BankTarget` in Main zone metadata. Derive navigator, track set, lifetime, activation scope, link routing, and internal FX context in the typed runtime model.
- ✅ Load each zone document once, use `#` to expand only channel-qualified bindings, and reject bare anonymous square-bracket groups in format 2.
- ✅ Confirm the zone action property names `Range`, `Delta`, `StepValues`, `AccelerationDeltas`, and `TicksPerStep`, and resolve legacy MIDI encoder direction ranges in the typed Surface contract.
- ✅ Define terminal `#` surface-channel expansion with exact numbered-family ordering and missing-member diagnostics, and reject free Widget wildcards.
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
| `Track` | Creates one track-navigator instance per surface channel and selects the normal track bank | `Target=Tracks`; each `#` binding resolves the matching channel track | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Page::AdjustBank()](../src/controls/page.h) |
| `VCA`, `Folder`, `SelectedTracks` | Create track-navigator instances, select a special track-list mode while active, and select the matching bank | `Target=VCA`, `Folder`, or `SelectedTracks`; runtime derives the track navigator, Page scope, track set, and bank | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::Activate()](../src/controls/zone.cpp), [Page::AdjustBank()](../src/controls/page.h) |
| `TrackSend`, `TrackReceive`, `TrackFXMenu` | Create track-navigator instances and derive each zone slot from a separate bank offset | `Target=Tracks` plus `BankTarget=Sends`, `Receives`, or `FX`; `#` resolves tracks and runtime derives Page routing | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::GetSlotIndex()](../src/controls/zone.cpp), [ZoneManager::AdjustBank()](../src/controls/zone_manager.h) |
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
| `GoZones` | Uses a deprecated file as a top-level zone allow-list and can pass one stale navigator name beside each zone ID | Remove the runtime path; the importer uses recognized navigator metadata during conversion and does not emit a replacement block or dependency | [ZoneManager::Initialize()](../src/controls/zone_manager.cpp) |
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
| Runtime initialization | `ParseFormat2IntegratorConfig()` produces `IntegratorConfig`; [config_parser.cpp](../src/controls/config_parser.cpp) applies it | One C++ parser and typed model |
| Native Settings and Devices protocols | Read source, call `ParseFormat2IntegratorConfigSource()`, then use the canonical serializer and atomic writer | Edit and validate the same typed document model through one transaction writer |
| Native configuration callback | Opens the shared Control Panel | No duplicate semantic path |
| Bun editor | Uses `parseProductConfig()` and the editor store | Independent TypeScript implementation of the same normative grammar, diagnostics, and fixtures |

The relevant implementations are [format2_integrator_config_parser.cpp](../src/controls/format2_integrator_config_parser.cpp), [settings_protocol.cpp](../src/controls/settings_protocol.cpp), [devices_protocol.cpp](../src/controls/devices_protocol.cpp), [config_dialog.cpp](../src/ui/config_dialog.cpp), and [product-config.ts](../tools/config-editor/src/product-config.ts).

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

## ✅ Phase 2: Format 2 specification

The confirmed format direction above is input to this phase, not the complete grammar. Main zone identity remains the `.zon` filename stem. `@Meta` carries format and behavior metadata. `MatchFX` identifies an external plugin match, not the zone itself. Format 2 uses `#`; legacy `Widget|` is accepted only by the importer and converted during migration.

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
- `@Meta` is the document metadata marker. One terminal `#` is the numeric surface-channel placeholder in a Widget reference. No other unquoted `@` marker is defined.
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
widget-reference = identifier, [ "#" ] ;
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

[Shift]+Rotary# TrackPan StepValues=[0.0, 0.5, 1.0]
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

### ✅ Surface-channel expansion

`#` is a numeric widget-family placeholder. It always means surface channel and never means track, selected track, send, receive, or FX:

```text
@Meta { Version=2 Target=Tracks }

Fader# TrackVolume
[Shift]+Rotary# TrackPan
```

The `#` contract is:

- `#` follows one non-empty widget-family identifier without whitespace and is valid only in a schema position that accepts a widget reference.
- The placeholder is exactly one terminal `#`. Bare `#`, embedded `#`, repeated `#`, and text after it are errors.
- `#` is data, not a comment marker. A leading `#RRGGBB` color remains an unambiguous scalar value because it appears in a value position instead of a Widget-reference position.
- For a surface with channel count `N`, `Fader#` resolves in numeric channel order to the exact real widget names `Fader1` through `FaderN`.
- Every expected real widget must exist on the selected surface. A missing family member is an error that names each missing widget. A family that resolves no widgets uses the same error code with the complete expected set.
- One zone document and one source binding remain in the parsed model. Expansion creates channel-specific runtime action contexts; it does not clone the zone or its channel-neutral bindings.
- `Target`, `BankTarget`, and action metadata decide how the supplied surface-channel index resolves a track, send, receive, FX, or other action target. The placeholder itself does not name that logical target.
- A widget reference without `#` names one exact widget and creates a channel-neutral binding unless the concrete widget itself supplies channel state.

Free Widget wildcards are not part of format 2. `Rotary*`, bare `*`, `?`, character ranges, and escape-based patterns are errors. Authors use an exact Widget ID or one terminal `#` numbered family, so `Rotary#` can never select `RotaryPush1`.

### ✅ Action value and feedback properties

Numeric action behavior uses only named properties. Action metadata declares which properties each action accepts.

| Property | Value | Rules |
|---|---|---|
| `Range` | Two-number list `[Minimum, Maximum]` | Both values are finite, `Minimum` is less than `Maximum`, and action output is clamped to this range |
| `Delta` | Positive number | Amount applied by one relative input event when no acceleration level is supplied |
| `AccelerationDeltas` | Non-empty list of positive numbers | Entry zero is the slowest acceleration level; a higher input level uses the last available entry when the list is shorter |
| `StepValues` | Non-empty list of finite numbers | Defines an ordered discrete sequence; source order is behavior and is preserved |
| `TicksPerStep` | Non-empty list of positive integers | Valid only with `StepValues`; entry zero is the slowest acceleration level; omitted higher levels reuse the final entry |
| `StateColors` | Non-empty list of colors or the single value `[Track]` | Entry zero is state zero, entry one is state one, and later entries are later indexed states |
| `RingStyle` | `Dot`, `Fill`, `BoostCut`, or `Spread` | Explicit override valid only when the target Widget has Ring feedback and its RingProfile supports the style |
| `RingColors` | One color or exactly the RingProfile Segments count of colors | Valid only when the target Widget has Ring Configure; one color fills every segment |
| `BarStyle` | `Normal`, `Bipolar`, `Fill`, `Spread`, or `Off` | Explicit override valid only when the target Widget has Bar feedback and its BarProfile supports the style |
| `TextAlign` | `Left`, `Center`, or `Right` | Valid only when the target TextProfile declares that alignment |
| `TextInvert` | Boolean | `true` requires an InvertCode in the target TextProfile |
| `FixedText` | Quoted string | Uses this text instead of action text; the action can still supply state for colors |
| `TopMargin`, `BottomMargin`, `Font` | Integer from `0` through `127` | Valid only when the target Text Payload contains the matching field; TopMargin cannot exceed BottomMargin |
| `TextColor`, `BackgroundColor` | One color | Constant presentation color for a Text field that supports it |
| `TextColors`, `BackgroundColors` | Non-empty list of colors | Indexed by action state; mutually exclusive with the matching constant color property |

`Range` is optional. Without it, action metadata supplies the action's valid range. Every `StepValues` entry must be inside the effective range. Continuous changes produced by `Delta` or `AccelerationDeltas` are clamped to that range.

`StepValues` selects discrete mode and cannot be combined with `Delta` or `AccelerationDeltas`. Duplicate adjacent or non-adjacent step values are errors because they create distinct positions with the same output. Without `TicksPerStep`, every input tick advances one step. `TicksPerStep` values map by acceleration level, not by `StepValues` position.

`Delta` and `AccelerationDeltas` can be used together. `Delta` handles relative input without an acceleration level; `AccelerationDeltas` handles indexed acceleration input. A decreasing acceleration-delta list is valid but produces a warning because faster input then changes the value by a smaller amount.

All format 2 Surface and Zone colors use exact opaque `#RRGGBB` syntax. Device feedback has no alpha-compositing contract. OSK and OSD transparency stays in separate UI appearance settings. `[Track]` cannot be combined with explicit colors. The property is valid only when action and widget feedback metadata support indexed or track color. Unsupported colors, too few colors for a fixed known state count, and extra unreachable colors are diagnostics.

FixedText is required when the action does not produce text. Without FixedText, the action must expose text feedback. TextColors and BackgroundColors require an action with discrete state feedback and use the same state-index rules as StateColors. Their presence derives Toggle and Color for the Text feedback. Constant TextColor and BackgroundColor derive Color but not Toggle. Presentation properties cannot add Text capability to a Widget whose Surface declaration has no Feedback Text block.

Examples:

```text
Rotary FXParam 0 Range=[0.0, 1.0] Delta=0.005 AccelerationDeltas=[0.005, 0.02, 0.1]
RotaryPush TrackPan StepValues=[0.5]
Rotary TrackAutoMode StepValues=[0, 1, 2, 3, 4] TicksPerStep=[4, 2, 1]
Touch TrackAutoMode 2 StateColors=[ #141400, #FFFF00 ]
ValueBar# TrackPan BarStyle=Bipolar
```

Legacy import identifies the old anonymous values by their parsed type, not only their punctuation. Decimal parenthesis lists become `AccelerationDeltas`, one decimal parenthesis value becomes `Delta`, integer parenthesis lists become `TicksPerStep`, `Minimum>Maximum` becomes `Range`, remaining numbers become `StepValues`, and anonymous RGB or `Track` blocks become `StateColors`. Every legacy `#RRGGBBAA` device color becomes `#RRGGBB` because existing hardware feedback and OSK behavior ignore the final alpha byte. If one old group is ambiguous or contains a combination rejected by format 2, preview reports it and leaves that binding unresolved instead of guessing.

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
- `Target=Tracks` with a child `BankTarget` keeps `#` mapped to tracks and applies one shared banked send, receive, or FX index.
- `Target=SelectedTrack` or `MasterTrack` with a child `BankTarget` keeps one track target and maps `#` to consecutive items in that bank.
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
[context selectors]+(input event)+Widget# Action positional-parameters NamedProperty=Value
```

Only the parts required by that binding are present. Examples:

```text
Fader# TrackVolume
[Shift]+Rotary# TrackPan RingStyle=Dot
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
- ✅ Load each zone document once. Expand `#` bindings across the surface, keep bindings without `#` channel-neutral, let zone layers inherit the activating context, and let the runtime activation context provide navigator and target-index data for FX zones.
- ✅ Define binding, `IncludedZones`, reusable `Role=Layer` overlays, `GoZone`, `GoHome`, `EnterZoneLayer`, `ExitZoneLayer`, exact-event fallback, and lifecycle schemas without treating navigation references as structural recursion.
- ✅ Reject free Widget wildcard matching and reserve one terminal `#` for exact numbered channel-family expansion.
- ✅ Specify the valid prefix selector, terminal `#` channel placeholder, and named property list positions. Reject anonymous bracket groups.
- ✅ Confirm the names and value rules for `Range`, `Delta`, `StepValues`, `AccelerationDeltas`, `TicksPerStep`, `StateColors`, `RingStyle`, `RingColors`, and `BarStyle`, and keep raw MIDI encoder decoding in the typed Surface schema.
- ✅ Define case-insensitive per-zone Main/FX overlays, uniqueness scopes, complete duplicate diagnostics, and distinct whole-file Surface, snippet, and Learn FX override rules.
- ✅ Define transactional User-zone rename, collision checks, Vendor-referrer overrides, stale-reference handling, and updates of every typed reference.
- ✅ Define `LearnFX.fxzon`, OSK FX edit mode, widget eligibility, display pairing, generated-binding copy semantics, live draft preview, explicit save, and legacy directive handling.
- ✅ Define the global `RingStyle` type, processor capabilities and defaults, the complete action `FeedbackShape` set, current processor mappings, actions without automatic shape, and explicit binding overrides.
- ✅ Specify the unversioned product `.conf` block schemas, identifiers, required and optional fields, defaults, Product and Device settings scopes, links, diagnostics, and shared semantic model ownership.
- ✅ Specify the Surface document structure, protocol identity, Widget/Input/Feedback blocks, EncoderProfile behavior, MIDI and OSC address rules, OSK layout, color calibration placement, and legacy encoder-range handling.
- ✅ Inventory the currently registered Input and Feedback processors, obsolete encoder input types, property shapes, matching behavior, and capabilities that format 2 must preserve.
- ✅ Define the closed semantic Input and Feedback primitive catalog and separate primitive meaning from reusable protocol Encoding.
- ✅ Define exact reusable Value, Color, Ring, Bar, Meter, and Text profile blocks, references, interpolation, lookup, quantization, and validation rules.
- ✅ Define the basic closed MIDI and OSC Encoding matrix, scalar packing, RGB and palette output, bounded SysEx fields, text characters, echo suppression, meter refresh, and encoder acknowledgement.
- ✅ Define exact composition syntax for Ring value plus configuration output and Surface-level shared FeedbackGroup packets.
- ✅ Review every remaining codec and keep it only when its smallest reusable behavior cannot be expressed safely as declarative data. Do not keep device-model names in the public schema; leave the invalid FPVUMeter output unresolved until hardware behavior is verified.
- ✅ Finalize the universal catalog's exact message matching, single-owner output keys, named properties, and derived capability sets.
- ✅ Confirm ColorCalibration property defaults, ranges, processor-native OutputMax behavior, and compatibility with color-capable Feedback entries.
- ✅ Specify snippets as versioned editor-only zone fragments with no semantic slot wrapper, explicit requirements, application identity, or saved provenance markers.
- ✅ Add representative valid and invalid fixture files for every top-level format under `tools/config-editor/fixtures/format2-spec` before runtime implementation starts.
- ✅ Complete the current golden legacy-input/output scenarios listed in `tools/config-editor/fixtures/format2-spec/golden/MANIFEST.md`. Keep the Phase 4 conversion matrix and manifest synchronized with every later format or action rename.

Ready when the normative specification and fixtures let C++, Bun, Lua, and documentation implement the same grammar without interpretation differences.

## [ ] Phase 3: Universal Surface I/O and one C++ parser model

### [ ] Universal Surface I/O layer

- [ ] Implement universal Surface Input decoders and Feedback primitives, then generate their C++, TypeScript, and Lua catalog from one schema. Reject runtime registrations without matching metadata.
  - ✅ Add the canonical `surface_io_schema.conf`, generate its C++ view, and make the typed Surface parser validate primitive names, protocol/encoding compatibility, required and allowed properties, nested blocks, and derived capabilities from that catalog.
  - ✅ Add catalog-owned value rules for MIDI messages and bytes, OSC addresses, identifiers, booleans, enums, finite numbers, ranges, colors, lists, plus typed validation of `Acknowledge` and `Configure` transport blocks.
  - ✅ Add catalog-owned cross-property constraints for encoder source selection, OSC matching, paired brightness, companion output, and continuous meter refresh. Parse `EncoderProfile` into a typed model and validate `Input Encoder Profile` references.
  - ✅ Parse `ValueProfile` and `ColorProfile` into typed models from catalog-owned profile and repeatable-line schemas. Validate point order, reversible value curves, palette uniqueness, complete hue coverage, reference kind, and input or feedback direction.
  - ✅ Parse `RingProfile`, `BarProfile`, `MeterProfile`, and `TextProfile` into typed models. Validate positional style arguments, unique style codes, mode-specific meter fields, text presentation codes, profile references, and MIDI-only byte or text restrictions.
  - ✅ Parse `ColorCalibration`, TrackColor `FeedbackGroup`, and `OSKLayout` into typed models from Surface-level schemas. Validate Widget and profile references, group channels and membership, derived TrackColor capability, calibration applicability, visible layout uniqueness, and OSK target capabilities.
  - ✅ Add the first format 2 MIDI runtime bridge for the converted FaderPort set: `MIDIExact` Press, Touch, and State; `MIDI14` Value input and feedback; `MIDI7` Encoder; and `MIDIRGB` Color. Construct Widgets only after typed validation and report unsupported primitives instead of sending format 2 source to the legacy parser.
  - ✅ Replace the bridge's FaderPort-specific RGB dispatch with one `MIDIRGB` codec driven by declared Enable, Red, Green, Blue, state-brightness, and ColorCalibration metadata. Resolve accelerated encoder values through the referenced EncoderProfile ID instead of a fixed WidgetClass name.
  - ✅ Add universal `MIDI7` Value input and feedback for one- or two-byte prefixes, ValueBase/Combine output, echo and touch suppression, plus explicit `SignedBit`, `SignedBitFixed`, and `Relative7Bit` Encoder modes.
  - ✅ Add universal `MIDI7` Ring value and RingStyle feedback through `RingProfile`, ValueBase/Combine, StyleTarget, StyleShift, and StyleCombine. Keep nested Ring color Configure for its separate remaining runtime step.
  - ✅ Add `MIDIPrefix` Press runtime behavior for legacy `AnyPress` conversion without a fabricated release message.
  - [ ] Add TypeScript and Lua readers for the same catalog when their format 2 consumers are implemented.
    - ✅ Add the TypeScript schema reader and use its representation lookup to validate declared legacy conversion targets.
    - [ ] Add the Lua reader when Lua begins consuming format 2 Surface primitives directly.
  - [ ] Implement the universal runtime decoders and feedback codecs and reject runtime registrations without catalog metadata.
- [ ] Move device message templates, value curves, display fields, color mappings, ring modes, meter mappings, and reusable SysEx data out of device-named C++ classes and into typed Surface metadata.
- [ ] Implement Ring Configure packet generation and Surface-level TrackColor FeedbackGroup ownership from the declarative Surface model.
- [ ] Implement Bar feedback with separate value and style outputs, plus the bounded MIDIPalette Companion message.
- [ ] Verify that a new device which uses an existing protocol shape needs only a Surface file and no new C++ class.
- [ ] Split processors that generate REAPER-specific content from device encoding. Actions supply values or formatted text; universal Feedback primitives encode and send them.
- [ ] Parse and publish `OSKLayout` through common ControlSurface initialization so MIDI and OSC Surfaces use the same OSK path.
  - ✅ Apply a typed format 2 MIDI OSK layout and ColorCalibration directly to `ControlSurface` without reparsing the Surface file. Keep the parent item open until OSC uses the same path.

### [ ] Shared parser and runtime model

- ✅ Implement one isolated common lexer for UTF-8 BOM, source paths, byte offsets, one-based line and column locations, quoted strings, comments, newlines, bare values, and structural delimiters.
- ✅ Parse the required first `@Meta` block into typed document metadata and validate delimiter pairs, allowed keys, duplicates, values, required Surface protocol, and Role/Target/BankTarget combinations.
- ✅ Build the shared syntax tree for line declarations and nested brace blocks, parse scalar and list properties, and provide one strict Widget selector parser for exact IDs and one terminal `#` channel placeholder.
- ✅ Parse the Surface top-level structure, named profile and group IDs, singleton calibration and OSK blocks, Widget identity and local properties, plus typed Input and Feedback block shells without creating runtime objects.
- ✅ Parse Main zone, FX zone, and snippet bodies into typed bindings, modifier declarations, structural references, and lifecycle actions. Validate binding expression structure, selector combinations, relation entries, modifier declarations, FX matching metadata, and zone-layer restrictions without creating runtime objects.
- ✅ Parse `LearnFX.fxzon` into typed FX widget roles, selectors, default properties, generated bindings, and lifecycle actions without lexing generated content again. Validate required and singleton blocks, roles, direct selector duplicates, filename identity, and generated-body restrictions before Surface resolution.
- ✅ Build document-specific semantic parsers for Surface, Main zone, FX zone, Learn FX, and snippet bodies on the shared syntax tree.
- ✅ Parse each zone, surface, Learn FX, and snippet source once through one typed-document entry point that preserves the shared syntax document, diagnostics, and source locations.
- ✅ Compile each `#` binding into channel-specific action-context specifications that reference the original binding by index, while each channel-neutral binding produces one specification and the containing typed zone is never cloned.
- ✅ Resolve Vendor and User Main/FX sources by case-insensitive zone ID into one deterministic per-zone active set before later role, reference, dependency, or runtime validation. A unique User source overrides only the matching Vendor source, an invalid User source blocks Vendor fallback, and same-layer duplicates leave that ID unavailable with source-linked diagnostics.
- ✅ Parse the new product `.conf` into `IntegratorConfig` and let every C++ consumer use that one semantic model.
  - ✅ Define unquoted case-insensitive `Page`, `Device`, and Page-local `Surface` identifiers with source spelling preserved for display.
  - ✅ Add a brace-format C++ source parser that uses the shared lexer, syntax tree, and delimiter validator and returns the existing `IntegratorConfig` model.
  - ✅ Add one canonical C++ brace-format serializer for Product Settings, MIDI and OSC Devices, Device Settings, Page Surfaces, and Links.
  - ✅ Change the generated product filename to `.conf` and switch runtime, Settings protocol, Devices protocol, and the Lua Devices writer to the brace parser and serializer.
  - ✅ Replace native-dialog parsing and writing, switch the Bun product-config consumer, and remove the legacy line parser.
- ✅ Change generated setting scope metadata and effective-value resolution from `Surface` to `Device`. Reject settings inside Page Surface assignments.
  - ✅ Change the canonical generated setting scopes to `Product,Device`, resolve each valid Device block as compiled defaults, Product, then Device, and let an invalid Device Settings block inherit Product values.
  - ✅ Reject nested Settings blocks in Page Surface assignments through the typed Page child schema.
  - ✅ Pass the referenced Device settings through the existing runtime Surface construction model and resolve Page Surface I/O through its explicit `Device` ID.
  - ✅ Switch the Settings protocol and Lua settings UI from Page Surface selection to Device selection.
  - ✅ Switch the Bun product-config consumer from Surface settings to Device settings.
- [ ] Expose ring feedback capabilities and resolved action feedback shapes through runtime and generated editor metadata.
- [ ] Replace the native Learn FX dialog with OSK FX edit mode, one in-memory live-preview draft, and atomic User FX-zone save through the shared validated model.
- [ ] Validate the complete active profile before runtime objects are created.
- [ ] Let an invalid non-required zone be skipped with focused diagnostics instead of disabling unrelated zones.
- [ ] Treat an invalid or missing Home role as a profile-level initialization error.
- [ ] Replace the metadata preprocessor, binding parser, surface block readers, Learn FX line readers, and OSK line edits with the shared model where they overlap.

Ready when runtime behavior consumes validated documents and no feature reparses the same zone with a different grammar.

## [ ] Phase 4: Bun editor and migration

Migration is a required part of every format 2 decision, not a later best-effort cleanup. A Phase 2 decision is ready for implementation only when this plan also states its legacy input, format 2 output, ambiguity behavior, and fixture requirement. Future syntax or action renames must add or update a row in the conversion matrix below.

Public legacy Surface conversion has one completion gate:

- [ ] Every processor used by `CSI/Surfaces/*/Surface.txt` is classified. Each supported processor must preserve every required address, byte, channel, profile, curve, display field, color, ring, meter, SysEx field, and meaningful option. Each unsupported or ambiguous processor must produce a blocking diagnostic at its source line. The importer must never silently omit a processor or one of its parameters.
  - ✅ Add a TypeScript reader for the canonical `surface_io_schema.conf` primitive and representation catalog.
  - ✅ Add `bun run surface-coverage` to inventory every processor occurrence inside legacy Widget blocks in the public Surface files, ignore OSK layout entries, report malformed Widget boundaries separately, and verify that each declared conversion target exists in the canonical catalog. Distinguish processors that wait for an approved runtime from unknown processor types.
  - ✅ Classify generic legacy OSC `Control` and `FB_Processor` as planned until the format 2 OSC runtime and explicit value-type conversion are implemented; do not report them as supported before then.
  - ✅ Convert legacy OSC `Control` to OSCFloat Input Value. Split one generic `FB_Processor` into typed OSCFloat Value, OSCString Text, and OSCString HexRGBA Color feedback, with the legacy `/Color` suffix written explicitly into the converted Color Address. Load these primitives through the format 2 OSC runtime without passing the document through the legacy parser.
  - [ ] Classify every currently unresolved inventory entry as a universal conversion, intentionally unsupported legacy behavior, or an ambiguity that requires user input.
  - [ ] Add parameter-complete golden fixtures for every supported processor family and malformed or ambiguous branch.
  - [ ] Make zero unclassified processors and zero invalid conversion targets a required migration verification result.

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
| Deprecated `GoZones.zon` | Dropped after its selected zone IDs and recognized navigator names have been processed; it does not become a format 2 block or relation |
| `TrackNavigator`, `SelectedTrackNavigator`, `MasterTrackNavigator`, or `FocusedFXNavigator` beside a zone ID in `GoZones.zon` | `Target=Tracks`, `SelectedTrack`, `MasterTrack`, or `FocusedFX` on the matching Main zone |
| Supported short legacy `NavType=Track|SelectedTrack|MasterTrack|FocusedFX` | The matching public format 2 `Target` on that Main zone |
| Exact standalone navigator-name line inside a zone body | Dropped with a notice because it did not select the legacy zone navigator |
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
| `Widget|` | `Widget#` |
| Decimal parenthesis list inside an anonymous action value group | `AccelerationDeltas=[...]` |
| One decimal parenthesis value inside an anonymous action value group | `Delta=...` |
| Integer parenthesis list inside an anonymous action value group | `TicksPerStep=[...]` when the binding also has discrete step values |
| `Minimum>Maximum` inside an anonymous action value group | `Range=[Minimum, Maximum]` with normalized ascending bounds |
| Remaining numeric values inside an anonymous action value group | `StepValues=[...]` in original order |
| Anonymous RGB, hexadecimal, or `Track` color block | `StateColors=[ ... ]` |
| Legacy `StepSize` and `AccelerationValues` entries for one WidgetClass | One local `EncoderProfile` with `Delta`, `Increase`, `Decrease`, and optional `AccelerationDeltas` |
| Legacy WidgetClass on an encoder Widget | `Encoding=MIDI7 Profile=ProfileId` on its typed `Input Encoder` block; the runtime class name is removed |
| Exact inline encoder range `[ > 01-3f < 41-7f ]` without a WidgetClass | `Encoding=MIDI7 Mode=SignedBit` |
| Inline encoder range on a Widget that already uses a WidgetClass | Removed with an import notice because current runtime ignores it and uses the class lookup table |
| Other inline encoder direction range | Unresolved import diagnostic; do not infer behavior that the current runtime did not implement |
| `Widget Name ... WidgetEnd` and positional hardware processor lines | `Widget Name { ... }` with catalog-driven `Input Type { ... }` and `Feedback Type { ... }` blocks whose message bytes or OSC address use named properties |
| Legacy processor with an explicit zero-based channel argument | Explicit one-based `Channel=N+1` in that Widget; an incompatible second processor channel on the same Widget is unresolved |
| Legacy processor that obtains its channel only from the Widget numeric suffix | Explicit `Channel=N` in that Widget; unresolved diagnostic when the processor requires a channel and the Widget ID has no unambiguous numeric suffix |
| Device-named legacy Input or `FB_*` processor | Universal Input or Feedback primitive plus declarative message, value conversion, display, color, ring, meter, or SysEx metadata; unresolved diagnostic when no approved primitive or reusable codec can preserve the behavior |
| Legacy Ring processor with normal value output and separate color SysEx | One Feedback Ring with RingProfile Segments and nested Configure; existing zone segment colors become RingColors |
| SCE24 `LEDRingColor`, `LEDRingColors`, and `PushColor` | One explicit 18-entry `RingColors` list: physical positions `0..2` contain the push color or default, and positions `3..17` contain the uniform or expanded ring colors |
| `FB_FaderportValueBar` and legacy `BarStyle` | Feedback Bar with one shared BarProfile, explicit value Message and StyleMessage; `BiPolar` becomes `Bipolar` |
| `FB_MFT_RGB` normal palette output | Feedback Color with MIDIPalette, a generated ColorProfile, and the constant mode Companion message |
| `FB_MFT_RGB` RGB value used as an arbitrary MIDI command | Unresolved import diagnostic; commands are not preserved as colors |
| Legacy TextAlign and TextInvert display properties | `TextAlign=Left|Center|Right` and Boolean `TextInvert`; the generated TextProfile contains the device codes |
| Legacy `DisplayText`, margin, font, text-color, and background-color properties | `FixedText`, `TopMargin`, `BottomMargin`, `Font`, `TextColor`, `BackgroundColor`, `TextColors`, and `BackgroundColors`; processor defaults move to the Text Feedback block; On/Off pairs become state-indexed lists in Off, On order |
| Repeated `ScribbleStripMode Mode=N` with one common N | One Feedback Value `InitialValue` in the Surface; repeated zone lines are removed |
| `ScribbleStripMode` with different modes in different zones | Unresolved import diagnostic; format 2 does not add per-zone hardware configuration syntax without a real bundled use case |
| Legacy XTouch display track-color processors that assemble one shared packet | Text feedback on each display Widget plus one Surface-level TrackColor FeedbackGroup with explicit Channel, Slot Source, and Members references |
| Legacy `OSKLayout Version=1`, Row blocks, and bare hexadecimal colors | Versionless `OSKLayout { Row { ... } }` inside the format 2 Surface for either protocol; colors gain the required `#` prefix |
| Legacy `ColorCalibration ... ColorCalibrationEnd` | Typed `ColorCalibration { ... }` block |
| `Snippet Version=1`, semantic Binding blocks, Role/Input/Feedback/Required fields, and nested Action lines | Format 2 `.snippet` with `@Meta` and direct zone binding lines; each old Binding ID becomes its source widget mapping name, `NoMod` is removed, modifier names become selectors, and inferred requirement fields are dropped |
| Saved `// @snippet` and `// @snippet-end` application markers | Marker comments are removed while their resolved zone statements remain ordinary destination-zone content |
| `BlockName ... BlockNameEnd` | `BlockName { ... }` |
| Hash-prefixed Learn FX directives and Learn FX pseudo-zones | `LearnFX.fxzon` plus normal generated FX bindings; `FXRowLayout` is dropped |
| Legacy single-slash comment lines | `//` comments when the line is recognized as a legacy comment; OSC address tokens remain data |
| Any renamed action | New action name and transformed parameters from the Bun-only action rename registry |

Action renames live in one declarative Bun-only registry separate from the general importer. Each entry identifies the legacy action, the new action, and any parameter transformation or context restriction. Runtime C++ does not keep old action aliases. The importer validates that every registry destination exists in the current generated action catalog.

Every conversion-matrix row requires at least one golden legacy-input and format-2-output fixture. Context-sensitive conversions require fixtures for every branch and for ambiguous input. Ambiguous input produces a focused diagnostic and remains unchanged until the user resolves it. The import preview lists every renamed action and structural conversion before files are written.

`GoZones.zon` is legacy loading metadata, not zone composition. A listed zone without a navigator imports normally and receives no Target from the manifest. A recognized long navigator maps to Target only when it agrees with the zone's accepted legacy `NavType` and magic-name behavior. A conflicting declaration is an import error linked to both locations. A magic name keeps its actual legacy runtime behavior when a conflicting GoZones navigator was ignored by the old branch order, and the preview reports that removal. `FixedTrackNavigator`, an unknown navigator, and a listed zone without a selected matching file remain unresolved. Exact standalone navigator-name lines are removed with a notice and never create Target metadata because the old zone-body parser did not use them.

MFT color migration requires the selected Surface and zone set. The importer converts the fixed legacy 128-entry color table to one shared nearest-match ColorProfile. Each `FB_MFT_RGB` output becomes MIDIPalette with its original two-byte destination and an After Companion whose status is the original status plus one, whose data byte is unchanged, and whose value is `0x2F`. A zone RGB value is command-shaped only when its resolved destination Widget uses `FB_MFT_RGB`, its red value is decimal `177` or `181`, and its green value is decimal `31`. That binding remains unresolved and the diagnostic shows the exact raw MIDI command. The same RGB value on another feedback type remains normal color data. One unresolved command-shaped binding does not block independently valid Surface or zone outputs.

SCE24 ring migration intentionally repairs the ineffective legacy `PushColor` path. The old processor reserves 18 configurable bits: positions `0..2` are the push LEDs and positions `3..17` are the 15-value ring. The old runtime checked `PushColor` only when an internal `Push` property existed, but no current parser or action supplied that property. Format 2 does not retain this dead branch. The importer combines `PushColor` with `LEDRingColor` on the same binding into one explicit 18-entry RingColors list. It expands each inclusive `LEDRingColors` range into the same list and fills unspecified positions from the RingProfile DefaultColor. Overlapping ranges, positions outside `0..17`, malformed colors, and different competing lists on one ordered multi-action binding are unresolved errors. The generated Surface uses an 18-segment RingProfile with 15 value steps, eight Spread steps, and one nested Configure packet schema. Runtime code then needs no SCE24 name branch.

XTouch display migration uses the complete selected Surface. Every recognized XTouch, XTouchXT, MCU, or MCUXT display processor becomes independent Text feedback with a one-based Channel from its explicit legacy argument. One XTouch track-color processor identifies the shared color-packet family and display type. The importer groups the complete numbered upper and lower display families by Channel, requires one unambiguous upper Source per slot, and emits one FeedbackGroup whose Members contain every paired display on that Channel. Its HueRanges ColorProfile preserves the current hue sectors and neutral thresholds. The group uses SourceTextPresent and white EmptyColor, so an empty upper display keeps that slot white. Legacy anonymous `{ Track }` colors on member bindings are removed with a notice because the group supplies track color. Missing channels, duplicate row members, incomplete families, conflicting display types, and more than one possible Source remain unresolved instead of creating competing packet owners.

If one legacy file is referenced both as a SubZone and as an independent zone, the importer cannot silently assign one format 2 `Role`. The preview reports both reference locations and asks the user to keep one role or create a renamed copy for one use. A file referenced as a SubZone by several parents is not ambiguous and converts to one reusable `Role=Layer` document.

- [ ] Add lossless format 2 parsing, validation, syntax highlighting, quick fixes, and cross-file references.
- [ ] Replace semantic snippet slots, explicit capability fields, application IDs, conflict actions, and saved marker comments with direct zone-fragment parsing and token-aware widget plus modifier mapping. Derive compatibility from the normal action, binding, and Surface catalogs, and insert only into the unsaved destination draft.
- [ ] Create the declarative Bun-only action rename registry and validate its destinations against the current action catalog.
- [ ] Keep the conversion matrix and golden fixture pairs synchronized with every later format or action rename.
- [ ] Show a Zone Layer badge, all current parent references, and context-valid navigation actions in the editor. Offer `ExitZoneLayer` only for zone layers.
- [ ] Add safe User-zone rename with complete-profile reference updates, case-only filesystem handling, hash checks, and focused User overrides for Vendor referrers.
- [ ] Replace whole-profile Main cloning with `Create User override` for one zone. Show a clear override confirmation when import or copy selects an existing Vendor ID.
- [ ] Convert old `Zone ... ZoneEnd`, `BlockName ... BlockNameEnd`, and surface blocks during legacy import.
- ✅ Convert legacy `FB_Encoder` output to one reusable `RotaryRing` profile and explicit `Feedback Ring`, including the historical output-address offset and style bits.
- [ ] Convert legacy `Widget|` channel placeholders to `Widget#` and include exact, missing-family, and rejected-wildcard golden fixtures.
- [ ] Convert every legacy anonymous zone value group to `Range`, `Delta`, `StepValues`, `AccelerationDeltas`, and `TicksPerStep` according to the conversion matrix.
- [ ] Convert legacy Surface Widget blocks through the completed universal Input and Feedback catalog. Move recognized device-specific messages, curves, display fields, colors, rings, meters, and SysEx values into the new metadata. Convert WidgetClass, `StepSize`, and `AccelerationValues` to EncoderProfile references. Convert the exact unclassified standard signed-bit range, remove ignored redundant ranges with a notice, and report other ranges as unresolved.
  - ✅ Route legacy Surface preview and import through a separate format 2 converter. Cover the FaderPortV2 processor set (`Press`, `Touch`, `Fader14Bit`, `FB_Fader14Bit`, `Encoder`, `FB_TwoState`, and `FB_FaderportRGB`), encoder profiles, color calibration, explicit legacy OSK rows, and generated fader-aware OSK layout. Keep the parent item open until every catalog processor and protocol conversion is implemented.
  - ✅ Convert `AnyPress` to its real two-byte `MIDIPrefix` behavior, preserve an ordinary `Press` without an `Off` message, and convert `Fader7Bit` plus `FB_Fader7Bit` to universal MIDI7 Value primitives.
  - ✅ Convert generic OSC `Control` and `FB_Processor` without guessing from Widget names or zones. Preserve their number, text, and legacy HexRGBA color transports as separate typed primitives.
- [ ] Add explicit Channel metadata from the legacy processor channel argument when present, otherwise from the Widget numeric suffix only for processors that currently depend on it. Convert supported ring color-configuration output to nested Configure and supported shared XTouch track-color output to FeedbackGroup. Report conflicting, missing, or ambiguous channel and group membership instead of inferring it at runtime.
- [ ] Convert FaderPort value bars to Feedback Bar and MIDI Fighter Twister palette output to MIDIPalette with Companion. Report legacy command-shaped MFT color values as unresolved.
- [ ] Convert legacy TextAlign and TextInvert to typed Text properties. Collapse identical repeated FaderPort scribble-strip modes into one Surface InitialValue and report differing per-zone modes as unresolved.
- [ ] Convert fixed display text, margin, font, and constant or state-indexed display colors to the typed Text feedback properties.
- [ ] Convert every legacy `OSKLayout Version=1` and ColorCalibration block to its format 2 Surface block regardless of protocol. Normalize OSK layout colors to opaque `#RRGGBB` and remove any ignored legacy alpha byte. When no explicit layout exists, generate an initial OSK layout from usable Input widgets. Treat a fader as a seven-row cell, place one or more faders in stable columns, fill remaining cells with buttons and rotaries in source order, combine separately declared push or touch targets when unambiguous, and keep the result editable in the import draft.
- ✅ Move the surface-channel count from product Device blocks to required Surface `@Meta Channels=N`. Derive legacy imports from the numbered widget families with a fallback of one, remove the field from I/O forms, and reject reuse of one Device with conflicting Surface channel counts.
- [ ] Convert legacy anonymous RGB groups to `StateColors` hexadecimal lists. Remove the ignored final alpha byte from every legacy device, action, text, ring, and layout color.
- [ ] Convert name-based navigator behavior into public `Role`, `Target`, and `BankTarget` metadata.
- [ ] Remove exact standalone legacy navigator-name lines from zone bodies and report other unknown lines.
- [ ] Convert Learn FX pseudo-zones into `LearnFX.fxzon`, derive supported entry defaults, report ambiguous display/default targets, and do not convert `FXRowLayout`.
- [ ] Convert brace-based `IncludedZones`, convert `SubZones` to the reusable `ZoneLayers` relation, and mark every referenced file with `Role=Layer`.
- [ ] Convert `GoZone SelectedTrackFX`, `GoSubZone`, `LeaveSubZone`, and all lifecycle pseudo-widgets to their explicit format 2 actions or blocks.
- ✅ Replace development product-config parsing, serialization, diagnostics, fixtures, drafts, and editor support with the new unversioned `.conf` model and Product plus Device settings scopes.
- [ ] Convert old `CSI.ini` only through the Bun legacy importer and do not add runtime fallback.
- [ ] Report ambiguous legacy content before import and keep the source unchanged.

Ready when old public CSI examples can be imported into format 2 without runtime legacy parsing.

## [ ] Phase 5: Bundled data and runtime cutover

- [ ] Convert all files under `resources/Zones`, `resources/Surfaces`, `resources/Snippets`, and related Learn FX data.
- [ ] Change `PRODUCT_CONFIG_FILENAME` to the selected `.conf` filename and update every generated consumer and package path.
- [ ] Remove duplicated ring-style lists from Learn FX data and remove repeated standard `RingStyle` properties where action metadata produces the same result.
- [ ] Remove duplicate bundled zone IDs and verify User and Vendor layer behavior.
- [ ] Add or complete OSK layouts for bundled hardware OSC templates where a desktop mirror is useful, starting with Behringer X32. Keep layouts optional for OSC tablet applications.
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
