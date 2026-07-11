# CSI Configurable Actions Documentation

## Overview

CSI provides a comprehensive set of actions that can be mapped to hardware widgets. Actions are organized into categories based on their function: FX control, track parameters, navigation, messaging, and more.

**Total Action Categories**: 10+ 
**Total Individual Actions**: 100+

## Action Syntax

All actions follow the same basic syntax in zone files:

```
Widget WidgetName ActionName [Parameters...]
```

**Examples**:
```
Widget Fader1 TrackVolume                    ; No parameters
Widget VPot1 FXParam 0                       ; With FX slot parameter
Widget Display1 TrackVolumeDisplay           ; Display action
Widget Button1 GoZone MixerZone              ; Navigation with zone name
```

## Action Categories

---

## 1. FX Parameter Actions

Control plugin/effect parameters across different FX contexts.

### FXParam
**Description**: Control FX parameter on a specific FX slot
**File**: `control_surface_Reaper_actions.h`
**Parameters**: `FXSlotIndex`
**Input Type**: Continuous or Discrete
**Feedback**: Returns 0.0-1.0 normalized value
**Context**: TrackNavigator, SelectedTrackNavigator

**Usage**:
```
Widget VPot1 FXParam 0              ; FX slot 0
Widget VPot2 FXParam 1              ; FX slot 1
Widget VPot3 FXParam 2              ; FX slot 2
```

**Value Range**: 0.0 (minimum) to 1.0 (maximum) normalized

**Special Features**:
- Supports stepped values for discrete parameters
- Acceleration for smooth parameter control
- Touch detection for automation

### JSFXParam
**Description**: Control JSFX (Jesusonic FX) parameter
**Parameters**: `FXSlotIndex`
**Notes**: Same interface as FXParam but specifically for JS FX

**Usage**:
```
Widget VPot1 JSFXParam 0
```

### TCPFXParam
**Description**: Control FX parameter in TCP (Track Control Panel) context
**Parameters**: `FXSlotIndex`
**Context**: TCP-specific FX focus
**Notes**: Used for TCP-resident FX only

**Usage**:
```
Widget VPot1 TCPFXParam 0
```

### LastTouchedFXParam
**Description**: Control the last-touched FX parameter
**Parameters**: None
**Context**: Dynamic FX tracking
**Notes**: Automatically follows user's last FX interaction in REAPER

**Usage**:
```
Widget VPot1 LastTouchedFXParam
```

**Special Behavior**:
- Monitors last-touched FX plugin
- Switches context automatically when user touches different FX
- Useful for hands-on FX tweaking

### ToggleFXBypass
**Description**: Toggle FX on/off
**Parameters**: `FXSlotIndex`
**Input Type**: Button (momentary or toggle)
**Feedback**: 0.0 = bypassed, 1.0 = enabled

**Usage**:
```
Widget Button1 ToggleFXBypass 0     ; Toggle FX 0
```

### FXBypassDisplay
**Description**: Display FX bypass state as text
**Parameters**: `FXSlotIndex`
**Output**: "Enabled" or "Bypassed"

**Usage**:
```
Widget ChannelDisplay1 FXBypassDisplay 0
```

### ToggleFXOffline
**Description**: Toggle FX offline state
**Parameters**: `FXSlotIndex`
**Input Type**: Button
**Feedback**: 0.0 = offline, 1.0 = online

**Usage**:
```
Widget Button1 ToggleFXOffline 0
```

### FXOfflineDisplay
**Description**: Display FX online/offline state
**Parameters**: `FXSlotIndex`
**Output**: "Online" or "Offline"

**Usage**:
```
Widget ChannelDisplay1 FXOfflineDisplay 0
```

---

## 2. Volume Actions

Control track, send, and receive volume levels.

### TrackVolume
**Description**: Control track main volume
**Parameters**: None
**Input Type**: Fader/continuous
**Feedback**: 0.0-1.0 normalized (0dB at ~0.75)
**Navigation**: TrackNavigator, SelectedTrackNavigator, MasterTrackNavigator

**Usage**:
```
Widget Fader1 TrackVolume
Widget Fader1+Shift TrackVolume              ; Alternative mapping
```

**Value Interpretation**:
- 0.0 = -∞ (muted)
- ~0.75 = 0dB
- 1.0 = +6dB (typically max)

### SoftTakeover7BitTrackVolume
**Description**: Track volume with soft takeover for 7-bit MIDI
**Parameters**: None
**Special Feature**: Prevents sudden jumps when fader doesn't match current value
**Best For**: MIDI controllers without motorized faders

