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

## [ ] Phase 1: Runtime behavior inventory

- [ ] List every exact zone-name comparison in C++ and group it by home selection, navigator choice, banking, FX matching, Learn FX, and listener behavior.
- [ ] Separate navigation references (`GoZone`, `GoSubZone`) from structural references (`ActiveZones`, `SubZones`).
- [ ] Define which missing references are errors, warnings, or valid external references.
- [ ] Define active Main and FX layer rules for Vendor and User files in one shared specification.
- [ ] Inventory every product configuration reader and writer and remove the native dialog's duplicate semantic parser path.
- [ ] Inventory ring feedback processor capabilities and group standard actions by semantic feedback shape.

Ready when every current special case has an explicit replacement and no behavior depends on an undocumented name.

## [ ] Phase 2: Format 2 specification

- [ ] Specify `@Meta`, brace blocks, lists, identity, alias, role, navigator, instances, FX matching, bindings, structural dependencies, comments, properties, quoting, and EOF behavior.
- [ ] Specify true wildcard selectors separately from the current channel placeholder.
- [ ] Specify the three valid square-bracket positions and reject all anonymous bracket groups.
- [ ] Specify named action value and MIDI encoder direction properties.
- [ ] Specify `StateColors` and convert legacy decimal RGB groups to hexadecimal color lists.
- [ ] Require one logical object per file.
- [ ] Define case handling and uniqueness within one zone profile.
- [ ] Define safe transactional rename behavior and reference updates.
- [ ] Define `LearnFX.fxzon`, OSK FX edit mode, widget eligibility, display selection, and generated bindings without hash-prefixed directives or pseudo-zones.
- [ ] Specify the global `RingStyle` enum, processor capabilities and defaults, action `FeedbackShape` metadata, fallback mappings, and explicit binding override behavior.
- [ ] Specify the unversioned product `.conf` blocks, identifiers, fields, nesting, defaults, links, comments, diagnostics, and shared semantic model ownership.
- [ ] Specify the brace-based surface and snippet syntax with the same lexical rules.
- [ ] Add representative valid and invalid fixtures before changing runtime code.

Ready when C++, Bun, Lua, and documentation can implement the same grammar without interpretation differences.

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
