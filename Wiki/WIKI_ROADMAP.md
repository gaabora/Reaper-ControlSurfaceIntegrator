# Wiki Creation Roadmap & Next Steps

**Status**: Foundation complete (5 pages created)  
**Created**: Home, Overview, Installation, Quick Start, Zones Fundamentals  
**Remaining**: 10 comprehensive pages  

---

## Completed Pages ✅

| # | Page | Purpose | Status |
|---|------|---------|--------|
| - | Home | Navigation hub | ✅ Complete |
| 01 | Overview | What is CSI | ✅ Complete |
| 02 | Installation | Setup guide | ✅ Complete |
| 03 | Quick Start | 15-min usage | ✅ Complete |
| 04 | Zones Fundamentals | Core concept | ✅ Complete |

---

## Remaining Pages to Create ⏳

### PHASE 1: Core Technical (High Priority)

These pages are essential for users to function:

#### 05 - FX Zones (Content Outline)
**Purpose**: Control plugin parameters with hardware

**Sections**:
- What are FX Zones?
- FX Zone Structure
- Manual Mapping
- Learn Mode (Auto-Mapping)
- FX Zone Actions:
  - `FXParam N` - Control FX slot N
  - `JSFXParam N` - JSFX-specific
  - `TCPFXParam N` - TCP FX
  - `LastTouchedFXParam` - Follow user
  - `ToggleFXBypass N` - Bypass control
  - `ToggleFXOffline N` - Offline toggle
  - Display actions for FX names/values
- Parameter Learning Workflow
- Real-world example (map Reverb)
- Stepped vs Continuous mapping
- Encoder response curves
- Soft takeover for FX
- Troubleshooting FX mapping

**Key Code References**:
- `control_surface_Reaper_actions.h`: FXParam, JSFXParam, TCPFXParam classes
- `LastTouchedFXParam` context handling
- FX bypass/offline state management

---

#### 06 - Surface Files (Content Outline)
**Purpose**: Define hardware control capabilities

**Sections**:
- What is surface.txt?
- Surface File Structure
- Widget Definition Format:
  - Faders
  - Buttons
  - Encoders/Rotaries
  - Displays
  - Meters
  - LEDs/Indicators
- Message Types:
  - MIDI (Note On/Off, CC, Pitch Bend, Sysex)
  - MCU Protocol specifics
  - OSC (Open Sound Control)
- Feedback Processors:
  - LED on/off
  - RGB color
  - Value scaling
  - Text display
  - Meter feedback
- Soft Takeover Configuration
- Multi-channel devices
- Device-specific examples:
  - Behringer X-Touch
  - MIDI Fighter Twister
  - FaderPort
  - iPad + TouchOSC
- Creating custom surface.txt from scratch

**Key Code References**:
- `control_surface_midi_widgets.h`: MIDI message parsing
- `control_surface_OSC_widgets.h`: OSC handling
- Message generator implementations

---

#### 07 - Complete Actions Reference (Content Outline)
**Purpose**: Comprehensive listing of all 100+ actions

**Sections**:
- **Transport & Timeline**:
  - Play, Stop, Pause, Record
  - FastForward, Rewind
  - PlayAndStopAtMarker, PlayAndStopAtRegion
  - Repeat, CycleTimeDisplayModes
  - Tempo, TempoDisplay
  - TimecodeDisplay, MoveEditCursor

- **Volume Control**:
  - TrackVolume, TrackVolumeDisplay
  - SoftTakeover7BitTrackVolume
  - SoftTakeover14BitTrackVolume
  - SendVolume, SenVolumeDisplay
  - RecvVolume, RecvVolumeDisplay
  - MasterVolume, MasterVolumeDisplay

- **Pan Control**:
  - TrackPan, TrackPanDisplay
  - TrackPanWidth, TrackPanLeft, TrackPanRight
  - SendPan, RecvPan
  - Track Pan percent variants
  - MasterPan

- **Track Control**:
  - Mute, Solo, RecArm
  - TrackSelect, TrackUniqueSelect
  - TrackNameDisplay, TrackNumberDisplay
  - InvertPolarity, InputMonitor

- **FX**:
  - FXParam, JSFXParam, TCPFXParam
  - LastTouchedFXParam
  - ToggleFXBypass, FXBypassDisplay
  - ToggleFXOffline, FXOfflineDisplay
  - FXGainReductionMeter
  - GoFXSlot, ShowFXSlot, HideFXSlot
  - FXNameDisplay, FXParamNameDisplay, FXParamValueDisplay

