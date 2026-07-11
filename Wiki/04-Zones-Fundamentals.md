# 04 - Zones Fundamentals

**Purpose**: Master the core concept of CSI - Zones  
**Reading Time**: 15 minutes  
**Prerequisites**: [Quick Start](03-QuickStart.md) recommended

> **Zones are the heart of CSI.** Understanding zones is understanding CSI.

---

## What Is a Zone?

A **Zone** is a configuration that defines how your hardware controls map to Reaper functions at a given moment.

Think of zones like **"modes"** or **"pages"** on your controller:
- Home Zone = Normal mixing mode
- FX Zone = FX parameter control mode
- Transport Zone = Playback control mode

Each zone completely defines behavior - same button can do different things in different zones.

---

## Zone File Structure

Every zone is defined in a `.zon` file (plain text):

```
Zone "ZoneName" "Zone Description"
    Widget Fader1 TrackVolume
    Widget Button1 Mute
    Widget Display1 TrackNameDisplay
    ... more widget mappings
ZoneEnd
```

### Required Elements

| Element | Purpose | Example |
|---------|---------|---------|
| `Zone` | Zone declaration | `Zone "Mixer"` |
| `"Name"` | Internal identifier | `"Mixer"` |
| `"Description"` | Human-readable label | `"8-Channel Mixer View"` |
| `Widget` | Map hardware widget to action | `Widget Fader1 TrackVolume` |
| `ZoneEnd` | Close zone definition | `ZoneEnd` |

---

## Zone Types by Function

### 1. **Home Zone**

The default zone that loads when CSI starts or when you return home.

**Characteristics**:
- Usually contains primary controls (mixer, transport)
- Navigates to other zones
- One per Page

**Example**:
```
Zone "Home" "Main Mixer and Transport"
    Widget Fader1 TrackVolume
    Widget Fader2 TrackVolume
    Widget Button1 Play
    Widget Button2 Record
    Widget Button3 GoZone FX          ; Navigate to FX zone
ZoneEnd
```

### 2. **Track Zones**

Navigate and control individual tracks. These are implicitly handled by CSI - when you use actions like `TrackVolume`, CSI intelligently applies to the currently-selected or currently-focused track.

**Actions that use Track Zones**:
```
TrackVolume          ; Current track's volume
TrackPan             ; Current track's pan
Mute, Solo, RecArm   ; Current track's state
TrackNameDisplay     ; Current track's name
```

**How it works**:
- Banking actions (NextTrack, PreviousTrack) move the focus
- All track actions then apply to the newly-focused track
- Displays update responsively

### 3. **FX Zones**

For controlling plugin parameters. Can be:
- **Manual**: You specify each parameter mapping
- **Auto-Generated**: CSI creates mappings from Learn mode

**Example**:
```
Zone "FXControl" "FX Parameter Mapping"
    Widget VPot1 FXParam 0           ; Control FX slot 0
    Widget VPot2 FXParam 1           ; Control FX slot 1
    Widget VPot3 FXParam 2           ; Control FX slot 2
    Widget Button1 ToggleFXBypass 0  ; Bypass FX slot 0
    Widget Button2 GoZone Home       ; Return to home
ZoneEnd
```

### 4. **Send/Receive Zones**

Control track sends and receives explicitly:

```
Zone "SendControl" "Track Sends Control"
    Widget Fader1 SendVolume 0       ; Send 0 volume
    Widget Fader2 SendVolume 1       ; Send 1 volume
    Widget Button1 Mute              ; Affects current track send
ZoneEnd
```

### 5. **Navigation/Custom Zones**

User-defined zones for specific workflows:

```
Zone "Transport" "Transport and Timeline"
    Widget Fader1 Tempo              ; Control BPM
    Widget Button1 Play
    Widget Button2 Stop
    Widget Button3 Record
    Widget Button4 Repeat
    Widget Dial1 MoveEditCursor      ; Move playheadby encoding
ZoneEnd

Zone "Markers" "Marker Management"
    Widget Button1 GoZone Home       ; Quick exit
    Widget VPot1 MarkerSearch        ; Navigate markers
    Widget Display1 TimecodeDisplay  ; Show timeline
ZoneEnd
```

