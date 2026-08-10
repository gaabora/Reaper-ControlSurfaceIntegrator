# ReaControlSurface Configuration Format

> This page contains legacy syntax examples that will be replaced during Phase 3 of the configuration workflow. The path structure below matches the current runtime; do not use later legacy path examples as the current layout.

## Overview

ReaControlSurface configuration consists of three main file types:
1. **ReaControlSurface.ini** - Main plugin configuration
2. **.zon files** - Zone definitions (widget-to-action mappings)
3. **.txt files (Surface Templates)** - Hardware widget definitions

## File Locations

All current configuration files are located in the REAPER resource path:

**Windows**: `%APPDATA%\REAPER\`
**macOS**: `~/Library/Application Support/REAPER/`
**Linux**: `~/.reaper/`

```
REAPER/
├── Data/ReaControlSurface/
│   ├── ReaControlSurface.ini
│   ├── Surfaces/
│   │   ├── Vendor/<surface-id>.txt
│   │   └── User/<surface-id>.txt
│   ├── Zones/
│   │   ├── Vendor/<profile-id>/
│   │   │   ├── Main/*.zon
│   │   │   └── FX/*.zon
│   │   └── User/<profile-id>/
│   │       ├── Main/*.zon
│   │       └── FX/*.zon
│   ├── Snippets/
│   │   ├── BuiltIn/*.snippet
│   │   └── User/*.snippet
│   ├── Backups/
│   └── Generated/ZoneRawFXFiles/
├── Scripts/ReaControlSurface/
└── UserPlugins/
    └── reaper_csurf_integrator.*
```

## ReaControlSurface.ini - Main Configuration File

The main configuration file controls plugin-wide settings and defines Pages and Surfaces.

### File Format

```ini
; Comment lines start with semicolon

; General version info
[Version]
MajorVersion=7.0 //FIXME: to change

; Global settings
[Options]
DebugLevel=Error              ; Error, Warning, Info, Debug
SurfaceInDisplay=0            ; Show surface input display
SurfaceOutDisplay=0           ; Show surface output display
SurfaceRawInDisplay=0         ; Show raw MIDI input
FXParamsWrite=0               ; Write FX param changes to console

; Pages section lists all UI pages
[Pages]
; Format: PageName ZoneFolder FXZoneFolder
MainPage          Zones FXZones
MixerPage         Zones FXZones
BrowserPage       Zones FXZones

; Surfaces section defines control devices
[Surfaces]
; Format: PageName|SurfaceType|SurfaceName|Config...

; MIDI Surface example
MainPage|MIDI|MCU|MCU.zon|0|Channels=8|Refresh=30|MaxSysEx=500
MainPage|MIDI|XTouch|XTouch.zon|0|Channels=8|Refresh=30

; OSC Surface example
MixerPage|OSC|X32|X32Config.zon|inPort=8000|outPort=8001|outIP=192.168.1.100

; MIDI Surface Parameters
;   - Refresh: Update frequency in ms (default: 30)
;   - MaxSysEx: Max SysEx messages per cycle
;   - Channels: Number of channels for MCU

; OSC Surface Parameters
;   - inPort: Incoming OSC port
;   - outPort: Outgoing OSC port
;   - outIP: Destination IP address
```

### Page Definition

Pages are organizational containers for surfaces. Each page can have multiple MIDI and OSC surfaces.

**Page Properties**:
- `ZoneFolder`: Path to zone definitions relative to `REAPER/CSI/`
- `FXZoneFolder`: Path to FX-specific zones relative to `REAPER/CSI/`
- Multiple surfaces can belong to one page
- Surfaces on same page can share zone definitions

## Surface Template Files (.zon format)

Surface templates define the hardware widget layout and their MIDI/OSC mappings.

### MIDI Surface Template

**Filename**: Surface definition in `REAPER/CSI/Surfaces/`

**Format**:
```
; Surface template for Mackie Control Universal

Widget Fader1 CC 0x0E
Widget Fader2 CC 0x0F
Widget Fader3 CC 0x10
Widget Fader4 CC 0x11
Widget Fader5 CC 0x12
Widget Fader6 CC 0x13
Widget Fader7 CC 0x14
Widget Fader8 CC 0x15

Widget VPot1 Encoder Single Pitch 0x10
Widget VPot2 Encoder Single Pitch 0x11
Widget VPot3 Encoder Single Pitch 0x12
Widget VPot4 Encoder Single Pitch 0x13

Widget Select1 Note 0x18
Widget Select2 Note 0x19
Widget Select3 Note 0x1A

Widget Mute1 Note 0x08
Widget Mute2 Note 0x09

; Display widgets (output only)
Widget ChannelDisplay1 Display

; Special MCU display
Widget MCUDisplay SpecialDisplay MCU 7SegmentDisplay
```

**MIDI Widget Types**:

| Type | Format | Description |
|------|--------|-------------|
| CC | `CC 0xNN` | Continuous Controller (CC) on channel 1 |
| Note | `Note 0xNN` | Note message |
| Program | `Program 0xNN` | Program Change |
| Pitch | `Pitch` | Pitch bend |
| Encoder | `Encoder Mode Accel Type` | Rotary encoder (see table below) |
| ChannelPressure | `ChannelPressure` | Channel pressure |
| PolyPressure | `PolyPressure 0xNN` | Polyphonic pressure |

**Encoder Modes**:
- `Single`: Single rotary encoder
- `Dual`: Dual (14-bit) encoder
- `SurfaceTrackVolume`: Fader for track volume
- `SurfaceTrackPan`: Potentiometer for pan

**Encoder Acceleration Types**:
- `Pitch`: Use pitch-bend acceleration
- `Relative`: Relative mode
- `Absolute`: Absolute mode

**Encoder Format Examples**:
```
Widget VPot1 Encoder Single Pitch 0x10
Widget SuperEncoder Encoder Dual Relative 0x40
```

### OSC Surface Template

**Format**:
```
; OSC Surface template for Behringer X32

Widget Fader1 /ch/01/mix/fader fader 0 1
Widget Fader2 /ch/02/mix/fader fader 0 1
Widget Fader3 /ch/03/mix/fader fader 0 1

Widget Button1 /ch/01/mix/on button
Widget Button2 /ch/02/mix/on button

Widget Pan1 /ch/01/mix/pan pan -0.5 0.5
Widget Pan2 /ch/02/mix/pan pan -0.5 0.5

; Display widgets
Widget Display1 /output/main/display1 display
Widget Display2 /output/main/display2 display

; Rotary controls
Widget Encoder1 /control/encoder1 encoder 0 1
```

**OSC Widget Types**:

| Type | Format | Example |
|------|--------|---------|
| fader | `/path fader min max` | `/ch/01/fader fader 0 1` |
| pan | `/path pan min max` | `/ch/01/pan pan -0.5 0.5` |
| button | `/path button` | `/ch/01/on button` |
| encoder | `/path encoder min max` | `/fader/1 encoder 0 1` |
| display | `/path display` | `/display/1 display` |
| color | `/path color` | `/button/1/color color` |

## Zone Files (.zon)

Zone files define the mapping between hardware widgets and REAPER actions.

### Basic Structure

```
Zone "ZoneName" ["Alias"] [Properties...]
  Widget WidgetName ActionName [Parameters...]
  Widget WidgetName+Modifier ActionName [Parameters...]
  
  SubZones
    SubZoneName1
    SubZoneName2
  SubZonesEnd
  
  IncludedZones
    IncludedZoneName
  IncludedZonesEnd
ZoneEnd
```

### Zone Definition

**Zone Header**:
```
Zone "TrackMixer" "Master Mixer" NavType=MasterTrackNavigator
```

**Properties**:
- `NavType`: Navigator type for this zone
  - `TrackNavigator` (default)
  - `MasterTrackNavigator`
  - `SelectedTrackNavigator`
  - `FocusedFXNavigator`
  - `VCANavigator`
  - `FolderNavigator`

**Zone Name and Alias**:
- Name: Internal identifier used for zone navigation
- Alias: Friendly display name
- Alias is optional; if omitted, name is used

### Widget Mapping

**Format**:
```
Widget WidgetName ActionName [Param1] [Param2] ...
```

**Simple Mapping**:
```
Zone "Mixer"
  Widget Fader1 TrackVolume
  Widget VPot1 TrackPan
  Widget Button1 Mute
ZoneEnd
```

**Mapping with Parameters**:
```
Widget FXParamKnob FXParam 0 FXParamType=Float Range=0>1
Widget DisplayWidget TrackVolumeDisplay DisplayFormat=dB
```

### Modifiers

Modifiers change the action based on key state:

**Format**: `Widget WidgetName+Modifier ActionName`

**Available Modifiers**:
- `Shift`: Shift key
- `Alt`: Alt/Option key
- `Control`: Control/Cmd key
- `Hold`: Hold (press and hold) mode
- `DoublePress`: Double-press pseudo-modifier

**Examples**:
```
Widget Fader1 TrackVolume
Widget Fader1+Shift TrackPan            ; Same widget, pan when Shift held
Widget Button1 Play
Widget Button1+Control Stop             ; Stop when Ctrl+Button1
Widget Button1+Shift+Control Record     ; Record when Shift+Ctrl+Button1
```

**Modifier Combinations**:
- Multiple modifiers can be combined
- Order doesn't matter: `Shift+Alt` = `Alt+Shift`

### Value Modifiers

Value modifiers affect how widget input is interpreted:

**Inversion Prefix**: `~` - Inverts the input value
```
Widget Fader1 ~TrackVolume              ; Reverse fader direction
```

**Feedback Inversion Prefix**: `^` - Inverts display feedback
```
Widget Button1 ^Mute                    ; Light is on when NOT muted
```

**Combination**:
```
Widget Control ~^MuteToggle             ; Inverted input and output
```

### Accelerated/Stepped Values

Accelerated and stepped values are enclosed in brackets after parameters.

**Format**:
```
Widget EncoderName ActionName [values...]
```

**Types**:

1. **Single Delta Value** (for smooth encoders):
```
Widget VPot1 TrackPan [0.01]            ; Increment by 0.01 per tick
Widget VPot2 Pitch [(0.1,0.2,0.3,0.4)]  ; Accelerated deltas
```

2. **Stepped Values** (discrete steps):
```
Widget Button1 Tempo [
  60>80
  80>120
  120>200
]                                        ; Stepped tempo values
```

3. **Acceleration Ticks** (for relative encoders):
```
Widget VPot3 TrackVolume [
  (10,20,30,40)                         ; Acceleration tick values
]
```

4. **Range Specification**:
```
Widget Slider1 FXParam [1.0>10.0]       ; Range from 1.0 to 10.0
```

### SubZones

SubZones provide hierarchical navigation within a zone:

```
Zone "TrackMixer"
  SubZones
    Send1Zone
    Send2Zone
    PluginsZone
  SubZonesEnd
ZoneEnd

Zone "Send1Zone"
  Widget VPot1 SendVolume 0
  Widget Fader1 SendPan 0
ZoneEnd
```

**Navigation Actions**:
```
Widget Button1 GoSubZone Send1Zone      ; Enter subzone
Widget Button2 LeaveSubZone             ; Exit subzone
```

### Included Zones

IncludedZones allow code reuse by including definitions from other zones:

```
Zone "MasterMixer"
  IncludedZones
    CommonMixerControls
    CommonDisplays
  IncludedZonesEnd
  
  Widget MasterFader MasterVolume
ZoneEnd

Zone "CommonMixerControls"
  Widget Button1 Play
  Widget Button2 Stop
ZoneEnd
```

## Action Parameters

Different actions accept different parameters:

### FX Parameter Actions
```
Widget VPot1 FXParam 0                          ; FX slot 0
Widget VPot2 TCPFXParam 1                       ; TCP FX slot 1
Widget VPot3 LastTouchedFXParam 2               ; Last touched FX
```

**Format**: `ActionName [FXSlotIndex]`

### Volume/Pan Actions
```
Widget Fader1 TrackVolume
Widget Fader2 SendVolume 0                      ; Send 0
Widget Fader3 RecvVolume 1                      ; Receive 1
Widget VPot1 SendPan 0                          ; Send 0 pan
Widget VPot2 RecvPan 1                          ; Receive 1 pan
```

### Display Actions
```
Widget LCD1 TrackVolumeDisplay
Widget LCD2 TrackNameDisplay
Widget LCD3 FXNameDisplay
```

### Navigation Actions
```
Widget Button1 GoZone MixerZone
Widget Button2 GoSubZone SendsZone
Widget Button3 LeaveSubZone
Widget Button4 GoHome                           ; Return to default zone
Widget Button5 GoFXSlot 0                       ; Navigate FX slot
```

### Messaging Actions
```
Widget Button1 SendMIDIMessage 90 3C 7F         ; Send Note On
Widget Button2 SendOSCMessage /fader1 0.5
Widget Button3 SpeakOSARAMessage "Play button pressed"
```

### Manager Actions
```
Widget Button1 SaveProject
Widget Button2 Undo
Widget Button3 Redo
Widget Button4 SetXTouchDisplayColors RGB(255,0,0)
```

### Color Specification

Colors can be specified in RGB format:

```
RGB(red, green, blue)                   ; Red=0-255, Green=0-255, Blue=0-255
RGB(255, 0, 0)                          ; Red
RGB(0, 255, 0)                          ; Green
RGB(0, 0, 255)                          ; Blue
```

**Example**:
```
Widget StatusLight SetXTouchDisplayColors RGB(0,255,0)
```

## Property System

Widgets and zones support properties for advanced configuration:

**Widget Properties**:
```
Widget WidgetName ActionName DisplayFormat=dB
Widget WidgetName ActionName Range=0>100
Widget WidgetName ActionName Smoothing=0.1
```

**Zone Properties**:
```
Zone "MixerZone" NavType=TrackNavigator ListenToSends=1
```

**Available Properties**:
- `DisplayFormat`: dB, Percent, Raw, Custom
- `Range`: Min>Max specification
- `Smoothing`: Value smoothing factor
- `NavType`: Navigator type
- `ListenToSends`: Enable send listening
- `ListenToReceives`: Enable receive listening
- `ListenToFXMenu`: Enable FX menu
- `ListenToSelectedTrackFX`: Enable selected track FX

## Comments and Special Lines

**Comments**:
```
; This is a comment
// This is also a comment
# This is also a comment

Zone "Mixer"
  ; Widget commented out below
  ; Widget Fader1 TrackVolume
  Widget Fader2 TrackPan
ZoneEnd
```

**Special Markers** (auto-generated sections):
```
Zone "Mixer"
  #Begin auto generated section
  Widget VPot1 FXParam 0
  Widget VPot2 FXParam 1
  #End auto generated section
ZoneEnd
```

Auto-generated sections are created by Learn mode and preserved during re-generation.

## Configuration Example: Complete Setup

### Minimal Configuration

**CSI.ini**:
```ini
[Version]
MajorVersion=7.0

[Options]
DebugLevel=Error

[Pages]
MainPage Zones FXZones

[Surfaces]
MainPage|MIDI|MyCU|MCU.zon|0|Channels=8|Refresh=30
```

**Zones/Mixer.zon**:
```
Zone "Mixer" "Main Mixer"
  Widget Fader1 TrackVolume
  Widget Fader2 TrackVolume
  Widget Fader3 TrackVolume
  Widget Fader4 TrackVolume
  Widget Fader5 TrackVolume
  Widget Fader6 TrackVolume
  Widget Fader7 TrackVolume
  Widget Fader8 TrackVolume
  
  Widget VPot1 TrackPan
  Widget VPot2 TrackPan
  Widget VPot3 TrackPan
  Widget VPot4 TrackPan
  Widget VPot5 TrackPan
  Widget VPot6 TrackPan
  Widget VPot7 TrackPan
  Widget VPot8 TrackPan
  
  Widget Select1 TrackSelect
  Widget Select2 TrackSelect
  Widget Select3 TrackSelect
  Widget Select4 TrackSelect
  Widget Select5 TrackSelect
  Widget Select6 TrackSelect
  Widget Select7 TrackSelect
  Widget Select8 TrackSelect
  
  Widget Mute1 Mute
  Widget Mute2 Mute
  Widget Mute3 Mute
  Widget Mute4 Mute
  Widget Mute5 Mute
  Widget Mute6 Mute
  Widget Mute7 Mute
  Widget Mute8 Mute
ZoneEnd
```

**Surfaces/MCU.zon**:
```
Widget Fader1 CC 0x0E
Widget Fader2 CC 0x0F
Widget Fader3 CC 0x10
Widget Fader4 CC 0x11
Widget Fader5 CC 0x12
Widget Fader6 CC 0x13
Widget Fader7 CC 0x14
Widget Fader8 CC 0x15

Widget VPot1 Encoder Single Pitch 0x10
Widget VPot2 Encoder Single Pitch 0x11
Widget VPot3 Encoder Single Pitch 0x12
Widget VPot4 Encoder Single Pitch 0x13
Widget VPot5 Encoder Single Pitch 0x14
Widget VPot6 Encoder Single Pitch 0x15
Widget VPot7 Encoder Single Pitch 0x16
Widget VPot8 Encoder Single Pitch 0x17

Widget Select1 Note 0x18
Widget Select2 Note 0x19
Widget Select3 Note 0x1A
Widget Select4 Note 0x1B
Widget Select5 Note 0x1C
Widget Select6 Note 0x1D
Widget Select7 Note 0x1E
Widget Select8 Note 0x1F

Widget Mute1 Note 0x08
Widget Mute2 Note 0x09
Widget Mute3 Note 0x0A
Widget Mute4 Note 0x0B
Widget Mute5 Note 0x0C
Widget Mute6 Note 0x0D
Widget Mute7 Note 0x0E
Widget Mute8 Note 0x0F
```

## Best Practices

1. **Use Descriptive Names**: Zone and widget names should clearly indicate purpose
   ```
   Zone "TrackMixer"      ; Good
   Zone "TM"              ; Avoid
   ```

2. **Organize with SubZones**: Keep zones focused and use subzones for additional views
   ```
   Zone "Mixer" → SubZones: SendsZone, PluginsZone, EQZone
   ```

3. **Use IncludedZones**: Reduce duplication with common control sets
   ```
   IncludedZones
     CommonTransports
     CommonSelection
   ```

4. **Document Complex Mappings**: Add comments explaining non-obvious configurations
   ```
   ; Shift+Fader1 controls send volume for special sends
   Widget Fader1+Shift SendVolume 2
   ```

5. **Test Incrementally**: Add widgets/zones gradually and test thoroughly

## Error Messages

Common errors during configuration loading:

| Error | Cause | Fix |
|-------|-------|-----|
| "Cannot find Zone folder" | Zone path missing | Create directory in REAPER/CSI/ |
| "Zone not found" | Referenced zone doesn't exist | Check zone file and name |
| "Widget not found" | Widget undefined in surface | Add widget to surface template |
| "Unknown Action" | Action doesn't exist | Check action name spelling |
| "Invalid parameter" | Action parameter incorrect | See action parameters section |

## References

- REAPER MIDI CC Reference: https://www.reaper.fm/
- MIDI Specification: https://en.wikipedia.org/wiki/MIDI
- OSC Specification: http://opensoundcontrol.org/
