# CSI Architecture Documentation

## Overview

The Reaper Control Surface Integrator (CSI) is a sophisticated plugin system that enables deep integration of hardware control surfaces with the REAPER digital audio workstation. It implements a modular, event-driven architecture that allows hardware controllers to be mapped to REAPER functions through configuration files.

**Version**: 7.0 (based on CSI fork)
**Type**: REAPER Plugin Extension (Control Surface API)
**Platforms**: Windows, macOS, Linux

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    REAPER DAW Core                              │
├─────────────────────────────────────────────────────────────────┤
│                  REAPER Plugin API                              │
├─────────────────────────────────────────────────────────────────┤
│            CSI Plugin (IReaperControlSurface)                   │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ CSurfIntegrator (Main Control Surface Manager)           │  │
│  │  ├─ Pages (UI contexts)                                 │  │
│  │  ├─ Surfaces (MIDI/OSC I/O devices)                     │  │
│  │  ├─ Zones (configuration contexts)                      │  │
│  │  ├─ Actions (command mappings)                          │  │
│  │  └─ ZoneManagers (context orchestrators)                │  │
│  └──────────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│    I/O Layer (MIDI/OSC)                                         │
│    ├─ Midi_ControlSurfaceIO (Hardware MIDI I/O)               │
│    ├─ OSC_ControlSurfaceIO (OSC Network I/O)                  │
│    └─ Widgets (Hardware widgets: Buttons, Sliders, etc.)      │
├─────────────────────────────────────────────────────────────────┤
│              Hardware Devices / Network                         │
└─────────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. CSurfIntegrator Class
**File**: `control_surface_integrator.h/.cpp`

The main plugin class that implements `IReaperControlSurface`. Responsibilities:
- Register plugin with REAPER
- Manage Pages, Surfaces, and ZoneManagers
- Route widget messages to appropriate action contexts
- Initialize and shutdown the plugin
- Maintain global plugin state (debug level, display flags, etc.)

**Key Properties**:
- `pages_`: Vector of UI pages
- `midiSurfacesIO_`: MIDI device I/O handlers
- `oscSurfacesIO_`: OSC network I/O handlers
- `actions_`: Dictionary of all registered actions

### 2. Page Class
**File**: `control_surface_integrator.h`

Represents a UI page/context containing surfaces and zones.

**Responsibilities**:
- Hold multiple control surfaces (MIDI/OSC)
- Manage zone activation/deactivation
- Route REAPER state changes to surfaces
- Track current track selection and navigation context

**Key Methods**:
- `Run()`: Called 30x/sec for real-time updates
- `SetTrackListChange()`: Notify of track list modifications
- `SetSurfaceVolume()`, `SetSurfacePan()`: Handle track parameter updates
- `GoZone()`: Switch to a specific zone

### 3. ControlSurface Base Class
**File**: `control_surface_integrator.h`

Abstract base for MIDI and OSC surfaces.

**Responsibilities**:
- Manage widgets (hardware controls)
- Track zone state
- Handle device-specific configuration

**Concrete Implementations**:
- `Midi_ControlSurface`: Hardware MIDI device handler
- `OSC_ControlSurface`: Network OSC handler

### 4. ZoneManager Class
**File**: `control_surface_integrator.h/.cpp`

Orchestrates zone loading, activation, and action execution.

**Responsibilities**:
- Load zone configuration files (.zon)
- Manage zone activation state
- Route widget input to actions
- Handle track/FX navigation
- Manage Learn mode for FX parameters
- Support multiple navigator types (TrackNavigator, MasterTrackNavigator, etc.)

**Key Methods**:
- `PreProcessZones()`: Scan and index all available zones
- `LoadZoneFile()`: Parse and load zone configuration
- `DoAction()`: Execute action for widget input
- `DoRelativeAction()`: Handle relative encoders/controllers
- `GoZone()`: Activate a zone by name

### 5. Zone Class
**File**: `control_surface_integrator.h`

Represents a configuration context mapping widgets to REAPER actions.

**Structure**:
- Contains widgets and their associated actions
- Can have subzones for hierarchical navigation
- Tracks active/inactive state
- Supports multiple navigators (for multi-slot FX, track sends, etc.)

**Key Methods**:
- `Activate()`: Switch zone into active state
- `Deactivate()`: Exit zone
- `AddWidget()`: Register widget with zone
- `AddActionContext()`: Map action to widget
- `InitSubZones()`: Setup nested zones

### 6. Widget Class
**File**: `control_surface_integrator.h`

Represents a physical hardware widget or OSC control.

**Types**:
- `Button_Midi_CSIMessageGenerator`: MIDI button input
- `Encoder_Midi_CSIMessageGenerator`: MIDI rotary encoder
- `Slider_Midi_CSIMessageGenerator`: MIDI slider/potentiometer
- `DisplayWidget`: MIDI/OSC display output
- `OSC_Widget`: OSC control

**Responsibilities**:
- Receive hardware input messages
- Route to associated actions
- Update display feedback
- Handle acceleration/stepped values for encoders