---

## Widget Concepts

### Widget Types

**Input Widgets** (user interacts):
```
Widget ButtonName ActionName
Widget FaderName ActionName
Widget DialName ActionName
Widget EncoderName ActionName
```

**Output Widgets** (displays):
```
Widget DisplayName DisplayActionName
Widget MeterName MeterActionName
Widget LedName FeedbackActionName
```

### Widget Naming

Widget names must match what's defined in `surface.txt`:

**From surface.txt**:
```
Fader "Fader1" 1 0xE0 0
Button "Play" 1 0x90 0
Display "TrackName" 1 2 7
```

**Zone file must use same names**:
```
Zone "Mixer"
    Widget Fader1 TrackVolume      ; ✓ Correct - matches surface.txt
    Widget play Play               ; ✗ Wrong - case sensitive
    Widget TrackName TrackNameDisplay  ; ✓ Correct
ZoneEnd
```

### Widget Actions

Actions specify what the widget does:

```
Widget Fader1 TrackVolume          ; Fader controls track volume
Widget Button1 Mute                ; Button toggles mute
Widget Button1+Shift Solo          ; Shift+button toggles solo
Widget Display1 TrackVolume Display ; Display shows volume value
```

---

## Modifiers (Shift, Control, Alt)

Multiple actions from single widget using modifiers:

### Global Modifiers
```
Widget Button1 Mute
Widget Button1+Shift Solo
Widget Button1+Control RecArm
Widget Button1+Shift+Control TrackSelect
```

**Supported Modifiers**:
- `Shift`
- `Control`
- `Alt`
- `Option` (macOS)

### Touch Modifier (Hardware Specific)

For devices supporting touch detection:
```
Widget Fader1 TrackVolume
Widget Fader1 [Touch] SomeOtherAction   ; Triggers when touched, not moved
```

### Pressed Modifier (Button States)

```
Widget Button1+Pressed TrackSelect      ; While held down
Widget Button1+Released Mute            ; When released
```

---

## Zone Navigation

Use these actions to switch between zones:

### GoZone (Switch Zones)
```
Zone "Mixer"
    Widget Button1 GoZone FX          ; Press button to enter FX zone
ZoneEnd

Zone "FX"
    Widget Button1 GoZone Home        ; Return to home
ZoneEnd
```

### GoHome (Return to Default)
```
Zone "Mixer"
    Widget Button2 GoHome             ; Always returns to home zone
ZoneEnd
```

### GoSubZone (Nested Zones - Advanced)

For creating hierarchies:
```
Zone "Main"
    Widget Button1 GoSubZone Sends    ; Enter send subzone
    
    SubZones
        Zone "Sends" "Send Control"
            Widget Fader1 SendVolume 0
        ZoneEnd
    SubZonesEnd
ZoneEnd
```

## GoZone vs GoHome vs GoSubZone vs LeaveSubZone — the architecture

| Action        | What it does                                                                 | Navigation scope                         |
|---------------|------------------------------------------------------------------------------|-----|
| **GoZone**     | Activates a goZone (top-level zone alongside Home). Deactivates other goZones. Toggle: if already active, deactivates it. | Global — works from any zone             |
| **GoHome**     | Deactivates ALL goZones, re-activates Home                                  | Global — works from any zone             |
| **GoSubZone**  | Activates a subZone nested inside the current zone                          | Local — within current zone only         |
| **LeaveSubZone** | Deactivates the current zone (calls `zone->Deactivate()`)                 | Local — deactivates the zone this context lives in |

---

### Why both GoZone and GoSubZone exist

- **GoZone** operates on the `goZones_` list — these are top-level zones that replace/overlay the Home zone.  
  Example: `GoZone MasterTrack` from `Home.zon` loads `MasterTrack.zon` as a goZone, deactivating Home's widget mappings.

