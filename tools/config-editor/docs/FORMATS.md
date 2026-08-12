# Configuration Format Contracts

## Common rules

- Surface and zone format version 1 use a first-class editor marker in a runtime-safe comment: `// @format <type> 1`.
- Missing surface or zone markers are warnings until current vendor files migrate with their curated content in Phase 6.
- Stable file and semantic IDs match `[a-z0-9][a-z0-9_-]*`.
- Names shown to users may contain spaces and Unicode when the owning field permits them.
- Parsers preserve all source text. Unknown data is a warning, not a reason to remove a line.
- A line whose first non-space character is `/` or `#` is a runtime comment. `// @format` remains an editor-readable runtime comment.
- Syntax errors prevent later apply or save operations.

## Product config 7.0

The product config keeps the current runtime header and property-list syntax:

```text
Version=7.0
SurfaceType=MIDI SurfaceName=fp2 SurfaceChannelCount=1 MidiInput=0 MidiOutput=0 MIDISurfaceRefreshRate=15 MaxMIDIMesssagesPerRun=200
PageName=Home PageFollowsMCP=No SynchPages=No ScrollLink=No ScrollSynch=No
  Surface=fp2 SurfaceFolder=faderportv2 ZoneFolder=faderportv2 FXZoneFolder=faderportv2 StartChannel=0
```

- `Version=7.0` must be the first physical line because the current C++ runtime checks line zero.
- `SurfaceFolder`, non-empty `ZoneFolder`, and non-empty `FXZoneFolder` values are stable IDs, not paths.
- `SurfaceFolder` is required for a surface assignment.
- Unknown property-list records are preserved with a warning.

## Surface 1

The surface document contains named hardware `Widget` blocks plus optional value blocks:

```text
// @format surface 1
Widget Play
  Press 90 5e 7f 90 5e 00
  FB_TwoState 90 5e 7f 90 5e 00
WidgetEnd
```

Supported top-level blocks are `StepSize`, `AccelerationValues`, `ColorCalibration`, `Widget`, and `OSKLayout`. Block names have matching `End` lines. Widget names are unique case-insensitively.

The formal layout grammar is:

```text
OSKLayout Version=1
  Row
    Widget Play Width=1.5 Height=1.5
    Spacer Width=0.25
  RowEnd
OSKLayoutEnd
```

Layout widget names and `PressTarget`, `ScrollTarget`, `ValueTarget`, and `TouchTarget` values must exist in hardware `Widget` blocks. Every target must provide the required input type. The MIDI runtime loads the formal block and ignores its layout cells during hardware parsing. Current `# OSKRow` and `# OSKSpacer` data remains lossless legacy import input.

## Zone 1

```text
// @format zone 1
Zone Home NavType=SelectedTrack
  SubZones
    transport
  SubZonesEnd
  Play Play
  Hold+Play GoSubZone transport Feedback=No HoldDelay=500
ZoneEnd
```

- One file contains one `Zone` block.
- `IncludedZones` and `SubZones` sections list zone dependencies.
- A binding contains a widget expression, runtime action, parameters, and optional `Key=Value` properties.
- `GoZone` and `GoSubZone` first parameters are also dependencies.
- Unknown runtime actions are warnings because external or future data must remain editable.
- Zone names and dependencies are checked case-insensitively across the validation set.
- Zone dependency cycles are errors.

## Functional snippet 1

Snippets store semantic bindings, not hardware widget names:

```text
Snippet Version=1 Id=transport Name="Transport controls"
  Binding Id=play Role=Button Input=Press Feedback=Toggle Required=Yes
    Action NoMod Play
    Action Shift Reaper 40044
  BindingEnd
SnippetEnd
```

- `Snippet` has a stable `Id` and a display `Name`. Every `Binding` has a stable `Id`.
- `Role` is one of `Button`, `Display`, `Fader`, `Meter`, or `Rotary`.
- `Input` contains one or more `Press`, `Relative`, `Absolute`, or `Touch` capabilities separated by `+`.
- `Feedback` contains one or more `Toggle`, `Color`, `Value`, `Text`, or `Meter` capabilities separated by `+`. Use `Feedback=None` when the binding needs no feedback.
- `Required` is `Yes` or `No`. An optional binding can be skipped, but the user must confirm the skip.
- Each binding contains at least one `Action`. An action contains a modifier expression, runtime action, and its parameters. `NoMod` cannot be combined with other modifiers.
- A `Widget` property is an error. The application UI resolves an exact compatible semantic-slot and widget-name match automatically. A different manual selection requires confirmation.

The editor compares each selected widget with the binding role and capabilities. A mismatch is blocked by default. The user can apply it only after the editor shows the mismatch and the user enables the override and confirms the widget.

Applying a snippet resolves semantic bindings into normal zone lines. The editor inserts the lines before `ZoneEnd` in a marked block:

```text
  // @snippet Application=transport Source=transport
  Play Play
  Stop Stop
  // @snippet-end Application=transport
```

`Application` is a stable ID inside the target zone. A repeated application requires `Replace`, `Rename`, or `Skip`. `Replace` changes only the matching marked block, `Rename` adds a separate block with a new application ID, and `Skip` writes nothing. The editor rechecks the snippet, surface, and target-zone hashes and saves the resolved zone through one validated transaction.
