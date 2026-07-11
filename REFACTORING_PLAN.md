# CSI Refactoring Plan — Safe, Incremental Decomposition

## Overview

This plan addresses 16 architectural problems from `ARCHITECTURE_PROBLEMS.md` through a series of safe, incremental refactoring phases. Each phase is designed to be buildable and testable independently — no phase leaves the code in a broken state.

**Guiding Principles:**
- Every commit must compile and behave identically to the previous one (purely mechanical refactoring first, behavioral changes later)
- Extract, don't rewrite — move code to new files/classes before restructuring it
- One responsibility per phase — easier to review, revert, and bisect
- Preserve the public API surface of each class initially; deprecate later
- Use generic, descriptive file names — no product-specific prefix (no `csi_`, no `control_surface_`)

**Naming Convention:**
- File names use `snake_case` and describe what the file contains (e.g., `action_context.h`, `zone_manager.h`)
- No product prefix — the directory structure provides namespace context
- Class names remain unchanged to avoid mass-renaming across the codebase (that can be a separate pass)

---

## Current File Structure (for reference)

| File | Lines | Primary Contents |
|------|-------|-----------------|
| `control_surface_integrator.h` | 5,116 | **Everything**: Action, Navigator, ActionContext, Zone, FeedbackProcessor, Widget, ZoneManager, ModifierManager, CSIMessageGenerator, ControlSurface, Midi_ControlSurfaceIO, Midi_ControlSurface, OSC_FeedbackProcessor, OSC_ControlSurfaceIO, OSC_ControlSurface, TrackNavigationManager, Page, CSurfIntegrator |
| `control_surface_integrator.cpp` | ~4,793 | Implementations for all the above + ProcessMidiWidget 245-line if-else chain |
| `control_surface_integrator_ui.cpp` | ~4,016 | Dialog procedures, Learn mode UI |
| `control_surface_Reaper_actions.h` | 3,430 | 50+ concrete Action subclasses (Track, Send, Receive, FX, Transport, Display) |
| `control_surface_action_contexts.h` | ~310 | Action base class hierarchy (NoAction, FXAction, VolumeAction, PanAction, etc.) |
| `control_surface_midi_widgets.h` | 2,881 | 34+ Midi_FeedbackProcessor subclasses + Midi_CSIMessageGenerator subclasses |
| `control_surface_OSC_widgets.h` | ~220 | OSC-specific widget/feedback classes + duplicated `rgbToColor()` |
| `control_surface_manager_actions.h` | 1,160 | Manager-level action classes (GoHome, Bank, Page navigation, etc.) |
| `control_surface_integrator_Reaper.h` | 652 | `DAW` static god-class, `osd_data`, `rgba_color`, `MIDI_event_ex_t` |
| `handy_functions.h` | ~200 | Static utility functions (conversions, logging, string helpers) |

---

## Phase 0: Folder Restructure + Preparatory Infrastructure
**Addresses**: #35 (File sizes), #28 (Duplicated rgbToColor), #37 (DAW god class partial)

### 0.0 — Migrate from `reaper_csurf_integrator/` to `src/`

The root `CMakeLists.txt` already has `#TODO: move and change to src`. This phase fulfills that.

**Steps:**
1. Create the new `src/` directory tree (see target structure below)
2. Move existing files into their new locations using `git mv`
3. Update `CMakeLists.txt`:
   - Change `add_subdirectory(reaper_csurf_integrator)` → `add_subdirectory(src)`
   - Change `set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/reaper_csurf_integrator)` → `set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)`
4. Update `src/CMakeLists.txt` — the existing `file(GLOB_RECURSE ...)` already handles subdirectories
5. Update all `#include` paths to use relative paths from `src/` root
6. Keep `reaper_csurf_integrator/` as an empty directory with a note, or remove it

**Initial placement of existing files:**

| Old location | New location | Rationale |
|---|---|---|
| `main.cpp` | `src/main.cpp` | Entry point |
| `res.rc`, `resource.h` | `src/res.rc`, `src/resource.h` | Resources |
| `res.rc_mac_dlg`, `res.rc_mac_menu` | `src/res.rc_mac_dlg`, `src/res.rc_mac_menu` | Platform resources |
| `reaper_plugin.h` | `src/shared/reaper_plugin.h` | REAPER SDK (shared dependency) |
| `reaper_plugin_functions.h` | `src/shared/reaper_plugin_functions.h` | REAPER SDK (shared dependency) |
| `handy_functions.h` | `src/shared/handy_functions.h` → later becomes `src/shared/utils.h` | Utilities |
| `control_surface_integrator_Reaper.h` | `src/shared/daw_api.h` | DAW abstraction layer |
| `oscpkt.hh` | `src/shared/oscpkt.hh` | Third-party OSC library |
| `udp.hh` | `src/shared/udp.hh` | Third-party UDP library |
| `control_surface_integrator.h` | `src/controls/integrator.h` → split in Phase 1 | Main monolith |
| `control_surface_integrator.cpp` | `src/controls/integrator.cpp` → split in Phase 1 | Main implementation |
| `control_surface_integrator_ui.cpp` | `src/ui/integrator_ui.cpp` | UI dialogs |
| `control_surface_action_contexts.h` | `src/actions/action_base.h` | Action base classes |
| `control_surface_Reaper_actions.h` | `src/actions/reaper_actions.h` → split in Phase 2 | Concrete actions |
| `control_surface_manager_actions.h` | `src/actions/manager_actions.h` | Manager actions |
| `control_surface_midi_widgets.h` | `src/controls/midi/midi_widgets.h` → split in Phase 4 | MIDI widgets |
| `control_surface_OSC_widgets.h` | `src/controls/osc/osc_widgets.h` | OSC widgets |