- **GoSubZone** operates on a zone's `subZones_` list — these are zones declared as children of another zone via the `SubZones` directive.  
  Example: `GoSubZone SomeMode` activates a nested sub-zone within the current zone.

---

### Why LeaveSubZone works from `GoZone MasterTrack`

Because `LeaveSubZone` simply calls:

```cpp
context->GetZone()->Deactivate();

```

It deactivates whatever zone the context belongs to.
Whether that zone is a goZone or a subZone doesn’t matter — it’s a generic "deactivate myself" action.

### When to use what
From Home.zon:
```
→ GoZone MasterTrack // to navigate to MasterTrack
```
From MasterTrack.zon:
```
→ GoHome // to deactivate MasterTrack and re-activate Home (recommended)
```
Use GoSubZone / LeaveSubZone for nested zones within the same zone file (e.g., a zone with multiple modes)

*Important note:* 
LeaveSubZone also appears to work because it deactivates the zone, but it does not explicitly re-activate Home.

Home stays active in the background via ZoneManager::RequestUpdate, which always processes homeZone_ last, filling in unused widgets.


//FIXME: need to rethink and force syntax for sub zones, or maybe even beter to rethink the entire format of files for clearer better , especially definition and navigation in/out, cause now sub zone file content defines exactly same way as zone file

---

## Variables & Context

CSI maintains context about what's currently being controlled:

### Track Context
When zone uses track actions, CSI knows which track:
```
Zone "Mixer"
    Widget Fader1 TrackVolume        ; Depends on which track is focused
    Widget Button1 Mute              ; Mutes the currently-focused track
    Widget Display1 TrackNameDisplay ; Shows focused track's name
ZoneEnd
```

### Bank Context
Banking changes which track is focused:
```
Zone "Mixer"
    Widget Fader1 TrackVolume        ; Changes when you bank
    Widget Button1 NextTrack         ; Move to next track
    Widget Button2 PreviousTrack     ; Move to prev track
ZoneEnd
```

### FX Context
FX focus can be explicit or automatic:
```
Zone "FX"
    Widget VPot1 FXParam 0           ; Explicitly targets FX slot 0
    Widget VPot2 LastTouchedFXParam  ; Follows user's last FX interaction
    Widget VPot3 FocusedFXParam      ; Targets explicitly-focused FX
ZoneEnd
```

---

## Zone File Organization

### Location Structure
```
CSI/
└── Surfaces/
    └── YourSurface/
        └── Zones/
            ├── HomeZones/           ; Default zones
            │   ├── Home.zon
            │   └── Mixer.zon
            ├── GoZones/             ; Zones loaded via GoZone
            │   ├── FX.zon
            │   ├── Transport.zon
            │   └── Sends.zon
            └── LearnZones/          ; For FX Learn mode
                └── LearnFocusedFXZone.zon
```

### Naming Conventions

Best practice naming:
- **Home zones**: Start with "Home" or "Mixer"
- **FX zones**: "FX_" prefix
- **Send zones**: "Sends_" prefix
- **Receive zones**: "Receives_" prefix
- **Custom zones**: Use underscores, avoid spaces

```
Home.zon               ✓ Home zone - loaded automatically
FX_Main.zon           ✓ FX zone - referenced by GoZone FX_Main
Transport.zon         ✓ Custom zone
Sends_8Ch.zon         ✓ Clear purpose and scale
my transport zone.zon ✗ Spaces - avoid!
```

---

## Complete Zone Example

