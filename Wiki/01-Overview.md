# 01 - CSI Overview & Core Concepts

**Purpose**: Understand what CSI is, how it works, and why you'd use it  
**Reading Time**: 5 minutes  
**Audience**: Everyone, especially new users

---

## What is CSI?

The **Control Surface Integrator (CSI)** is a Reaper plugin that integrates hardware control surfaces with Reaper, providing deep touchpoint control far beyond Reaper's default capabilities.

### Problem It Solves

By default, Reaper allows you to map MIDI to Reaper actions, but setup is tedious and you get limited feedback. CSI solves this by:

✅ Providing pre-configured surface templates for popular devices  
✅ Enabling sophisticated zone-based workflow (different controls for different tasks)  
✅ Offering real-time feedback on displays and LEDs  
✅ Supporting multiple surfaces working together seamlessly  
✅ Enabling FX parameter mapping with a simple Learn function  

---

## Core Concepts

### 1. **Pages**

A Page is the highest organizational level in CSI. Each Page contains a complete surface setup.

**Uses**:
- Create separate Pages for different workflows (Recording Page, Mixing Page, Editing Page)
- Enable/disable surfaces dynamically
- Switch between completely different control layouts

**Example**:
```
HomePage
├── X-Touch 8-channel mixer
└── MFT Twister for FX control

EditingPage  
├── X-Touch in transport-only mode
└── Jog wheel for timeline scrubbing
```

Only one Page is active at a time, but switching is instant via navigation actions.

---

### 2. **Surfaces**

Each Page contains one or more **Surfaces** - the physical control hardware devices.

A Surface consists of two components:

#### A) **surface.txt** - Hardware Definition
Describes the physical capabilities:
- What controls exist (faders, buttons, displays, etc.)
- What messages the hardware sends (MIDI/OSC)
- What messages control the hardware (how to light LEDs, update displays)

`surface.txt` can also contain optional global blocks that affect the whole surface, such as `StepSize`, `AccelerationValues`, and `ColorCalibration`.

Example:
```
ColorCalibration
  OutputMax 127
  NeutralTolerancePercent 5
  NeutralRedScale 0.80
  NeutralCurve 2.0
ColorCalibrationEnd
```

This lets a surface template tune RGB feedback without hardcoding device-specific color compensation in C++.

Example: "This X-Touch has 8 faders, each with a button above it, a display, and RGB LEDs"

#### B) **Zone Files** (.zon) - Control Mapping
Defines how those physical controls map to Reaper functions:
- "Fader 1 controls track volume"
- "Button 1 toggles track mute"
- "Display 1 shows track name"

---

### 3. **Zones**

**Zones are the core building block of CSI.**

A Zone defines a specific control layout by mapping hardware widgets (controls, displays) to CSI actions.

**Zone Types**:

| Zone Type | Purpose | Example |
|-----------|---------|---------|
| **Home Zone** | Default layout when no special mode active | Mixer controls, transport |
| **Track Zone** | Navigate and control individual tracks | Per-track volume, pan, effects |
| **Send Zone** | Control track sends | Send volume, pan, pre/post |
| **Receive Zone** | Control track receives | Receive volume, pan settings |
| **FX Zone** | Control FX parameters | Plugin parameters mapped to encoders |
| **Custom Zone** | Any user-defined zone | Buttons, custom pages, etc. |

**Example from a .zon file**:
```
Zone "Mixer" "8-Channel Mixer View"
  Widget Fader1 TrackVolume
  Widget Fader2 TrackVolume
  Widget Button1 Mute
  Widget Button2 Solo
  Widget Display1 TrackNameDisplay
  Widget VPot1 TrackPan
ZoneEnd
```

---

### 4. **Actions**

An Action is something CSI *can do* - a control function you can map to hardware.

CSI provides 100+ actions in categories:

**Transport**: Play, Stop, Record, Pause, FastForward, Rewind  
**Volume**: TrackVolume, SendVolume, RecvVolume, MasterVolume  
**Pan**: TrackPan, TrackPanWidth, SendPan, RecvPan  
**State**: Mute, Solo, RecArm, TrackSelect  
**FX**: FXParam, JSFXParam, ToggleFXBypass, Offline  
**Navigation**: GoHome, GoZone, GoFXSlot, NextTrack  
**Display**: TrackVolumeDisplay, FXParamNameDisplay, etc.  

Every zone is built by mapping hardware widgets to actions.

---

### 5. **Widgets**

A **Widget** is a single control or display element defined in surface.txt.

**Input Widgets** (things you interact with):
- Faders (continuous values)
- Rotary Encoders (continuous or stepped)
- Buttons (binary on/off)

**Output Widgets** (feedback):
- Displays (text or values)
- LEDs (on/off or RGB color)
- Meters (visual feedback)

