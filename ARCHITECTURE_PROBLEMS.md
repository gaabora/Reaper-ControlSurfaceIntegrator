# CSI Architecture - Problems and Anti-Patterns Analysis

## Executive Summary

This document provides a detailed analysis of architectural issues, design anti-patterns, and potential problems identified in the Reaper Control Surface Integrator (CSI) codebase. These issues range from moderate design concerns to potential runtime problems.

---

## Critical Issues

### 1. **No Thread Safety / Synchronization Mechanisms**
**Severity**: CRITICAL  
**Location**: Throughout `control_surface_integrator.cpp`, `control_surface_integrator.h`  
**Evidence**:
- Zero use of mutexes, locks, or atomic variables
- Real-time 30Hz update loop in `Page::Run()` accessing shared state
- MIDI/OSC input processed asynchronously while widgets are being updated
- REAPER callbacks can trigger state changes during widget processing

**Problem**:
```cpp
// No synchronization between:
// 1. Main loop updating widget feedback (30x/sec)
// 2. MIDI/OSC input processing (variable rate)
// 3. REAPER callbacks modifying track state
// 4. Zone switching operations
```

**Impact**:
- Race conditions in state access
- Potential crashes on high MIDI input rates
- Inconsistent widget state between hardware and REAPER
- Data corruption in `actionContextDictionary_` access

**Recommended Fix**:
- Add mutex protection for shared state
- Use thread-safe containers (lock-free queues for MIDI input)
- Implement double-buffering for widget feedback

---

### 2. **Bidirectional Circular Dependencies**
**Severity**: CRITICAL  
**Location**: `Page` ↔ `ZoneManager` ↔ `Zone` ↔ `ControlSurface`

**Evidence**:
```cpp
// Page holds ControlSurface
class Page {
    vector<unique_ptr<ControlSurface>> surfaces_;
};

// ControlSurface holds ZoneManager which holds reference back to ControlSurface
class ZoneManager {
    ControlSurface *const surface_;
};

// Zone holds ZoneManager and ControlSurface
class Zone {
    ZoneManager *const zoneManager_;
    CSurfIntegrator *const csi_;
};

// ControlSurface holds Page (indirectly through Navigator)
class Navigator {
    Page *const page_;
};
```

**Problem**:
- Memory management is complex and fragile
- Deleting one object doesn't guarantee cleanup of dependent objects
- Difficult to reason about object lifetime
- Potential for dangling pointers if one component is freed before another

**Impact**:
- Use-after-free bugs possible if zones are unloaded while actions are executing
- Page destruction doesn't properly clean up all dependent surfaces
- Hard to add new components without introducing new circular references

---

### 3. **Massive Page Class - Violates Single Responsibility Principle**
**Severity**: HIGH  
**Location**: `control_surface_integrator.h` lines 4290+

**Evidence**:
```cpp
class Page {
    // Direct surface management
    vector<unique_ptr<ControlSurface>> surfaces_;
    unique_ptr<TrackNavigationManager> trackNavigationManager_;
    unique_ptr<ModifierManager> modifierManager_;
    
    // 100+ public methods including:
    void GoHome();
    void GoZone(const char *name);
    void AdjustBank(const char *zoneName, int amount);
    void TrackFXListChanged(MediaTrack *track);
    void OnTrackSelection(MediaTrack *track);
    void SignalStop();
    void SignalPlay();
    void SignalRecord();
    
    // All TrackNavigationManager facade methods (~40 methods):
    bool GetSynchPages();
    bool GetScrollLink();
    bool GetFollowMCP();
    Navigator * GetSelectedTrackNavigator();
    void ActivateVCAMode();
    void DeactivateVCAMode();
    // ... 35+ more facade methods
};
```

**Problems**:
- Page is a God Object managing 3+ major responsibilities:
  1. UI page lifecycle management
  2. Surface I/O orchestration
  3. Track navigation and selection (via facade pattern)
  4. Modifier state management
  5. Transport state (play, stop, record)
- 100+ public methods make the interface overwhelming
- Difficult to test in isolation
- Changes to one responsibility require modifying the entire class

**Impact**:
- Difficult to maintain and extend
- Adding new page features is complex
- Testing individual features is nearly impossible
- Reusing logic across projects is difficult

---

## High-Priority Issues

### 4. **String-Based Configuration with No Type Safety**
**Severity**: HIGH  
**Location**: `ZoneManager::LoadZoneFile()`, `control_surface_integrator.cpp` lines 3500+

**Evidence**:
```cpp
// Zone files loaded by string matching on zone names
if (tokens.size() > 1) {
    string widgetName;
    int modifier = 0;
    // Parse "Widget WidgetName ActionName param1 param2..."
    GetWidgetNameAndModifiers(tokens[0], widgetName, modifier, ...);
    
    Widget *widget = GetSurface()->GetWidgetByName(widgetName);
    if (widget == NULL)
        continue; // Silent failure - wrong widget name goes unnoticed
    
    // Action lookup by string
    const auto& action = csi_->GetAction(actionName);
    if (!IsSameString(action->GetName(), actionName)) {
        LogToConsole("[ERROR] InvalidAction: %s\n", actionName);
    }
}
```

**Problems**:
- No validation of widget names until runtime
- Invalid action names create `InvalidAction` objects silently
- Missing zones only detected during zone switching
- Configuration errors can propagate through entire surface
- No schema validation

**Impact**:
- User configuration errors go undetected until runtime
- Silent failures make debugging difficult
- Generated zone files may have invalid references
- No way to catch mistakes early

**Recommended Fix**:
- Create zone file validator/parser
- Validate widget existence before loading
- Provide detailed error reporting with file line numbers
- Use enum-based action lookup where possible

---

### 5. **Action Class Explosion - Inheritance Hierarchy Complexity**
**Severity**: HIGH  
**Location**: `control_surface_action_contexts.h`, `control_surface_Reaper_actions.h`

**Evidence**:
```cpp
// Base class
class Action { /* ~20 virtual methods */ };

// Trait classes (marker interfaces)
class NoAction : public Action {};
class InvalidAction : public NoAction {};
class ReaperAction : public Action {};
class FXAction : public Action {};
class SettingsAction : public Action {};
class SwitchAction : public Action {};
class ModifierAction : public Action {};
class TransportAction : public Action {};
class DisplayAction : public Action {};

// Concrete FX-related actions (very similar)
class FXParam : public FXAction {};
class JSFXParam : public FXAction {};
class TCPFXParam : public FXAction {};
class LastTouchedFXParam : public FXAction {};

// Volume actions (very similar)
class TrackVolume : public Action {};
class SoftTakeover7BitTrackVolume : public Action {};
class SoftTakeover14BitTrackVolume : public Action {};
class TrackVolumeDB : public Action {};

// Pan actions (very similar)
class TrackPan : public Action {};
class SendPan : public Action {};
class RecvPan : public Action {};
class TrackPanWidth : public Action {};
// ... 50+ action classes total
```

