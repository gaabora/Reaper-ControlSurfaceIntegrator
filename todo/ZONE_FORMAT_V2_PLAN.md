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
- `IncludedZones` does not explain that referenced independent zones remain active at the same time as their parent.
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
@Meta { Version=2 Role=Home Navigator=SelectedTrack }

Play Play
Control+ButtonB1 GoZone SelectedTrack
```

The formatter can expand a long metadata block:

```text
@Meta {
  Version=2
  Role=Home
  Navigator=SelectedTrack
}
```

Structural blocks use matched braces:

```text
SubZones {
  Paning
  LinkLock
}
```

The same brace rule applies to the new surface format:

```text
Widget Fader {
  Fader14Bit e0 7f 7f
  FB_Fader14Bit e0 7f 7f
  Touch 90 68 7f 90 68 00
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
@Meta { Version=2 MatchFX="VST3: ReaEQ (Cockos)" Alias="ReaEQ" Navigator=FocusedFX }

Fader FXParam 0
```

Normal zones omit `Role`. Exact role, navigator, instance, banking, and mode properties must come from the C++ behavior inventory. A file name must not select these behaviors.

### Multi-channel widget references

Legacy `Fader|` is not a wildcard. C++ replaces `|` with the suffix of the current zone instance. In a channel zone, separate instances therefore bind `Fader1`, `Fader2`, and later widgets to their matching track navigators.

Do not replace this syntax with `Fader*` without changing its meaning. A true `Fader*` wildcard can match several widget families and can make every matched widget bind to every channel instance. For example, `Rotary*` can also match `RotaryPush1`.

The recommended explicit replacement is a channel placeholder:

```text
@Meta { Version=2 Navigator=Track Instances=SurfaceChannels }

Fader[Channel] TrackVolume
Rotary[Channel] TrackPan
RotaryPush[Channel] TrackVolume StepValues=[0.716]
ButtonA[Channel] TrackSolo
```

`[Channel]` means the channel number of the current zone instance. It is not a glob. Actual wildcard matching remains available in selectors where matching several widgets is intended.

Square brackets remain unambiguous because their position defines their role:

- `Fader[Channel]` is a postfix channel index on a widget family.
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
RotaryPush[Channel] TrackPan StepValues=[0.5]
RotaryC[Channel] TrackVolume AcceleratedDeltas=[0.0002, 0.001, 0.005, 0.01, 0.05]
RotaryPushP1 FXParam 21 StepValues=[0, 1]
```

A complete generated FX parameter can declare each value type independently:

```text
Rotary1 FXParam 0 Range=[0.0, 1.0] Delta=0.025 AcceleratedDeltas=[0.025, 0.05, 0.1] AccelerationTicks=[0] StepValues=[0.0, 0.5, 1.0]
```

The surface encoder protocol also replaces its anonymous direction block:

```text
Encoder b0 12 7f Increment=01-0F Decrement=41-4F
```

The importer must decode the legacy value types. It must not copy one old anonymous bracket group into one new generic list.

### Learn FX through OSK

The native Learn FX window is replaced by an OSK FX edit mode. OSK already knows the real surface layout and real widget names, so format 2 removes `FXRowLayout` instead of converting its modifier and suffix matrix.

The runtime supplies all available modifiers and their valid combinations. The Learn FX profile does not enumerate them.

One dedicated `LearnFX.fxzon` file belongs to each zone profile. It contains the editable FX widget whitelist and bindings added to generated FX zones:

```text
@Meta { Version=1 }

FXWidgets {
  Parameter Fader*
  Parameter RotaryBig*
  Parameter RotaryBigPush*
  NameDisplay DisplayUpper*
  ValueDisplay DisplayLower*
}

GeneratedBindings {
  OnZoneActivation ToggleUseLocalModifiers
  OnZoneDeactivation ToggleUseLocalModifiers
  Plugin ClearFXSlot
  OnZoneDeactivation HideFXSlot
}
```

Wildcards in `FXWidgets` are true glob selectors. They select every matching real surface widget. They do not represent a current channel instance. Ring styles and other feedback capabilities come from the matched widget feedback processor instead of a repeated profile list.

FX edit mode works as follows:

1. The user focuses an FX and opens OSK FX edit mode.
2. Widgets selected by `FXWidgets.Parameter` entries are editable.
3. Modifier buttons remain usable as mode controls but are not FX assignment targets.
4. Widgets outside the FX whitelist are disabled and shown with a muted style.
5. A physical or OSK widget click opens the FX version of the widget editor.
6. The editor can assign or remove a focused FX parameter and edit supported range, step, ring, color, name display, and value display properties.
7. Changes update the unsaved draft of a normal `.zon` FX file with `MatchFX` metadata.
8. The user explicitly saves the FX zone.

During legacy import:

- `#WidgetType` entries become `FXWidgets.Parameter` selectors.
- `#DisplayRow` entries become name or value display selectors according to their old binding role.
- `#RingStyle`, `#DisplayFont`, and `#SupportsColor` become validated capabilities or options without hash-prefixed directives.
- `FXPrologue` and `FXEpilogue` active bindings are merged into `GeneratedBindings` while preserving their relative order.
- `FXRowLayout` is not imported because OSK uses real widgets and runtime modifier combinations.
- Conflicts between generated parameter bindings and `GeneratedBindings` are reported instead of preserving hidden before/after ordering.

Learn FX does not reference functional snippets. The small profile-specific generated bindings remain directly visible in `LearnFX.fxzon`.

### Ring style behavior

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

The initial semantic shapes are:

| Feedback shape | Preferred ring result |
|---|---|
| `Level` | `Fill` |
| `Centered` | `BoostCut`, with `Dot` as a supported fallback |
| `Spread` | `Spread`, with the processor-defined fallback |
| `Position` | `Dot` |

For example, `TrackVolume` declares `FeedbackShape=Level`, `TrackPan` declares `FeedbackShape=Centered`, and `TrackPanWidth` declares `FeedbackShape=Spread`. The processor maps the semantic shape to a style that the target widget supports.

Ring style resolution uses this precedence:

1. Explicit `RingStyle=...` on the binding.
2. The action `FeedbackShape` mapped through the widget processor capabilities.
3. The widget processor default.

Normal zones therefore do not repeat standard ring styles:

```text
Rotary[Channel] TrackVolume
[Shift]+Rotary[Channel] TrackPan
[Option]+Rotary[Channel] TrackPanWidth
```

An explicit override remains available:

```text
[Shift]+Rotary[Channel] TrackPan RingStyle=Dot
```

`FXParam` has no reliable global semantic shape because plugin parameters do not expose one common meaning. OSK uses the widget processor default, lets the user select a supported style, and stores a non-default choice as an explicit FX-zone binding property.

### Simultaneous zones, subzones, and snippets

`IncludedZones` is renamed to `ActiveZones`:

```text
@Meta { Version=2 Role=Home Navigator=SelectedTrack }

ActiveZones {
  Track
  MasterTrack
}
```

`ActiveZones` means that each referenced independent zone remains active at the same time as the current zone. Each referenced zone keeps its own explicit navigator and instance rules. For example, `Home` can own global transport bindings while `Track` creates one channel instance for every surface channel.

A functional snippet is different. The editor inserts snippet bindings into the current zone draft, and those bindings use the current zone context. A snippet cannot replace a multi-instance `Track` zone.

Legacy `IncludedZones` sections are converted directly to `ActiveZones`. The importer does not try to classify or flatten them into snippets.

`SubZones` remains a declared parent-child relation:

```text
SubZones {
  Paning
  LinkLock
}
```

Subzones inherit the parent navigator and slot. An active subzone temporarily has priority over its parent. `GoSubZone` remains only the navigation action that activates a declared subzone, and `LeaveSubZone` returns to the parent.

### New product configuration format

The current `ReaControlSurface.ini` grammar is a renamed continuation of `CSI.ini`. Format 2 replaces it instead of carrying its `Version=7.0`, order-dependent page records, and `Broadcaster` state into the new product.

The canonical product identity changes `PRODUCT_CONFIG_FILENAME` to a `.conf` filename because the new document is not INI. The new file lives at the existing product configuration root. Old `CSI.ini` files remain read-only sources for the Bun importer.

The new product configuration has no embedded `Version` property. The product-owned path and the current parser identify its contract. The app is not released, so runtime backward compatibility with the current development file is not required.

The configuration uses the shared brace lexer and a product-specific typed model:

```text
Settings {
  DefaultModifierMode=Latch
  HoldDelayMs=1000
  LongHoldDelayMs=2000
}

Device fp2 {
  Type=MIDI
  Channels=1
  Input=0
  Output=0
  RefreshRate=15
  MaxMessagesPerRun=200
}

Page Home {
  FollowsMCP=No
  SyncPages=No
  ScrollLink=No
  ScrollSync=No

  Surface fp2 {
    Template=faderportv2
    Zones=faderportv2
    FXZones=faderportv2
    StartChannel=0

    Settings {
      HoldDelayMs=750
    }
  }

  Link {
    Broadcaster=fp2
    Listener=xtouch
    Share=[GoHome, Modifiers, FXMenu]
  }
}
```

`Device` defines one MIDI or OSC endpoint. `Page.Surface` assigns that device to a surface template and zone profiles. Nested `Settings` overrides product settings for that assignment. `Link` replaces the order-dependent `Broadcaster` and following `Listener` records with one complete relationship.

All C++ consumers use one parsed `IntegratorConfig` model. The native configuration dialog must not open and interpret the same file a second time. The Bun editor and C++ must use the same field names, nesting, defaults, and validation rules.

## Remaining design decisions

- [ ] Confirm the exact `Role`, `Navigator`, `Instances`, banking, mode, and FX context values from the complete C++ name-branch inventory.
- ✅ Use `[Channel]` as the postfix current-instance channel index and reject bare anonymous square-bracket groups in format 2.
- [ ] Confirm the exact property names for `StepValues`, `Range`, `Delta`, `AcceleratedDeltas`, `AccelerationTicks`, `Increment`, and `Decrement`.
- [ ] Define wildcard case handling, escaping, match ordering, and the diagnostic for a selector that matches no widgets.
- [ ] Confirm whether `LearnFX.fxzon` belongs under the zone profile root or a dedicated profile metadata directory. It must not be embedded in one surface file because a zone profile can be selected independently.
- [ ] Confirm the complete global `FeedbackShape` set and the fallback mapping for every ring feedback processor.

## ✅ Phase 1: Runtime behavior inventory

The replacement column below names the required semantic field. Phase 2 confirms its exact spelling and grammar. It must not collapse every special string into `Role`.

### Zone construction and lifecycle branches

| Current name or construct | Current hidden behavior | Required explicit replacement | Source |
|---|---|---|---|
| `Home` | Required single zone, uses the selected-track navigator, activates after initialization, and remains the fallback after other zones | `Role=Home`, selected-track navigator, one instance, required profile entry | [ZoneManager::Initialize()](../src/controls/zone_manager.cpp) |
| `LastTouchedFXParam` zone | Optional single high-priority mapping loaded during initialization with the focused-FX navigator | Dedicated last-touched-FX role, focused-FX navigator, one instance | [ZoneManager::Initialize()](../src/controls/zone_manager.cpp), [zone_parser.cpp](../src/controls/zone_parser.cpp) |
| Legacy `NavType` | Preprocessing accepts `Track`, `FixedTrack`, `MasterTrack`, `SelectedTrack`, and `FocusedFX`, but zone construction still compares stale `TrackNavigator`, `MasterTrackNavigator`, and `FocusedFXNavigator` strings. `FixedTrack` also has no construction path here | Store a typed navigator value and switch on it. Define the required fixed-track selector or reject `FixedTrack` explicitly | [NavigatorType](../src/shared/types.h), [ZoneManager::PreProcessZoneFile() and GetNavigatorsForZone()](../src/controls/zone_manager.cpp) |
| `MasterTrack` | Selects the master-track navigator and one instance | Master-track navigator, one instance | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp) |
| `Track` | Creates one track-navigator instance per surface channel and selects the normal track bank | Track navigator, surface-channel instances, normal track bank | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Page::AdjustBank()](../src/controls/page.h) |
| `VCA`, `Folder`, `SelectedTracks` | Create track-navigator instances, select a special track-list mode while active, and select the matching bank | Track navigator, surface-channel instances, explicit track mode and bank | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::Activate()](../src/controls/zone.cpp), [Page::AdjustBank()](../src/controls/page.h) |
| `TrackSend`, `TrackReceive`, `TrackFXMenu` | Create track-navigator instances and derive each zone slot from a separate bank offset | Track navigator, surface-channel instances, explicit bank and slot source | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::GetSlotIndex()](../src/controls/zone.cpp), [ZoneManager::AdjustBank()](../src/controls/zone_manager.h) |
| `SelectedTrack` | Creates one selected-track instance per surface channel, uses the instance slot, selects the selected-track bank, and deactivates on track deselection | Selected-track navigator, surface-channel instances, instance slot, selected-track bank, selected-track lifetime | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::GetSlotIndex()](../src/controls/zone.cpp), [ZoneManager::OnTrackDeselection()](../src/controls/zone_manager.h) |
| `SelectedTrackSend`, `SelectedTrackReceive`, `SelectedTrackFXMenu` | Create selected-track instances, add a dedicated bank offset to the instance slot, and deactivate on track deselection | Selected-track navigator, surface-channel instances, explicit bank and slot source, selected-track lifetime | [zone_manager.cpp](../src/controls/zone_manager.cpp), [zone.cpp](../src/controls/zone.cpp), [zone_manager.h](../src/controls/zone_manager.h) |
| `MasterTrackFXMenu` | Creates one master-track instance per surface channel and derives each slot from the master FX-menu offset | Master-track navigator, surface-channel instances, master FX-menu bank and slot source | [ZoneManager::GetNavigatorsForZone()](../src/controls/zone_manager.cpp), [Zone::GetSlotIndex()](../src/controls/zone.cpp) |
| `SelectedTrackFX` target | Normal `GoZone` activation also creates and activates matching FX zones for every FX on the selected track | Explicit selected-track FX-chain activation behavior on the target zone | [ZoneManager::GoZone()](../src/controls/page.h), [ZoneManager::GoSelectedTrackFX()](../src/controls/zone_manager.cpp) |
| FX plugin title used as the zone map key | Focused, selected-track, and slot FX loading finds a zone by the current REAPER plugin title | Build a separate `MatchFX` index; the zone ID remains the filename stem | [zone_manager.cpp](../src/controls/zone_manager.cpp) |

