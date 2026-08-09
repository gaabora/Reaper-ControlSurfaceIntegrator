# Lua and C++ ExtState Interface

## Scope

This document defines the current private interface between the ReaControlSurface C++ extension and
the bundled OSK/OSD Lua scripts. The scripts have not been published, so this contract
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

## OSK Data

Keys in `ReaCtrlSurf_OSK`:

| Key | Payload |
| --- | --- |
| `Command` | `Close` requests script shutdown |
| `Surfaces` | `surface|surface...` |
| `Layout_<surface>` | Newline-separated rows containing pipe-separated cells |
| `State_<surface>` | `widget=V:<value>,C:#RRGGBB,A:<0-or-1>,K:<kind>;...` |
| `Labels_<surface>` | `widget=label;...` |
| `LabelMap_<surface>` | `widget=modifier:label|modifier:label;...` |
| `ActionList` | Comma-separated CSI action names |
| `ConfigResult_<surface>_<widget>` | `modifier:action line;...` |
| `ConfigZoneName_<surface>_<widget>` | Active edit-target zone name |
| `ConfigZonePath_<surface>_<widget>` | Active edit-target zone file path |
| `ConfigStatus_<surface>_<widget>` | Structured operation status |

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

`ReaCtrlSurf_OSD` contains the shared OSD event consumed by both the standalone OSD
script and OSK embedded OSD bars:

| Key | Payload |
| --- | --- |
| `OSD` | `text;background;timeoutMs` |
| `OSD_ID` | Monotonically increasing event id |

`background` is `0`, `1`, or `#RRGGBB`. C++ updates `OSD_ID` for every accepted OSD
publish. Lua consumers keep their own last-seen id and do not delete the shared keys,
so the standalone OSD and OSK embedded bars can consume the same event without racing
each other while identical payloads can still refresh the visible timeout.

## Settings

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

## Unsupported Keys

`ZoneInfo_*`, `ReloadZones`, `ActiveZone_*`, `ActiveSurface`, global
`ConfigStatus`, and legacy `WidgetPress` are not part of this interface.