This is purely a file-move operation. All `#include` directives update to relative paths. The `GLOB_RECURSE` in CMake automatically picks up files in subdirectories.

### 0.1 — Create shared `types.h`
Extract standalone types that have no dependencies on major classes:

**Move from `daw_api.h` (formerly `control_surface_integrator_Reaper.h`):**
- `struct osd_data`
- `struct rgba_color`
- `struct MIDI_event_ex_t`
- Constants: `MEDBUF`, `SMLBUF`

**Move from `integrator.h` (formerly `control_surface_integrator.h`):**
- `class PropertyList` (lines 233–358)
- `enum PropertyType` and its X-macro `DECLARE_PROPERTY_TYPES`
- `class ReloadPluginException`
- `enum class ActionType` and the `ACTION_TYPE_LIST` X-macro
- `enum class NavigatorType` and `NAVIGATOR_TYPE_LIST`
- All `static const` string/int constants (s_CSIName, REAPER__* IDs, s_BadFileChars, etc.)

**New file:** `src/shared/types.h`

This file becomes the single "forward declarations + value types" header that everything includes.

### 0.2 — Create shared `utils.h` / `utils.cpp`
**Addresses**: #28 (Duplicated rgbToColor), #37 (DAW partial)

Extract ALL static utility functions that are currently duplicated or scattered:

**Move from `handy_functions.h` (keep that file as a thin include-forwarder initially):**
- `int14ToNormalized`, `normalizedToVol`, `volToNormalized`, `panToNormalized`, `normalizedToPan`
- `IsSameString` (all overloads)
- `LogToConsole`, `LogMessage`
- `GetRelativePath`, `ExtractSuffixNumber`
- `format_number`, `IsCommentedOrEmpty`

**Move from `osc_widgets.h` AND `midi_widgets.h`:**
- `rgbToColor()` — single implementation, remove the "mirror this" comments

**Move from `daw_api.h`:**
- `ExplodeString()`
- `GetColorValue()`

**Move from `integrator.h`:**
- `format_number()`
- `IsCommentedOrEmpty()`

**New files:** `src/shared/utils.h` + `src/shared/utils.cpp`

Make functions non-static (or `inline` in header) to avoid per-TU duplication. This also fixes issue #31 (static functions in headers creating bloat).

### 0.3 — Create `sysex_builder.h`
**Addresses**: #29 (Massive SysEx Construction Duplication)

Extract the repeated SysEx message construction pattern into a reusable builder:

```cpp
// src/shared/sysex_builder.h
class SysExBuilder {
    struct {
        MIDI_event_ex_t evt;
        char data[256];
    } midiSysExData_;
public:
    SysExBuilder();
    SysExBuilder& begin();  // resets, adds 0xF0
    SysExBuilder& add(unsigned char byte);
    SysExBuilder& addBytes(const unsigned char *bytes, int count);
    SysExBuilder& addText(const char *text, int maxLen = -1);
    SysExBuilder& end();    // adds 0xF7
    MIDI_event_ex_t* message();
    int size() const;
};
```

All 20+ SysEx construction sites in `midi_widgets.h` would use this builder instead of raw struct manipulation. This is a mechanical replacement — behavior stays identical.

---

## Phase 1: File Decomposition of the Monolith (Mechanical extraction)
**Addresses**: #35 (Massive file sizes), #2 (Circular dependencies — makes them explicit)

Split the 5,116-line monolith (`integrator.h`) into focused header files within the `src/` tree. **No logic changes** — purely moving code to new files and adding `#include` directives.

### 1.1 — Extract `src/actions/action.h`
**Contents:**
- `class Action` (base class, lines 576–651)
- Keep `ActionType` enum in `src/shared/types.h` (from Phase 0)

### 1.2 — Extract `src/controls/navigator.h`
**Contents:**
- `class Navigator` (base, lines 654–720)
- `class TrackNavigator` (lines 722–742)
- `class FixedTrackNavigator` (lines 744–758)
- `class MasterTrackNavigator` (lines 760–771)
- `class SelectedTrackNavigator` (lines 773–784)
- `class FocusedFXNavigator` (lines 786–797)

### 1.3 — Extract `src/actions/action_context.h`
**Contents:**
- `class ActionContext` (lines 799–1202)

### 1.4 — Extract `src/controls/zone.h`
**Contents:**
- `class Zone` (lines 1205–1328)
- `class SubZone` (lines 1330–1345)
- `struct CSIZoneInfo` (lines 1543–1554)

### 1.5 — Extract `src/controls/feedback.h`
**Contents:**
- `class FeedbackProcessor` (base, lines 1348–1397)
- `class Midi_FeedbackProcessor` (lines 1399–1429)

### 1.6 — Extract `src/controls/widget.h`
**Contents:**
- `class Widget` (lines 1431–1541)

### 1.7 — Extract `src/controls/zone_manager.h`
**Contents:**
- `class ZoneManager` (lines 1556–2258)

### 1.8 — Extract `src/controls/message_generator.h`
**Contents:**
- `class CSIMessageGenerator` (lines 2261–2277)
- `class AnyPress_CSIMessageGenerator` (lines 2280–2292)
- `class Touch_CSIMessageGenerator` (lines 2294–2306)
- `class Midi_CSIMessageGenerator` (lines 2308–2317)

### 1.9 — Extract `src/controls/modifier_manager.h`
**Contents:**
- `class ModifierManager` (lines 2319–2590)

### 1.10 — Extract `src/controls/control_surface.h`
**Contents:**
- `struct ChannelTouch`, `struct ChannelToggle`
- `class ControlSurface` (lines 2612–3090)