**Example surface.txt definition**:
```
Fader "Fader1" 1 0xE0 0
    Feedback "FaderDisplay" 1 0xB0 0 //FIXME: describe what number mean what
    
Button "Button1" 1 0x90 0
    Feedback "ButtonLED" 1 0x90 0

Display "Display1" 1 2 7
```

---

## Typical Workflow

### 1. Hardware Setup
Your physical control surface is connected and configured in Reaper Preferences.

### 2. Define surface.txt
You create or download a `surface.txt` file that describes all your hardware's controls.

### 3. Create Zone Files
You create `.zon` files mapping hardware widgets to CSI actions.

```
Zone "Mixer"
  Widget Fader1 TrackVolume      ; Control track volume
  Widget Button1 Mute             ; Toggle track mute
  Widget Display1 TrackVolume Display  ; Show volume value
ZoneEnd
```

### 4. Navigate Zones
Your hardware now acts as specified. Use GoZone or GoHome actions to switch layouts.

### 5. Add FX Control
Create FX zone files to control plugin parameters.

---

## Key Distinctions

### CSI Actions vs Reaper Actions

**CSI Actions** - Built-in (TrackVolume, Mute, Solo, etc.). Work with zone context.

**Reaper Actions** - Reaper's built-in actions. Can be called from CSI using the `Reaper` prefix.

```
Zone "Buttons"
  Widget Button1 TrackSelect            ; CSI action
  Widget Button2 Reaper 40044           ; Reaper action (Play)
ZoneEnd
```

### Soft Takeover vs Hard Control

**Hard Control**: Immediate mapping (slider moves instantly)  
**Soft Takeover**: Prevents jumps when hardware/DAW values don't match

```
Widget Fader1 TrackVolume              ; Hard control
Widget Fader1 SoftTakeover7BitTrackVolume  ; Soft takeover for 7-bit MIDI
```

---

## Benefits of Using Zones

Instead  of one flat control layout, CSI lets you:

1. **Context-Aware Controls** - Same hardware does different things in different zones
2. **Efficient Use of Controls** - 8 faders can control 100+ parameters via banking/zones
3. **Workflow-Specific Layouts** - Recording feels different from mixing
4. **Easy Mode Switching** - Press a button to completely change what hardware does

---

## Multi-Surface Integration

CSI can synchronize multiple surfaces:

```
Page "Mixing"
  Surface 1: X-Touch One (mixer)
  Surface 2: MIDI Fighter Twister (FX)
  
  - X-Touch broadcasts GoZone commands
  - MFT Twister listens and switches zones
  - Both stay in sync
```

This creates a unified virtual control surface from disparate hardware.

---

## How It Actually Works (Under the Hood)

```
User Input
    ↓
Hardware → MIDI/OSC Message
    ↓
CSI Receives (via surface.txt mapping)
    ↓
Zone File interpreter
    ↓
"This message = Widget X = Action Y"
    ↓
Execute Action (modify Reaper parameter)
    ↓
Get Updated Value from Reaper
    ↓
Apply Feedback Processor (from surface.txt)
    ↓
Send Response Message to Hardware
    ↓
Hardware Updates Display/LED
```

---

## Real-World Example

### Scenario: Mixing with X-Touch One

**Hardware**: Behringer X-Touch One (8 faders, buttons, display, master fader)

**Setup**:
1. `surface.txt` defines all 8 faders send CC 0-7, display accepts text
2. Home Zone (`Mixer.zon`):
   - Faders 1-8 map to TrackVolume actions
   - Buttons below faders map to Solo
   - Left buttons map to Mute
   - Display shows track name
3. When you move Fader 1, X-Touch sends CC 0
4. CSI intercepts CC 0, applies TrackVolume action
5. Volume changed in Reaper
6. Reaper sends back new volume value
7. CSI converts back to CC (via feedback processor)
8. X-Touch display updates showing new volume

Result: You have a hardware mixer that perfectly reflects Reaper's state.

---

## Key Takeaways

✅ **CSI unifies hardware control** via Zones  
✅ **Actions are the language** - 100+ to choose from  
✅ **Multiple Surfaces work together** via Pages  
✅ **Text-based config** - powerful but requires learning  
✅ **FX mapping** - auto-map plugin parameters  
✅ **Display feedback** - hardware stays in sync with Reaper  

---

## Next Steps

**Ready to set up?** → [02 - Installation Guide](02-Installation.md)  
**Want immediate usage?** → [03 - Quick Start](03-QuickStart.md)  
**Dive deep into Zones?** → [04 - Zones Fundamentals](04-Zones-Fundamentals.md)  

---

**Last Updated**: March 2026  
**Based on**: CSI v7.0+ codebase
