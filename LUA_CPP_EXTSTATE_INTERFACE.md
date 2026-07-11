# Lua and C++ ExtState Interface

## Scope

This document defines the current private interface between the CSI C++ extension and
the bundled OSK/OSD Lua scripts. The scripts have not been published, so this contract
has no legacy aliases or migration fallbacks.

All command and response entries are session-only unless a settings section is named.
Lua-to-C++ commands are consumed once and deleted by C++.

## Sections

| Section | Direction | Purpose |
| --- | --- | --- |
| `ReaCtrlSurf_OSK` | C++ to Lua | OSK discovery, display data, configuration responses |
| `ReaCtrlSurf_OSK_CMD` | Lua to C++ | Semantic widget and configuration commands |
| `ReaCtrlSurf_OSK_SETTINGS` | Lua persistent | OSK appearance, interaction, surface positions |
| `ReaCtrlSurf_OSD` | C++ to Lua | Current OSD message |
| `ReaCtrlSurf_OSD_SETTINGS` | Lua persistent | Standalone and embedded OSD appearance |

## OSK Data

Keys in `ReaCtrlSurf_OSK`:

| Key | Payload |
| --- | --- |
| `Command` | `Close` requests script shutdown |
| `Surfaces` | `surface|surface...` |
| `Layout_<surface>` | Newline-separated rows containing pipe-separated cells |
| `State_<surface>` | `widget=V:<value>,C:#RRGGBB;...` |
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

## OSK Commands

Keys in `ReaCtrlSurf_OSK_CMD`:

| Key | Payload |
| --- | --- |
| `WidgetPressDown` | `surface|widget` |
| `WidgetPressUp` | `surface|widget` |
| `WidgetScroll` | `surface|widget|accelerationIndex|signedEventCount` |
| `ConfigQuery` | `surface|widget` |
| `ConfigApplyLive` | `surface|widget|serializedBindings` |
| `ConfigSave` | `surface|widget` |
| `ConfigRevert` | `surface|widget` |
| `ActionListQuery` | Empty payload |

`WidgetScroll` uses a non-negative acceleration index and an event count from `-8` to
`8`, excluding zero. The sign is the direction. Lua rate-limits and coalesces wheel
events before publishing this command.

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

`ReaCtrlSurf_OSD` contains one key:

| Key | Payload |
| --- | --- |
| `OSD` | `text;background;timeoutMs` |

`background` is `0`, `1`, or `#RRGGBB`. The OSK embedded bar and standalone OSD
consume the same message. Lua treats each write as an event and deletes the key after
consuming it so identical payloads can refresh the visible timeout.

## Settings

Persistent keys in `ReaCtrlSurf_OSK_SETTINGS`:

- `zoom`
- `interactive`
- `aspect`
- `pad_h`
- `pad_v`
- `transparency`
- `btn_transparency`
- `tooltip_delay`
- `arrow_angle`
- `titlebar_enabled`
- `label_replacements`
- `SurfacePosition_<surface>` with payload `x,y`

OSK uses one independent window per surface. Position writes are delayed while a
window is moving and flushed on shutdown.

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