### 1.11 — Extract `src/controls/midi/midi_surface.h`
**Contents:**
- `class Midi_ControlSurfaceIO` (lines 3092–3195)
- `class Midi_ControlSurface` (lines 3197–3272)

### 1.12 — Extract `src/controls/osc/osc_surface.h`
**Contents:**
- `class OSC_FeedbackProcessor` (lines 3275–3293)
- `class OSC_IntFeedbackProcessor` (lines 3295–3307)
- `class OSC_ControlSurfaceIO` (lines 3309–3475)
- `class OSC_X32ControlSurfaceIO` (lines 3477–3503)
- `class OSC_ControlSurface` (lines 3505–3547)

### 1.13 — Extract `src/controls/track_nav_manager.h`
**Contents:**
- `enum class TrackVCAFolderMode`
- `class TrackNavigationManager` (lines 3549–4395)

### 1.14 — Extract `src/controls/page.h`
**Contents:**
- `class Page` (lines 4397–4667)

### 1.15 — Keep `src/controls/integrator.h`
**Contents:**
- `class CSurfIntegrator` (lines 4669–5102)
- All `#include` directives for the above extracted headers
- Create compatibility shim: `reaper_csurf_integrator/control_surface_integrator.h` → `#include "../src/controls/integrator.h"` (if anyone includes the old path)

### 1.16 — Split `control_surface_integrator.cpp`
Correspondingly split the .cpp implementations into separate .cpp files matching the headers above:
- `src/actions/action_context.cpp` — ActionContext method implementations
- `src/controls/zone.cpp` — Zone/SubZone methods
- `src/controls/zone_manager.cpp` — ZoneManager methods (including zone loading/parsing)
- `src/controls/control_surface.cpp` — ControlSurface methods
- `src/controls/midi/midi_surface.cpp` — Midi_ControlSurface methods + `ProcessMidiWidget`
- `src/controls/osc/osc_surface.cpp` — OSC_ControlSurface methods
- `src/controls/track_nav_manager.cpp` — TrackNavigationManager methods
- `src/controls/page.cpp` — Page methods
- `src/controls/integrator.cpp` — CSurfIntegrator methods (Init, Run, action dictionary)

The existing `src/CMakeLists.txt` uses `file(GLOB_RECURSE sources CONFIGURE_DEPENDS ./*.c* ./*.h*)` so new files in subdirectories are automatically found. No manual source list update needed.

### Dependency Graph After Phase 1
```
shared/types.h ← shared/utils.h
      ↑                ↑
actions/action.h   controls/feedback.h
      ↑                ↑
controls/navigator.h   controls/widget.h
      ↑                ↑
actions/action_context.h
      ↑
controls/zone.h → controls/zone_manager.h
      ↑                   ↑
controls/message_generator.h  controls/modifier_manager.h
      ↑                   ↑
controls/control_surface.h
      ↑              ↑
controls/midi/       controls/osc/
midi_surface.h       osc_surface.h
      ↑
controls/track_nav_manager.h
      ↑
controls/page.h
      ↑
controls/integrator.h
```

Some forward declarations will be needed to break circular includes (e.g., `ControlSurface` in `navigator.h` needs forward-declared, not included).

---

## Phase 2: Reduce Action Class Duplication
**Addresses**: #5 (Action class explosion), #26 (Send/Receive duplication)

### 2.1 — Parameterize Send vs Receive Actions
The ~22 pairs of near-identical Send/Receive classes differ only in the slot index calculation:
- **Send**: `context->GetSlotIndex() + numHardwareSends`  
- **Receive**: `-(context->GetSlotIndex() + 1)`

Create a single parameterized template:

```cpp
enum class SendDirection { Send, Receive };

// Helper to compute the REAPER send index
inline int GetSendIndex(ActionContext *context, MediaTrack *track, SendDirection dir) {
    if (dir == SendDirection::Send) {
        int numHardwareSends = GetTrackNumSends(track, 1);
        return context->GetSlotIndex() + numHardwareSends;
    } else {
        return -(context->GetSlotIndex() + 1);
    }
}

// One class instead of TrackSendVolume + TrackReceiveVolume
template<SendDirection Dir>
class TrackSendReceiveVolume : public VolumeAction {
public:
    ActionType GetType() const override {
        return Dir == SendDirection::Send ? ActionType::TrackSendVolume : ActionType::TrackReceiveVolume;
    }
    double GetCurrentNormalizedValue(ActionContext *context) override { /* unified impl */ }
    void RequestUpdate(ActionContext *context) override { /* unified impl */ }
    void Do(ActionContext *context, double value) override { /* unified impl */ }
    void Touch(ActionContext *context, double value) override { /* unified impl */ }
};

using TrackSendVolume = TrackSendReceiveVolume<SendDirection::Send>;
using TrackReceiveVolume = TrackSendReceiveVolume<SendDirection::Receive>;
```

**Classes to unify (each pair → one template):**
1. `TrackSendVolume` / `TrackReceiveVolume`
2. `TrackSendVolumeDB` / `TrackReceiveVolumeDB`
3. `TrackSendPan` / `TrackReceivePan`
4. `TrackSendPanPercent` / `TrackReceivePanPercent`
5. `TrackSendMute` / `TrackReceiveMute`
6. `TrackSendInvertPolarity` / `TrackReceiveInvertPolarity`
7. `TrackSendStereoMonoToggle` / `TrackReceiveStereoMonoToggle`
8. `TrackSendPrePost` / `TrackReceivePrePost`
9. `TrackSendNameDisplay` / `TrackReceiveNameDisplay`
10. `TrackSendVolumeDisplay` / `TrackReceiveVolumeDisplay`
11. `TrackSendPanDisplay` / `TrackReceivePanDisplay`
12. `TrackSendStereoMonoDisplay` / `TrackReceiveStereoMonoDisplay`
13. `TrackSendPrePostDisplay` / `TrackReceivePrePostDisplay`