- **Navigation**:
  - GoHome, AllSurfacesGoHome
  - GoZone, NextPage, GoPage
  - GoSubZone, LeaveSubZone
  - Bank, NextTrack, PreviousTrack
  - ClearFXSlot, ClearFocusedFX
  - SelectedTrackFX

- **Project**:
  - SaveProject, Undo, Redo
  - NewProject, OpenProject

- **Display & Output**:
  - FixedTextDisplay
  - FixedRGBColorDisplay
  - SetXTouchDisplayColors
  - RestoreXTouchDisplayColors
  - SendMIDIMessage, SendOSCMessage
  - SpeakOSARAMessage

- **Modifiers**:
  - Shift, Control, Alt, Option
  - Touch, InvertFB, Hold, Flip
  - Marker, Nudge, Scrub, Zoom
  - Global, GlobalModeDisplay
  - Toggle, Increase, Decrease
  - DoublePress, ClearModifiers
  - Pressed, Released

**Grouping by Type**:
- Continuous Actions (0.0-1.0 values)
- Discrete Actions (button presses)
- Display Actions (feedback only)
- Navigation Actions (zone switching)
- Feedback Actions (modifying displays/LEDs)

**Key Code References**:
- All classes in `control_surface_Reaper_actions.h`
- `ActionType` enum definitions
- Each action's parameters and value ranges

---

### PHASE 2: Reference & Configuration (Medium Priority)

#### 08 - Configuration Format (Content Outline)
**Purpose**: Detailed syntax and format reference

**Sections**:
- Zone File Syntax:
  - Zone declaration
  - Widget syntax (variations)
  - Modifiers syntax
  - SubZones syntax
  - Properties block
- Widget Definition Format:
  - Input widgets
  - Output/display widgets
  - Feedback specification
- Action Parameter Format:
  - Stepped values `[]`
  - Accelerated values `()`
  - Range limits `>`
- Modifier Syntax:
  - Single modifiers
  - Combined modifiers
  - Advanced modifiers
- Comments in zone files
- Best practices for readability
- CFG/INI format for surface.txt
- Parameter encoding formats

**Examples**:
```
// Valid syntax examples
Widget Fader1 TrackVolume
Widget Button1+Shift+Control Mute
Widget VPot1 FXParam 0 [values]
```

---

#### 09 - Troubleshooting (Content Outline)
**Purpose**: Solutions to common problems

**Sections**:
- Installation Issues:
  - CSI not appearing in dropdown
  - Plugin not loading
  - Missing surface files
  - Permissions errors (macOS)
  - MIDI port conflicts

- Configuration Issues:
  - Zones not loading
  - Widgets not responding
  - Wrong feedback
  - Display shows incorrect values
  - Modifiers not working

- Performance Issues:
  - Dropped MIDI messages
  - Display lag
  - CPU spike when moving faders
  - OSC connection unstable

- Hardware-Specific Issues:
  - X-Touch LED colors wrong
  - MIDI Fighter Twister not recognized
  - OSC device disconnects
  - FaderPort fader jumps

- FX Mapping Issues:
  - Learn mode not working
  - Parameter not responding
  - Slow parameter motion
  - FX zone won't load

- Multi-Surface Issues:
  - Surfaces interfering with each other
  - Broadcast/Listen not working
  - Sync issues

**Diagnostic Techniques**:
- Using monitoring panels
- Checking CSI logs
- MIDI input/output monitoring
- OSC packet inspection
- Zone reload verification

---

#### 10 - Examples & Templates (Content Outline)
**Purpose**: Real-world, copy-paste examples

**Sections**:
- **Complete Zone Examples**:
  - Simple 4-fader mixer zone
  - Full 8-channel mixer with pan/mute/solo
  - FX parameter mapping zone
  - Transport/timeline zone
  - Multi-zone complete setup

- **Surface Templates**:
  - Generic MIDI controller
  - X-Touch One basic setup
  - MIDI Fighter Twister FX layout
  - iPad TouchOSC interface
  - Custom DIY controller

- **Workflow Examples**:
  - Recording workflow
  - Mixing workflow
  - Post-production workflow
  - Live performance setup

- **Advanced Patterns**:
  - Banking for 16+ tracks
  - Nested zones (GoSubZone)
  - Conditional mappings
  - Multi-surface broadcasting

- **Copy-Paste Templates**:
  - Minimal working zone
  - Full-featured mixer
  - Typical FX zone
  - Common transport setup

---

### PHASE 3: Supporting Documentation (Lower Priority)

These pages provide deeper dives into specific topics:

#### Advanced Topics (Creating as separate pages)

