# Lua and C++ ExtState Interface

## Scope

This document defines the current private interface between the ReaControlSurface C++ extension and
the bundled Control Panel, OSK, OSD, and Notifications Lua scripts. The scripts have not been published, so this contract
has no legacy aliases or migration fallbacks.

The current `ReaCtrlSurf` prefix comes from [`../Scripts/product_identity.conf`](../Scripts/product_identity.conf). CMake generates the C++ constants from it, and Lua reads it directly through `product_identity.lua`.

All command and response entries are session-only unless a settings section is named.
Lua-to-C++ commands are consumed once and deleted by C++.

## Sections

| Section | Direction | Purpose |
| --- | --- | --- |
| `ReaCtrlSurf_OSK` | C++ to Lua | OSK discovery, display data, configuration responses |
| `ReaCtrlSurf_OSK_CMD` | Lua to C++ | Semantic widget and configuration commands |
| `ReaCtrlSurf_OSK_SETTINGS` | Lua persistent | OSK appearance, interaction, surface positions |
| `ReaCtrlSurf_OSD` | C++ to Lua | Shared OSD message bus |
| `ReaCtrlSurf_OSD_SETTINGS` | Lua persistent | Standalone and embedded OSD appearance |
| `ReaCtrlSurf_NOTIFICATIONS` | C++ to Lua | Notification log reader startup state |
| `ReaCtrlSurf_NOTIFICATIONS_SETTINGS` | Lua persistent | Notifications appearance |
| `ReaCtrlSurf_APPEARANCE_SETTINGS` | Lua persistent and session-only | Common appearance values and appearance revision notification |
| `ReaCtrlSurf_CONTROL_PANEL` | C++ and Lua | Control Panel lifecycle requests, window state, and Lua-persistent shell state |
| `ReaCtrlSurf_SETTINGS_CMD` | Lua to C++ | Product and Surface setting Query, Apply, and Reload requests |
| `ReaCtrlSurf_SETTINGS` | C++ to Lua | Correlated setting responses and effective values |

## Control Panel Lifecycle

C++ writes one session-only `Request` entry in `ReaCtrlSurf_CONTROL_PANEL`. Lua consumes and deletes it without blocking the REAPER UI thread. The Phase 1 payload is a newline-separated property list:

```text
Version=1
RequestId=1
Command=Open|Focus|SelectTab
Tab=Devices|General|Appearance|Logging
Surface=fp2
Page=Home
```

`Tab` is required only for `SelectTab`. `Open` starts the Control Panel when it is not active. `Focus` brings the existing window forward. `SelectTab` selects one known page and focuses the window. `Surface` and `Page` are optional navigation context for General. They identify which configured Surface scope General must query through the separate settings protocol. This lifecycle section does not carry configuration values.

Lua writes the session-only `State` key as `Open` after startup and `Closed` during shutdown. The stable `_REACTRLSURF_OPEN_CONTROL_PANEL` action uses this lifecycle state for its toggle value. Repeating the action while the ReaScript is active sends `Focus` and does not start a second instance.

Lua also stores these persistent shell keys in the same section:

- `WindowPosition` as `x,y`.
- `WindowSize` as `width,height`.
- `SelectedTab` as one known tab name.
- `Scroll.<tab>` as the last vertical scroll position for that tab.

## OSK Data

Keys in `ReaCtrlSurf_OSK`:

| Key | Payload |
| --- | --- |
| `Command` | `Close` requests script shutdown |
| `Surfaces` | `surface|surface...` |
| `Page` | Current Page name used to scope configured Surface settings |
| `Layout_<surface>` | Newline-separated rows containing pipe-separated cells |
| `State_<surface>` | `widget=V:<value>,C:#RRGGBB,A:<0-or-1>,K:<kind>;...` |
| `Labels_<surface>` | `widget=label;...` |
| `LabelMap_<surface>` | `widget=modifier:label|modifier:label;...` |
| `ActionList` | Comma-separated CSI action names |
| `ConfigResult_<surface>_<widget>` | `modifier:action line;...` |
| `ConfigZoneName_<surface>_<widget>` | Active edit-target zone name |
| `ConfigZonePath_<surface>_<widget>` | Active edit-target zone file path |
| `ConfigStatus_<surface>_<widget>` | Structured operation status |
| `ZoneCreateStatus_<surface>` | `outcome|path|message` for one zone-file creation request |