### Navigation, propagation, and internal command branches

| Current name or construct | Current hidden behavior | Required explicit replacement | Source |
|---|---|---|---|
| `GoZone Folder`, `VCA`, `TrackSend`, `TrackReceive`, `TrackFXMenu`, or `MasterTrackFXMenu` | Routes activation through the Page and all its surfaces; other targets use the local surface or configured listeners | Target zone declares page or surface navigation scope; `GoZone` resolves the target and does not compare its ID | [GoZone::Do()](../src/actions/manager_actions.h) |
| `SelectedTrackSend`, `SelectedTrackReceive`, `SelectedTrackFX`, `SelectedTrackFXMenu` listener targets | Select one listener category by target name | Target zone declares its listener event category | [ZoneManager::ListenToGoZone()](../src/controls/zone_manager.h) |
| `TrackFXMenu`, `SelectedTrackFXMenu` | Reactivate the active FX-menu zone after an FX-slot mapping closes | Explicit FX-menu context or return target | [ZoneManager::ReactivateFXMenuZone()](../src/controls/zone_manager.h) |
| `LastTouchedFXParam`, `FocusedFX`, `SelectedTrackFX`, `FXSlot` passed by clear actions | Select one hard-coded clear operation | Keep separate typed clear actions or a typed FX context enum; these strings are commands, not zone roles | [ZoneManager::DeclareClearFXZone()](../src/controls/zone_manager.h), [manager_actions.h](../src/actions/manager_actions.h) |
| `GoZones` | Uses a deprecated file as the top-level zone list | Remove the runtime path; the importer reads it and emits normal format 2 references | [ZoneManager::Initialize()](../src/controls/zone_manager.cpp) |
| `IncludedZones`, `SubZones` | Load simultaneously active child zones or declared subzones | `ActiveZones` remains composition; `SubZones` remains the parent-child relation | [zone_parser.cpp](../src/controls/zone_parser.cpp), [zone.cpp](../src/controls/zone.cpp) |
| `GoZone`, `GoSubZone` action names | Mark their string parameter as a reference when the parser recognizes the action name | Action metadata declares a typed zone-reference parameter; dependency extraction does not compare action text | [zone_parser.cpp](../src/controls/zone_parser.cpp) |
| `OnZoneActivation`, `OnZoneDeactivation` widget names | Run bindings during zone lifecycle events | Explicit lifecycle event selectors in the binding grammar, not physical widget names | [Zone::Activate() and Zone::Deactivate()](../src/controls/zone.cpp) |

