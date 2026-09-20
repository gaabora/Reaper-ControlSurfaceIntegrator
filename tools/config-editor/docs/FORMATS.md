# Configuration Format Contracts

## Common rules

- Surface and zone format version 1 use a first-class editor marker in a runtime-safe comment: `// @format <type> 1`.
- Missing surface or zone markers are warnings until current vendor files migrate with their curated content in Phase 6.
- Stable file and semantic IDs match `[a-z0-9][a-z0-9_-]*`.
- Names shown to users may contain spaces and Unicode when the owning field permits them.
- Parsers preserve all source text. Unknown data is a warning, not a reason to remove a line.
- Only `//` starts a comment in current Surface and Zone files. A single leading `/` and a leading `#` are not comments. The exact Learn FX directives listed below are reserved metadata lines that start with `#`. The legacy importer converts old single-slash and other hash comment lines to `//` without changing OSC address tokens such as `X32Fader /ch/01/mix/fader`.
- Syntax errors prevent later apply or save operations.

## Product configuration

The product `.conf` uses unversioned brace blocks:

```text
Settings {
  HoldDelayMs=1000
}

Device fp2 {
  Type=MIDI
  Input=0
  Output=0
}

Page Home {
  FollowMCP=false

  Surface mixer {
    Device=fp2
    Template=faderportv2
    MainProfile=faderportv2
    FXProfile=faderportv2
    StartChannel=0
  }
}
```

- `Page`, `Device`, and Page-local `Surface` IDs are unquoted. They start with an ASCII letter and contain only ASCII letters, digits, or `_`.
- Root `Settings` values apply to the product. A nested `Settings` block in a `Device` overrides the allowed values for that Device.
- A Page `Surface` uses a separate local ID and references one `Device`. `Template`, `MainProfile`, and `FXProfile` are stable resource IDs.
- `Channels` belongs to the referenced `@Surface` header, not to the I/O Device. Assigning one Device to Surface templates with different channel counts is an error.
- `Link` blocks connect two Page-local Surface IDs and declare a non-empty `Share=[ ... ]` category list.
- Only `//` starts a comment. `#` is invalid in the product configuration.
- Unknown blocks and properties are errors.
- Old `CSI.ini` and renamed legacy `ReaControlSurface.ini` files are read only by the Bun legacy importer. The import preview converts devices, pages, Surface assignments, links, and supported global settings to this `.conf` model. Replacing an existing product configuration requires explicit confirmation and fresh source and target hash checks.

## Surface 2

Current Surface files use `@Surface { Version=2 Protocol=MIDI|OSC Channels=<positive integer> ... }`, brace blocks, and universal `Input` and `Feedback` primitives. The legacy importer converts supported processor lines to this format and reports an error for a processor that it cannot preserve.

`OSKLayout { Row { ... } }` has no separate version. If a legacy Surface has no explicit layout, the importer creates an editable initial layout from usable Input widgets. A fader occupies seven rows while buttons and rotaries fill the remaining cells in source order.

### X32 feedback during legacy import

OSC `Feedback State` supports `ActiveOnly=true` for both `OSCInt` and `OSCFloat`. It sends `OnValue` for a nonzero state and sends nothing for zero or clear updates. The default is `ActiveOnly=false`, which sends `OffValue` for zero and clear updates.

- `FB_X32IntProcessor` becomes integer `Value` feedback for ordinary addresses. A numbered address such as `/-stat/selidx/70` becomes `Feedback State { Encoding=OSCInt Address="/-stat/selidx" OnValue=70 ActiveOnly=true }`. A nonzero state selects channel index 70. Zero state and clear updates send nothing, so other Widgets do not reset the selection. Index 0 and leading zeros are supported.
- `FB_X32Processor` with a `Control` line at the same OSC address becomes numeric `Value` feedback. `/config/name` becomes `Text` feedback and `/config/color` becomes `Color` feedback with the X32 palette.
- For another `FB_X32Processor` address, the import Problem offers four source-draft quick fixes: `Text`, `X32 palette Color`, numeric `Value`, and `State`. Choose the data that the address sends. For example, choose `Text` for a label, `Color` for an X32 palette color, `Value` for a number, or `State` for an off/on value.
- The quick fix adds `// @x32-feedback=<type>` to the old processor line. It changes only the import draft. You can select a different type before import. The converted Surface does not keep this hint.

## Legacy Surface 1

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

Layout widget names and `PressTarget`, `ScrollTarget`, `ValueTarget`, and `TouchTarget` values must exist in hardware `Widget` blocks. Every target must provide the required input type. The MIDI runtime loads the formal block and ignores its layout cells during hardware parsing.

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
- An `FXWidgetLayout` file can place the exact `#WidgetType`, `#DisplayRow`, `#RingStyle`, `#DisplayFont`, and `#SupportsColor` Learn template directives after `ZoneEnd`. These are metadata keywords, not comments.

## Zone 2

Format 2 Zone files start with `@Zone { Version=2 ... }`. A normal `Bank <amount>` binding uses the Zone `Target` and optional `BankTarget`. One binding can select a different child collection by supplying both action properties:

```text
@Zone { Version=2 Target=Tracks BankTarget=FX }

Left  Bank -1 Target=SelectedTrack BankTarget=FX
Right Bank 1 Target=SelectedTrack BankTarget=FX
```

The paired override accepts `Target=Tracks|SelectedTrack` with `BankTarget=Sends|Receives|FX`, or `Target=MasterTrack` with `BankTarget=FX`. Supplying only one property or an incompatible pair is an error.

## Functional snippet 2

Snippets are direct Zone fragments:

```text
@Snippet { Version=2 Name="Transport controls" }

play Play
[Shift]+play Reaper 40044
stop Stop
```

- The first significant line is `@Snippet { Version=2 }`.
- The remaining lines use the normal format 2 Zone binding grammar.
- Exact Widget names are mapped automatically. A different Widget requires user confirmation.
- The editor inserts resolved lines directly into the unsaved Zone draft. It does not add application IDs or hidden marker comments.
- Normal Zone and Surface validation checks the result before save.
