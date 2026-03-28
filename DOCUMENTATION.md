# CSI Documentation Summary

This documentation provides comprehensive coverage of the Reaper Control Surface Integrator (CSI) plugin architecture, configuration system, and available actions.

## Documentation Files

### 1. [ARCHITECTURE.md](ARCHITECTURE.md)
**Purpose**: Deep dive into CSI plugin architecture and design

**Contents**:
- High-level system overview with ASCII diagrams
- Detailed component descriptions (10+ major classes)
- Data flow diagrams (widget input → action execution)
- Configuration system explanation
- Execution model and lifecycle
- Advanced features (Learn Mode, Multi-Zone navigation, Soft Takeover, FX Navigation, Acceleration)
- Performance considerations
- Error handling strategies
- File organization

**Key Sections**:
- Core Components (CSurfIntegrator, Page, ControlSurface, ZoneManager, Zone, Widget, Action, etc.)
- Action Class Hierarchy (100+ actions organized by type)
- Navigator Classes (TrackNavigator, MasterTrackNavigator, SelectedTrackNavigator, etc.)
- I/O Components (MIDI and OSC)
- Data Flow Diagrams

**Best For**: Understanding how CSI works internally, extending CSI with custom actions/widgets, debugging issues

---

### 2. [CONFIG_FORMAT.md](CONFIG_FORMAT.md)
**Purpose**: Complete reference for all configuration file formats

**Contents**:
- File location guide (Windows/macOS/Linux)
- CSI.ini main configuration file format
- Surface template file format (MIDI and OSC)
- Zone file definition and structure
- Widget mapping syntax
- Modifiers (Shift, Alt, Control, Hold, DoublePress)
- Value modifiers (inversion, feedback inversion)
- Accelerated/stepped values
- SubZones and IncludedZones
- Action parameters for all action types
- Property system
- Complete configuration example
- Best practices
- Troubleshooting guide

**Key Sections**:
- File Locations and Directory Structure
- CSI.ini Format Reference
- Surface Template Format (MIDI/OSC widgets)
- Zone File Structure
- Widget Mapping Syntax
- Modifier System
- Comments and Special Lines
- Configuration Examples

**Best For**: Setting up CSI, creating custom zones, configuring hardware surfaces, writing surface templates

---

### 3. [ACTIONS.md](ACTIONS.md)
**Purpose**: Complete reference of all configurable actions

**Contents**:
- 11 action categories with 100+ individual actions
- Detailed documentation for each action:
  - Description and purpose
  - Parameters and syntax
  - Input/output types
  - Feedback values
  - Usage examples
  - Special behaviors

**Action Categories**:

1. **FX Parameter Actions** (8 actions)
   - FXParam, JSFXParam, TCPFXParam, LastTouchedFXParam
   - ToggleFXBypass, FXBypassDisplay, ToggleFXOffline, FXOfflineDisplay

2. **Volume Actions** (8 actions)
   - TrackVolume, SoftTakeover7BitTrackVolume, SoftTakeover14BitTrackVolume
   - SendVolume, SendVolumeDisplay, RecvVolume, MasterVolume, MasterVolumeDisplay

3. **Pan Actions** (8 actions)
   - TrackPan, TrackPanWidth, TrackPanLeft, TrackPanRight
   - SendPan, SendPanDisplay, RecvPan, MasterPan

4. **Track Control Actions** (8 actions)
   - Mute, MuteDisplay, Solo, SoloDisplay, RecArm, RecArmDisplay
   - TrackSelect, TrackNameDisplay, MasterTrackName

5. **Transport/Playback Actions** (12 actions)
   - Play, Stop, Pause, Record, RecordDisplay, Repeat, RepeatDisplay
   - FastForward, Rewind, Tempo, TempoDisplay

6. **Navigation Actions** (9 actions)
   - GoZone, GoHome, GoSubZone, LeaveSubZone, GoFXSlot
   - SelectedTrackFX, NextTrack, PreviousTrack, NextSend, PreviousSend

7. **Project Management Actions** (5 actions)
   - SaveProject, Undo, Redo, NewProject, OpenProject

8. **Messaging Actions** (3 actions)
   - SendMIDIMessage, SendOSCMessage, SpeakOSARAMessage

9. **Display Actions** (8 actions)
   - TrackVolumeDisplay, TrackNameDisplay, TrackPanDisplay
   - MuteDisplay, SoloDisplay, FXNameDisplay, TempoDisplay, TimecodeDisplay

10. **XTouch Specific Actions** (2 actions)
    - SetXTouchDisplayColors, RestoreXTouchDisplayColors