Configuration status format:

```text
outcome|operation|surface|widget|zone|message
```

`outcome` is `OK` or `ERR`. Operations are `Query`, `ApplyLive`, `Save`, and
`Revert`.

Layout cell metadata uses comma-separated `key=value` pairs after the widget name.
String values may be quoted with `"` when they contain delimiters. Inside quoted
metadata values, `\\`, `\"`, `\n`, and `\r` are backslash escapes.

`State_<surface>` value availability `A` is `1` when the current zone/action can
provide a continuous value for the widget and `0` when a fader/rotary-like widget is
currently bound to a non-valued action such as navigation. Continuous kind `K` is
empty for generic values, `V` for volume-related actions, and `P` for pan-related
actions.

OSK layout cells may include semantic metadata derived from the real surface widget
definition:

- `Role`: `button`, `rotary`, `fader`, `display`, `meter`, or `unknown`.
- `Input`: `+`-separated capabilities such as `press`, `relative`, `absolute`, and
  `touch`.
- `Feedback`: `+`-separated capabilities such as `value`, `toggle`, `color`, `text`,
  and `meter`.
- `Class`: widget class from the `Widget <name> <class>` line, when present.
- `PressTarget`, `ScrollTarget`, `ValueTarget`, and `TouchTarget`: widget names that
  receive the corresponding semantic command.
- `RotaryStyle`: `dot` or `wiper`.

Lua must prefer semantic metadata over visual `Shape` when deciding interaction
behavior. `Shape` remains a visual hint only.

## OSK Commands

Keys in `ReaCtrlSurf_OSK_CMD`:

| Key | Payload |
| --- | --- |
| `WidgetPressDown` | `surface|widget` |
| `WidgetPressUp` | `surface|widget` |
| `WidgetScroll` | `surface|widget|accelerationIndex|signedEventCount` |
| `WidgetValue` | `surface|widget|normalizedValue` |
| `WidgetTouch` | `surface|widget|0-or-1` |
| `ConfigQuery` | `surface|widget` |
| `ConfigApplyLive` | `surface|widget|serializedBindings` |
| `ConfigSave` | `surface|widget` |
| `ConfigRevert` | `surface|widget` |
| `ActionListQuery` | Empty payload |
| `SurfaceEnabled` | `surface|0-or-1` |
| `ZoneCreate` | `surface|scaffoldType|zoneName|alias|navigator` |

`ZoneCreate` supports `main`, `home`, `go`, `subzone`, `included`, `learn`, and `fx` scaffold types. C++ resolves the active surface profile and writes only below its user profile. Creating a Main zone from Vendor Main requires confirmation and a Main-only User copy first. Creating an FX zone writes directly to the configured User FX directory. The command creates one file and does not add it to a parent zone. `zoneName` is also the file stem and uses ASCII letters, digits, `_`, and `-`. The optional alias must not contain `|`, quotes, or line breaks. The optional navigator is empty or one of the navigator names offered by the OSK dialog.

`WidgetScroll` uses a non-negative acceleration index and an event count from `-8` to
`8`, excluding zero. The sign is the direction. Lua rate-limits and coalesces wheel
events before publishing this command.

`WidgetValue` sends fader-style input, including drag and fader-wheel changes. The
preferred payload is an absolute normalized value in the range `0.0` to `1.0`;
dB-valued fader feedback may be echoed as a dB command value for DB volume actions.
`WidgetTouch` brackets fader drags so actions that support touch automation can
observe touch start and release.

When layout metadata supplies semantic targets, Lua sends press commands to
`PressTarget`, wheel commands to `ScrollTarget`, absolute value commands to
`ValueTarget`, and touch commands to `TouchTarget`. This lets one visible OSK control,
such as a rotary, scroll the encoder widget while clicking a hidden paired push
widget.

Serialized bindings use:

```text
modifier:action line;modifier:action line
```

The action line may end with internal metadata tokens used to preserve structured
editor state:

- `__OSK_HOLD`
- `__OSK_DOUBLE_PRESS`
- `__OSK_INVERT`
- `__OSK_INVERT_FB`
- `__OSK_INCREASE`
- `__OSK_DECREASE`

## OSD

`ReaCtrlSurf_OSD` contains the shared OSD event consumed by both the standalone OSD script and OSK embedded OSD bars:

| Key | Payload |
| --- | --- |
| `OSD` | `text;background;timeoutMs;explicitMessage` |
| `OSD_ID` | Monotonically increasing event id |

`background` is `0`, `1`, or `#RRGGBB`. `explicitMessage` is `1` when `text` came from an explicit action-line `OSD` value and `0` for automatic runtime text. C++ updates `OSD_ID` for every accepted OSD request, including an identical payload. Lua consumers keep their own last-seen id and do not delete the shared keys, so the standalone OSD and OSK embedded bars can consume the same event without racing each other. Repeated actions refresh the visible timeout and re-evaluate Lua runtime templates even when their source text is unchanged.

Track selection publishes three text lines in previous, selected, and next order. The entire OSD background uses the selected track color. Missing adjacent tracks produce empty outer lines. CSI edit cursor movement, rewind, and fast-forward publish the resulting position as `[bar/beat]`.

An explicit action-line `OSD` value can contain these case-sensitive runtime variables:

| Variable | Value |
| --- | --- |
| `{currTrackName}` | First selected track name |
| `{prevTrackName}` | Track name before the first selected track, or empty at the boundary |
| `{nextTrackName}` | Track name after the first selected track, or empty at the boundary |
| `{currMinSec}` | Current edit cursor position as `M:SS.mmm` |
| `{currBarBeat}` | Current edit cursor position as `bar/beat`, including the project measure offset |

C++ treats an explicit action-line `OSD` value as the authoritative event text and publishes it with `explicitMessage=1`. Both Lua consumers use `osd_templates.lua` to enumerate, validate, and expand variables after they receive the event. Unknown variables remain visible in the result. Use `\n` inside the `OSD` value for a new line. For example, `OSD="{prevTrackName}\n{currTrackName}\n{nextTrackName}"` creates a three-line track template, and `OSD="[{currBarBeat}] {currMinSec}"` shows both supported cursor formats.

The standalone OSD calculates percentage width from the ReaImGui monitor work area. The REAPER client rectangle and `my_getViewport` are fallbacks when that work area is unavailable.

## Notifications

`ReaCtrlSurf_NOTIFICATIONS` contains `StartOffset`, the byte offset in `Data/ReaControlSurface/ReaControlSurface.log` from which a newly started `Notifications.lua` instance begins reading. C++ sets the value immediately before it starts the script.

The plugin and normal Lua runtime write logs to the product log without calling `ShowConsoleMsg`. `Notifications.lua` tails new log entries and shows NOTICE, WARNING, and ERROR entries. INFO and DEBUG remain file-only. Explicit diagnostic tools such as `OSK state debug.lua` and parser self-check output may use the REAPER console when the user starts them manually.

Notifications uses persistent `opacity` from `ReaCtrlSurf_NOTIFICATIONS_SETTINGS`. Its compiled Lua default is `0.8`. Each visible record has its own square `×` dismiss control with a shared theme size. Dismiss removes only that popup record from memory, does not remove its log data, and does not stop the Notifications script.

## Product and Surface Settings Protocol

Lua writes one session-only `Request` entry in `ReaCtrlSurf_SETTINGS_CMD`. Lua must wait while this entry exists. C++ consumes and deletes it, then writes `Response_<RequestId>` in `ReaCtrlSurf_SETTINGS`. Lua consumes and deletes its response.

A Lua client can cancel only its own pending request before C++ consumes it. Cancellation removes the matching request and any matching response; it is not a new C++ command. General uses cancellation when its Product or Surface query context changes before C++ consumes the old request, when no C++ response arrives before its timeout, and during Control Panel shutdown.

Every request is a newline-separated property list:

```text
Version=1
RequestId=172345_1
Command=Query|Apply|Reload
Scope=Product|Surface
Page=Home
Surface=fp2
Set.HoldDelayMs=750
Unset.LongHoldDelayMs=1
```

