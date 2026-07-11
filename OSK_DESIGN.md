# On-Screen Keyboard (OSK) Feature Design

## Overview

Add an on-screen visual representation of a control surface's button layout, rendered via a ReaImGui Lua script. The OSK shows each surface widget as a colorful, labeled button reflecting its real-time state (active/inactive, color, current zone label). It follows the same communication pattern as the existing OSD feature: C++ pushes state via `SetExtState`, Lua polls and renders.

---

## 1. Surface.txt Layout Extensions

### 1.1 New Directives

Add **comment-style directives** (prefixed with `#`) to Surface.txt so they are ignored by the current parser (which already skips lines starting with `//` or empty lines) but can be parsed by a new secondary pass. This avoids breaking existing Surface.txt parsing.

```
# OSKRow
# OSKSpacer Width=0.5
```

### 1.2 Per-Widget Properties

Add optional inline properties after the widget name on the `Widget` line, delimited by `#`:

```
Widget Prev # Shape=LeftArrow
Widget Rotary RotaryWidgetClass # Shape=Round Width=2 Group=RotaryGroup
Widget RotaryPush # Group=RotaryGroup
Widget Next # Shape=RightArrow
Widget Fader # OSKHidden
```

### 1.3 Supported Widget Properties

| Property | Values | Default | Description |
|----------|--------|---------|-------------|
| `Shape` | `Rect`, `Round`, `LeftArrow`, `RightArrow`, `UpArrow`, `DownArrow` | `Rect` | Visual shape on the OSK |
| `Width` | float (in button-units) | `1.0` | Relative width (e.g., `2` = double-wide rotary) |
| `Height` | float (in button-units) | `1.0` | Relative height |
| `Group` | string | (none) | Groups multiple widgets into one visual block (e.g., Rotary + RotaryPush) |
| `OSKHidden` | (flag, no value) | false | Exclude this widget from OSK display (e.g., faders, touch strips) |
| `Label` | string | Widget name | Override default display name on the OSK button |

### 1.4 Row/Spacer Directives

| Directive | Description |
|---|---|
| `# OSKRow` | Start a new row of buttons. Placed before the first `Widget` of that row. |
| `# OSKSpacer Width=N` | Insert an empty space of N button-widths in the current row. |

### 1.5 Complete FaderPort V2 Example

```
StepSize
  RotaryWidgetClass 0.003
StepSizeEnd

AccelerationValues
  RotaryWidgetClass Dec 41    42   43   44   45   46   47
  RotaryWidgetClass Inc 01    02   03   04   05   06   07
  RotaryWidgetClass Val 0.005 0.01 0.02 0.04 0.08 0.16 0.32
AccelerationValuesEnd

// ===========================================
// BUTTONS ROW 1
// ===========================================

# OSKRow
Widget Solo
  Press           90 08 7f 90 08 00
  FB_TwoState     90 08 7f 90 08 00
WidgetEnd

Widget Mute
  Press           90 10 7f 90 10 00
  FB_TwoState     90 10 7f 90 10 00
WidgetEnd

Widget Arm
  Press           90 00 7f 90 00 00
  FB_TwoState     90 00 7f 90 00 00
WidgetEnd

Widget Shift
  Press           90 46 7f 90 46 00
  FB_TwoState     90 46 7f 90 46 00
WidgetEnd

// ===========================================
// BUTTONS ROW 2
// ===========================================

# OSKRow
Widget Bypass
  Press           90 03 7f 90 03 00
  FB_TwoState     90 03 7f 90 03 00
WidgetEnd

Widget Touch
  Press           90 4d 7f 90 4d 00
  FB_FaderportRGB 90 4d 7f
WidgetEnd

Widget Write
  Press           90 4b 7f 90 4b 00
  FB_FaderportRGB 90 4b 7f
WidgetEnd

Widget Read
  Press           90 4a 7f 90 4a 00
  FB_FaderportRGB 90 4a 7f
WidgetEnd

// ===========================================
// NAV + WHEEL
// ===========================================

# OSKRow
Widget Prev # Shape=LeftArrow
  Press           90 2e 7f 90 2e 00
  FB_TwoState     90 2e 7f 90 2e 00
WidgetEnd

Widget Rotary RotaryWidgetClass # Shape=Round Width=2 Group=RotaryGroup
  Encoder         b0 10 7f [ > 01-3f < 41-7f ]
WidgetEnd

Widget RotaryPush # Group=RotaryGroup OSKHidden
  Press           90 20 7f 90 20 00
WidgetEnd

Widget Next # Shape=RightArrow
  Press           90 2f 7f 90 2f 00
  FB_TwoState     90 2f 7f 90 2f 00
WidgetEnd

// ===========================================
// BUTTONS ROW 4
// ===========================================

# OSKRow
Widget Link
  Press           90 05 7f 90 05 00
  FB_FaderportRGB 90 05 7f
WidgetEnd

Widget Pan
  Press           90 2a 7f 90 2a 00
  FB_FaderportRGB 90 2a 7f
WidgetEnd

Widget Channel
  Press           90 36 7f 90 36 00
  FB_FaderportRGB 90 36 7f
WidgetEnd

Widget Scroll
  Press           90 38 7f 90 38 00
  FB_FaderportRGB 90 38 7f
WidgetEnd

// ===========================================
// BUTTONS ROW 5
// ===========================================

# OSKRow
Widget Master
  Press           90 3a 7f 90 3a 00
  FB_TwoState     90 3a 7f 90 3a 00
WidgetEnd

Widget Click
  Press           90 3b 7f 90 3b 00
  FB_TwoState     90 3b 7f 90 3b 00
WidgetEnd

Widget Section
  Press           90 3c 7f 90 3c 00
  FB_TwoState     90 3c 7f 90 3c 00
WidgetEnd

Widget Marker
  Press           90 3d 7f 90 3d 00
  FB_TwoState     90 3d 7f 90 3d 00
WidgetEnd

// ===========================================
// TRANSPORT
// ===========================================

# OSKRow
Widget Loop
  Press           90 56 7f 90 56 00
  FB_TwoState     90 56 7f 90 56 00
WidgetEnd

Widget Rewind
  Press           90 5b 7f 90 5b 00
  FB_TwoState     90 5b 7f 90 5b 00
WidgetEnd

Widget FastForward
  Press           90 5c 7f 90 5c 00
  FB_TwoState     90 5c 7f 90 5c 00
WidgetEnd

# OSKRow
# OSKSpacer Width=0.5
Widget Stop # Shape=Round
  Press           90 5d 7f 90 5d 00
  FB_TwoState     90 5d 7f 90 5d 00
WidgetEnd

Widget Play # Shape=Round
  Press           90 5e 7f 90 5e 00
  FB_TwoState     90 5e 7f 90 5e 00
WidgetEnd

Widget Record # Shape=Round
  Press           90 5f 7f 90 5f 00
  FB_TwoState     90 5f 7f 90 5f 00
WidgetEnd
# OSKSpacer Width=0.5

// ===========================================
// FADER (hidden from OSK)
// ===========================================
Widget Fader # OSKHidden
  Fader14Bit      e0 7f 7f
  FB_Fader14Bit   e0 7f 7f
  Touch           90 68 7f 90 68 00
WidgetEnd
```