**Usage**:
```
Widget Fader1 SoftTakeover7BitTrackVolume
```

**How It Works**:
1. Surface fader at 50, REAPER at 75
2. Fader must move within 5% threshold (70-80) to take control
3. Once in threshold, fader has full control

### SoftTakeover14BitTrackVolume
**Description**: Track volume with soft takeover for 14-bit MIDI
**Parameters**: None
**Notes**: Higher resolution than 7-bit version

**Usage**:
```
Widget Fader1 SoftTakeover14BitTrackVolume
```

### SendVolume
**Description**: Control send volume for a track
**Parameters**: `SendIndex` (0-based)
**Input Type**: Fader/continuous
**Navigation**: TrackNavigator, SelectedTrackNavigator

**Usage**:
```
Widget Fader1 SendVolume 0          ; Send 0 volume
Widget Fader2 SendVolume 1          ; Send 1 volume
Widget Fader3 SendVolume 2          ; Send 2 volume
```

**Range**: Same as TrackVolume

### SendVolumeDisplay
**Description**: Display send volume as numeric value
**Parameters**: `SendIndex`
**Output**: "dB" format

**Usage**:
```
Widget Display1 SendVolumeDisplay 0
```

### RecvVolume
**Description**: Control receive volume for a track
**Parameters**: `RecvIndex` (0-based)
**Input Type**: Fader/continuous
**Navigation**: TrackNavigator, SelectedTrackNavigator

**Usage**:
```
Widget Fader1 RecvVolume 0          ; Receive 0 volume
Widget Fader2 RecvVolume 1          ; Receive 1 volume
```

### MasterVolume
**Description**: Control master track volume
**Parameters**: None
**Input Type**: Fader/continuous

**Usage**:
```
Widget Fader1 MasterVolume
```

### MasterVolumeDisplay
**Description**: Display master volume
**Parameters**: None
**Output**: "dB" format

**Usage**:
```
Widget Display1 MasterVolumeDisplay
```

---

## 3. Pan Actions

Control stereo panning and width.

### TrackPan
**Description**: Control track pan position (standard balance/pan)
**Parameters**: None
**Input Type**: Potentiometer/continuous
**Feedback**: -1.0 (left) to 1.0 (right), 0.0 = center
**Navigation**: TrackNavigator, SelectedTrackNavigator, MasterTrackNavigator

**Usage**:
```
Widget VPot1 TrackPan
```

**Pan Modes**:
- Default: Balance mode (traditional pan)
- Requires PanMode property for other modes

### TrackPanWidth
**Description**: Control stereo width (dual-mono pan)
**Parameters**: None
**Input Type**: Potentiometer
**Feedback**: 0.0-1.0 (0.0 = mono, 1.0 = full stereo)

**Usage**:
```
Widget VPot1 TrackPanWidth
```

### TrackPanLeft
**Description**: Control left channel of dual pan
**Parameters**: None

**Usage**:
```
Widget VPot1 TrackPanLeft
```

### TrackPanRight
**Description**: Control right channel of dual pan
**Parameters**: None

**Usage**:
```
Widget VPot2 TrackPanRight
```

### SendPan
**Description**: Control send pan position
**Parameters**: `SendIndex`
**Input Type**: Potentiometer
**Feedback**: -1.0 to 1.0

**Usage**:
```
Widget VPot1 SendPan 0              ; Send 0 pan
Widget VPot2 SendPan 1              ; Send 1 pan
```

### SendPanDisplay
**Description**: Display send pan value
**Parameters**: `SendIndex`
**Output**: "-100L" to "+100R" format

**Usage**:
```
Widget Display1 SendPanDisplay 0
```

### RecvPan
**Description**: Control receive pan position
**Parameters**: `RecvIndex`
**Input Type**: Potentiometer
**Feedback**: -1.0 to 1.0

**Usage**:
```
Widget VPot1 RecvPan 0
```

### MasterPan
**Description**: Control master pan
**Parameters**: None

**Usage**:
```
Widget VPot1 MasterPan
```

---

## 4. Track Control Actions

Control mute, solo, arm, and selection states.

### Mute
**Description**: Toggle track mute
**Parameters**: None
**Input Type**: Button (momentary or toggle)
**Feedback**: 0.0 = unmuted, 1.0 = muted

**Usage**:
```
Widget Button1 Mute
```