### Learn FX name branches

| Current name or construct | Current hidden behavior | Required explicit replacement | Source |
|---|---|---|---|
| `FXRowLayout` | Lists modifier and row combinations for the native Learn FX dialog | Do not import it; OSK uses real widgets and runtime modifier combinations | [learn_dialog.cpp](../src/ui/learn_dialog.cpp) |
| `FXWidgetLayout` | Supplies eligible widgets, display rows, ring choices, fonts, and color flags | Convert eligible widget and display selectors to `LearnFX.fxzon`; derive capabilities from matched widgets and feedback processors | [learn_dialog.cpp](../src/ui/learn_dialog.cpp) |
| `FXPrologue`, `FXEpilogue` | Inject bindings before and after generated FX parameter bindings | Merge active bindings into ordered `GeneratedBindings`; diagnose conflicts instead of keeping hidden ordering | [learn_dialog.cpp](../src/ui/learn_dialog.cpp) |
| `#WidgetType`, `#DisplayRow`, `#RingStyle`, `#DisplayFont`, `#SupportsColor` | Parsed only by the native Learn FX dialog and skipped by the normal zone parser | Legacy importer input only; format 2 emits selectors and typed capabilities without hash directives | [learn_dialog.cpp](../src/ui/learn_dialog.cpp), [zone_parser.cpp](../src/controls/zone_parser.cpp) |
| Surface name `SCE24` in Learn FX | Enables device-specific display behavior by exact surface name | Widget and feedback processor capability metadata | [learn_dialog.cpp](../src/ui/learn_dialog.cpp) |