**Estimated code reduction**: ~1000 lines eliminated.

### 2.2 — Split `reaper_actions.h` Into Logical Groups

After deduplication, split into focused files under `src/actions/`:
- `actions_track.h` — Track volume, pan, mute, solo, select, arm, polarity
- `actions_send_receive.h` — Unified send/receive templates
- `actions_fx.h` — FX param, bypass, offline, gain reduction, TCP FX
- `actions_transport.h` — Play, stop, record, rewind, FF, cursor, timeline
- `actions_display.h` — All *Display classes 
- `actions_navigation.h` — Bank, GoHome, GoZone, GoPage, etc.

### 2.3 — Fix Inconsistent Inheritance
**Addresses**: #5 (item 44 from problems doc)

```cpp
// Before (wrong base class):
class TrackSendInvertPolarity : public Action {};
class TrackSendStereoMonoToggle : public Action {};
class TrackReceiveMute : public Action {};

// After (correct base class):
class TrackSendInvertPolarity : public TrackSendAction {};
class TrackSendStereoMonoToggle : public TrackSendAction {};
class TrackReceiveMute : public TrackReceiveAction {};
```

---

## Phase 3: Widget Type Factory Pattern
**Addresses**: #36 (Monolithic if-else-if dispatch), #16 (Limited extensibility)

### 3.1 — Create Widget Type Registry

Replace the 245-line `ProcessMidiWidget()` if-else-if chain with a registration-based factory:

```cpp
// src/controls/widget_factory.h
using MidiMessageGeneratorFactory = std::function<
    CSIMessageGenerator*(CSurfIntegrator*, Widget*, Midi_ControlSurface*, 
                         const vector<string>&)>;

using MidiFeedbackProcessorFactory = std::function<
    FeedbackProcessor*(CSurfIntegrator*, Midi_ControlSurface*, Widget*,
                       MIDI_event_ex_t, MIDI_event_ex_t)>;

class MidiWidgetRegistry {
    static std::map<std::string, MidiMessageGeneratorFactory>& GetGenerators();
    static std::map<std::string, MidiFeedbackProcessorFactory>& GetFeedbackProcessors();
public:
    static void RegisterGenerator(const std::string& type, MidiMessageGeneratorFactory factory);
    static void RegisterFeedbackProcessor(const std::string& type, MidiFeedbackProcessorFactory factory);
    static CSIMessageGenerator* CreateGenerator(const std::string& type, ...);
    static FeedbackProcessor* CreateFeedbackProcessor(const std::string& type, ...);
};
```

### 3.2 — Self-Registering Widget Types

Each widget type registers itself in its own header file:

```cpp
// In midi_widgets.h or a device-specific file:
static bool registered_Press = MidiWidgetRegistry::RegisterGenerator("Press", 
    [](CSurfIntegrator *csi, Widget *w, Midi_ControlSurface *s, const vector<string> &tokens) {
        return new PressRelease_Midi_CSIMessageGenerator(csi, w, ...);
    });
```

### 3.3 — Refactor `ProcessMidiWidget()`

The 245-line method becomes ~20 lines:

```cpp
void Midi_ControlSurface::ProcessMidiWidget(...) {
    Widget *widget = GetWidgetByName(widgetName);
    if (!widget) { LogError(...); return; }
    
    auto *generator = MidiWidgetRegistry::CreateGenerator(widgetType, csi_, widget, this, tokens);
    if (generator) {
        CSIMessageGeneratorsByMessage_.insert(...);
        return;
    }
    
    auto *feedback = MidiWidgetRegistry::CreateFeedbackProcessor(widgetType, csi_, this, widget, tokens);
    if (feedback) {
        widget->GetFeedbackProcessors().push_back(unique_ptr<FeedbackProcessor>(feedback));
        return;
    }
    
    LogError("Unknown widget type: %s", widgetType.c_str());
}
```

---

## Phase 4: Decompose Feedback Processor Hierarchy  
**Addresses**: #29 (SysEx duplication), #6 (Tight coupling Widget ↔ Feedback), #35 (midi_widgets.h size)

### 4.1 — Split `midi_widgets.h` Into Device Files

Organize the 34 feedback processor classes by device family under `src/controls/midi/`:

| File | Contents |
|------|----------|
| `fb_generic.h` | TwoState, Fader14Bit, Fader7Bit, Encoder (shared base processors) |
| `fb_mcu.h` | MCUDisplay, MCUVUMeter, MCUTimeDisplay, MCUAssignmentDisplay |
| `fb_xtouch.h` | XTouchDisplay, XTouchScribble |
| `fb_faderport.h` | FPDisplay, FPVUMeter, FPTwoStateRGB, FaderportRGB, FaderportClassicFader |
| `fb_sce24.h` | SCE24TwoStateLED, SCE24OLED, SCE24Text, SCE24Encoder |
| `fb_asparion.h` | AsparionDisplay, AsparionRGB, AsparionVUMeter, AsparionEncoder |
| `fb_novation.h` | NovationLaunchpadMiniRGB7Bit |
| `fb_qcon.h` | QConLiteDisplay, QConProXMasterVUMeter |
| `fb_icon.h` | IconDisplay |
| `midi_generators.h` | All Midi_CSIMessageGenerator subclasses |

### 4.2 — Extract Shared SysEx Patterns Using `SysExBuilder`

Using the builder from Phase 0.3, refactor all classes to use it. For device families with very similar code, extract shared base classes:

```cpp
// Shared base for MCU-protocol displays (MCU, Icon, QConLite share 90% logic)
class MCUProtocolDisplay_Midi_FeedbackProcessor : public Midi_FeedbackProcessor {
protected:
    int displayRow_;
    int displayOffset_;
    int displayLength_;
    virtual void GetSysExHeader(SysExBuilder &builder) = 0;
public:
    void ForceValue(const PropertyList &properties, const char * const &value) override {
        SysExBuilder builder;
        GetSysExHeader(builder);
        builder.add(displayOffset_);
        builder.addText(value, displayLength_);
        builder.end();
        SendMidiSysExMessage(builder.message());
    }
};
```

---

## Phase 5: Decompose `Page` and Reduce Circular Dependencies
**Addresses**: #3 (Massive Page class), #2 (Bidirectional circular dependencies), #12 (No DI)

### 5.1 — Remove TrackNavigationManager Facade From Page

The `Page` class has ~30 one-liner methods that just forward to `TrackNavigationManager`. Instead of calling `page->GetScrollLink()`, callers should call `page->GetTrackNavigationManager()->GetScrollLink()`.

**Step 1**: Add `TrackNavigationManager* GetTrackNavigationManager()` to `Page`.
**Step 2**: Update all callers (in actions, zones, surfaces) to use the direct accessor.
**Step 3**: Remove the ~30 facade methods from `Page`.

**Estimated reduction**: ~40 methods removed from `Page`.

### 5.2 — Extract Transport State Management