### MuteDisplay
**Description**: Display mute state
**Parameters**: None
**Output**: "Muted" or "Unmuted"

**Usage**:
```
Widget Display1 MuteDisplay
```

### Solo
**Description**: Toggle track solo
**Parameters**: None
**Input Type**: Button
**Feedback**: 0.0 = not soloed, 1.0 = soloed

**Usage**:
```
Widget Button1 Solo
```

### SoloDisplay
**Description**: Display solo state
**Parameters**: None
**Output**: "Soloed" or "Normal"

**Usage**:
```
Widget Display1 SoloDisplay
```

### RecArm
**Description**: Toggle track record arm
**Parameters**: None
**Input Type**: Button
**Feedback**: 0.0 = not armed, 1.0 = armed

**Usage**:
```
Widget Button1 RecArm
```

### RecArmDisplay
**Description**: Display record arm state
**Parameters**: None
**Output**: "Armed" or "Disarmed"

**Usage**:
```
Widget Display1 RecArmDisplay
```

### TrackSelect
**Description**: Select track
**Parameters**: None
**Input Type**: Button
**Effect**: Selects track in REAPER mixer

**Usage**:
```
Widget Button1 TrackSelect
```

**Multi-Select Behavior**:
```
Widget Button1 TrackSelect              ; Normal: select this track only
Widget Button1+Shift TrackSelect         ; Shift: add to selection
Widget Button1+Control TrackSelect       ; Control: toggle selection
```

### TrackNameDisplay
**Description**: Display track name
**Parameters**: None
**Output**: Track name string

**Usage**:
```
Widget Display1 TrackNameDisplay
```

### MasterTrackName
**Description**: Master track label
**Parameters**: None
**Output**: "Master" or configured name

**Usage**:
```
Widget Display1 MasterTrackName
```

---

## 5. Transport/Playback Actions

Control REAPER transport: play, stop, record, etc.

### Play
**Description**: Toggle play state
**Parameters**: None
**Input Type**: Button
**Feedback**: 0.0 = stopped, 1.0 = playing

**Usage**:
```
Widget Button1 Play
```

### Stop
**Description**: Stop playback
**Parameters**: None
**Input Type**: Button
**Effect**: Returns to previous play position

**Usage**:
```
Widget Button1 Stop
```

### PlayAndStopAtMarker / PlayAndStopAtRegion
**Description**: Play and stop at next marker/region
**Parameters**: None

**Usage**:
```
Widget Button1 PlayAndStopAtMarker
```

### Pause
**Description**: Pause playback (toggle)
**Parameters**: None

**Usage**:
```
Widget Button1 Pause
```

### Record
**Description**: Toggle record mode
**Parameters**: None
**Input Type**: Button
**Feedback**: 0.0 = not recording, 1.0 = recording

**Usage**:
```
Widget Button1 Record
```

### RecordDisplay
**Description**: Display record state
**Parameters**: None
**Output**: "Recording" or "Stopped"

**Usage**:
```
Widget Display1 RecordDisplay
```

### Repeat
**Description**: Toggle repeat/loop mode
**Parameters**: None
**Input Type**: Button
**Feedback**: 0.0 = repeat off, 1.0 = repeat on

**Usage**:
```
Widget Button1 Repeat
```

### RepeatDisplay
**Description**: Display repeat state
**Parameters**: None
**Output**: "Loop" or "No Loop"

**Usage**:
```
Widget Display1 RepeatDisplay
```

### FastForward
**Description**: Jump forward in timeline
**Parameters**: Optional distance (seconds)
**Input Type**: Button

**Usage**:
```
Widget Button1 FastForward
```

### Rewind
**Description**: Jump backward in timeline
**Parameters**: Optional distance
**Input Type**: Button

**Usage**:
```
Widget Button1 Rewind
```

### Tempo
**Description**: Control project tempo (BPM)
**Parameters**: None
**Input Type**: Fader/continuous
**Feedback**: 0.0-1.0 (typical range 20-300 BPM)

**Usage**:
```
Widget Fader1 Tempo [
  60>80
  80>120
  120>200
]
```

### TempoDisplay
**Description**: Display current tempo
**Parameters**: None
**Output**: "120.0 BPM" format

**Usage**:
```
Widget Display1 TempoDisplay
```

---

## 6. Navigation Actions

Navigate between zones, tracks, and FX slots.

### GoZone
**Description**: Switch to a different zone
**Parameters**: `ZoneName`
**Input Type**: Button (momentary best)
**Effect**: Deactivates current zone, activates target zone