`Query` requires `Scope`. `Surface` scope requires `Surface`; `Page` is optional when the runtime Surface name is unique across Pages. `Apply` requires at least one `Set.<name>` or `Unset.<name>`. `Reload` ignores scope and reads settings from the product INI. Lua does not read or write the INI directly.

Every response starts with:

```text
Version=1
Status=OK|ERROR
```

An error response adds `Message=<text>`. A successful Apply or Reload adds a completion message. A successful Query adds `Scope`, optional `Page` and `Surface`, and three properties for each setting:

```text
Value.HoldDelayMs=750
Source.HoldDelayMs=Surface
Inherited.HoldDelayMs=1000
```

Every successful Query also returns the current runtime Page and Surface assignments as one-based option pairs:

```text
SurfaceOption.1.Page=Home
SurfaceOption.1.Surface=fp2
SurfaceOption.2.Page=Mix
SurfaceOption.2.Surface=xtouch
```

General renders these pairs as one Surface dropdown. The Page value keeps equal Surface names on different Pages unambiguous. Lua does not accept a raw Surface name for this selector.

`Source` is `Compiled`, `Product`, or `Surface`. `Inherited` is the value that becomes effective when the explicit override is removed. C++ validates the complete candidate scope before changing runtime state. Apply writes a complete temporary file, atomically replaces the product INI, and then applies the already validated values. Reload leaves current runtime values unchanged when the file has invalid settings or cannot be matched safely to the current Pages and Surfaces.

## Lua Appearance Persistent Settings

Persistent keys in `ReaCtrlSurf_APPEARANCE_SETTINGS`:

- `item_spacing`
- `rounding`
- `disabled_alpha`
- `error_color`

The same section contains session-only `Revision`. Every Common, OSK, OSD, or Notifications schema save that changes persisted values increments it. `Revision` is a change notification, not a persistent setting or configuration revision.

The Control Panel also writes a session-only live preview overlay in this section:

- `PreviewActive=1` while an Appearance draft preview exists.
- `PreviewRevision` increments after the preview is published or cleared.
- `Preview.<group>.<setting>` contains every current draft value, where `<group>` is `Common`, `OSK`, `OSD`, or `Notifications`.

Running OSK, OSD, and Notifications contexts compare both revisions. After either changes, they load persistent appearance values and then apply the active preview overlay. Save persists the draft and clears the overlay. Revert, Don't Save, and Control Panel shutdown clear the overlay without persisting it.

Persistent keys in `ReaCtrlSurf_OSK_SETTINGS`:

- `zoom`
- `interactive`
- `invert_scroll`
- `aspect`
- `font_size`
- `font_family`
- `line_height`
- `label_case`
- `pad_h`
- `pad_v`
- `transparency`
- `btn_transparency`
- `inactive_led_boost`
- `tooltip_delay`
- `arrow_angle`
- `titlebar_enabled`
- `label_replacements`
- `SurfacePosition_<surface>` with payload `x,y`
- `SurfaceEnabled_<surface>` with value `true` or `false`
- `WidgetConfigPosition` with payload `x,y`
- `WidgetConfigSize` with payload `width,height`
- `OSDBarPosition_<surface>` with value `off`, `top`, or `bottom`

OSK uses one independent window per surface. Position writes are delayed while a
window is moving and flushed on shutdown. The latest enabled/hidden state is
persisted per surface and restored by the C++ bridge on product startup.

Persistent keys in `ReaCtrlSurf_OSD_SETTINGS`:

- `osd_position`
- `osd_alignment`
- `osk_bar_position`
- `osd_width_percent`
- `osd_height_px`
- `osd_transparency`
- `osd_h_margin_px`
- `osd_v_margin_px`
- `osd_font_px`
- `osd_bg_on`
- `osd_bg_off`

Persistent keys in `ReaCtrlSurf_NOTIFICATIONS_SETTINGS`:

- `opacity`

## Unsupported Keys

`ZoneInfo_*`, `ReloadZones`, `ActiveZone_*`, `ActiveSurface`, global
`ConfigStatus`, and legacy `WidgetPress` are not part of this interface.