11. **Advanced/Special Actions** (4 actions)
    - Learn FX, ModifierStates, MCU Specific

**Best For**: Finding available actions, learning action syntax, understanding parameters, complete action reference

---

## Quick Start Guide

### For New Users (Setting up CSI):
1. Read [CONFIG_FORMAT.md](CONFIG_FORMAT.md) - File Locations section
2. Read [CONFIG_FORMAT.md](CONFIG_FORMAT.md) - CSI.ini and Surface Templates
3. Read [ACTIONS.md](ACTIONS.md) - Action Syntax and examples
4. Create your first zone using examples

### For Plugin Developers (Extending CSI):
1. Read [ARCHITECTURE.md](ARCHITECTURE.md) - High-Level Architecture
2. Read [ARCHITECTURE.md](ARCHITECTURE.md) - Core Components section
3. Read [ARCHITECTURE.md](ARCHITECTURE.md) - Extension Points section
4. Reference source code in `reaper_csurf_integrator/`

### For Power Users (Advanced Configuration):
1. Read [CONFIG_FORMAT.md](CONFIG_FORMAT.md) - All sections
2. Read [ACTIONS.md](ACTIONS.md) - All action categories
3. Study [ARCHITECTURE.md](ARCHITECTURE.md) - Advanced Features
4. Explore example zones in CSI/Zones directory

### For Troubleshooting:
1. Check [CONFIG_FORMAT.md](CONFIG_FORMAT.md) - Error Messages section
2. Check [ARCHITECTURE.md](ARCHITECTURE.md) - Error Handling section
3. Verify configuration syntax matches [CONFIG_FORMAT.md](CONFIG_FORMAT.md)
4. Verify action names match [ACTIONS.md](ACTIONS.md)

---

## Key Concepts

### Zones
Zones are configuration contexts that map hardware widgets to REAPER actions. Each zone defines a specific interface layout (mixer, sends, FX, etc.).

### Actions
Actions are the commands that can be executed when a widget is activated. Examples: TrackVolume, Mute, Play, GoZone.

### Widgets
Widgets represent physical hardware controls (faders, buttons, encoders, displays) or virtual OSC controls.

### Pages
Pages are top-level organizational containers that hold multiple surfaces and zone managers.

### Surfaces
Surfaces represent hardware devices (MIDI controllers) or network endpoints (OSC devices).

### Navigators
Navigators determine which track/FX slot a zone operates on (specific track, master, selected, focused, etc.).

---

## File Statistics

| Document | Lines | Sections | Code Examples |
|----------|-------|----------|----------------|
| ARCHITECTURE.md | 473 | 15+ | 20+ |
| CONFIG_FORMAT.md | 658 | 20+ | 40+ |
| ACTIONS.md | 1,169 | 40+ | 80+ |
| **Total** | **2,300+** | **75+** | **140+** |

---

## Related Resources

- **REAPER Official**: https://www.reaper.fm/
- **CSI GitHub**: https://github.com/GeoffAWaddington/CSICode
- **CSI Fork (this project)**: https://github.com/gaabora/Reaper-ControlSurfaceIntegrator
- **REAPER Plugin API**: https://www.reaper.fm/sdk/plugin/plugin.php
- **MIDI Specification**: https://en.wikipedia.org/wiki/MIDI
- **OSC Specification**: http://opensoundcontrol.org/

---

## Document Maintenance

These documentation files were generated from analysis of:
- CSI source code (control_surface_integrator.h/cpp, action classes, widget classes)
- REAPER plugin API (reaper_plugin.h, REAPER API functions)
- Configuration system (zone file parsing, surface templates, config loading)
- Existing project README and structure

**Last Updated**: March 2026
**CSI Version**: 7.0 (fork)
**Status**: Comprehensive documentation covering architecture, configuration, and all actions

---

## How to Use This Documentation

1. **Find It Quickly**: Each document has a table of contents and section headers
2. **Navigate Between Docs**: Cross-references link between related sections
3. **Search**: Use your editor's search (Ctrl+F / Cmd+F) to find specific topics
4. **Code Examples**: Look for example blocks showing real usage patterns
5. **Tables**: Reference tables summarize options and parameters
6. **Best Practices**: Each document includes "Best Practices" sections

---

## Contributing & Feedback

If you find errors, ambiguities, or missing information:
- Check against source code in `reaper_csurf_integrator/`
- Verify against REAPER plugin API documentation
- Test configurations and report issues

---

**Happy controlling!**