**Problems**:
- Similar actions duplicated across multiple subclasses
- Adding new FX type requires creating 3 new classes (JSFXParam, TCPFXParam, etc.)
- Trait markers used inconsistently with inheritance
- No composition-based approach for combining behaviors
- Each new hardware feature adds more subclasses

**Impact**:
- Very difficult to add support for new plugin formats
- Code duplication and inconsistency
- Hard to test action behavior in isolation
- Modifier the base class could break 50+ subclasses

**Recommended Fix**:
- Use composition over inheritance (Strategy pattern for action execution)
- Create parameter templates for similar actions
- Use traits/mixins for behavior composition
- Implement action factory with plugin registry

---

### 6. **Tight Coupling Between Widget and Feedback**
**Severity**: HIGH  
**Location**: `control_surface_integrator.h`, `control_surface_midi_widgets.h`, `control_surface_OSC_widgets.h`

**Evidence**:
```cpp
class Widget {
    vector<unique_ptr<FeedbackProcessor>> feedbackProcessors_;
    
    // Direct feedback update
    void UpdateValue(const PropertyList &properties, double value) {
        for (auto &feedbackProcessor : feedbackProcessors_)
            feedbackProcessor->SetValue(properties, value);
    }
};

// Feedback processors directly access MIDI/OSC surfaces
class Midi_FeedbackProcessor {
    Midi_ControlSurfaceIO *surface_;
    void SendMidiMessage(int first, int second, int third) {
        surface_->SendMidiMessage(first, second, third);
    }
};
```

**Problems**:
- Widget knows about ALL feedback mechanisms (MIDI, OSC, Display)
- Adding new feedback type requires modifying Widget class
- FeedbackProcessor tightly coupled to specific I/O surface type
- No abstraction layer for feedback handling

**Impact**:
- Hard to add new widget types or feedback mechanisms
- Widget class becomes bloated with feedback logic
- Testing widget behavior separate from feedback is difficult

---

## Medium-Priority Issues

### 7. **Learn Mode Auto-Generation Without Conflict Resolution**
**Severity**: MEDIUM  
**Location**: `AutoGeneratedFXZones` folder, zone file generation

**Evidence**:
- System automatically generates zone files in `AutoGeneratedFXZones`
- No versioning or conflict detection
- Generated files can be overwritten silently
- No user notification of auto-generation

**Problem**:
```cpp
// Somewhere in zone loading:
string fxZoneFolder = baseDir + surfaceFolderProp + "/FXZones";
// Auto-generates zones, but:
// - No conflict resolution if file exists
// - No backup of old versions
// - User modifications could be lost
```

**Impact**:
- User customizations lost during re-learning
- No way to track what was auto-generated vs. manual
- Debugging zones is difficult

---

### 8. **Hard-Coded Constants and Magic Numbers**
**Severity**: MEDIUM  
**Location**: Throughout `control_surface_integrator.h/.cpp`

**Evidence**:
```cpp
// Hard-coded update frequency
static const int REAPER__CONTROL_SURFACE_REFRESH_ALL_SURFACES = 41743;

// Device-specific logic
if (widgetType == "FB_MCUVUMeter" || widgetType == "FB_MCUXTVUMeter") {
    int displayType = widgetType == "FB_MCUVUMeter" ? 0x14 : 0x15;
    // ...
}

// Magic numbers for acceleration
const int baseTickCount = (NUM_ELEM(s_tickCounts_) > stepCount)
    ? s_tickCounts_[stepCount]
    : s_tickCounts_[NUM_ELEM(s_tickCounts_) - 1];
```

**Problems**:
- Device-specific handling scattered throughout code
- Difficult to add new device support
- Magic numbers without explanation
- Hard-coded paths and configurations

---

### 9. **Navigator Complexity - Repetitive Track Navigation Logic**
**Severity**: MEDIUM  
**Location**: `control_surface_integrator.h` lines 600-900

**Evidence**:
```cpp
class TrackNavigator : public Navigator {
    // Complex track navigation logic
    virtual MediaTrack *GetTrack() override;
};

class SelectedTrackNavigator : public Navigator {
    // Similar logic but for selected track
    virtual MediaTrack *GetTrack() override;
};

class MasterTrackNavigator : public Navigator {
    // Similar logic but for master track
    virtual MediaTrack *GetTrack() override;
};

class FocusedFXNavigator : public Navigator {
    // Similar logic but for focused FX track
    virtual MediaTrack *GetTrack() override;
};

class FixedTrackNavigator : public Navigator {
    // Direct track pointer
    virtual MediaTrack *GetTrack() override { return track_; }
};
```

**Problems**:
- 5+ similar navigator classes with duplicated logic
- Should use composition or strategy pattern
- Track selection logic duplicated across navigators
- Adding new track selection mode requires new class

---

### 10. **Weak Error Handling and Silent Failures**
**Severity**: MEDIUM  
**Location**: Throughout codebase

**Evidence**:
```cpp
// Zone loading - missing widget silently skipped
Widget *widget = GetSurface()->GetWidgetByName(widgetName);
if (widget == NULL)
    continue; // Silent failure

// Invalid action creates InvalidAction object
const auto& action = csi_->GetAction(actionName);
if (!IsSameString(action->GetName(), actionName)) {
    // Logs warning but continues with invalid action
}

// Zone not found - just returns NULL
if (zoneInfo.find(zoneName) == zoneInfo.end()) {
    // No zone - action silently does nothing
}
```

**Problems**:
- Silent failures make debugging difficult
- Users don't know configuration is broken
- Error messages could be more descriptive
- No recovery mechanisms

---

### 11. **ActionContext - Kitchen Sink Class**
**Severity**: MEDIUM  
**Location**: `control_surface_integrator.h` lines 800-1200

**Evidence**:
```cpp
class ActionContext {
    // Widget and action binding
    Action *action_;
    Widget *widget_;
    Zone *zone_;
    
    // Range management
    double rangeMinimum_;
    double rangeMaximum_;
    vector<double> steppedValues_;
    
    // Value inversion and feedback
    bool isValueInverted_;
    bool isFeedbackInverted_;
    
    // Hold/repeat logic
    int holdDelayMs_;
    int holdRepeatIntervalMs_;
    DWORD lastHoldRepeatTs_;
    bool holdActive_;
    bool holdRepeatActive_;
    double deferredValue_;
    
    // Blink state
    bool blinkSet_;
    bool blinkActive_;
    int blinkIntervalMs_;
    DWORD lastBlinkTs_;
    
    // Acceleration
    vector<double> acceleratedDeltaValues_;
    vector<int> acceleratedTickValues_;
    int accumulatedIncTicks_;
    
    // Command execution
    int commandId_;
    const char* commandText_;
    bool needsReloadAfterRun_;
    
    // OSD (On-Screen Display)
    osd_data osdData_;
    
    // Color support
    bool supportsColor_;
    vector<rgba_color> colorValues_;
    int currentColorIndex_;
    
    // Properties
    PropertyList widgetProperties_;
    
    // ... 50+ methods
};
```