---

## 2. Zone File Extensions

### 2.1 New Property: `KeyLabel`

Add `KeyLabel` as a new property type that can be attached to any action binding in a `.zon` file. It provides a human-readable label for what the button does in that zone context.

**Syntax**: `KeyLabel="label text"` or `KeyLabel=LabelText` (quotes required if spaces)

```zon
Zone "Home"
  Rewind                    Reaper 41041 KeyLabel="|< bar"
  Shift+Rewind              Reaper 41045 KeyLabel="|< beat"
  FastForward               Reaper 41040 KeyLabel="bar >|"
  Shift+FastForward          Reaper 41044 KeyLabel="beat >|"
  
  Solo                      TrackSolo KeyLabel="Solo"
  Mute                      TrackMute KeyLabel="Mute"
  Arm                       TrackRecordArm KeyLabel="Arm"
  
  Rotary                    Reaper 40838 KeyLabel="<< Measure"
  Shift+Rotary              Reaper 40837 KeyLabel="<< Beat"
  RotaryPush                Reaper 40434 KeyLabel="GoPlay"
  
  Increase+Rotary           Reaper 40837 KeyLabel="Measure >>"
  Decrease+Rotary           Reaper 40838 KeyLabel="<< Measure"
  
  DoublePress+Shift         ToggleOSK
ZoneEnd
```

### 2.2 Implementation: `KeyLabel` Property Type

