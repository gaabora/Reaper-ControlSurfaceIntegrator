# ReaControlSurface Quick Reference Guide

A fast lookup guide for the most common CSI tasks and configurations.

> The minimal configuration examples below still describe the legacy syntax. Phase 3 will replace them. The file-location tree is current.

## Essential File Locations

```
REAPER/
├── Data/ReaControlSurface/
│   ├── ReaControlSurface.ini
│   ├── Surfaces/Vendor/<surface-id>.txt
│   ├── Surfaces/User/<surface-id>.txt
│   ├── Zones/Vendor/<profile-id>/Main/*.zon
│   ├── Zones/Vendor/<profile-id>/FX/*.zon
│   ├── Zones/User/<profile-id>/Main/*.zon
│   ├── Zones/User/<profile-id>/FX/*.zon
│   ├── Snippets/
│   ├── Backups/
│   └── Generated/ZoneRawFXFiles/
├── Scripts/ReaControlSurface/
└── UserPlugins/
    └── reaper_csurf_integrator.*  # Plugin binary
```

## Minimal Configuration

### Step 1: Create ReaControlSurface.ini
```ini
[Version]
MajorVersion=7.0

[Pages]
Main Zones FXZones

[Surfaces]
Main|MIDI|MCU|MCU.zon|0|Channels=8|Refresh=30
```

### Step 2: Create Surfaces/MCU.zon
```
Widget Fader1 CC 0x0E
Widget Fader2 CC 0x0F
Widget Button1 Note 0x18
Widget VPot1 Encoder Single Pitch 0x10
```

### Step 3: Create Zones/Mixer.zon
```
Zone "Mixer"
  Widget Fader1 TrackVolume
  Widget Fader2 TrackVolume
  Widget VPot1 TrackPan
  Widget Button1 Mute
ZoneEnd
```

## Common Action Patterns

### Volume Controls
```
Widget Fader1 TrackVolume           ; Main volume
Widget Fader1+Shift SendVolume 0    ; Send 0 with modifier
Widget Fader1+Alt RecvVolume 0      ; Receive 0 with modifier
Widget Fader8 MasterVolume          ; Master fader
```

### FX Parameter Control
```
Widget VPot1 FXParam 0              ; FX slot 0 parameter
Widget VPot2 FXParam 1              ; FX slot 1 parameter
Widget VPot3 LastTouchedFXParam     ; Follow user's touches
```

### Transport Controls
```
Widget Button1 Play
Widget Button2 Stop
Widget Button3 Record
Widget Button4 Repeat
```

### Zone Navigation
```
Widget Button5 GoZone SendsZone
Widget Button6 GoHome
Widget Button7 GoSubZone PluginsZone
Widget Button8 LeaveSubZone
```

### Display Feedback
```
Widget Display1 TrackNameDisplay
Widget Display2 TrackVolumeDisplay
Widget Display3 MuteDisplay
```

## Modifiers Syntax

```
Widget Control Action              ; Ctrl modifier
Widget Shift+Alt+Control Action    ; Multiple modifiers
Widget Control+Double Action       ; Hold and double-press
```

**Available Modifiers**: Shift, Alt, Control, Hold, DoublePress

## Value Specifications

### Single Value
```
Widget VPot1 TrackPan [0.01]
```

### Multiple Acceleration Values
```
Widget VPot1 TrackPan [(0.01, 0.02, 0.03, 0.04)]
```

### Stepped Values
```
Widget Button1 Tempo [60, 80, 100, 120, 140]
```

### Range Limits
```
Widget Fader1 FXParam 0 [0.0>1.0]
```

## Zone File Template

```
Zone "MyZone" "Friendly Name" NavType=TrackNavigator
  ; Volume controls
  Widget Fader1 TrackVolume
  Widget Fader2 TrackVolume
  
  ; Pan controls
  Widget VPot1 TrackPan
  Widget VPot2 TrackPan
  
  ; Mute/Solo
  Widget Button1 Mute
  Widget Button2 Solo
  Widget Button3 RecArm
  
  ; Selection
  Widget Button4 TrackSelect
  
  ; Navigation
  Widget Button5 GoSubZone SendsZone
  Widget Button6 LeaveSubZone
  
  ; Display feedback
  Widget Display1 TrackNameDisplay
  Widget Display2 TrackVolumeDisplay
  
  SubZones
    SendsZone
    PluginsZone
  SubZonesEnd
ZoneEnd

Zone "SendsZone"
  Widget Fader1 SendVolume 0
  Widget Fader2 SendVolume 1
ZoneEnd

Zone "PluginsZone"
  Widget VPot1 FXParam 0
  Widget VPot2 FXParam 1
  Widget Button1 ToggleFXBypass 0
ZoneEnd
```

## Common MIDI Messages