Move `SignalStop()`, `SignalPlay()`, `SignalRecord()` and related transport coordination out of `Page` into a focused helper, or inline them (they're just surface iteration loops).

### 5.3 — Break Circular Page ↔ ControlSurface Dependency

Currently:
- `Page` owns `vector<unique_ptr<ControlSurface>>`
- `ControlSurface` holds `Page *const page_`
- `Navigator` holds `Page *const page_`

**Strategy**: Introduce a `PageInterface` (pure abstract) that exposes only what `ControlSurface`/`Navigator` actually need:

```cpp
// src/controls/page_interface.h
class IPageContext {
public:
    virtual ~IPageContext() {}
    virtual TrackNavigationManager* GetTrackNavigationManager() = 0;
    virtual ModifierManager* GetModifierManager() = 0;
    virtual const char* GetName() = 0;
    // Only methods actually called by ControlSurface/Navigator
};
```

`ControlSurface` and `Navigator` hold `IPageContext*` instead of `Page*`. `Page` implements `IPageContext`. This breaks the header-level circular dependency.

### 5.4 — Break Circular Zone ↔ ZoneManager ↔ ControlSurface

Currently:
- `Zone` holds `ZoneManager*`
- `ZoneManager` holds `ControlSurface*`
- `ControlSurface` holds `unique_ptr<ZoneManager>`

**Strategy**: `Zone` should not need to reach through `ZoneManager` to `ControlSurface`. Audit all `zone->GetZoneManager()->GetSurface()->...` call chains and inject the needed object directly, or provide the needed method on `ZoneManager` itself.

---

## Phase 6: Decompose ActionContext
**Addresses**: #11 (ActionContext Kitchen Sink)

### 6.1 — Extract Timing/Hold/Repeat Logic

```cpp
// src/actions/action_timing.h
class ActionTiming {
    int holdDelayMs_ = 0;
    int holdRepeatIntervalMs_ = 0;
    DWORD lastHoldRepeatTs_ = 0;
    DWORD lastHoldStartTs_ = 0;
    bool holdActive_ = false;
    bool holdRepeatActive_ = false;
    double deferredValue_ = 0.0;
    bool isDoublePress_ = false;
    DWORD doublePressStartTs_ = 0;
public:
    // timing-related methods extracted from ActionContext
};
```

### 6.2 — Extract Blink State

```cpp
// src/controls/blink_state.h
class BlinkState {
    bool blinkSet_ = false;
    bool blinkActive_ = true;
    int blinkIntervalMs_ = 0;
    DWORD lastBlinkTs_ = 0;
public:
    bool UpdateBlinkState();
    void SetBlinkInterval(int value);
    int GetBlinkInterval();
};
```

### 6.3 — Extract Color Management

```cpp
// src/actions/action_color.h
class ActionColorState {
    bool supportsColor_ = false;
    bool supportsTrackColor_ = false;
    vector<rgba_color> colorValues_;
    int currentColorIndex_ = 0;
public:
    void SetColor(const vector<string> &params, ...);
    void UpdateTrackColor();
    void UpdateColorValue(double value);
};
```

### 6.4 — Extract Acceleration/Stepping Logic

```cpp
// src/actions/action_value.h
class ActionValueProcessor {
    double deltaValue_ = 0.0;
    double rangeMinimum_ = 0.0;
    double rangeMaximum_ = 1.0;
    vector<double> steppedValues_;
    int steppedValuesIndex_ = 0;
    vector<double> acceleratedDeltaValues_;
    vector<int> acceleratedTickValues_;
    int accumulatedIncTicks_ = 0;
    int accumulatedDecTicks_ = 0;
    bool isValueInverted_ = false;
    bool isFeedbackInverted_ = false;
public:
    void DoRangeBoundAction(double value);
    void DoSteppedValueAction(double value);
    void DoAcceleratedSteppedValueAction(int accelerationIndex, double value);
    void DoAcceleratedDeltaValueAction(int accelerationIndex, double value);
};
```

### 6.5 — Compose ActionContext From Components

```cpp
class ActionContext {
    // Core binding (stays)
    CSurfIntegrator *const csi_;
    Action *action_;
    Widget *const widget_;
    Zone *const zone_;
    
    // Extracted components
    ActionTiming timing_;
    BlinkState blink_;
    ActionColorState color_;
    ActionValueProcessor valueProcessor_;
    osd_data osdData_;
    PropertyList widgetProperties_;
    
    // FX context tracking (could also be extracted)
    MediaTrack* track_ = nullptr;
    int fxSlotNum_ = -1;
    int fxParamNum_ = -1;
    // ...
};
```

---

## Phase 7: Decompose DAW Static Class
**Addresses**: #37 (DAW god class)

### 7.1 — Split by Responsibility

Split `src/shared/daw_api.h` into focused files:

| File | Contents |
|------|----------|
| `src/shared/daw_api.h` | Central include — pulls in all sub-headers |
| `src/shared/daw_tracks.h` | `DAW::` track volume, pan, solo, mute, arm, color |
| `src/shared/daw_fx.h` | `DAW::` FX name, instrument check, param queries |
| `src/shared/daw_transport.h` | `DAW::` SendCommandMessage, Undo/Redo |
| `src/shared/daw_display.h/.cpp` | `DAW::ShowOSD`, `DAW::GetShortFXName` |
| `src/shared/daw_utils.h` | `DAW::RoundDouble`, `DAW::CompareFaderValues` |

Keep `DAW::` as a namespace instead of a class (since all methods are static):

```cpp
namespace DAW {
    namespace Tracks { ... }
    namespace FX { ... }  
    namespace Transport { ... }
    namespace Display { ... }
}
```

Or keep the flat `DAW::` for compatibility but organize methods into separate files.

### 7.2 — Fix `ShowOSD()` Static Variable Per-TU Bug

Move `ShowOSD()` implementation to `src/shared/daw_display.cpp` so the `static string lastValue` and `static DWORD lastUpdateTs` are truly global singletons, not per-translation-unit copies.

---

## Phase 8: Navigator Simplification
**Addresses**: #9 (Navigator complexity, repetitive track navigation logic)

### 8.1 — Navigator Strategy Pattern

Currently 5 navigator subclasses with very similar structures. The key difference is just how `GetTrack()` resolves:

```cpp
class Navigator {
public:
    using TrackResolver = std::function<MediaTrack*(const Navigator&)>;
private:
    TrackResolver resolver_;
public:
    Navigator(CSurfIntegrator *csi, Page *page, TrackResolver resolver)
        : csi_(csi), page_(page), resolver_(std::move(resolver)) {}
    
    MediaTrack *GetTrack() override { return resolver_(*this); }
};

// Factory functions replace subclasses:
Navigator* CreateTrackNavigator(CSurfIntegrator *csi, Page *page, 
                                 TrackNavigationManager *tnm, int channelNum);
Navigator* CreateMasterTrackNavigator(CSurfIntegrator *csi, Page *page);
Navigator* CreateSelectedTrackNavigator(CSurfIntegrator *csi, Page *page);
Navigator* CreateFocusedFXNavigator(CSurfIntegrator *csi, Page *page);
Navigator* CreateFixedTrackNavigator(CSurfIntegrator *csi, Page *page, MediaTrack *track);
```

**Alternative (less invasive)**: Keep the subclasses but extract the shared touch state (isVolumeTouched_, isPanTouched_, etc.) into a `TouchState` struct shared by composition.

---

## Phase 9: Improve Separation of Concerns
**Addresses**: #15 (Poor separation), #12 (No DI)

### 9.1 — Extract Zone File Parser

Move zone file parsing logic from `ZoneManager::LoadZoneFile()` into a dedicated parser class:

```cpp
// src/controls/zone_parser.h
class ZoneFileParser {
public:
    struct ParsedZone {
        string name;
        string alias;
        vector<ParsedAction> actions;
        vector<string> subZones;
    };
    
    static ParsedZone Parse(const string &filePath);
    static vector<string> Validate(const ParsedZone &zone, ControlSurface *surface);
};
```

This separates parsing from zone activation/lifecycle management.

### 9.2 — Extract Surface Template Parser

Move `ProcessMidiWidget()`, `ProcessOSCWidget()`, and surface template file parsing into:

```cpp
// src/controls/surface_parser.h
class SurfaceTemplateParser {
public:
    static void ParseMidiTemplate(const string &filePath, Midi_ControlSurface *surface);
    static void ParseOSCTemplate(const string &filePath, OSC_ControlSurface *surface);
};
```

### 9.3 — Constructor Injection Preparation

Where classes currently reach through multiple layers (e.g., `zone->GetZoneManager()->GetSurface()->GetPage()->GetTrackNavigationManager()`), pass needed dependencies via constructor or method parameters instead. This doesn't require a DI container — just explicit parameter passing.

---

## Execution Order & Risk Assessment

| Phase | Risk | LOC Changed | Dependencies | Description |
|-------|------|-------------|-------------|-------------|
| **0.0** | Very Low | ~0 (move) | None | Migrate `reaper_csurf_integrator/` → `src/` tree |
| **0.1–0.3** | Very Low | ~400 | Phase 0.0 | Infrastructure: shared types, utils, SysEx builder |
| **1** | Low | ~200 (move) | Phase 0 | File decomposition (mechanical) |
| **2** | Medium | ~1200 | Phase 1 | Action deduplication (Send/Receive templates) |
| **3** | Medium | ~300 | Phase 1 | Widget factory pattern |
| **4** | Low-Medium | ~200 (move) | Phase 0.3, 1 | Feedback processor file split + SysEx refactor |
| **5** | Medium | ~500 | Phase 1 | Page decomposition + circular dependency break |
| **6** | Medium | ~600 | Phase 1 | ActionContext decomposition |
| **7** | Low | ~200 | Phase 0 | DAW class split |
| **8** | Medium | ~200 | Phase 5 | Navigator simplification |
| **9** | Low-Medium | ~400 | Phase 1, 5 | Parser extraction + DI prep |

---

## Testing Strategy

Since CSI is a REAPER plugin without a unit test framework:

1. **Compile-test each phase** — every phase must produce a buildable plugin
2. **Binary diff test** — compare behavior before/after by:
   - Loading a complex CSI configuration with multiple surfaces
   - Verifying all zones load without new errors in CSI.log
   - Checking MIDI/OSC I/O still works
3. **Incremental commits** — each sub-step (e.g., 0.0, 1.1, 1.2) is a separate commit so `git bisect` can find any regression
4. **Compatibility shims** during transition — old include paths forward to new locations

---

## Target File Structure (After All Phases)

```
src/
├── CMakeLists.txt
├── main.cpp
├── res.rc
├── res.rc_mac_dlg
├── res.rc_mac_menu
├── resource.h
│
├── shared/                              # Framework-agnostic utilities and types
│   ├── types.h                          # Value types, enums, constants (osd_data, rgba_color, PropertyList, ActionType, etc.)
│   ├── utils.h                          # Shared utilities: rgbToColor, conversions, logging, string helpers
│   ├── utils.cpp                        #   (implementations — non-static to avoid per-TU bloat)
│   ├── sysex_builder.h                  # SysEx message construction helper
│   ├── daw_api.h                        # DAW namespace — central include for all DAW wrappers
│   ├── daw_tracks.h                     # DAW:: track volume/pan/solo/mute/arm/color
│   ├── daw_fx.h                         # DAW:: FX name, instrument, param queries
│   ├── daw_transport.h                  # DAW:: SendCommandMessage, Undo/Redo
│   ├── daw_display.h                    # DAW:: ShowOSD, GetShortFXName (declaration)
│   ├── daw_display.cpp                  #   (implementation — fixes per-TU static variable bug)
│   ├── daw_utils.h                      # DAW:: RoundDouble, CompareFaderValues
│   ├── reaper_plugin.h                  # REAPER SDK header (existing, unchanged)
│   ├── reaper_plugin_functions.h        # REAPER API function pointers (existing, unchanged)
│   ├── oscpkt.hh                        # Third-party OSC packet library (existing, unchanged)
│   └── udp.hh                           # Third-party UDP socket library (existing, unchanged)
│
├── actions/                             # Action hierarchy and context
│   ├── action.h                         # Action base class
│   ├── action_context.h                 # ActionContext (composed from components below)
│   ├── action_context.cpp               #   (implementations)
│   ├── action_base.h                    # Action category classes: NoAction, FXAction, VolumeAction, PanAction, etc.
│   ├── action_timing.h                  # Hold/repeat/double-press timing component
│   ├── action_value.h                   # Range/step/acceleration value processing component
│   ├── action_color.h                   # Color state management component
│   ├── actions_track.h                  # Track volume, pan, mute, solo, select, arm, polarity
│   ├── actions_send_receive.h           # Unified Send/Receive templates (eliminates 22 duplicate pairs)
│   ├── actions_fx.h                     # FX param, bypass, offline, gain reduction, TCP FX
│   ├── actions_transport.h              # Play, stop, record, rewind, FF, cursor, timeline
│   ├── actions_display.h               # All *Display action classes
│   ├── actions_navigation.h             # Bank, GoHome, GoZone, GoPage, GoSubZone
│   └── actions_manager.h               # Manager-level actions (SaveProject, Undo, Redo, toggles, etc.)
│
├── controls/                            # Core control surface framework
│   ├── control_surface.h               # ControlSurface base class
│   ├── control_surface.cpp             #   (implementations)
│   ├── page.h                           # Page class (implements IPageContext)
│   ├── page.cpp                         #   (implementations)
│   ├── page_interface.h                 # IPageContext abstract interface (breaks circular deps)
│   ├── zone.h                           # Zone, SubZone, CSIZoneInfo
│   ├── zone.cpp                         #   (implementations)
│   ├── zone_manager.h                   # ZoneManager
│   ├── zone_manager.cpp                 #   (implementations)
│   ├── zone_parser.h                    # Zone file parser (separated from ZoneManager)
│   ├── zone_parser.cpp                  #   (implementations)
│   ├── widget.h                         # Widget
│   ├── widget.cpp                       #   (implementations)
│   ├── widget_factory.h                 # MidiWidgetRegistry — widget type registration + factory
│   ├── widget_factory.cpp               #   (implementations)
│   ├── feedback.h                       # FeedbackProcessor base class
│   ├── navigator.h                      # Navigator hierarchy (base + 5 subclasses)
│   ├── navigator.cpp                    #   (implementations)
│   ├── modifier_manager.h              # ModifierManager
│   ├── message_generator.h             # CSIMessageGenerator base + AnyPress, Touch
│   ├── blink_state.h                    # Blink logic component
│   ├── track_nav_manager.h             # TrackNavigationManager
│   ├── track_nav_manager.cpp           #   (implementations)
│   ├── integrator.h                     # CSurfIntegrator (top-level plugin class)
│   ├── integrator.cpp                   #   (implementations)
│   ├── surface_parser.h                 # Surface template file parser
│   ├── surface_parser.cpp               #   (implementations)
│   │
│   ├── midi/                            # MIDI-specific surface implementations
│   │   ├── midi_surface.h              # Midi_ControlSurfaceIO + Midi_ControlSurface
│   │   ├── midi_surface.cpp            #   (implementations)
│   │   ├── midi_generators.h           # All Midi_CSIMessageGenerator subclasses
│   │   ├── fb_generic.h                # TwoState, Fader14Bit, Fader7Bit, Encoder
│   │   ├── fb_mcu.h                    # MCU protocol displays + VU meters
│   │   ├── fb_xtouch.h                 # X-Touch displays + scribble strips
│   │   ├── fb_faderport.h              # Faderport family (display, VU, RGB, fader)
│   │   ├── fb_sce24.h                  # SCE24 family (LED, OLED, text, encoder)
│   │   ├── fb_asparion.h               # Asparion family (display, RGB, VU, encoder)
│   │   ├── fb_novation.h               # Novation Launchpad Mini RGB
│   │   ├── fb_qcon.h                   # QCon Lite display + Pro X master VU
│   │   └── fb_icon.h                   # Icon display
│   │
│   └── osc/                             # OSC-specific surface implementations
│       ├── osc_surface.h               # OSC_ControlSurfaceIO + OSC_ControlSurface
│       ├── osc_surface.cpp             #   (implementations)
│       ├── osc_feedback.h              # OSC_FeedbackProcessor + OSC_IntFeedbackProcessor
│       └── osc_x32.h                   # X32-specific OSC widgets + heartbeat
│
├── ui/                                  # User interface (dialogs, Learn mode)
│   └── integrator_ui.cpp               # Dialog procedures — kept as-is initially
│
├── i18n/                                # Internationalization (future)
│   └── .gitkeep                         # Placeholder for localization resources
│
└── assets/                              # Static assets (future)
    └── .gitkeep                         # Placeholder for embedded resources
```

### CMakeLists.txt Changes

**Root `CMakeLists.txt`:**
```cmake
# Change:
#   add_subdirectory(reaper_csurf_integrator) #TODO: move and change to src
# To:
add_subdirectory(src)

# Change:
#   set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/reaper_csurf_integrator)
# To:
set(SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src)
```

**`src/CMakeLists.txt`** — the existing `file(GLOB_RECURSE sources CONFIGURE_DEPENDS ./*.c* ./*.h*)` automatically picks up all files in the new subdirectory tree. No changes needed beyond moving the file.

### Old → New Path Mapping (Quick Reference)

| Old Path (`reaper_csurf_integrator/`) | New Path (`src/`) |
|---|---|
| `main.cpp` | `main.cpp` |
| `res.rc` / `resource.h` | `res.rc` / `resource.h` |
| `reaper_plugin.h` | `shared/reaper_plugin.h` |
| `reaper_plugin_functions.h` | `shared/reaper_plugin_functions.h` |
| `oscpkt.hh` / `udp.hh` | `shared/oscpkt.hh` / `shared/udp.hh` |
| `handy_functions.h` | `shared/utils.h` (contents merged) |
| `control_surface_integrator_Reaper.h` | `shared/daw_api.h` (then split to `daw_*.h`) |
| `control_surface_integrator.h` | Split into `controls/*.h` + `actions/*.h` + `shared/types.h` |
| `control_surface_integrator.cpp` | Split into `controls/*.cpp` + `actions/*.cpp` |
| `control_surface_integrator_ui.cpp` | `ui/integrator_ui.cpp` |
| `control_surface_action_contexts.h` | `actions/action_base.h` |
| `control_surface_Reaper_actions.h` | Split into `actions/actions_*.h` |
| `control_surface_manager_actions.h` | `actions/actions_manager.h` |
| `control_surface_midi_widgets.h` | Split into `controls/midi/fb_*.h` + `controls/midi/midi_generators.h` |
| `control_surface_OSC_widgets.h` | `controls/osc/osc_x32.h` + `controls/osc/osc_feedback.h` |

---

## Summary of Problem Coverage

| Problem # | Description | Addressed In |
|-----------|-------------|-------------|
| 2 | Bidirectional Circular Dependencies | Phase 5.3, 5.4 |
| 3 | Massive Page Class | Phase 5.1, 5.2 |
| 5 | Action Class Explosion | Phase 2.1, 2.2, 2.3 |
| 6 | Tight Coupling Widget ↔ Feedback | Phase 4.1, 4.2 |
| 9 | Navigator Complexity | Phase 8.1 |
| 11 | ActionContext Kitchen Sink | Phase 6.1–6.5 |
| 12 | No Clear Dependency Injection | Phase 5.3, 9.3 |
| 15 | Poor Separation of Concerns | Phase 9.1, 9.2 |
| 16 | Limited Extensibility | Phase 3.1–3.3 |
| 26 | Send vs Receive Duplication | Phase 2.1 |
| 28 | Duplicated rgbToColor() | Phase 0.2 |
| 29 | SysEx Construction Duplication | Phase 0.3, 4.2 |
| 35 | Massive File Sizes | Phase 0, 1, 2.2, 4.1 |
| 36 | Monolithic Widget Dispatching | Phase 3.1–3.3 |
| 37 | DAW Static God Class | Phase 7.1, 7.2 |

---

## Notes on Safety

- **Phase 0.0 is a pure `git mv` operation** — no code changes, just file relocation + include path updates
- **No behavioral changes in Phase 0 and 1** — purely mechanical extraction
- **Phase 2 uses `using` aliases** so all existing code referencing `TrackSendVolume` still compiles  
- **Phase 3 preserves the same widget type strings** — zone/surface files don't change
- **Phase 5 introduces interface** without changing existing code paths initially
- **All phases are independently revertible** via git
- **The GLOB_RECURSE in CMake** means new subdirectories are automatically discovered — no manual source list maintenance
- **`#include` paths within `src/`** use relative paths (e.g., `#include "../shared/types.h"`, `#include "midi/fb_mcu.h"`) so the namespace is clear from the include alone