**Usage**:
```
Widget Button1 GoZone MixerZone
Widget Button2 GoZone SendsZone
Widget Button3 GoZone PluginsZone
```

**Zone Names** (example):
- MixerZone
- SendsZone
- PluginsZone
- FXZone
- Custom zone names defined in .zon files

### GoHome
**Description**: Return to home/default zone
**Parameters**: None
**Input Type**: Button
**Effect**: Navigates to the first/home zone

**Usage**:
```
Widget Button1 GoHome
```

### GoSubZone
**Description**: Activate a subzone within current zone
**Parameters**: `SubZoneName`
**Input Type**: Button
**Effect**: Enters subzone context

**Usage**:
```
Zone "Mixer"
  Widget Button1 GoSubZone SendsZone
  
  SubZones
    SendsZone
    PluginsZone
  SubZonesEnd
ZoneEnd
```

### LeaveSubZone
**Description**: Exit current subzone
**Parameters**: None
**Input Type**: Button
**Effect**: Returns to parent zone

**Usage**:
```
Widget Button2 LeaveSubZone
```

### GoFXSlot
**Description**: Navigate to specific FX slot
**Parameters**: `FXSlotIndex`
**Input Type**: Button
**Effect**: Changes focused FX for current track

**Usage**:
```
Widget Button1 GoFXSlot 0           ; Focus FX slot 0
Widget Button2 GoFXSlot 1           ; Focus FX slot 1
```

### SelectedTrackFX
**Description**: Navigate to selected track's FX
**Parameters**: None
**Input Type**: Button

**Usage**:
```
Widget Button1 SelectedTrackFX
```

### NextTrack / PreviousTrack
**Description**: Navigate to next/previous track
**Parameters**: None
**Input Type**: Button

**Usage**:
```
Widget Button1 NextTrack
Widget Button2 PreviousTrack
```

### NextSend / PreviousSend
**Description**: Navigate through sends
**Parameters**: None

**Usage**:
```
Widget Button1 NextSend
Widget Button2 PreviousSend
```

---

## 7. Project Management Actions

Save, undo, redo, and project operations.

### SaveProject
**Description**: Save project to disk
**Parameters**: None
**Input Type**: Button
**Feedback**: 0.0 = clean, 1.0 = dirty (needs save)

**Usage**:
```
Widget Button1 SaveProject
```

**Behavior**:
- Saves only if project is dirty
- No-op if already saved

### Undo
**Description**: Undo last action
**Parameters**: None
**Input Type**: Button
**Feedback**: 0.0 = no undo available, 1.0 = undo available

**Usage**:
```
Widget Button1 Undo
```

### Redo
**Description**: Redo last undone action
**Parameters**: None
**Input Type**: Button
**Feedback**: 0.0 = no redo available, 1.0 = redo available

**Usage**:
```
Widget Button1 Redo
```

### NewProject
**Description**: Create new project
**Parameters**: None
**Input Type**: Button

**Usage**:
```
Widget Button1 NewProject
```

### OpenProject
**Description**: Open project dialog
**Parameters**: None
**Input Type**: Button

**Usage**:
```
Widget Button1 OpenProject
```

---

## 8. Messaging Actions

Send MIDI messages, OSC messages, and accessibility announcements.

### SendMIDIMessage
**Description**: Send custom MIDI message
**Parameters**: `StatusByte` `Data1` `Data2` (hex format)
**Input Type**: Button
**Output**: MIDI to hardware

**Usage**:
```
Widget Button1 SendMIDIMessage 90 3C 7F      ; Note On, C3, velocity 127
Widget Button1 SendMIDIMessage B0 7B 00      ; CC 123 (All Notes Off)
```

**Hex Format Examples**:
- `90 3C 7F`: Note On (channel 1), note C3, velocity 127
- `80 3C 00`: Note Off
- `B0 07 64`: CC 7 (volume), value 100

### SendOSCMessage
**Description**: Send OSC (Open Sound Control) message
**Parameters**: `OSCPath` [Value]
**Input Type**: Button
**Output**: OSC packet to network

**Usage**:
```
Widget Button1 SendOSCMessage /play
Widget Button1 SendOSCMessage /fader1 0.5
Widget Button1 SendOSCMessage /button1 1
```

**Value Types**:
- No value: sends message with no argument
- Integer: `123`
- Float: `0.5`, `1.5`
- String: `"text"`