### CC (Continuous Controller)
```
Widget VolumeFader CC 0x07         ; MIDI CC#7 (volume standard)
Widget PanPot CC 0x0A              ; MIDI CC#10 (pan standard)
```

### Notes
```
Widget Button1 Note 0x18           ; Note C1
Widget Button2 Note 0x19           ; Note C#1
```

### Encoders
```
Widget Knob1 Encoder Single Pitch 0x10        ; Single 7-bit
Widget Knob2 Encoder Dual Relative 0x11       ; 14-bit dual
```

## Action Quick Reference

| Category | Examples |
|----------|----------|
| **Volume** | TrackVolume, SendVolume, RecvVolume, MasterVolume |
| **Pan** | TrackPan, TrackPanWidth, SendPan, RecvPan |
| **FX** | FXParam, JSFXParam, ToggleFXBypass, LastTouchedFXParam |
| **Transport** | Play, Stop, Record, Pause, Repeat |
| **Navigation** | GoZone, GoHome, GoSubZone, GoFXSlot |
| **Track** | Mute, Solo, RecArm, TrackSelect |
| **Project** | SaveProject, Undo, Redo |
| **Display** | TrackNameDisplay, TrackVolumeDisplay, MuteDisplay |
| **Messaging** | SendMIDIMessage, SendOSCMessage |

## Navigator Types

```
NavType=TrackNavigator              ; Fixed track index
NavType=MasterTrackNavigator        ; Master track
NavType=SelectedTrackNavigator      ; Currently selected
NavType=FocusedFXNavigator          ; Track with focused FX
NavType=VCANavigator                ; VCA tracks
NavType=FolderNavigator             ; Folder tracks
```

## Troubleshooting Checklist

- [ ] Zone files in correct folder: `REAPER/CSI/Zones/`
- [ ] Surface templates in: `REAPER/CSI/Surfaces/`
- [ ] Widget names match exactly between surface and zone
- [ ] Action names spelled correctly (case-sensitive)
- [ ] Action parameters are valid (FX slot indices, send indices)
- [ ] Zone names referenced in GoZone actions exist
- [ ] CSI.ini points to correct zone and FX zone folders
- [ ] MIDI surface file names match CSI.ini configuration
- [ ] All zones properly closed with `ZoneEnd`
- [ ] No syntax errors in configuration files

## Common Errors & Fixes

| Error | Fix |
|-------|-----|
| Widget not found | Check widget name in surface template |
| Zone not found | Verify zone file exists and name matches |
| Unknown Action | Check action spelling and case |
| No output from surface | Check MIDI device in CSI.ini |
| Faders don't move | Ensure surface template CC values correct |
| Display blank | Verify display widget defined in surface |

## Performance Tips

1. **Use SubZones** instead of multiple zones for organization
2. **Limit zones per surface** to 10-15 for performance
3. **Use IncludedZones** to avoid duplicate definitions
4. **Enable output display only during testing** (impacts CPU)

## Learning Resources

1. [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) - How CSI works internally
2. [Configuration-Format.md](Configuration-Format.md) - All configuration options
3. [Actions-Reference.md](Actions-Reference.md) - Complete action reference
4. [Home.md](Home.md) - Documentation index

## Online Resources

- **REAPER**: https://www.reaper.fm/
- **CSI Original**: https://github.com/GeoffAWaddington/CSICode
- **CSI Fork**: https://github.com/gaabora/Reaper-ControlSurfaceIntegrator
- **MIDI Info**: https://www.midi.org/

## Essential Commands (Debug)

In `CSI.ini` for troubleshooting:
```ini
[Options]
DebugLevel=Debug                ; Verbose logging
SurfaceInDisplay=1              ; Show input display
SurfaceOutDisplay=1             ; Show output display
SurfaceRawInDisplay=1           ; Show raw MIDI
```

NOTICE, WARNING, and ERROR messages appear through the ReaControlSurface Notifications script. Full output is stored in `Data/ReaControlSurface/ReaControlSurface.log`.

## Zone File Comment Syntax

```
// Comment at line start
```

## Special Characters

| Character | Purpose | Example |
|-----------|---------|---------|
| `~` | Invert input value | `Widget Button1 ~Mute` |
| `^` | Invert feedback | `Widget LED1 ^Mute` |
| `+` | Add modifier | `Widget Knob1+Shift Action` |
| `[` `]` | Value specification | `Widget Fader1 Action [0.01]` |

## Getting Help

1. Check ReaControlSurface notifications and `Data/ReaControlSurface/ReaControlSurface.log`
2. Verify syntax against [Configuration-Format.md](Configuration-Format.md)
3. Verify action names against [Actions-Reference.md](Actions-Reference.md)
4. Review example zones in CSI folder
5. Consult [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) for detailed explanations

---

**Last Updated**: March 2026 | CSI v7.0