Add to `DECLARE_PROPERTY_TYPES` macro in [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h#L163):

```cpp
D(KeyLabel) \
```

The existing property parsing system will then automatically recognize `KeyLabel=value` in zone action parameters and store it on the `ActionContext`.

### 2.3 Grouped Widget Labels

For grouped widgets (like Rotary + RotaryPush), the OSK should display multiple labels on the same visual block:
- **Outer ring labels**: From `Rotary` / `Increase+Rotary` / `Decrease+Rotary` actions
- **Center label**: From `RotaryPush` action

The Lua script resolves this by the `Group` property — it collects all `KeyLabel` values for widgets sharing the same group and renders them together.

---

## 3. New Action: `ToggleOSK`

### 3.1 Registration

Add to `ACTION_TYPE_LIST(X)` in [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h#L371):

```cpp
X(ToggleOSK, "ToggleOSK") \
```

### 3.2 Implementation

Create in [control_surface_manager_actions.h](reaper_csurf_integrator/control_surface_manager_actions.h):

```cpp
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class ToggleOSK : public Action
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
public:
    ActionType GetType() const override { return ActionType::ToggleOSK; }

    void RequestUpdate(ActionContext *context) override
    {
        context->UpdateWidgetValue(context->GetSurface()->GetOskEnabled() ? 1.0 : 0.0);
    }

    void Do(ActionContext *context, double value) override
    {
        if (value == ActionContext::BUTTON_RELEASE_MESSAGE_VALUE) return;
        
        ControlSurface *surface = context->GetSurface();
        bool newState = !surface->GetOskEnabled();
        surface->SetOskEnabled(newState);
        
        if (newState) {
            surface->PublishOSKLayout();
            surface->PublishOSKState();
            context->GetCSI()->OpenOSKPanel();
        } else {
            context->GetCSI()->CloseOSKPanel();
        }
    }
};
```

### 3.3 Zone Usage

```zon
Zone "Home"
  DoublePress+Shift    ToggleOSK
ZoneEnd
```

---

## 4. C++ → Lua Communication Protocol

### 4.1 Communication Channel

Follow the existing OSD pattern using `SetExtState`/`GetExtState`:

| ExtState Section | Key | Direction | Description |
|---|---|---|---|
| `CSI_OSK` | `Layout_<SurfaceName>` | C++ → Lua | Static layout data (sent once on OSK open) |
| `CSI_OSK` | `State_<SurfaceName>` | C++ → Lua | Dynamic widget states (sent periodically) |
| `CSI_OSK` | `Labels_<SurfaceName>` | C++ → Lua | Current zone key labels (sent on zone change) |
| `CSI_OSK` | `Surfaces` | C++ → Lua | List of active surfaces with OSK enabled |
| `CSI_OSK_CMD` | `WidgetPress` | Lua → C++ | Optional: user clicked a button on the OSK |

### 4.2 Layout Data Format (`Layout_<SurfaceName>`)

Sent once when OSK opens. Encodes the visual layout parsed from Surface.txt.

```
ROW|Widget1:Shape=Rect,Width=1,Height=1,Group=|Widget2:Shape=Round,Width=2,Group=RotaryGroup
ROW|Widget3:Shape=LeftArrow,Width=1|Widget4:Shape=Rect,Width=1
SPACER:0.5|Widget5:Shape=Round,Width=1
```

Format: Newline-separated rows. Each row is `|`-delimited cells. Each cell is either:
- `ROW` row marker (implicit, first token)
- `SPACER:width` — empty space
- `WidgetName:Prop1=Val1,Prop2=Val2,...` — widget with visual properties

### 4.3 State Data Format (`State_<SurfaceName>`)

Sent periodically (~10Hz, or on widget state change). Compact format:

```
Widget1=V:1,C:#FF0000;Widget2=V:0,C:#333333;Widget3=V:0.5,C:#00FF00
```

Fields per widget:
- `V:` — value (0.0-1.0, where >0 = active/highlighted)
- `C:` — current color as `#RRGGBB` hex (from `rgba_color` feedback)

### 4.4 Label Data Format (`Labels_<SurfaceName>`)

Sent when active zone changes. Contains `KeyLabel` values for current modifier state.

```
Widget1=Solo;Widget2=Mute;Widget3=Arm;Rotary=<< Measure|Measure >>;RotaryPush=GoPlay
```

For widgets with `Increase`/`Decrease` variants, labels are `|`-delimited within the widget entry:
- `RotaryName=DecreaseLabel|IncreaseLabel`

For grouped widgets, labels for all group members are collected under the visible group widget.

### 4.5 Surfaces List (`Surfaces`)

```
fp2|MCU
```

Pipe-delimited list of surface names with OSK enabled/visible.

---

## 5. C++ Implementation Details

### 5.1 New Members on `ControlSurface`

```cpp
// In control_surface_integrator.h, class ControlSurface:
private:
    bool isOskEnabled_ = false;
    
    // OSK layout data parsed from Surface.txt
    struct OskWidgetInfo {
        string name;
        string shape = "Rect";    // Rect, Round, LeftArrow, RightArrow, etc.
        float width = 1.0f;
        float height = 1.0f;
        string group;             // group name, empty = standalone
        string label;             // override label from Surface.txt
        bool hidden = false;      // OSKHidden flag
    };
    
    struct OskRow {
        vector<variant<OskWidgetInfo, float>> cells; // float = spacer width
    };
    
    vector<OskRow> oskLayout_;
    string cachedLayoutString_;
    string cachedStateString_;
    string cachedLabelsString_;

public:
    bool GetOskEnabled() const { return isOskEnabled_; }
    void SetOskEnabled(bool v) { isOskEnabled_ = v; }
    void ParseOSKLayout(const string &surfaceFilePath);
    void PublishOSKLayout();
    void PublishOSKState();
    void PublishOSKLabels();
```

### 5.2 Parsing OSK Layout from Surface.txt

Add a secondary parsing pass in `ControlSurface::ParseOSKLayout()` that reads the same Surface.txt file but only processes `# OSKRow`, `# OSKSpacer`, and `Widget ... # ...` lines.

```cpp
void ControlSurface::ParseOSKLayout(const string &surfaceFilePath)
{
    ifstream file(surfaceFilePath);
    OskRow currentRow;
    bool hasRow = false;
    
    for (string line; getline(file, line);)
    {
        TrimLine(line);
        
        // Check for # OSKRow
        if (line == "# OSKRow") {
            if (hasRow && !currentRow.cells.empty())
                oskLayout_.push_back(std::move(currentRow));
            currentRow = OskRow();
            hasRow = true;
            continue;
        }
        
        // Check for # OSKSpacer Width=N
        if (line.find("# OSKSpacer") == 0) {
            float width = 0.5f; // default
            // parse Width=N from the line
            auto pos = line.find("Width=");
            if (pos != string::npos)
                width = atof(line.c_str() + pos + 6);
            if (hasRow)
                currentRow.cells.push_back(width);
            continue;
        }
        
        // Check for Widget line with # properties
        if (line.find("Widget ") == 0) {
            auto hashPos = line.find('#');
            string widgetPart = (hashPos != string::npos) ? line.substr(0, hashPos) : line;
            string propsPart = (hashPos != string::npos) ? line.substr(hashPos + 1) : "";
            
            // Extract widget name (second token)
            vector<string> tokens;
            GetTokens(tokens, widgetPart);
            if (tokens.size() < 2) continue;
            
            OskWidgetInfo info;
            info.name = tokens[1];
            
            // Parse # properties
            // Shape=Round Width=2 Group=RotaryGroup OSKHidden Label="Play/Pause"
            ParseOskProperties(propsPart, info);
            
            if (!hasRow) {
                currentRow = OskRow();
                hasRow = true;
            }
            currentRow.cells.push_back(info);
        }
    }
    
    if (hasRow && !currentRow.cells.empty())
        oskLayout_.push_back(std::move(currentRow));
        
    BuildCachedLayoutString();
}
```

### 5.3 Publishing State

Called from `CSurfIntegrator::Run()` at ~10Hz (every 3rd cycle of the 30Hz loop):

```cpp
void ControlSurface::PublishOSKState()
{
    if (!isOskEnabled_) return;
    
    string state;
    for (auto &[name, widget] : widgetsByName_) {
        if (/* widget is in oskLayout_ and not hidden */) {
            double value = /* last feedback value from widget's FeedbackProcessors */;
            rgba_color color = /* last color value */;
            
            if (!state.empty()) state += ";";
            state += name + "=V:" + to_string(value) 
                   + ",C:" + color.to_hex_string();
        }
    }
    
    if (state != cachedStateString_) {
        cachedStateString_ = state;
        ::SetExtState("CSI_OSK", ("State_" + name_).c_str(), state.c_str(), false);
    }
}
```

### 5.4 Publishing Labels

Called when active zone changes (in `ZoneManager::GoZone()`, `GoHome()`, etc.):

```cpp
void ControlSurface::PublishOSKLabels()
{
    if (!isOskEnabled_) return;
    
    string labels;
    // Iterate active zone's actionContextDictionary_
    // For each widget, find KeyLabel property on its ActionContexts
    // For Increase/Decrease variants, combine with | separator
    
    // Format: Widget1=Label1;Widget2=Label2;...
    
    if (labels != cachedLabelsString_) {
        cachedLabelsString_ = labels;
        ::SetExtState("CSI_OSK", ("Labels_" + name_).c_str(), labels.c_str(), false);
    }
}
```

### 5.5 Open/Close OSK Panel

Follow the OSD pattern. Add to `CSurfIntegrator`:

```cpp
static const char * const REASCRIPT_PATH__CSI_OSK = "/Scripts/CSI/CSI OSK on-screen keyboard.lua";
static const char * const REASCRIPT_HASH__CSI_OSK = "_RS<hash_to_be_generated>";

void OpenOSKPanel() {
    // Same pattern as OpenOSDPanel() — AddRemoveReaScript + SendCommandMessage
    // ...
}

void CloseOSKPanel() {
    // Set ExtState flag that tells Lua to self-close
    ::SetExtState("CSI_OSK", "Command", "Close", false);
}
```

### 5.6 Tracking Widget Feedback Values for OSK

The key challenge: `FeedbackProcessor::SetValue()` updates `lastDoubleValue_` and `lastColor_`, but these are on the feedback processor, not easily queryable from outside.

**Approach**: Add public getters to `Widget`:

```cpp
// In Widget class:
double GetLastFeedbackValue() const {
    if (!feedbackProcessors_.empty())
        return feedbackProcessors_[0]->GetLastDoubleValue();
    return 0.0;
}

rgba_color GetLastFeedbackColor() const {
    if (!feedbackProcessors_.empty())
        return feedbackProcessors_[0]->GetLastColor();
    return {0,0,0,0};
}
```

And corresponding getters on `FeedbackProcessor`:

```cpp
double GetLastDoubleValue() const { return lastDoubleValue_; }
const rgba_color &GetLastColor() const { return lastColor_; }
```

---

## 6. Lua Script Design

### 6.1 Script Structure: `CSI OSK on-screen keyboard.lua`

Located at: `Scripts/CSI/CSI OSK on-screen keyboard.lua`

Uses **ReaImGui** (like the track icon selector reference script).

### 6.2 Architecture

```
Init()
  ├─ CreateContext / CreateFont
  ├─ Read Layout from ExtState("CSI_OSK", "Layout_*")
  ├─ Parse into internal table structure
  └─ Enter main loop
  
main() [defer loop @ ~30fps]
  ├─ Poll State from ExtState("CSI_OSK", "State_*")
  ├─ Poll Labels from ExtState("CSI_OSK", "Labels_*")
  ├─ Check close command from ExtState("CSI_OSK", "Command")
  ├─ Render surface tabs (if multiple surfaces)
  └─ For each row in layout:
      ├─ For each cell in row:
      │   ├─ Spacer → imgui.Dummy(width, height)
      │   ├─ Widget → Draw button with:
      │   │   ├─ Shape (rect/round via DrawList)
      │   │   ├─ Background color from state
      │   │   ├─ Highlight glow when value > 0
      │   │   ├─ Label text from labels data
      │   │   └─ Optional click → SetExtState("CSI_OSK_CMD", "WidgetPress", widgetName)
      │   └─ SameLine() between cells
      └─ NewLine between rows
```

### 6.3 Visual Design

```
┌────────────────────────────────────────┐
│ [fp2]  [MCU]                   [⚙] [×] │ ← Surface tabs + settings
├────────────────────────────────────────┤
│                                        │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
│  │ Solo │ │ Mute │ │ Arm  │ │Shift │   │ ← Row 1
│  │      │ │      │ │      │ │      │   │
│  └──────┘ └──────┘ └──────┘ └──────┘   │
│                                        │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
│  │Bypass│ │Touch │ │Write │ │ Read │   │ ← Row 2 (Touch/Write/Read have color)
│  │      │ │      │ │      │ │      │   │
│  └──────┘ └──────┘ └──────┘ └──────┘   │
│                                        │
│   ◄     ┌─── << Measure ───┐     ►     │ ← Row 3 (arrows + grouped rotary)
│  Prev   │    ⟳ GoPlay      │   Next   │
│         └───  Measure >> ──┘           │
│                                        │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
│  │ Link │ │ Pan  │ │ Chan │ │Scroll│   │ ← Row 4
│  └──────┘ └──────┘ └──────┘ └──────┘   │
│                                        │
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
│  │Master│ │Click │ │Sectn │ │Marker│   │ ← Row 5
│  └──────┘ └──────┘ └──────┘ └──────┘   │
│                                        │
│       ┌──────┐ ┌──────┐ ┌──────┐       │
│       │ Loop │ │  RW  │ │  FF  │       │ ← Row 6
│       └──────┘ └──────┘ └──────┘       │
│                                        │
│        ◯ Stop   ◯ Play   ◯ Rec      | ← Row 7 (round buttons)
│                                        │
└────────────────────────────────────────┘
```

### 6.4 Rendering Details

**Button Sizing & Aspect Ratio**:
- Base button size is `BUTTON_SIZE * ZOOM` (height) by `BUTTON_SIZE * ZOOM * BUTTON_ASPECT` (width)
- Default `BUTTON_ASPECT = 1.2` (6:5 ratio) — buttons are slightly wider than tall
- Aspect ratio is configurable via the Settings popup slider (range 0.5–2.0)
- **Round buttons ignore the aspect ratio** — they use `baseH` (not `baseW`) for their width unit, so `Width=1 Height=1` always produces a circle regardless of aspect setting
- `Width` and `Height` properties from Surface.txt multiply respectively on the base width and base height
- Spacers use the base width for their width unit and the tallest cell height of the current row
- Horizontal padding (`BUTTON_PAD_H`, default 6) and vertical padding (`BUTTON_PAD_V`, default 6) between buttons are independently configurable via Settings sliders (range 0–20)

**Button Colors**:
- **Inactive**: Dark gray background (`#333333`)
- **Active (value > 0)**: Use the widget's reported color, or bright highlight if no color
- **Active fallback**: Golden outline (like the track icon selector's `outline_col`)

**Shape Rendering**:

| Shape | Description | Visual |
|-------|-------------|--------|
| `Rect` | Rounded rectangle with 4px corner radius | Standard rectangular button |
| `Round` | **Circle** when `Width == Height`; **Stadium/discorectangle** (half-circle caps + rectangular body) when `Width != Height`. **Ignores aspect ratio** — uses `baseH` for width unit so `Width=1` = square | `(====)` horizontal or vertical capsule |
| `LeftArrow` | Rectangular body with a **triangular point** glued to the left edge. Apex angle is configurable via `ARROW_ANGLE` (default 120°, obtuse). Point depth = `(height/2) / tan(angle/2)` | `◁▭` pentagon shape |
| `RightArrow` | Rectangular body with a **triangular point** glued to the right edge. Same configurable apex angle | `▭▷` pentagon shape |
| `UpArrow` | Rectangular body with a **triangular point** glued to the top edge. Point depth = `(width/2) / tan(angle/2)` | `△` over `▭` pentagon shape |
| `DownArrow` | Rectangular body with a **triangular point** glued to the bottom edge. Same configurable apex angle | `▭` over `▽` pentagon shape |

**Round Button (Stadium) Details**:
- **Round buttons ignore the aspect ratio** — their width is computed from `baseH * cell.width` (not `baseW`), ensuring `Width=1 Height=1` produces a perfect circle regardless of the aspect ratio setting
- When `Width == Height` (or nearly equal, tolerance < 2px): Draws a solid circle via path segments
- When `Width > Height`: Draws a horizontal stadium — left half-circle cap, rectangular body, right half-circle cap. Cap radius = half the height
- When `Height > Width`: Draws a vertical stadium — top half-circle cap, rectangular body, bottom half-circle cap. Cap radius = half the width
- Active state adds a golden outline stroke following the same stadium/circle path

**Arrow Button Details**:
- The arrow is a **5-vertex pentagon** (not a small triangle inside a bounding box)
- The rectangular body fills most of the button area
- The triangle apex angle is **configurable** via `ARROW_ANGLE` setting (default 120°, range 60°–150°). At 90° it matches the previous right-triangle behavior; at 120° the point is shallower/wider (obtuse)
- Point depth formula: `pointDepth = (dimension/2) / tan(ARROW_ANGLE/2)` — where dimension = body height for left/right arrows, body width for up/down arrows
- Point depth is clamped to 45% of the button length to prevent the body from disappearing
- The full bounding box (`bw × bh`) is consumed by the shape
- Label text is centered within the rectangular body portion (excluding the triangle point area)
- Active state draws a golden outline following the pentagon path

**Height Support**:
- Each widget cell uses `Height` property to scale the base button height: `bh = baseH * cell.height`
- Per-row max height is computed to size spacers correctly within each row
- Height values like `Height=1.5` produce a 50% taller button; `Height=0.5` produces a half-height button
- Examples: `Widget BigButton # Height=2` creates a double-height button

**Grouped Widgets (Rotary example)**:
- Draw a larger rounded rect spanning `Width` button-units
- Decrease label on top-left, Increase label on top-right
- Push label centered
- Show rotation indicator or ring when applicable

**Button Labels**:
- Primary: `KeyLabel` from zone file (if defined)
- Fallback: Widget name from Surface.txt `Label` property
- Final fallback: Widget name itself
- **PascalCase splitting**: Labels like `FastForward` are auto-split to `Fast Forward`, `RotaryPush` → `Rotary Push`. The splitting handles both `camelCase` and `PascalCase` boundaries (including `ABCDef` → `ABC Def`)
- **Configurable word replacements**: A user-configurable list of word→replacement mappings is applied after splitting. Default: `Toggle` → (removed), `Reaper` → (removed). Configured via the Settings popup text field as semicolon-separated `key=value` pairs (e.g., `Toggle=;Reaper=;FastFwd=FF`). Empty value removes the word entirely
- **Label processing pipeline**: `stripPrefix → splitPascalCase → applyReplacements → trim`

**Tooltips**:
- Tooltips appear on hover with a **configurable delay** (default 2.0 seconds)
- During the first `TOOLTIP_DELAY` seconds of hovering, no tooltip is shown
- When the mouse leaves the button, the timer resets
- Delay is configurable via Settings slider (range 0.0–5.0 seconds)

**Modifier Awareness**:
- When a modifier key (Shift, etc.) is active, labels should update to show the shifted action labels
- The C++ side re-publishes labels when modifier state changes

**Window Behavior**:
- The window **auto-sizes to content** (`AlwaysAutoResize` flag) — no manual sizing needed
- Window **transparency** is configurable via Settings slider (range 0.2–1.0, default 1.0 = fully opaque)
- Window is dockable/undockable via the Settings menu

### 6.5 Key Lua Functions

```lua
function ParseLayout(layoutStr)          -- Parse ROW|widget|... format into table
function ParseState(stateStr)            -- Parse Widget1=V:1,C:#FF0000;... into table  
function ParseLabels(labelsStr)          -- Parse Widget1=Label;... into table
function splitPascalCase(text)           -- "FastForward" → "Fast Forward"
function applyLabelReplacements(text)    -- Apply word→replacement mappings
function processLabel(text)              -- Full pipeline: strip prefix → split → replace
function parseLabelReplacements(str)     -- Parse "key1=val1;key2=val2" settings string
function ShowDelayedTooltip(ctx, id, t)  -- Show tooltip only after TOOLTIP_DELAY seconds
function DrawRectButton(...)             -- Render rectangular button
function DrawRoundButton(...)            -- Render circle/stadium button (no aspect ratio)
function DrawArrowButton(...)            -- Render 5-vertex pentagon arrow button
function RenderSurface(ctx, surfName)    -- Render all rows of a surface layout
function main()                          -- Main defer loop
```

### 6.6 Settings (stored in `CSI_OSK_SETTINGS` ExtState)

| Setting | Key | Type | Default | Range | Description |
|---------|-----|------|---------|-------|-------------|
| Zoom | `zoom` | float | `1.0` | 0.5–3.0 | Overall size multiplier for all buttons and spacing |
| Aspect Ratio | `aspect` | float | `1.2` | 0.5–2.0 | Button width/height ratio (1.2 = 6:5, buttons slightly wider than tall). Does **not** affect round buttons |
| H Padding | `pad_h` | float | `6` | 0–20 | Horizontal padding (pixels, pre-zoom) between buttons in a row |
| V Padding | `pad_v` | float | `6` | 0–20 | Vertical padding (pixels, pre-zoom) between rows |
| Arrow Angle | `arrow_angle` | float | `120` | 60–150 | Apex angle (degrees) for arrow button triangle. 90° = right triangle, 120° = obtuse/shallow |
| Transparency | `transparency` | float | `1.0` | 0.2–1.0 | Window background alpha (0.2 = mostly transparent, 1.0 = opaque) |
| Tooltip Delay | `tooltip_delay` | float | `2.0` | 0.0–5.0 | Seconds to hover before tooltip appears |
| Clickable | `clickable` | bool | `true` | — | Whether OSK buttons can be clicked to trigger actions via CSI |
| Label Replacements | `label_replacements` | string | `"Toggle=;Reaper="` | — | Semicolon-separated `word=replacement` pairs. Empty replacement removes the word |
| Window position | `xpos`, `ypos` | int | `200` | — | Saved window position |
| Window size | `wlen`, `hlen` | int | `500` | — | Saved window dimensions |
| Docked | `docked` | int | `0` | — | Docking state |

---

## 7. Modifier-Aware Label Updates

When the user presses Shift (or any modifier), the OSK should update labels to reflect what each button does with that modifier active.

### 7.1 C++ Side

In `ModifierManager` or wherever modifier state changes are tracked, call:

```cpp
void ControlSurface::OnModifierChanged()
{
    if (isOskEnabled_)
        PublishOSKLabels(); // Re-publish with current modifier context
}
```

`PublishOSKLabels()` iterates the active zone's `actionContextDictionary_` and for the current modifier bitmask, finds each widget's `KeyLabel`:

```
For widget W with current modifier M:
  1. Look up actionContextDictionary_[W][M]
  2. If found, get KeyLabel from that context's properties
  3. If not found, fall back to actionContextDictionary_[W][0] (no modifier)
  4. For Increase/Decrease contexts, combine labels
```

### 7.2 Zone Change

When `GoZone()`, `GoHome()`, `GoSubZone()`, or `LeaveSubZone()` is called, also call `PublishOSKLabels()` to reflect the new zone's bindings.

---

## 8. Update Frequency / Performance

| Data | Update Trigger | Rate |
|---|---|---|
| Layout | OSK open, surface reload | Once |
| State | Every N-th `Run()` cycle | ~10Hz (every 3rd of 30Hz) |
| Labels | Zone change, modifier change | Event-driven |

**Optimization**: Only call `SetExtState` when the serialized string actually changes (already shown via `cachedStateString_` comparison).

**Lua side**: Poll `GetExtState` in each `defer` iteration (~30Hz is fine for ImGui). Parse only when value changes.

---

## 9. Clickable OSK Buttons (Lua → C++)

OSK buttons are clickable **by default** (controlled by the `clickable` setting). When a user clicks an OSK button, it triggers the same CSI action as pressing the physical button on the surface.

### 9.1 Communication (Lua → C++)

When user clicks a button on the OSK (all three draw functions: Rect, Round, Arrow):

```lua
-- On InvisibleButton click, when vars.clickable is true:
reaper.SetExtState("CSI_OSK_CMD", "WidgetPress", surfaceName .. "|" .. widgetName, false)
```

The `surfaceName` is resolved from `surfaces[currentSurface]`, ensuring the click is routed to the correct surface even in multi-surface setups.

### 9.2 C++ Polling

In `CSurfIntegrator::Run()`:

```cpp
if (HasExtState("CSI_OSK_CMD", "WidgetPress")) {
    string cmd = GetExtState("CSI_OSK_CMD", "WidgetPress");
    DeleteExtState("CSI_OSK_CMD", "WidgetPress", false);
    
    // Parse "surfaceName|widgetName"
    auto surface = GetSurfaceByName(surfaceName);
    if (surface) {
        auto widget = surface->GetWidgetByName(widgetName);
        if (widget) {
            surface->GetZoneManager()->DoAction(widget, 1.0);
            // Schedule release after one cycle
            pendingReleases_.push_back({widget, 1}); // release after 1 cycle
        }
    }
}
```

This is **optional for Phase 1** but architecturally straightforward.

### 9.3 Lua-Side Click Behavior

- All three button draw functions (`DrawRectButton`, `DrawRoundButton`, `DrawArrowButton`) use `imgui.InvisibleButton` for hit detection
- Clicks are only processed when `vars.clickable == true` (default)
- The clickable setting is togglable in the Settings popup
- Users can disable clicking if they only want a visual-only OSK display

---

## 10. Implementation Phases

### Phase 1: Core Infrastructure
1. Add `KeyLabel` to `DECLARE_PROPERTY_TYPES` macro
2. Add `ToggleOSK` to `ACTION_TYPE_LIST` macro
3. Implement `ToggleOSK` action class
4. Add `isOskEnabled_`, `oskLayout_` members to `ControlSurface`
5. Implement `ParseOSKLayout()` — parse `# OSKRow`, `# OSKSpacer`, `Widget # ...` from Surface.txt
6. Add `GetLastDoubleValue()` / `GetLastColor()` getters to `FeedbackProcessor`
7. Implement `PublishOSKLayout()`, `PublishOSKState()`, `PublishOSKLabels()`
8. Add `OpenOSKPanel()` / `CloseOSKPanel()` to `CSurfIntegrator`
9. Hook `PublishOSKState()` into `Run()` loop at ~10Hz
10. Hook `PublishOSKLabels()` into zone change and modifier change paths

### Phase 2: Lua Script
1. Create `Scripts/CSI/CSI OSK on-screen keyboard.lua`
2. Implement layout parser
3. Implement state/label parser
4. Implement button rendering (rect, round/stadium, arrow shapes via ImGui DrawList)
5. Round buttons ignore aspect ratio (use baseH for width unit)
6. Arrow buttons with configurable obtuse apex angle
7. PascalCase → Title Case label splitting
8. Configurable label word replacement system
9. Delayed tooltips (configurable, default 2s)
10. Window auto-resize to content + configurable transparency
11. Configurable horizontal/vertical padding between buttons
12. Clickable buttons → CSI via ExtState
13. Implement grouped widget rendering
14. Implement modifier-aware label display
15. Add settings menu (zoom, aspect, padding, arrow angle, transparency, tooltip delay, label replacements, clickable, dock)

### Phase 3: Surface.txt Annotations
1. Add `# OSKRow` / `# OSKSpacer` / `Widget # Shape=...` to FaderPort V2 Surface.txt
2. Add `KeyLabel=` to FaderPort V2 zone files
3. Add `ToggleOSK` binding to FaderPort V2 Home.zon
4. Test and iterate

### Phase 4: Polish & Optional
1. Clickable OSK buttons (Lua → C++)
2. Multi-surface tab support
3. Blinking state support
4. Tooltip with full action description on hover
5. Keyboard shortcut to toggle OSK globally

---

## 11. Files to Modify

| File | Changes |
|---|---|
| [control_surface_integrator.h](reaper_csurf_integrator/control_surface_integrator.h) | Add `KeyLabel` to `DECLARE_PROPERTY_TYPES`, `ToggleOSK` to `ACTION_TYPE_LIST`, OSK members to `ControlSurface`, `OpenOSKPanel`/`CloseOSKPanel` to `CSurfIntegrator`, getters on `FeedbackProcessor`/`Widget`, REASCRIPT path constants |
| [control_surface_manager_actions.h](reaper_csurf_integrator/control_surface_manager_actions.h) | Add `ToggleOSK` action class |
| [control_surface_integrator.cpp](reaper_csurf_integrator/control_surface_integrator.cpp) | Implement `ParseOSKLayout()`, `PublishOSKLayout/State/Labels()`, hook into `Run()`, zone change paths, add `ToggleOSK` to `InitActionsDictionary` (auto via X-macro) |
| [CSI/Surfaces/FaderPortV2/Surface.txt](CSI/Surfaces/FaderPortV2/Surface.txt) | Add `# OSKRow`, `# OSKSpacer`, `Widget # Shape=...` annotations |
| FaderPortV2 Zone files | Add `KeyLabel=` properties, `ToggleOSK` binding |
| **New:** `Scripts/CSI/CSI OSK on-screen keyboard.lua` | Full ReaImGui-based OSK renderer |

---

## 12. Open Questions

1. **Should clicking OSK buttons fire the action?** (Phase 4 scope, but affects design)
2. **Multiple surfaces**: Show as tabs or separate windows?
3. **Should faders/encoders show as sliders on the OSK, or just their push-button component?** → Current design hides them via `OSKHidden`, but could optionally show a read-only slider.
4. **Should the OSK auto-close when CSI reloads?** → Yes, send `Close` command.
5. **Should `# OSKRow` directives be required, or should the parser auto-infer rows from sequential widgets?** → Required is explicit and less error-prone.
6. **Label updates for Increase/Decrease modifiers**: Should both directions always be shown, or only the active label? → Show both (top-left / top-right) for rotary groups.




# initial prompt

 need to plan feature to show some simple on-screen-keyboard-like ui with colorful highlightable buttons (based on button aka widget state) using ui generated with lua script, the idea is to have some custom actions and params, like KeyLabel
```Home.zon
  Rewind                    Reaper 41041 KeyLabel="|< bar" // Move edit cursor to start of current measure
  Shift+Rewind              Reaper 41045 KeyLabel="|< beat" // Move edit cursor back one beat
```
so for example we add some action like ToggleOSK
```Home.zon
    DoublePress+Shift      ToggleOSK
```
and OSK will show full surface buttons according their current color/highlight state with ley labels according current zone and defined keyLabels.


it is almost like we curently do OSD with ShowOSD in CSI, but now we probably need to feed current surface(s) layouts to lua script on script launch with some special format (to be defined)

each used surface has Surface.txt, where we need to define some params for its  buttons, like this particular surface of faderport v2 has 7 rows of buttons, from top left:
1 solo mute arm shift
2 bypass touch write read
3 prev(shape=arrowleft) wheel (type=rotarypush shape=round width=2) next(shape=arrowright)
4 link pan channel scroll
5 master click section marker
6 loop rewind fastforward
7 (spacer width=0.5)  stop(shape=round) playpause(shape=round) rec(shape=round) (spacer width=0.5)  
and Surface.txt probably need to also define something like this: (added after #)
```
# NewRow
Widget Prev # Shape=LeftArrow
  Press           90 2e 7f 90 2e 00
  FB_TwoState     90 2e 7f 90 2e 00
WidgetEnd

Widget Rotary RotaryWidgetClass
  Encoder         b0 10 7f [ > 01-3f < 41-7f ]
WidgetEnd

Widget RotaryPush
  Press           90 20 7f 90 20 00
WidgetEnd

Widget Next # Shape=RightArrow
  Press           90 2f 7f 90 2f 00
  FB_TwoState     90 2f 7f 90 2f 00
WidgetEnd

# NewRow
...
```
not sure how to better describe group of widgets as one, like we have rotary button, that probably should have like 2 or 3 labels, one for push and one or 2 for rotary action, depends on zone definition, like we can have separate actions for inc-/decrease or one action 
```
  Shift+Increase+Rotary  Reaper 40837 // Move edit cursor to start of next measure (no seek)
  Increase+Rotary        Reaper 40837 // Move edit cursor to start of next measure (no seek)
  Shift+Decrease+Rotary  Reaper 40838 // Move edit cursor to start of current measure (no seek)
  Decrease+Rotary        Reaper 40838 // Move edit cursor to start of current measure (no seek)
  
  RotaryPush             Reaper 40434 // View: Move edit cursor to play cursor
  Shift+RotaryPush       NoAction
  
  Flip+Rotary            TrackPan
  Flip+RotaryPush        TrackPan [ 0.5 ]

```
so we need to add some syntax to surface parser to define rows/spacers that will be defining how to render OSK buttons layout with lua script, and once per N seconds or after some button(aka widget) press update the state of buttons on that OSK (like collors, highlight, labels) etc.

u can also check attached track icons selector script can be used as some lua ui building example