### SpeakOSARAMessage
**Description**: Send accessibility text-to-speech announcement
**Parameters**: `Message` (text string)
**Input Type**: Button
**Output**: Spoken audio if accessibility enabled

**Usage**:
```
Widget Button1 SpeakOSARAMessage "Play button pressed"
Widget Button1 SpeakOSARAMessage "Track 1 selected"
```

---

## 9. Display Actions

Update hardware displays and indicators.

### TrackVolumeDisplay
**Description**: Show track volume on display
**Parameters**: None
**Output**: Numeric dB value
**Updates**: Real-time from track parameter

**Usage**:
```
Widget Display1 TrackVolumeDisplay
```

**Display Formats**:
- dB: "-6.02 dB"
- Percent: "45%"
- Raw: "0.5"

### TrackNameDisplay
**Description**: Show track name on display
**Parameters**: None
**Output**: Track name string
**Updates**: When track changes

**Usage**:
```
Widget Display1 TrackNameDisplay
```

### TrackPanDisplay
**Description**: Show track pan position on display
**Parameters**: None
**Output**: "-100L", "C", "+100R" format

**Usage**:
```
Widget Display1 TrackPanDisplay
```

### MuteDisplay
**Description**: Show mute state on display
**Parameters**: None
**Output**: "Muted" or "Unmuted"

**Usage**:
```
Widget Display1 MuteDisplay
```

### SoloDisplay
**Description**: Show solo state on display
**Parameters**: None
**Output**: "Soloed" or "Normal"

**Usage**:
```
Widget Display1 SoloDisplay
```

### FXNameDisplay
**Description**: Show FX plugin name
**Parameters**: Optional FXSlotIndex
**Output**: Plugin name string

**Usage**:
```
Widget Display1 FXNameDisplay 0     ; FX slot 0 name
```

### TempoDisplay
**Description**: Show project tempo
**Parameters**: None
**Output**: "120.0 BPM" format

**Usage**:
```
Widget Display1 TempoDisplay
```

### TimecodeDisplay
**Description**: Show timeline position
**Parameters**: Optional format
**Output**: "0:00.000" or timecode format

**Usage**:
```
Widget Display1 TimecodeDisplay
```

---

## 10. XTouch Specific Actions

Actions for Behringer X-Touch and X-Touch Compact devices.

### SetXTouchDisplayColors
**Description**: Set XTouch display backlight color
**Parameters**: `RGB(red, green, blue)`
**Input Type**: Button
**Effect**: Changes button LED color

**Usage**:
```
Widget Button1 SetXTouchDisplayColors RGB(255, 0, 0)      ; Red
Widget Button2 SetXTouchDisplayColors RGB(0, 255, 0)      ; Green
Widget Button3 SetXTouchDisplayColors RGB(0, 0, 255)      ; Blue
```

**Color Values**:
- RGB(255, 0, 0) = Red
- RGB(0, 255, 0) = Green
- RGB(0, 0, 255) = Blue
- RGB(255, 255, 0) = Yellow
- RGB(255, 0, 255) = Magenta
- RGB(0, 255, 255) = Cyan
- RGB(255, 255, 255) = White
- RGB(0, 0, 0) = Off

### RestoreXTouchDisplayColors
**Description**: Restore XTouch default colors
**Parameters**: None
**Input Type**: Button

**Usage**:
```
Widget Button1 RestoreXTouchDisplayColors
```

---

## 11. Advanced/Special Actions

### Learn FX
**Description**: Enter Learn mode to map FX parameters
**Parameters**: None
**Input Type**: Button
**Effect**: Opens Learn window for current FX

**Usage**:
```
Widget Button1 Learn
```

**Learn Mode Workflow**:
1. Click widget on Learn button
2. CSI opens Learn dialog
3. Move FX parameters to automatically map them
4. Mapping saved to AutoGeneratedFXZones

### ModifierStates
**Description**: Query/display modifier key states
**Parameters**: None
**Output**: "Shift", "Alt", "Control", combinations

**Usage** (Display):
```
Widget Display1 ModifierDisplay
```

### MCU Specific
**Description**: Mackie Control Universal specific features
**Parameters**: Various
**Features**: 7-segment displays, LED matrices, motor faders

**Usage**:
```
; MCU-specific widget for 7-segment display
Widget MCUDisplay SpecialDisplay MCU 7SegmentDisplay
```

---

## Action Modifiers

Actions can be combined with modifiers for alternative behaviors:

```
Widget Fader1 TrackVolume              ; Normal: volume control
Widget Fader1+Shift TrackVolume         ; Shift+Fader: fine volume control
Widget Fader1+Control TrackPan          ; Ctrl+Fader: pan control
Widget Fader1+Shift+Control TrackPanWidth ; Shift+Ctrl: width control
```

---

## Action Value Specifications

### Stepped Values

For actions accepting discrete steps:

```
Widget Button1 Tempo [
  60.0
  80.0
  100.0
  120.0
  140.0
]
```

### Accelerated Values

For encoder actions with acceleration:

```
Widget VPot1 TrackPan [
  (0.001)                           ; Single acceleration value
  (0.01, 0.02, 0.03, 0.04)         ; Multi-tier acceleration
]
```

### Range Limits

Constrain action value ranges:

```
Widget Fader1 FXParam 0 [0.0>1.0]    ; Explicit range
```

---

## Best Practices

### 1. Clear Naming
```
Widget Fader1 TrackVolume       ; Good: immediately clear
Widget F1 TV                    ; Avoid: cryptic names
```

### 2. Logical Organization
```
Zone "Mixer"
  ; Volume controls
  Widget Fader1 TrackVolume
  Widget Fader2 TrackVolume
  
  ; Pan controls
  Widget VPot1 TrackPan
  Widget VPot2 TrackPan
  
  ; Mute/Solo
  Widget Button1 Mute
  Widget Button2 Solo
ZoneEnd
```

### 3. Use Modifiers Strategically
```
Widget Fader1 TrackVolume           ; Primary: volume
Widget Fader1+Shift SendVolume 0    ; Secondary: send 0
Widget Fader1+Alt RecvVolume 0      ; Tertiary: receive 0
```

### 4. Leverage Display Actions
```
Zone "Mixer"
  Widget Fader1 TrackVolume
  Widget Display1 TrackVolumeDisplay
  Widget Display2 TrackNameDisplay
ZoneEnd
```

### 5. Test All Action Types
- Test each mapped action
- Verify feedback displays
- Check modifier combinations
- Test edge cases (no track, empty FX chain, etc.)

---

## Troubleshooting Actions

| Issue | Solution |
|-------|----------|
| Action does nothing | Check action name spelling; verify parameters |
| Widget shows wrong value | Ensure correct property format for display |
| Modifier doesn't work | Check modifier syntax (Shift, Alt, Control, not shift, alt, ctrl) |
| Display blank | Verify display widget defined in surface template |
| FX param won't control | Ensure FX plugin has parameters; check FX slot index |
| Zone navigation broken | Check zone names match exactly; verify GoZone zone exists |

---

## Complete Example Zone

```
Zone "CompleteMixer" "Full Mixer Control"
  ; Track 1-8 Faders and Pans
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
  
  ; Mute and Solo
  Widget Mute1 Mute
  Widget Mute2 Mute
  Widget Mute3 Mute
  Widget Mute4 Mute
  Widget Mute5 Mute
  Widget Mute6 Mute
  Widget Mute7 Mute
  Widget Mute8 Mute
  
  Widget Solo1 Solo
  Widget Solo2 Solo
  Widget Solo3 Solo
  Widget Solo4 Solo
  Widget Solo5 Solo
  Widget Solo6 Solo
  Widget Solo7 Solo
  Widget Solo8 Solo
  
  ; Track Selection
  Widget Select1 TrackSelect
  Widget Select2 TrackSelect
  Widget Select3 TrackSelect
  Widget Select4 TrackSelect
  Widget Select5 TrackSelect
  Widget Select6 TrackSelect
  Widget Select7 TrackSelect
  Widget Select8 TrackSelect
  
  ; Transport
  Widget Play Play
  Widget Stop Stop
  Widget Record Record
  Widget Repeat Repeat
  
  ; Navigation
  Widget Shift+Play GoZone SendsZone
  Widget Shift+Stop GoHome
  
  ; Project
  Widget Save SaveProject
  Widget Undo Undo
  Widget Redo Redo
  
  ; Displays
  Widget ChannelDisplay1 TrackNameDisplay
  Widget ChannelDisplay2 TrackVolumeDisplay
ZoneEnd
```

---

## See Also

- [Configuration-Format.md](Configuration-Format.md) - Detailed configuration syntax
- [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) - System architecture and design
- REAPER Plugin API - https://www.reaper.fm/sdk/plugin/plugin.php