**Problems**:
- ActionContext is too large and does too much:
  1. Manages widget-action binding
  2. Handles range/step logic
  3. Manages inversion/feedback
  4. Handles hold/repeat timing
  5. Manages blink state
  6. Handles color values
  7. Manages acceleration
  8. Processes OSD messages
- Difficult to test individual aspects
- Adding new feature (e.g., new modifier type) affects entire class
- Should be split into multiple smaller classes

---

### 12. **No Clear Dependency Injection Pattern**
**Severity**: MEDIUM  
**Location**: Throughout codebase

**Evidence**:
```cpp
// Hard-wired dependencies everywhere
class Page {
    Page(...) : csi_(csi), name_(name), 
                trackNavigationManager_(make_unique<TrackNavigationManager>(csi_, this, ...)),
                modifierManager_(make_unique<ModifierManager>(csi_, this, ...)) {}
};

class ZoneManager {
    ZoneManager(...) : csi_(csi), surface_(surface) {}
};
```

**Problems**:
- Difficult to create test instances
- Hard to swap implementations for testing
- Dependencies are implicit, not obvious from constructor
- No way to inject mock objects

---

## Low-Priority Issues (Code Quality)

### 13. **Inconsistent Error Handling Patterns**
- Some errors logged to console, others silently ignored
- No centralized error handling strategy
- Exception throwing from some operations but not others

### 14. **Magic Configuration Values**
- Refresh rates hard-coded to 30Hz
- Double-press detection timing not configurable
- Acceleration curves not easily customizable

### 15. **Poor Separation of Concerns**
- Configuration parsing mixed with state management
- UI logic mixed with business logic
- File I/O scattered throughout classes

### 16. **Limited Extensibility**
- Adding new action types requires modifying multiple files
- Adding new widget types requires modifying widget factory
- Adding new feedback types requires modifying widget class

---

## Detailed Problem Scenarios

### Scenario 1: Adding Support for New MIDI Controller Type
**Current Approach**: 
1. Create new MIDI widget feedback processor class (e.g., `NewDeviceRGB_Midi_FeedbackProcessor`)
2. Add case in `Midi_ControlSurface::ProcessMidiWidget()` to instantiate it
3. Create device-specific surface template file
4. Update CSI.ini with new surface

**Issues**:
- Code duplication with similar RGB processors
- Changes to core widget class if new capabilities needed
- No way to enable/disable device without code changes
- Hard-coded device-specific logic scattered throughout

---

### Scenario 2: Diagnosing Failed Zone Load
**Current Situation**:
- User loads invalid zone configuration
- Zone silently fails to load
- Widgets don't display anything
- No clear error message about what's wrong

**Better Approach**:
- Validate zone file before loading
- Report which widgets/actions are missing
- Provide recovery options

---

### Scenario 3: Race Condition Under High MIDI Load
**Current Situation**:
- Rapid MIDI input processing updates `actionContextDictionary_`
- Main 30Hz loop reads from same dictionary for feedback updates
- No synchronization between threads/callbacks
- Potential crash or data corruption

**Proper Approach**:
- Use mutex or lock-free queue
- Double-buffer state updates
- Ensure atomic widget state changes

---

## Recommendations Summary

### Immediate (Critical)
1. Add thread-safe synchronization for shared state
2. Create circular dependency documentation and cleanup strategy
3. Add validation for zone file loading

### Short-term (High-priority)
1. Break up `Page` class into smaller focused components
2. Extract configuration parsing into separate validator
3. Create action factory with plugin registry
4. Add mutex protection to `actionContextDictionary_`

### Medium-term (Medium-priority)
1. Refactor Action hierarchy using composition
2. Create feedback abstraction layer
3. Implement dependency injection
4. Create navigator strategy pattern
5. Add comprehensive error handling framework

### Long-term (Architectural)
1. Consider model-view-controller (MVC) pattern
2. Implement event-driven architecture
3. Create plugin system for actions/widgets/feedback
4. Separate domain logic from I/O handling
5. Add comprehensive test suite

---

## Files Most Affected by Issues

1. `control_surface_integrator.h` - 4972 lines, needs decomposition
2. `control_surface_integrator.cpp` - 4523 lines, too many responsibilities
3. `control_surface_action_contexts.h` - Action hierarchy explosion
4. `control_surface_Reaper_actions.h` - Massive file with 50+ action classes

---

## Conclusion

While CSI is a functional and sophisticated plugin system, it suffers from typical monolithic architecture issues. The most critical concern is thread safety given the real-time MIDI processing in a plugin environment. The codebase would benefit from significant refactoring to break down large classes, reduce coupling, and introduce proper synchronization mechanisms.

The good news is that these are solvable problems, and the modular nature of the code (zones, pages, surfaces) provides a foundation for incremental improvements.

---

## Additional Issues Found (Supplemental Analysis)

The following issues were identified through a second pass of the codebase and are **not** covered by the 16 issues above.

---
---