### 7. Action Class Hierarchy
**Files**: `control_surface_action_contexts.h`, `control_surface_Reaper_actions.h`, `control_surface_manager_actions.h`

Abstract base for all mappable actions.

**Action Types** (ActionType enum):
- **NoAction** / **InvalidAction**: Placeholder actions
- **ReaperAction**: Execute REAPER native action commands
- **FXAction**: Interact with FX parameters
  - `FXParam`: Track FX parameter control
  - `JSFXParam`: JS FX parameter control
  - `TCPFXParam`: TCP-specific FX control
  - `LastTouchedFXParam`: Control last-touched FX
- **VolumeAction**: Track/Send volume control
  - `TrackVolume`: Main track volume
  - `SoftTakeover7BitTrackVolume`: Soft takeover for 7-bit MIDI
  - `SendVolume`: Track send volume
  - `RecvVolume`: Track receive volume
- **PanAction**: Pan/width control
  - `TrackPan`, `SendPan`, `RecvPan`: Various pan modes
  - `TrackPanWidth`: Stereo width
  - `TrackPanLeft`, `TrackPanRight`: Individual pan controls
- **Manager Actions**: Plugin control flow
  - `GoZone`, `GoSubZone`, `LeaveSubZone`: Zone navigation
  - `GoHome`: Return to home zone
  - `GoFXSlot`: Navigate FX chain
  - `Undo`, `Redo`, `SaveProject`
- **Messaging Actions**: External communication
  - `SendMIDIMessage`: Send MIDI messages
  - `SendOSCMessage`: Send OSC messages
  - `SpeakOSARAMessage`: Accessibility speech output

**Key Methods**:
```cpp
virtual void RequestUpdate(ActionContext *context) = 0;  // Query state
virtual void Do(ActionContext *context, double value) = 0;  // Execute
virtual ActionType GetType() const = 0;  // Identify action type
```

### 8. ActionContext Class
**File**: `control_surface_integrator.h`

Context object that bridges widgets, actions, and REAPER state.

**Responsibilities**:
- Hold action parameters
- Query/modify REAPER state
- Manage widget feedback (values, colors, text)
- Track modifier keys (Shift, Alt, Control, etc.)

**Key Methods**:
- `DoAction()`: Execute the associated action
- `UpdateWidgetValue()`: Update widget display
- `GetCurrentNormalizedValue()`: Query current state
- `GetTrackFxParamValue()`: Get FX parameter value
- `GetTrack()`: Navigate to target track

### 9. Navigator Classes
**File**: `control_surface_integrator.h`

Handle track/FX navigation logic for zones.

**Types**:
- `TrackNavigator`: Fixed track index
- `MasterTrackNavigator`: Master track
- `SelectedTrackNavigator`: Currently selected track
- `FocusedFXNavigator`: Track with focused FX UI
- `VCANavigator`, `FolderNavigator`: Special track types

**Purpose**: Allow same zone to work with different tracks based on context

### 10. I/O Components

#### Midi_ControlSurfaceIO
**File**: `control_surface_integrator.h`

Handles low-level MIDI input/output.

**Features**:
- MIDI message buffering
- SysEx support
- Device enumeration
- MCU (Mackie Control Universal) special handling
- XTouch display support

#### OSC_ControlSurfaceIO
**File**: `control_surface_integrator.h`

Handles OSC (Open Sound Control) network communication.

**Features**:
- UDP-based communication
- OSC message formatting/parsing
- X32 mixer specific support
- Customizable OSC paths

## Data Flow

### Widget Input → Action Execution

```
Hardware MIDI/OSC Input
    ↓
Widget::ProcessMidiMessage() / Widget::ProcessOSCMessage()
    ↓
ZoneManager::DoAction(widget, value)
    ↓
Zone::GetActionContexts(widget)
    ↓
ActionContext::DoAction(value)
    ↓
Action::Do(context, value) ← Specific action implementation
    ↓
REAPER State Modification / Hardware Output
```

### Action Update (Feedback)

```
REAPER State Change (track selection, FX param, etc.)
    ↓
Page::SetSurfaceVolume() / SetSurfacePan() / etc.
    ↓
Zone::RequestUpdate()
    ↓
Widget::Configure(actionContexts)
    ↓
ActionContext::RequestUpdate()
    ↓
Action::RequestUpdate(context) ← Query current state
    ↓
Widget::UpdateValue() → Send to hardware display
```

## Configuration System

### Zone Files (.zon)

Configuration files define widget-to-action mappings.

**Structure**:
```
Zone "ZoneName" "Alias" NavType=Navigator
  Widget WidgetName ActionName parameters...
  Widget WidgetName+Shift ActionName parameters...
  SubZones
    ZoneName1
    ZoneName2
  SubZonesEnd
  IncludedZones
    IncludedZoneName
  IncludedZonesEnd
ZoneEnd
```