### Reference diagnostics and active layers

| Condition | Format 2 result |
|---|---|
| Assigned Main profile does not exist | Error on the surface assignment; skip only that surface |
| No zone has `Role=Home`, or more than one active Main zone has it | Profile error; do not initialize that surface profile |
| Duplicate zone ID in one active Main layer or one FX source layer | Error that links both files |
| Exact User FX ID matches a Vendor FX ID | Valid override, not a duplicate |
| `ActiveZones`, `SubZones`, `GoZone`, or `GoSubZone` target is missing from the complete active profile | Error on the reference |
| A partial editor validation set does not contain the target | Defer the diagnostic and resolve it from the profile index; do not report it as missing |
| Main zone is valid but no structural or navigation reference reaches it | Warning for an orphan zone |
| Vendor or User FX directory is absent or empty | Valid empty layer |
| `MatchFX` names a plugin that is not installed | Valid external matcher; runtime simply has no matching active FX |

The layer model is fixed: [ProductPaths::FindMainZones()](../src/shared/product_paths.cpp) selects the complete User Main directory when it exists, otherwise Vendor Main. [ZoneManager::PreProcessZones()](../src/controls/zone_manager.cpp) reads Vendor and User FX directories together, and [ZoneManager::AddZoneFilePath()](../src/controls/zone_manager.h) gives an exact User FX match priority over Vendor.

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