**Topics to Document**:
1. **Multi-Surface Setup**
   - Broadcaster/Listener configuration
   - Synchronized surfaces
   - Channel offsets
   - Page management

2. **OSC Integration**
   - Network setup
   - iPad/Android tablet usage
   - TouchOSC configuration
   - Custom OSC layouts

3. **Encoder Customization**
   - Acceleration curves
   - Smooth vs stepped
   - Custom response timing
   - Parameter scaling

4. **Feedback Processors**
   - LED control
   - RGB colors
   - Text display formatting
   - Meter visualization
   - Value scaling formulas

5. **Learn Mode Deep Dive**
   - Auto-mapping workflow
   - Manual refinement
   - Parameter tweaking
   - Saving and organizing FX zones

---

## Implementation Priority

### Week 1 (High Priority - Core Functionality)
- [ ] 05 - FX Zones (critical for users)
- [ ] 06 - Surface Files (needed for custom setups)
- [ ] 07 - Complete Actions Reference (reference for all users)

### Week 2 (Medium Priority - Usability)
- [ ] 08 - Configuration Format (syntax reference)
- [ ] 09 - Troubleshooting (support document)
- [ ] 10 - Examples & Templates (enable rapid setup)

### Week 3+ (Lower Priority - Advanced)
- [ ] Multi-Surface Advanced
- [ ] OSC Integration
- [ ] Encoder Customization
- [ ] Advanced Feedback Processors
- [ ] Learn Mode Mastery

---

## Content Verification Checklist

For each page created, verify:
- [ ] Syntax is current (matches CSI v7.0+)
- [ ] Examples tested or verified against code
- [ ] Cross-references to other pages work
- [ ] Covers both basic and advanced usage
- [ ] Includes troubleshooting section when relevant
- [ ] References actual code files when helpful
- [ ] Provides copy-paste examples
- [ ] Consistent with established tone/style

---

## Code References for Each Page

**05 - FX Zones**:
- See: `control_surface_Reaper_actions.h` lines 13-500
- Actions: FXParam, JSFXParam, TCPFXParam, LastTouchedFXParam
- Classes: ToggleFXBypass (l.53), FXBypassDisplay (l.89), ToggleFXOffline (l.117)

**06 - Surface Files**:
- See: `control_surface_midi_widgets.h` (MIDI widget parsing)
- See: `control_surface_OSC_widgets.h` (OSC widget parsing)
- Message generators for feedback
- Widget property specifications

**07 - Complete Actions Reference**:
- See: `control_surface_Reaper_actions.h` (ALL action classes)
- See: `control_surface_action_contexts.h` (ActionType enum)
- Search codebase for `class.*Action` to find all actions

**08 - Configuration Format**:
- See: ACTIONS.md (current action syntax)
- Zone file parser logic in core plugin
- Configuration format validation

**09 - Troubleshooting**:
- Error messages from CSI code
- Common user support issues
- Debug output from monitoring facilities

**10 - Examples & Templates**:
- Existing zone files in CSI/Surfaces/*/Zones/
- Example configurations from GitHub repos
- User-submitted configurations

---

## Style Guide for New Pages

- **Tone**: Friendly, technical but accessible
- **Structure**: Subheadings, code blocks, tables, lists
- **Examples**: At least 1 complete, copy-paste example
- **Cross-refs**: Link to related pages
- **Code blocks**: Use proper syntax highlighting
- **Troubleshooting**: Include "Common Issues" section
- **Length**: 10-50 KB (varies by topic)
- **Images**: Optional but helpful (especially for GUI steps)

---

## How to Continue

1. **Pick a page** from PHASE 1 (suggest: 05-FX-Zones first)
2. **Use the outline provided** to structure content
3. **Reference specific code** (see "Code References" above)
4. **Test examples** before including
5. **Cross-link** to existing pages
6. **Add to Home.md** index when complete
7. **Verify syntax** against current CSI version

---

## Quick Stats

- **Completed**: 5 pages (~35 KB)
- **Planned**: 10+ pages (~100+ KB)
- **Total Target**: Comprehensive reference (150+ KB)
- **Reading Time**: 2-3 hours for complete documentation
- **Practical Setup Time**: 30 minutes to 2 hours depending on complexity

---

## Next Page to Create

**RECOMMENDED**: Create **05 - FX Zones** next - most users want FX parameter control.

Use the outline provided above to structure the page comprehensively, then verify against the actual code in `control_surface_Reaper_actions.h`.

---

**Status**: Foundation solid. Ready for community contributions!  
**Last Updated**: March 2026  
**Created By**: AI Documentation Assistant