**Key Features**:
- Modifiers: Shift, Alt, Control, Double-Press, Hold
- Value Inversion: `~` prefix
- Feedback Inversion: `^` prefix
- Acceleration: Stepped and accelerated values in brackets
- Included Zones: Reuse zone definitions

### Surface Template Files

Define hardware widget layout and names.

**MIDI Surface Format**:
```
Widget WidgetName CC 0x0C
Widget WidgetName2 Note 0x3C
Widget WidgetName3 Encoder Single Pitch 0x40
```

**OSC Surface Format**:
```
Widget WidgetName /fader1 fader
Widget WidgetName2 /button1 button
```

### CSI Configuration File (CSI.ini)

Main plugin configuration in REAPER resource path.

**Sections**:
- `[CSI]`: Global settings
- `[Pages]`: Page definitions
- `[Surfaces]`: Surface configurations
- MIDI device mappings
- OSC network settings

## Execution Model

### Initialization
1. REAPER loads plugin via `ReaperPluginEntry()`
2. `CSurfIntegrator::Init()` called
3. Zones indexed via `PreProcessZones()`
4. Surfaces and Pages loaded from config
5. Home zone activated

### Main Loop (30x/sec)
```cpp
Page::Run() {
    for each Surface in Page {
        Surface::Run();
        ZoneManager::CheckFocusedFXState();
        Zone::RequestUpdate() → Widget::UpdateValue();
    }
}
```

### Event Handling

**REAPER State Change**:
- Triggered by user action or external change
- Page methods called: `SetSurfaceVolume()`, `SetSurfacePan()`, etc.
- Zones update widget feedback

**Widget Input**:
- Captured by Surface's I/O handler
- Routed through ZoneManager
- Action executed with current value/modifier state

## Advanced Features

### Learn Mode
- User can manually train FX parameters
- System generates zone file with FX params
- Stored in `AutoGeneratedFXZones` folder

### Multi-Zone Navigation
- Subzones allow hierarchical organization
- Modifier keys switch contexts without changing page
- IncludedZones support zone reuse

### Soft Takeover
- Prevents sudden jumps in continuous controls
- Maps hardware value to current REAPER value
- Gradually allows control as values approach

### FX Navigation
- Multiple FX slot indices per zone
- Track, Selected, Master, and Focused FX contexts
- Automatic FX list updates

### Acceleration
- Stepped values for rotary encoders
- Accelerated deltas based on rotation speed
- Configurable acceleration curves

## Plugin Lifecycle

### Startup
1. Plugin entry point validation
2. REAPER API function loading
3. Action dictionary initialization
4. Configuration file parsing
5. Zone preprocessing

### Running
- Real-time MIDI/OSC I/O processing
- 30Hz feedback updates
- FX focus monitoring
- Auto-generate FX zones as needed

### Shutdown
- Surfaces closed gracefully
- All dialogs closed
- Zone data freed
- Plugin unloaded from REAPER

## Extension Points

### Custom Actions
- Inherit from `Action` base class
- Implement `Do()` and `RequestUpdate()` methods
- Register in action dictionary
- Add to appropriate category file

### Custom Widgets
- Inherit from `Widget` base class
- Implement MIDI/OSC message processing
- Add to surface template system

### Navigator Types
- Create custom `Navigator` subclass
- Register with zone system
- Support dynamic track selection logic

## Performance Considerations

1. **Zone Preprocessing**: All zones indexed on startup to avoid lookup delays
2. **Widget Caching**: Widgets cached by name for O(1) lookup
3. **Event Batching**: MIDI messages processed in batches
4. **Lazy Loading**: Zones loaded on-demand
5. **Update Throttling**: Widget feedback updates limited to 30Hz

## Error Handling

- Configuration parse errors logged with line numbers
- Invalid zone references skip gracefully
- Missing REAPER API functions detected early
- Widget input errors caught and logged
- Device connection failures don't crash plugin

## File Organization

```
reaper_csurf_integrator/
├── main.cpp                                # Plugin entry point
├── reaper_plugin.h                         # REAPER API definitions
├── reaper_plugin_functions.h               # Generated API functions
├── control_surface_integrator.h/cpp        # Main CSI class
├── control_surface_action_contexts.h       # Base action classes
├── control_surface_Reaper_actions.h        # REAPER-specific actions
├── control_surface_manager_actions.h       # Management actions
├── control_surface_midi_widgets.h          # MIDI widget types
├── control_surface_OSC_widgets.h           # OSC widget types
├── control_surface_integrator_Reaper.h     # REAPER API wrapper
├── control_surface_integrator_ui.cpp       # UI dialogs
├── handy_functions.h                       # Utility functions
├── oscpkt.hh / udp.hh                      # OSC network code
├── res.rc / resource.h                     # Windows resources
└── CMakeLists.txt                          # Build configuration
```

## References

- REAPER Plugin API: https://www.reaper.fm/sdk/plugin/plugin.php
- CSI GitHub: https://github.com/GeoffAWaddington/CSICode
- Original CSI Fork: https://github.com/gaabora/Reaper-ControlSurfaceIntegrator