### 20. **`using namespace std` in Header Files**
**Severity**: HIGH  
**Location**: [handy_functions.h](reaper_csurf_integrator/handy_functions.h#L24), [control_surface_integrator_Reaper.h](reaper_csurf_integrator/control_surface_integrator_Reaper.h#L20)

**Evidence**:
```cpp
// handy_functions.h line 24:
using namespace std;

// control_surface_integrator_Reaper.h line 20:
using namespace std;
```

**Problem**: These headers are included (directly or transitively) by every translation unit. `using namespace std` in a header pollutes the global namespace for all downstream code, creating name collision risks (e.g., `std::map`, `std::min`, `std::max`, `std::move` conflicting with WDL or REAPER symbols).

**Impact**: Hard-to-diagnose compilation errors or silent wrong-function-selected bugs. Violates C++ Core Guidelines (SF.7).

---

### 21. **Mutable Global State in Header-Declared `static` Variables**
**Severity**: HIGH  
**Location**: [control_surface_midi_widgets.h](reaper_csurf_integrator/control_surface_midi_widgets.h#L687), [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h#L110)

**Evidence**:
```cpp
// control_surface_integrator.h line 110 — included by 2 .cpp files
static vector<HWND> g_openDialogs;

// control_surface_midi_widgets.h line 688
vector<LEDRingRangeColor> s_encoderRingColors;  // non-static global in header

// Function that populates it (line 689):
static const vector<LEDRingRangeColor> &GetColorValues(const string &inputProperty)
{
    s_encoderRingColors.clear();
    // ... populates and returns reference to global
    return s_encoderRingColors;
}

// control_surface_integrator.h line 110:
static vector<HWND> g_openDialogs;
```

**Problem**: 
- `static` in a header creates a **separate copy** per translation unit. `g_openDialogs` in `control_surface_integrator.cpp` and `control_surface_integrator_ui.cpp` are two different vectors. Dialogs pushed from the UI file are invisible to `CloseAllDialogs()` when called from the main cpp file, and vice versa.
- `s_encoderRingColors` is a non-static, non-const global vector in a **header file** that gets included by multiple translation units — each TU gets its own copy. The `GetColorValues()` function returns a const reference to this global, but clears and repopulates it on every call. If two callers use the returned reference concurrently, or if one caller holds the reference while another calls `GetColorValues()`, the data is invalidated. `g_openDialogs` as `static` in a header similarly produces separate copies per TU, meaning dialogs registered in one TU are invisible to another.
**Impact**: Open dialogs are not properly tracked across compilation units. Memory and resource leaks from undestroyed windows.

**Impact**: Data corruption between concurrent callers of `GetColorValues()`. The `g_openDialogs` vector being per-TU means `CloseAllDialogs()` won't actually close dialogs registered by different source files.

---

### 22. **`strcpy` Without Bounds Checking (Buffer Overflow Risk)**
**Severity**: HIGH  
**Location**: [control_surface_integrator_ui.cpp](reaper_csurf_integrator/control_surface_integrator_ui.cpp#L546-L547), [lines 604-608](reaper_csurf_integrator/control_surface_integrator_ui.cpp#L604-L608)

**Evidence**:
```cpp
// Line 546-547 — copying user-parsed tokens to fixed-size buffers:
strcpy(t.suffix, tokens[1].c_str());   // suffix is char[SMLBUF] (256)
strcpy(t.modifiers, tokens[0].c_str()); // modifiers is char[SMLBUF] (256)

// Lines 604-608 — same pattern:
strcpy(fxTemplate->paramWidget, tokens[0].c_str());
strcpy(fxTemplate->nameWidget, tokens[0].c_str());
strcpy(fxTemplate->valueWidget, tokens[0].c_str());
```

**Problem**: Tokens are parsed from user-editable zone/config files. If a user-provided string exceeds the buffer size, `strcpy` writes past the buffer boundary — classic buffer overflow.

**Impact**: Memory corruption, crashes, potential security vulnerability (plugin runs in REAPER's address space).

**Fix**: Use `lstrcpyn_safe()` (already used elsewhere in the codebase) or `snprintf`.

---

### 23. **OSC Socket Resource Leak on Error Paths**
**Severity**: HIGH  
**Location**: [control_surface_integrator.cpp](reaper_csurf_integrator/control_surface_integrator.cpp#L393-L430)

**Evidence**:
```cpp
static oscpkt::UdpSocket *GetInputSocketForPort(string surfaceName, int inputPort)
{
    // ... check existing sockets ...
    
    oscpkt::UdpSocket *newInputSocket = new oscpkt::UdpSocket();
    
    if (newInputSocket)
    {
        newInputSocket->bindTo(inputPort);
        
        if (! newInputSocket->isOk())
        {
            return NULL;   // LEAK: newInputSocket is never deleted!
        }
        
        s_inputSockets.Add(new OSCSurfaceSocket(surfaceName, newInputSocket));
        return newInputSocket;
    }
    return NULL;
}

static oscpkt::UdpSocket *GetOutputSocketForAddressAndPort(...)
{
    oscpkt::UdpSocket *newOutputSocket = new oscpkt::UdpSocket();
    
    if (newOutputSocket)
    {
        if ( ! newOutputSocket->connectTo(address, outputPort))
        {
            return NULL;   // LEAK: newOutputSocket is never deleted!
        }
        
        if ( ! newOutputSocket->isOk())
        {
            return NULL;   // LEAK: newOutputSocket is never deleted!
        }
        // ...
    }
}
```

**Problem**: When socket creation succeeds but binding/connecting fails, the allocated `UdpSocket` object is leaked — `new` is called but `delete` is never called on the error path.

**Impact**: Memory leak every time a port binding or connection fails. If a user misconfigures OSC ports and retries, memory accumulates.

---

### 24. **Excessive Static Mutable State in UI Code**
**Severity**: HIGH  
**Location**: [control_surface_integrator_ui.cpp](reaper_csurf_integrator/control_surface_integrator_ui.cpp#L17-L31), [line 339](reaper_csurf_integrator/control_surface_integrator_ui.cpp#L339)

**Evidence** (partial):
```cpp
static int s_dlgResult = IDCANCEL;
static bool s_isUpdatingParameters = false;
static HWND s_hwndLearnFXDlg = NULL;
static Widget *s_currentWidget = NULL;
static int s_currentModifier = 0;
static string s_pageSurfaceFXLearnLevel;
static int s_lastTouchedParamNum = -1;
static double s_lastTouchedParamValue = -1.0;
static MediaTrack *s_focusedTrack = NULL;
static int s_fxSlot = 0;
static char s_fxName[MEDBUF];
static char s_fxAlias[MEDBUF];
// ... and 20+ more
static bool s_editMode = false;
static string s_pageName;
static string s_oldSurfaceName;
static string s_surfaceName;
static string s_surfaceType;
static int s_surfaceInPort = 0;
static int s_surfaceOutPort = 0;
// ... and more:
static ActionContext *context = NULL;  // line 339 — raw pointer as global state!
```

**Problem**: Over 20 static mutable variables are used to share state between dialog procedures. This is a fragile, non-reentrant design. The `static ActionContext *context` on line 339 is particularly dangerous — it's set in one dialog's `WM_INITDIALOG` and used across multiple `WM_COMMAND` handlers with no synchronization or ownership tracking.

**Impact**: Opening multiple dialogs or interacting with dialogs while state is being updated can cause data corruption. The stale `s_focusedTrack` pointer can become dangling if the track is removed while the dialog is open.

---

### 25. **SysEx Buffer Overflow — No Bounds Check on Display Text Length**
**Severity**: HIGH  
**Location**: Throughout [control_surface_midi_widgets.h](reaper_csurf_integrator/control_surface_midi_widgets.h) — multiple FeedbackProcessor classes

**Evidence**:
```cpp
// Typical pattern (seen in SCE24OLED, SCE24Text, MCUDisplay, XTouchDisplay, etc.):
struct {
    MIDI_event_ex_t evt;    // midi_message[4] declared in base struct
    char data[256];         // overflow space
} midiSysExData;

// Header bytes are added (6-15 bytes), then:
for (int i = 0; displayText[i]; ++i)
    midiSysExData.evt.midi_message[midiSysExData.evt.size++] = displayText[i];

midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
```

**Problem**: The `data[256]` field provides 256 bytes of extra space beyond the 4-byte `midi_message` array. With ~15 bytes of header, this leaves room for ~245 characters of text. If display text exceeds this, _or_ if `GetRestrictedLengthText()` is not called (as in several code paths), the buffer will overflow. Additionally, the `SendMIDIMessage` action class ([control_surface_manager_actions.h](reaper_csurf_integrator/control_surface_manager_actions.h#L36-L44)) uses `data[128]` and iterates over user-provided tokens with no length check.

**Impact**: A long display string or a long SysEx message in a zone file can overflow the stack buffer, causing crashes.

---

### 26. **Massive Code Duplication in Action Classes (Send vs Receive vs Track)**
**Severity**: MEDIUM  
**Location**: [control_surface_Reaper_actions.h](reaper_csurf_integrator/control_surface_Reaper_actions.h) — lines 1008-1580 (Send actions) and lines 1580-2000 (Receive actions)

**Evidence**:
```cpp
// TrackSendVolume — uses GetSlotIndex() + numHardwareSends
class TrackSendVolume : public VolumeAction {
    virtual void Do(ActionContext *context, double value) override {
        if (MediaTrack *track = context->GetTrack()) {
            int numHardwareSends = GetTrackNumSends(track, 1);
            SetTrackSendUIVol(track, context->GetSlotIndex() + numHardwareSends, normalizedToVol(value), 0);
        }
    }
};

// TrackReceiveVolume — nearly identical but uses -(GetSlotIndex()+1)
class TrackReceiveVolume : public VolumeAction {
    virtual void Do(ActionContext *context, double value) override {
        if (MediaTrack *track = context->GetTrack())
            SetTrackSendUIVol(track, -(context->GetSlotIndex() + 1), normalizedToVol(value), 0);
    }
};
```

The following class groups are near-identical copies differing only in direction parameter:
- `TrackSendVolume` / `TrackReceiveVolume` / `TrackSendVolumeDB` / `TrackReceiveVolumeDB`
- `TrackSendPan` / `TrackReceivePan` / `TrackSendPanPercent` / `TrackReceivePanPercent`
- `TrackSendMute` / `TrackReceiveMute`
- `TrackSendInvertPolarity` / `TrackReceiveInvertPolarity`
- `TrackSendStereoMonoToggle` / `TrackReceiveStereoMonoToggle`
- `TrackSendPrePost` / `TrackReceivePrePost`
- All corresponding Display classes

**Problem**: There are approximately 20+ pairs of near-identical Send/Receive classes spanning ~1000 lines that differ only in the send-direction constant (0 vs -1) and the slot index calculation.
The same pattern repeats for Pan, Mute, InvertPolarity, StereoMonoToggle, PrePost, NameDisplay, VolumeDisplay, PanDisplay, StereoMonoDisplay, and PrePostDisplay — **22 pairs** of nearly identical classes that differ only in the index calculation (`sendIndex` vs `-1-recvIndex`).

**Impact**: ~1000 lines of duplicated code. Any bug fix must be applied to both Send and Receive variants. Adding a new send/receive property requires creating two new classes.

**Fix**: Parameterize with a sign/offset, or use a template/strategy to differentiate Send vs Receive.

---

### 27. **Inconsistent Use of `GetSlotIndex()` vs `GetParamIndex()` in Send/Receive Actions**
**Severity**: MEDIUM  
**Location**: [control_surface_Reaper_actions.h](reaper_csurf_integrator/control_surface_Reaper_actions.h) — `TrackSendVolumeDB` vs `TrackSendVolume`

**Evidence**:
```cpp
// TrackSendVolume::Do uses GetSlotIndex():
SetTrackSendUIVol(track, context->GetSlotIndex() + numHardwareSends, ...);

// But TrackSendVolumeDB::Do uses GetParamIndex():
SetTrackSendUIVol(track, context->GetParamIndex() + numHardwareSends, ...);

// Same inconsistency for TrackSendPan (uses GetSlotIndex) vs TrackSendPanPercent (uses GetParamIndex)
// And TrackReceiveVolume (uses GetSlotIndex) vs TrackReceiveVolumeDB (uses GetParamIndex)
```

**Problem**: Some action classes use `GetSlotIndex()` and others use `GetParamIndex()` for what should be the same value. While they may currently return the same result, this is fragile and likely a copy-paste oversight from the class explosion.

**Impact**: If `GetSlotIndex()` and `GetParamIndex()` diverge in the future, send/receive volume and pan controls will target the wrong send slot.

---

### 28. **Duplicated `rgbToColor()` Implementation with Explicit Comment Warning**
**Severity**: MEDIUM  
**Location**: [control_surface_OSC_widgets.h](reaper_csurf_integrator/control_surface_OSC_widgets.h#L30-L80), [control_surface_midi_widgets.h](reaper_csurf_integrator/control_surface_midi_widgets.h#L1940-L1990)

**Evidence**:
```cpp
// control_surface_OSC_widgets.h line 28-29:
/*
 * NOTE: Any changes made to rgbToColor here must also be mirrored in class
 * XTouchDisplay_Midi_FeedbackProcessor in control_surface_midi_widgets.h
 */
static int rgbToColor(int r, int g, int b) { ... }

// control_surface_midi_widgets.h line 1943-1944:
/*
 * NOTE: Any changes made to rgbToColor here must also be mirrored in class
 * OSC_X32FeedbackProcessor in control_surface_OSC_widgets.h
 */
static int rgbToColor(int r, int g, int b) { ... }
```

**Problem**: The code literally has comments warning developers to manually keep two copies in sync. This is the textbook definition of DRY violation.


**Fix**: Extract to a shared utility (e.g., in `handy_functions.h`).
---

### 29. **Massive SysEx Construction Code Duplication in Widget Feedback Processors**
**Severity**: MEDIUM  

**Location**: [control_surface_midi_widgets.h](reaper_csurf_integrator/control_surface_midi_widgets.h) (2661 lines, 34 `Midi_FeedbackProcessor` subclasses)
 Throughout SCE24 classes, MCU classes, FP classes, XTouch classes

**Evidence**: Listing of feedback processor classes (partial):
- `TwoState_Midi_FeedbackProcessor`
- `FPTwoStateRGB_Midi_FeedbackProcessor`
- `SCE24TwoStateLED_Midi_FeedbackProcessor`
- `NovationLaunchpadMiniRGB7Bit_Midi_FeedbackProcessor`
- `FaderportRGB_Midi_FeedbackProcessor`
- `AsparionRGB_Midi_FeedbackProcessor`
- `MCUVUMeter_Midi_FeedbackProcessor`
- `AsparionVUMeter_Midi_FeedbackProcessor`
- `FPVUMeter_Midi_FeedbackProcessor`
- `MCUDisplay_Midi_FeedbackProcessor`
- `IconDisplay_Midi_FeedbackProcessor`
- `AsparionDisplay_Midi_FeedbackProcessor`
- `XTouchDisplay_Midi_FeedbackProcessor`
- `FPDisplay_Midi_FeedbackProcessor`
- `QConLiteDisplay_Midi_FeedbackProcessor`
- ... and 19 more

The following SysEx construction boilerplate is duplicated 20+ times across the file:
```cpp
struct {
    MIDI_event_ex_t evt;
    char data[256];
} midiSysExData;
midiSysExData.evt.frame_offset=0;
midiSysExData.evt.size=0;
midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF0;
midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0x00;
// ... device-specific bytes ...
midiSysExData.evt.midi_message[midiSysExData.evt.size++] = 0xF7;
SendMidiSysExMessage(&midiSysExData.evt);
```

Classes with near-identical SysEx construction: `SCE24TwoStateLED`, `SCE24OLED` (ForceClear + ForceValue), `SCE24Text` (ForceClear + ForceValue), `SCE24Encoder` (ForceClear + Configure), `MCUDisplay`, `IconDisplay`, `AsparionDisplay`, `XTouchDisplay`, `FPDisplay`, `QConLiteDisplay`, `NovationLaunchpadMiniRGB7Bit`.

**Problem**:  Many classes share 80–90% identical code with slight variations in SysEx header bytes, display offset calculations, or color encoding. Adding support for a new controller requires creating yet another copy-pasted class.
The same structural pattern of building a SysEx message is copy-pasted with minor variations in header bytes and payload. There is no helper function or builder class to construct SysEx messages.

**Impact**: Maintenance nightmare. Bug fixes must be applied across many near-identical classes.
The file is 2881 lines, much of which is boilerplate. The sheer size (113KB) makes the file difficult to navigate and review.

---

### 30. **`LogMessage()` Opens and Closes Log File on Every Call**
**Severity**: MEDIUM  
**Location**: [handy_functions.h](reaper_csurf_integrator/handy_functions.h#L88-L97)

**Evidence**:
```cpp
static void LogMessage(const char* msg) {
    ofstream logFile(string(GetResourcePath()) + "/CSI/CSI.log", ios::app);
    if (logFile.is_open()) {
        char timeStr[32];
        time_t rawtime;
        time(&rawtime);
        struct tm* timeinfo = localtime(&rawtime);
        strftime(timeStr, sizeof(timeStr), "[%y-%m-%d %H:%M:%S] ", timeinfo);
        logFile << timeStr << msg;
    }
}
```

**Problem**: Every log message opens the file, writes, and closes it (via RAII). In a real-time audio plugin running at 30Hz with multiple surfaces, this can mean hundreds of file open/close operations per second when debug logging is enabled. Additionally, `localtime()` is not thread-safe on all platforms.

**Impact**: Performance degradation during debug logging. Potential file corruption if called concurrently from multiple threads (no synchronization). `localtime()` can return inconsistent data under concurrent access.

---

### 31. **Static Functions Defined in Headers Create Bloat and ODR Risks**
**Severity**: MEDIUM  
**Location**: [handy_functions.h](reaper_csurf_integrator/handy_functions.h) (entire file), [control_surface_integrator_Reaper.h](reaper_csurf_integrator/control_surface_integrator_Reaper.h#L27), [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h#L142-L151)

**Evidence**:
```cpp
// handy_functions.h — 15+ static functions including:
static double int14ToNormalized(...) { ... }
static double normalizedToVol(double val) { ... }
static double volToNormalized(double vol) { ... }
static void LogToConsole(...) { ... }
static bool IsSameString(...) { ... } // 4 overloads

// control_surface_integrator_Reaper.h:
static vector<string> ExplodeString(...) { ... }
static bool GetColorValue(...) { ... }

// control_surface_integrator.h:
static char *format_number(...) { ... }
static bool IsCommentedOrEmpty(...) { ... }
```

**Problem**: `static` functions in headers produce **separate copies** in every translation unit that includes them. This bloats the binary and means each TU has its own copy of the function. Any non-trivial static local state (e.g., the `static std::string relativePath` in `GetRelativePath()` at [handy_functions.h line 168](reaper_csurf_integrator/handy_functions.h#L168)) is per-TU, which can cause subtle inconsistencies.

**Impact**: Binary bloat from duplicated function bodies across ~5 translation units. Static local variables are not shared as might be expected.

---

### 32. **File Handle Leak Risk in `SaveZone()` on Exception Path**
**Severity**: MEDIUM  
**Location**: [control_surface_integrator_ui.cpp](reaper_csurf_integrator/control_surface_integrator_ui.cpp#L681-L865)

**Evidence**:
```cpp
FILE *fxFile = fopenUTF8(filePath,"wb");
if (fxFile)
{
    fprintf(fxFile, "Zone \"%s\" \"%s\"\n", s_fxName, s_fxAlias);
    
    // ~180 lines of fprintf calls and ifstream operations that could throw
    // ...
    
    fclose(fxFile);  // Line 858 — only reached if no exception
}
```

**Problem**: The `fxFile` is opened with `fopenUTF8` (which returns a `FILE*`) but is inside a `try` block. If any of the `ifstream` operations or string operations throw an exception before `fclose(fxFile)` is reached, the file handle leaks. Using `FILE*` with C++ exception-throwing code is inherently unsafe without RAII wrappers.

**Impact**: File handle leak on exception. On Windows, this can leave the file locked, preventing subsequent writes.

---

### 33. **`int` Used Where `bool` is Appropriate (Type Confusion)**
**Severity**: LOW  
**Location**: [control_surface_midi_widgets.h](reaper_csurf_integrator/control_surface_midi_widgets.h#L1026) — `AsparionRGB_Midi_FeedbackProcessor`, [line 1922](reaper_csurf_integrator/control_surface_midi_widgets.h#L1922) — `XTouchDisplay_Midi_FeedbackProcessor`

**Evidence**:
```cpp
// AsparionRGB_Midi_FeedbackProcessor line 1026:
private:
    int preventUpdateTrackColors_;   // used as boolean
// Constructor:
    preventUpdateTrackColors_ = false;  // assigned bool to int

// XTouchDisplay_Midi_FeedbackProcessor line 1922:
    int preventUpdateTrackColors_;   // same issue
```

**Problem**: `preventUpdateTrackColors_` is declared as `int` but used exclusively as a boolean (assigned `true`/`false`, tested in `if` statements). This obscures intent and wastes memory.

**Impact**: Minor — misleading to readers, but functionally correct due to implicit conversions.

---

### 34. **Commented-Out Code and TODO/FIXME Markers Left in Production Code**
**Severity**: LOW  
**Location**: Throughout multiple files

**Evidence**:
```cpp
// control_surface_integrator.cpp line 399-400:
//cerr << "Error opening port " << PORT_NUM << ": " << inSocket_.errorMessage() << "\n";
return NULL;

// control_surface_Reaper_actions.h ~line 2612-2620 (MoveCursor::Do):
if (IsSameString(amount, "Bar")) //FIXME: replace all string- with enum- comparison
    DAW::SendCommandMessage(41042);
else if (IsSameString(amount, "Marker")) //FIXME: replace all string- with enum- comparison
    DAW::SendCommandMessage(40173);

// control_surface_midi_widgets.h ~line 1424 (MCUVUMeter):
// THE FOLLOWING COMMENTED OUT CODE WILL BE PUSHED AT A LATER DATE.

// control_surface_integrator.cpp line 1041-1042:
//FIXME: so what? make backup and generate new, or at least prompt to confirm
```

**Problem**: Multiple FIXME comments indicate known issues that haven't been addressed. Commented-out code (error handling via `cerr`, alternative VU meter code) is dead weight that suggests incomplete development.

**Impact**: Dead code increases cognitive load. FIXME markers indicate unresolved design decisions that could affect users.

---

### 35. **Massive Individual File Sizes Impede Maintainability**
**Severity**: MEDIUM  
**Location**: All main source files

| File | Lines | Size |
|------|-------|------|
| [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h) | 5,115 | 177KB |
| [control_surface_integrator.cpp](reaper_csurf_integrator/control_surface_integrator.cpp) | 4,793 | 183KB |
| [control_surface_integrator_ui.cpp](reaper_csurf_integrator/control_surface_integrator_ui.cpp) | 4,016 | 177KB |
| [control_surface_Reaper_actions.h](reaper_csurf_integrator/control_surface_Reaper_actions.h) | 3,430 | 132KB |
| [control_surface_midi_widgets.h](reaper_csurf_integrator/control_surface_midi_widgets.h) | 2,881 | 114KB |
| [control_surface_manager_actions.h](reaper_csurf_integrator/control_surface_manager_actions.h) | 1,160 | 45KB |

**Problem**: Six source files total over 21,000 lines and 828KB. Industry guidelines suggest files over 500-1000 lines need decomposition. Headers exceeding 100KB result in significant compile times since they're textually included in every TU.

**Impact**: Long compile times, difficult code reviews, high probability of merge conflicts in team development.

---

### 36. **Monolithic Widget Type Dispatching via Long if-else-if Chains**
**Severity**: MEDIUM  
**Location**: [control_surface_integrator.cpp](reaper_csurf_integrator/control_surface_integrator.cpp#L525-L770) — `ProcessMidiWidget()` method

**Evidence**:
```cpp
// 245 lines of if-else-if comparing string widget types:
if (widgetType == "AnyPress" && (size == 4 || size == 7))
    CSIMessageGeneratorsByMessage_.insert(...);
else if (widgetType == "Press" && size == 4)
    CSIMessageGeneratorsByMessage_.insert(...);
else if (widgetType == "Press" && size == 7)
    { ... }
else if (widgetType == "Fader14Bit" && size == 4)
    ...
// ... ~50 more branches for feedback processors:
else if (widgetType == "FB_TwoState" && size == 7)
    ...
else if (widgetType == "FB_NovationLaunchpadMiniRGB7Bit" && size == 4)
    ...
else if (widgetType == "FB_MCUDisplayUpper" || widgetType == "FB_MCUDisplayLower" ...)
    ...
```

**Impact**: Adding a new widget type requires modifying this chain. No compile-time validation. O(n) lookup on every widget processed during surface loading.

**Fix**: Use a factory registry (`map<string, factory_function>`) populated at initialization.

---



**Problem**: A single 245-line if-else-if chain handles all widget type strings. Adding a new widget type requires modifying this method. There's no registration/factory pattern.

**Impact**: Very high cyclomatic complexity. Error-prone — easy to miss a condition or add a new branch in the wrong place. Violates Open/Closed Principle.

---

### 37. **`DAW` Class Is a Static Utility Class With Unrelated Responsibilities**
**Severity**: MEDIUM  
**Location**: [control_surface_integrator_Reaper.h](reaper_csurf_integrator/control_surface_integrator_Reaper.h#L157-L652) — `class DAW`

**Evidence**:
The `DAW` class contains 50+ static methods covering:
- Track volume/pan control (`GetTrackVolumeValue`, `SetTrackVolumeValue`)
- Solo/mute/arm (`GetTrackSolo`, `SetTrackSolo`, `GetTrackMute`, etc.)
- FX management (`GetTrackFxName`, `IsFxInstrument`, `CheckTrackFxParamHasVolumeCurve`)
- Undo/redo (`CanUndo`, `Undo`, `CanRedo`, `Redo`)
- Track colors (`GetTrackColor`)
- OSD display (`ShowOSD`)
- Focused FX query (`CheckTouchedOrFocusedFX`)
- String utilities (`GetShortFXName`)
- Math utilities (`RoundDouble`, `CompareFaderValues`)
- Automation (`CycleTrackAutoMode`)

**Problem**: This is a God Class made entirely of static methods — essentially a dumping ground for any function that wraps a REAPER API call. There's no cohesion; math utilities sit next to UI functions.

**Impact**: Difficult to test, difficult to find relevant methods, no way to mock for unit testing.

---

## Low-Priority Issues



### 38. **`static` Local Variables in `DAW::ShowOSD()` Create Hidden Global State**
**Severity**: LOW  
**Location**: [control_surface_integrator_Reaper.h](reaper_csurf_integrator/control_surface_integrator_Reaper.h#L264-L275)

**Evidence**:
```cpp
static void ShowOSD(const osd_data osdData)
{
    static string lastValue;       // persists between calls
    static DWORD lastUpdateTs = 0; // persists between calls
    DWORD now = GetTickCount();

    if (lastValue == osdData.lastValue) {
        if (osdData.timeoutMs == -1) return;
        if (osdData.timeoutMs >= 0 && (now - lastUpdateTs) < (DWORD)osdData.timeoutMs) return;
    }
    // ...
}
```

**Problem**: `ShowOSD` is a static member function defined in a header, and it uses function-level `static` variables. Because this function lives in a header, each TU gets its own copy of these static variables, meaning OSD throttling won't work correctly across different source files.

**Impact**: OSD messages may not be throttled correctly, leading to duplicate or flickering OSD updates.

---

### 39. **`osd_data` Passed by Value (Unnecessary Copy)**
**Severity**: LOW  
**Location**: [control_surface_integrator_Reaper.h](reaper_csurf_integrator/control_surface_integrator_Reaper.h#L263) — `DAW::ShowOSD`

**Evidence**:
```cpp
static void ShowOSD(const osd_data osdData)  // pass by value
```

The `osd_data` struct contains `std::string` members (`origValue`, `message`, `bgColor`, `lastValue`) and a `vector<string>` (`bgColors`). Passing by value creates unnecessary deep copies every call.

**Problem**: Should be `const osd_data &osdData` (pass by const reference).

**Impact**: Unnecessary heap allocations on every OSD update call. Minor performance issue but called frequently.

---

### 40. **`configtmp` Buffer in `CSurfIntegrator` — Unused Large Stack Member**
**Severity**: LOW  
**Location**: [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h#L4641)

**Evidence**:
```cpp
class CSurfIntegrator : public IReaperControlSurface {
private:
    char configtmp[1024];  // 1KB member, unclear usage
```

**Problem**: A 1KB `char` array as a class member with no clear purpose visible in the header. May be used in `GetConfigString()` implementation, but storing persistent temp buffers as class members is a code smell.

---

---

### 41. **`IsCommentedOrEmpty()` Doesn't Handle All Edge Cases**
**Severity**: LOW  
**Location**: [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h#L151)

**Evidence**:
```cpp
static bool IsCommentedOrEmpty(string& line) {
    return line == "" || line[0] == '\r' || line[0] == '/' || line[0] == '#';
}
```

**Problem**: 
- Takes `string&` instead of `const string&`
- Treats a line starting with a single `/` as a comment (not just `//`)
- Doesn't handle lines with only whitespace (spaces/tabs)
- A file path like `/usr/local/...` would be skipped as a "comment"

---

### 42. **X Macro Pattern Used Without Documentation**
**Severity**: LOW  
**Location**: [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h#L159-L571)

**Evidence**:
```cpp
#define ACTION_TYPE_LIST(X) \
X(MoveCursor, "MoveEditCursor") \
X(Rewind, "Rewind") \
// ... 120+ entries
```

**Problem**: The X-macro pattern is powerful but obscure. The `ACTION_TYPE_LIST` macro (120+ entries) and `DECLARE_PROPERTY_TYPES` macro are used to generate enums, switch statements, and name-lookup functions. There is no documentation explaining the pattern, making it bewildering for new contributors.


---

### 43. **`ExtractSuffixNumber()` Potential Underflow**
**Severity**: LOW  
**Location**: [handy_functions.h](reaper_csurf_integrator/handy_functions.h#L193-L200)

**Evidence**:
```cpp
static int ExtractSuffixNumber(const std::string& name) {
    int result = -1;
    size_t index = name.length() - 1;
    while (index >= 0 && isdigit(name[index])) index--;
    // ...
}
```

**Problem**: `index` is `size_t` (unsigned). If `name` is entirely digits, `index` wraps to `SIZE_MAX` instead of going negative. The `index >= 0` check is always true for unsigned types.

**Impact**: Infinite loop if the widget name is all digits (unlikely but possible).

---


---

### 44. **Some Action Classes Don't Follow Base Class Hierarchy Consistently**
**Severity**: LOW  
**Location**: [control_surface_Reaper_actions.h](reaper_csurf_integrator/control_surface_Reaper_actions.h)

**Evidence**:
```cpp
class TrackSelect : public Action {};          // Not TrackAction
class TrackUniqueSelect : public Action {};    // Not TrackAction — comment says "TrackAction?"
class TrackRangeSelect : public Action {};     // Not TrackAction — comment says "TrackAction?"
class TrackSendMute : public TrackSendAction {};
class TrackSendInvertPolarity : public Action {};    // Should be TrackSendAction?
class TrackSendStereoMonoToggle : public Action {};  // Should be TrackSendAction?
class TrackReceiveMute : public Action {};           // Should be TrackReceiveAction?
```

**Problem**: Several Send/Receive action classes inherit from `Action` directly instead of `TrackSendAction`/`TrackReceiveAction`. The `// TrackAction?` comments suggest the developer was unsure. This means `IsTrackRelated()`, `IsTrackSendRelated()` etc. return incorrect values for these classes.

**Impact**: Any code filtering actions by category (e.g., for display grouping or validation) will miss these actions.


### 45. **`static` Local Variables in `DAW::ShowOSD()` Create Hidden Global State**
**Severity**: LOW  
**Location**: [control_surface_integrator_Reaper.h](reaper_csurf_integrator/control_surface_integrator_Reaper.h#L264-L275)

**Evidence**:
```cpp
static void ShowOSD(const osd_data osdData)
{
    static string lastValue;       // persists between calls
    static DWORD lastUpdateTs = 0; // persists between calls
    DWORD now = GetTickCount();

    if (lastValue == osdData.lastValue) {
        if (osdData.timeoutMs == -1) return;
        if (osdData.timeoutMs >= 0 && (now - lastUpdateTs) < (DWORD)osdData.timeoutMs) return;
    }
    // ...
}
```

**Problem**: `ShowOSD` is a static member function defined in a header, and it uses function-level `static` variables. Because this function lives in a header, each TU gets its own copy of these static variables, meaning OSD throttling won't work correctly across different source files.

**Impact**: OSD messages may not be throttled correctly, leading to duplicate or flickering OSD updates.

---

### 46. **Missing `const` Correctness Throughout**
**Severity**: LOW  
**Location**: Throughout all files

**Examples**:
```cpp
// Should be const
bool GetIsActive() { return isActive_; }           // Zone
const char *GetName() { return name_.c_str(); }     // Zone, Widget, ControlSurface
bool GetShift() { return modifiers_[Shift].isEngaged; }  // ModifierManager

// Accessor returning by value should be const
int GetChannelNumber() { return channelNumber_; }    // Widget
double GetStepSize() { return stepSize_; }           // Widget
```

**Impact**: Cannot call these methods on const references or pointers. Prevents compiler optimizations. Violates C++ best practices.

---



### 47. **Commented-Out Dead Code Throughout**
**Severity**: MEDIUM  
**Location**: Multiple files

**Evidence**:
```cpp
// control_surface_integrator.h lines 4305-4340 (EnterPage/LeavePage)
void EnterPage()
{
    /*
     if (colorTracks_)
     {
     // capture track colors
     for (auto *navigator : trackNavigators_)
         ...
     }
     */
}

// control_surface_integrator.h lines 4505-4570 — entire commented-out Run() with timing
/*
    void Run()
    {
        int start = std::chrono::duration_cast<std::chrono::microseconds>(...
    }
*/

// control_surface_integrator.h lines 5102-5115 — another commented-out timing block
```

**Problem**: Large blocks of commented-out code (totaling 100+ lines) obscure the actual implementation and make the already-large files even harder to navigate. Version control should be used to preserve old approaches.

---