The confirmed format direction above is input to this phase, not the complete grammar. Main zone identity remains the `.zon` filename stem. `@Meta` carries format and behavior metadata. `MatchFX` identifies an external plugin match, not the zone itself. Format 2 uses `[Channel]`; legacy `Widget|` is accepted only by the importer and converted during migration.

- [ ] Write the normative lexical grammar for `@Meta`, brace blocks, identifiers, selectors, lists, properties, strings, comments, whitespace, and EOF behavior.
- [ ] Define the allowed metadata keys, value types, defaults, combinations, and diagnostics for zones, surfaces, snippets, and `LearnFX.fxzon`.
- [ ] Define the exact `Role`, `Navigator`, `Instances`, banking, mode, and FX context values from the completed Phase 1 inventory.
- [ ] Define binding, `ActiveZones`, `SubZones`, `GoZone`, and `GoSubZone` schemas without treating navigation references as structural recursion.
- [ ] Specify true wildcard matching, case handling, escaping, match order, and no-match diagnostics separately from `[Channel]`.
- [ ] Specify the valid prefix selector, postfix `[Channel]`, and named property list positions. Reject anonymous bracket groups.
- [ ] Confirm the names and value rules for `Range`, `Delta`, `StepValues`, `AcceleratedDeltas`, `AccelerationTicks`, `Increment`, `Decrement`, and `StateColors`.
- [ ] Define case-insensitive zone uniqueness within one active profile and keep the Main, FX, Surface, Vendor, and User layer rules distinct.
- [ ] Define transactional file rename, collision checks, stale-reference handling, and updates of every typed reference.
- [ ] Define `LearnFX.fxzon`, OSK FX edit mode, widget eligibility, display selection, and generated bindings. The legacy importer reads hash-prefixed Learn FX directives but emits only normal format 2 data.
- [ ] Define the global `RingStyle` type, processor capabilities and defaults, the complete action `FeedbackShape` set, fallback mappings, and optional explicit binding overrides.
- [ ] Specify the unversioned product `.conf` block schemas, identifiers, required and optional fields, defaults, settings scopes, links, diagnostics, and shared semantic model ownership.
- [ ] Specify brace-based surface and snippet schemas using the common lexical grammar.
- [ ] Add representative valid and invalid fixture files for every top-level format before runtime implementation starts.