```
Zone "CompleteMixer" "Full Control Setup"
    // ===== TRACK VOLUME & PAN =====
    Widget Fader1 TrackVolume
    Widget Fader2 TrackVolume
    Widget VPot1 TrackPan
    Widget VPot2 TrackPan
    
    // ===== TRACK STATE =====
    Widget Button1 Mute
    Widget Button2 Solo
    Widget Button3 RecArm
    Widget Button4 TrackSelect
    
    // ===== TRACK NAVIGATION =====
    Widget Button5 NextTrack
    Widget Button6 PreviousTrack
    
    // ===== FX ACCESS =====
    Widget Button7 FXParam 0         ; Focus FX slot 0
    Widget Button8 GoZone FX         ; Enter FX zone
    
    // ===== TRANSPORT =====
    Widget Play Play
    Widget Stop Stop
    Widget Record Record
    
    // ===== DISPLAYS =====
    Widget Display1 TrackNameDisplay
    Widget Display2 TrackVolumeDisplay
    Widget Display3 TrackPanDisplay
    
    // ===== NAVIGATION HOME =====
    Widget HomeButton GoHome
    
    // ===== MODIFIERS =====
    Widget Button1+Shift Solo        ; Multi-function
    Widget Button2+Shift RecArm      ; Shift variations
ZoneEnd
```

---

## Zone Parameters & Properties

Optional zone-level parameters:

```
Zone "Mixer" "8-Channel Mixer"
    Properties
        FollowMCP On                 ; Follow Mixer Console Panel visibility
        ScrollLink Off               ; Don't scroll with Reaper mixer
        EnsureTrackVisible On        ; Ensure track visible when navigating
    PropertiesEnd
    
    Widget Fader1 TrackVolume
    ...
ZoneEnd
```

---

## Common Zone Patterns

### Pattern 1: Multi-Control Single Widget
```
Widget Button1 Mute
Widget Button1+Shift Solo
Widget Button1+Control TrackSelect
```

### Pattern 2: Progressive Disclosure (Navigation)
```
Zone "Home"
    Widget Button1 GoZone FX
    Widget Button2 GoZone Sends
    Widget Button3 GoZone Transport
ZoneEnd
```

### Pattern 3: Banking for More Tracks
```
Zone "Mixer"
    Widget Fader1 TrackVolume        ; 8 faders = 8 tracks with banking
    Widget Button1 NextTrack         ; Move to next 8-track bank
    Widget Button2 PreviousTrack
ZoneEnd
```

### Pattern 4: Context-Specific Zones
```
Zone "Recording"
    Widget Fader1 Input Volume       ; Different than mixing faders
    Widget Button1 RecArm
ZoneEnd

Zone "Mixing"
    Widget Fader1 TrackVolume        ; Normal mixing faders
    Widget Button1 Mute
ZoneEnd
```

---

## Debugging Zones

### Zone Won't Load
- Check syntax: Is `ZoneEnd` present?
- Check folder: Is it in correct location?
- Check capitalization: Widget names must match surface.txt exactly
- Restart Reaper after editing

### Widget Does Nothing
- Action name spelled correctly? (no spaces)
- Widget name in surface.txt?
- Zone loaded? (debug via monitoring)
- Modifiers supported by device?

### Display Shows Wrong Info
- Correct display action?
- Matching widget name?
- Correct parameters for the action?

---

## Key Takeaways

✅ **Zones define control behavior** at any moment  
✅ **Modifiers create multiple actions** from single widget  
✅ **Navigation actions** switch between zones  
✅ **Widget names** must match surface.txt  
✅ **Actions specify** what widget does  
✅ **Track context is automatic** - no special setup needed  
✅ **FX zones** allow plugin parameter mapping  

---

## Next Steps

→ **Learn about FX Zones**: [05 - FX Zones](05-FX-Zones.md)  
→ **Understand surface.txt**: [06 - Surface Files](06-Surface-Files.md)  
→ **See all actions**: [07 - Complete Actions Reference](07-Complete-Actions-Reference.md)  
→ **Configuration syntax**: [08 - Configuration Format](08-Configuration-Format.md)  
→ **Issues?**: [09 - Troubleshooting](09-Troubleshooting.md)  

---

**Difficulty**: Intermediate  
**Time to Master**: 30 minutes of practice  
**Critical Concept**: Yes - all CSI revolves around zones