Ready when the normative specification and fixtures let C++, Bun, Lua, and documentation implement the same grammar without interpretation differences.

## [ ] Phase 3: One C++ parser model

- [ ] Implement one lexer for metadata, brace blocks, lists, properties, quotes, comments, wildcard selectors, and channel placeholders.
- [ ] Parse each zone, surface, Learn FX, and snippet file once into a typed document model with source locations.
- [ ] Parse the new product `.conf` into `IntegratorConfig` and let every C++ consumer use that one semantic model.
- [ ] Expose ring feedback capabilities and resolved action feedback shapes through runtime and generated editor metadata.
- [ ] Validate the complete active profile before runtime objects are created.
- [ ] Let an invalid non-required zone be skipped with focused diagnostics instead of disabling unrelated zones.
- [ ] Treat an invalid or missing Home role as a profile-level initialization error.
- [ ] Replace the metadata preprocessor, binding parser, surface block readers, Learn FX line readers, and OSK line edits with the shared model where they overlap.

Ready when runtime behavior consumes validated documents and no feature reparses the same zone with a different grammar.

## [ ] Phase 4: Bun editor and migration

- [ ] Add lossless format 2 parsing, validation, syntax highlighting, quick fixes, and cross-file references.
- [ ] Add safe zone rename with reference updates and collision checks.
- [ ] Convert old `Zone ... ZoneEnd`, `BlockName ... BlockNameEnd`, and surface blocks during legacy import.
- [ ] Convert legacy `Widget|` channel placeholders to the selected explicit channel syntax.
- [ ] Convert every legacy anonymous zone value group and surface encoder direction group to typed named properties.
- [ ] Convert legacy anonymous RGB groups to `StateColors` hexadecimal lists.
- [ ] Convert name-based navigator behavior into explicit properties.
- [ ] Convert Learn FX pseudo-zones into `LearnFX.fxzon` and do not convert `FXRowLayout`.
- [ ] Convert `IncludedZones` to `ActiveZones` and keep `SubZones` as a structural relation.
- [ ] Replace current development product INI parsing, serialization, diagnostics, fixtures, drafts, and editor support with the new unversioned `.conf` model.
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

## Non-goals

- Runtime loading from legacy CSI folders.
- Multiple zones in one `.zon` file.
- Keeping magic zone names for compatibility.
- Moving full configuration editing into Lua OSK.
