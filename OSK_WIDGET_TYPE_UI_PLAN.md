# OSK Widget Type and Rotary UI Plan

## Purpose

This plan changes the OSK GUI from shape-driven behavior to widget-type-driven behavior.
`Shape` should describe how a widget looks. It must not be the primary signal for
whether the OSK treats a widget as a button, rotary, fader, touch control, or value
display.

The immediate goals are:

- Detect faders and rotaries from the parsed widget type/class, even when `Shape` is
  omitted.
- Keep pushable rotaries from firing relative Increase/Decrease actions on mouse
  click.
- Add rotary rendering that can show current feedback value, including dot and wiper
  styles similar to hardware encoder displays.
- Preserve the existing Surface.txt layout hints, hidden push widgets, OSK config
  editor, and ExtState command flow.

## Current Baseline

Example surface layout:

```text
Widget Fader # Shape=Fader Height=7.5
  Fader14Bit      e0 7f 7f
  FB_Fader14Bit   e0 7f 7f
  Touch           90 68 7f 90 68 00
WidgetEnd

Widget RotaryBig RotaryWidgetClass # Shape=Round Height=1.5 Width=1.5 Group=RotaryGroup
  Encoder         b0 10 7f [ > 01-3f < 41-7f ]
WidgetEnd

Widget RotaryBigPush # Group=RotaryGroup OSKHidden
  Press           90 20 7f 90 20 00
WidgetEnd
```

Current behavior and weaknesses:

- `PublishOSKState()` and Lua rendering treat faders mostly by checking
  `Shape=Fader`.
- Lua detects relative widgets with name/group heuristics such as `rotary`,
  `encoder`, or `Group=Rotary`.
- A visible rotary with only an `Encoder` line is still drawn as a button-like item
  when `Shape=Round`; clicking it sends press commands to the rotary widget name.
- If the zone maps Increase/Decrease actions to the rotary widget, mouse click can
  trigger relative-style behavior instead of the paired push button.
- Hidden push widgets can share a `Group`, but OSK does not publish or consume a
  formal push-target relationship.
- Rotary feedback values are present in the generic widget feedback path, but the OSK
  renderer has no rotary value visualization.

## Target Model

Separate semantics from presentation:

- `Role`: what the OSK should consider the control to be: `button`, `rotary`,
  `fader`, `display`, `meter`, or `unknown`.
- `Shape`: visual preference: `rect`, `round`, `leftarrow`, `rightarrow`, `uparrow`,
  `downarrow`, `fader`, etc. Shape can override appearance, but not input semantics.
- `Input`: supported input capabilities: `press`, `relative`, `absolute`, `touch`.
- `Feedback`: supported feedback capabilities: `toggle`, `value`, `color`, `text`,
  `meter`.
- `Targets`: semantic targets used by Lua commands:
  `PressTarget`, `ScrollTarget`, `ValueTarget`, and `TouchTarget`.

For the example above, the desired OSK meaning is:

- `Fader` has `Role=fader`, `Input=absolute,touch`, `Feedback=value`, and sends
  `WidgetValue`/`WidgetTouch` to `Fader`.
- `RotaryBig` has `Role=rotary`, `Input=relative`, `Feedback=value` if any value
  feedback or current action value is available, and sends wheel/drag-relative input
  to `RotaryBig`.
- `RotaryBig` also has `PressTarget=RotaryBigPush`, so clicking the visible rotary
  sends `WidgetPressDown`/`WidgetPressUp` to `RotaryBigPush`.
- `RotaryBigPush` remains hidden and does not create a separate OSK cell.

## Phase 1: Capture Widget Capabilities in C++

Goal: make the native parser remember what kind of control each widget actually is.

### Work

1. Extend `Widget` with OSK-facing metadata.
   - Store the optional widget class from `Widget <name> <class>`.
   - Track input capabilities: press, relative, absolute, touch.
   - Track feedback capabilities: value, toggle/color, text/display, meter.
   - Keep this metadata protocol-neutral enough for MIDI and OSC.
2. Populate metadata during normal surface parsing.
   - For MIDI, set metadata from widget type lines before or during
     `MidiWidgetRegistry::Dispatch()`.
   - Treat `Press` and `AnyPress` as press-capable.
   - Treat `Touch` as touch-capable.
   - Treat `Encoder`, `MFTEncoder`, `EncoderPlain`, `Encoder7Bit`, and accelerated
     rotary classes as relative-capable.
   - Treat `Fader14Bit`, `Fader7Bit`, `FaderportClassicFader14Bit`, and OSC fader
     controls as absolute-capable.
   - Treat `FB_Fader*`, `FB_Encoder`, `FB_AsparionEncoder`, `FB_SCE24Encoder`, and
     similar numeric feedback processors as value-feedback-capable.
3. Add small helper methods on `Widget`.
   - `GetOskRole()`
   - `HasOskPressInput()`
   - `HasOskRelativeInput()`
   - `HasOskAbsoluteInput()`
   - `HasOskTouchInput()`
   - `HasOskValueFeedback()`
4. Keep metadata conservative.
   - Unknown widget types should not be guessed as rotary/fader in C++.
   - Lua may keep old heuristics only as fallback for older or partial payloads.

### Acceptance

- A `Fader14Bit` widget is identified as fader-capable without `Shape=Fader`.
- A `RotaryWidgetClass` plus `Encoder` widget is identified as rotary/relative
  without `Shape=Round`.
- Existing button widgets continue to publish as button-like controls.

## Phase 2: Publish Capability Metadata in OSK Layout

Goal: update the C++ to Lua `Layout_<surface>` payload so Lua does not infer behavior
from names, groups, or visual shape.

### Work

1. Extend `OskWidgetInfo`.
   - Add `widgetClass`.
   - Add `role`.
   - Add `input`.
   - Add `feedback`.
   - Add `pressTarget`.
   - Add `scrollTarget`.
   - Add `valueTarget`.
   - Add `touchTarget`.
   - Add optional `rotaryStyle`.
2. Build these fields in `ParseOSKLayout()` after normal widget parsing.
   - Query `GetWidgetByName(info.name)` for metadata.
   - Default `role` from metadata, not shape.
   - Let explicit `Role=...` in the OSK comment override auto detection for edge
     cases.
   - Let explicit `RotaryStyle=Dot` or `RotaryStyle=Wiper` choose rotary rendering.
3. Auto-pair grouped hidden push widgets.
   - Keep parsing `OSKHidden` widgets into a hidden metadata map instead of dropping
     all knowledge of them immediately.
   - If one visible rotary and one hidden press-capable widget share a `Group`, set
     the visible rotary's `PressTarget` to the hidden widget.
   - If there are multiple possible hidden push widgets in the same group, require an
     explicit `PushTarget=WidgetName` or `PressTarget=WidgetName` property to avoid
     guessing.
4. Serialize the new metadata.
   - Keep the existing `name:Shape=...,Width=...,Height=...` format.
   - Add fields such as `Role=Rotary,Input=Relative,Feedback=Value,Class=RotaryWidgetClass,PressTarget=RotaryBigPush,ScrollTarget=RotaryBig`.
   - Preserve old fields for compatibility with current Lua.
5. Update `LUA_CPP_EXTSTATE_INTERFACE.md`.
   - Document `Role`, `Input`, `Feedback`, and target metadata as part of layout cell
     metadata.

### Acceptance

- Lua receives enough metadata to identify faders and rotaries without `Shape`.
- Hidden push widgets remain hidden but can still be targeted by visible grouped
  rotaries.
- Old `Shape=Fader` and `Shape=Round` layouts continue to render.

## Phase 3: Parse and Centralize Widget Semantics in Lua

Goal: make Lua ask one semantic helper what a cell is and where commands should go.

### Work

1. Extend `Scripts/CSI/layout_parser.lua`.
   - Parse `Role`, `Input`, `Feedback`, `Class`, `PressTarget`, `ScrollTarget`,
     `ValueTarget`, `TouchTarget`, and `RotaryStyle`.
   - Normalize these fields to lowercase where appropriate.
   - Keep unknown fields harmless.
2. Add semantic helpers in `osk_data.lua`.
   - `GetWidgetRole(surfaceName, widgetName)`
   - `IsButtonWidget(surfaceName, widgetName)`
   - `IsRotaryWidget(surfaceName, widgetName)`
   - `IsFaderWidget(surfaceName, widgetName)`
   - `IsRelativeWidget(surfaceName, widgetName)`
   - `GetPressTarget(surfaceName, cell)`
   - `GetScrollTarget(surfaceName, cell)`
   - `GetValueTarget(surfaceName, cell)`
   - `GetTouchTarget(surfaceName, cell)`
3. Reduce name/group heuristics.
   - Use metadata first.
   - Keep current name/group detection only as fallback when no metadata exists.
   - Remove fallback reliance once the layout payload is considered stable.
4. Update fader debug and state handling.
   - Replace shape checks such as `cell.shape == "fader"` with `IsFaderWidget()`.

### Acceptance

- `data.IsRelativeWidget()` returns true for metadata-driven rotaries.
- `data.IsFaderWidget()` returns true for metadata-driven faders.
- Lua still works against an older payload during development.

## Phase 4: Fix Rotary Input Semantics

Goal: separate visible rotary movement from push-button clicks.

### Work

1. Update `osk_input.lua` command target handling.
   - `HandleWheel()` sends `WidgetScroll` to `ScrollTarget`.
   - `HandlePressDown()` and `HandlePressUp()` send press commands to `PressTarget`.
   - If a rotary has no `PressTarget` and is not press-capable itself, clicking it
     should do nothing except focus/hover.
2. Update `osk_render.lua` interaction routing.
   - Rotary controls should call rotary-specific interaction code.
   - Wheel on rotary sends relative scroll.
   - Mouse click on rotary sends press only when a press target exists.
   - Do not call generic button press handling for relative-only rotaries.
3. Preserve right-click behavior.
   - Right-click on a rotary should still open config editing.
   - If a rotary has a separate push target, expose a clear way to choose which
     target to edit, for example a small popup with `Edit rotary` and `Edit push`.
4. Keep tooltips accurate.
   - Show rotary bindings for the visible rotary.
   - Include push-target bindings when `PressTarget` differs from the visible widget,
     clearly marked as push/click bindings.

### Acceptance

- Clicking `RotaryBig` sends `WidgetPressDown/WidgetPressUp` for `RotaryBigPush`,
  not `RotaryBig`.
- Scrolling `RotaryBig` still sends `WidgetScroll` for `RotaryBig`.
- Relative Increase/Decrease bindings no longer fire from mouse click.
- Button-like round widgets, such as `Play # Shape=Round`, still click normally.

## Phase 5: Generalize Value Feedback for Faders and Rotaries

Goal: make OSK value display work for rotary and fader roles without shape checks.

### Work

1. Rename and broaden C++ fader helpers.
   - Replace fader-only checks like `IsOskFaderValueAction()` with a value-capable
     helper such as `IsOskContinuousValueAction()`.
   - Use it for faders and rotaries when publishing state.
2. Publish normalized GUI values where possible.
   - Prefer `Action::GetCurrentNormalizedValue(context)` for continuous value
     actions.
   - Use `widget->GetLastFeedbackValue()` as fallback.
   - Keep command-value mapping inside C++ when Lua sends `WidgetValue`; Lua should
     not need to know whether an action is dB, pan percent, or FX normalized.
3. Preserve local shadow behavior.
   - Keep local value shadows for actively dragged faders.
   - Add a small equivalent for absolute rotary drag only if absolute rotary drag is
     implemented.
4. Track color remains available.
   - Reuse existing track-color fallback for both faders and rotaries when the action
     can resolve a track.

### Acceptance

- Faders without `Shape=Fader` still show current value and track color.
- Rotaries can show current normalized value when feedback/current action value is
  available.
- Existing fader drag and scroll behavior stays intact.

## Phase 6: Add Rotary Renderers

Goal: make rotary widgets visually communicate value state.

### Work

1. Add `DrawRotaryControl()` in `osk_render.lua`.
   - Use `Role=rotary` rather than `Shape=Round`.
   - Respect `Width`, `Height`, `Top`, `Color`, `Label`, and `Group`.
   - Use `RotaryStyle=Dot` or `RotaryStyle=Wiper`.
2. Dot style.
   - Draw a circular body.
   - Draw a small indicator dot positioned by normalized value.
   - Good default for simple encoder feedback.
3. Wiper style.
   - Draw a base ring.
   - Draw a colored arc from the minimum angle to the current value.
   - Optionally draw an inner cap/body with the dimmed track color.
4. Use sensible defaults.
   - Default rotary angle range should avoid a full 360-degree ring, for example
     225 to -45 degrees.
   - Missing feedback should draw a neutral inactive indicator.
   - Toggle/on state may tint the body, but continuous value should drive the dot or
     arc.
5. Keep labels readable.
   - Place label under the rotary by default for square controls.
   - Allow current compact wrapped text behavior for small controls.

### Acceptance

- `Role=rotary` draws as a rotary even when `Shape` is omitted.
- `Shape=Round` button widgets still draw as buttons when their role is button.
- Dot and wiper rotary styles both display feedback value.

## Phase 7: Config Editor and Tooltip Follow-Through

Goal: keep editing and discovery usable after one visible control can own multiple
semantic targets.

### Work

1. Update hover tooltips.
   - Show visible rotary scroll bindings.
   - Show paired push bindings when a separate `PressTarget` exists.
   - Keep `+ ` prefixes for modifier/pseudo-modifier lines.
2. Update right-click config.
   - If only one target exists, open it directly.
   - If rotary and push targets both exist, let the user pick the target before
     opening the editor.
3. Update label resolution.
   - Visible rotary label should come from the visible rotary by default.
   - Push-target labels should not override the visible rotary label unless an
     explicit OSK label property requests it.
4. Preserve hidden widget behavior.
   - Hidden push widgets should not appear as separate cells.
   - Hidden push widgets should remain editable through the visible paired rotary.

### Acceptance

- Hovering a pushable rotary exposes both rotary and push meanings.
- Right-click editing can reach both the visible rotary and hidden push target.
- Existing single-target buttons and faders keep current config behavior.

## Phase 8: Documentation and Verification

Goal: make the new contract hard to regress.

### Work

1. Update local DOX contracts.
   - `Scripts/CSI/AGENTS.md`: Lua must use widget metadata before shape/name
     heuristics.
   - `src/controls/AGENTS.md`: C++ must publish OSK semantic metadata with layout
     cells.
2. Update `LUA_CPP_EXTSTATE_INTERFACE.md`.
   - Document the new layout metadata keys.
   - Document target-routing rules for press, scroll, value, and touch commands.
3. Add parser self-checks.
   - Layout parser handles role/input/feedback/targets.
   - Old layout strings still parse.
4. Manual REAPER checks.
   - Fader with `Fader14Bit` and no `Shape=Fader` renders and controls as a fader.
   - Rotary with `RotaryWidgetClass` and no `Shape=Round` renders as a rotary.
   - Pushable rotary scroll sends relative input to the rotary widget.
   - Pushable rotary click sends press input to the hidden push widget.
   - Round Play button remains a normal push button.
   - Rotary dot and wiper styles show current feedback value.
   - Right-click config can edit both rotary and push target.
   - Existing fader feedback, track color, wheel, touch, and drag behavior still work.

## Suggested Implementation Order

1. Add C++ widget metadata and populate it during MIDI/OSC parsing.
2. Extend OSK layout publishing with role/input/feedback/target fields.
3. Extend Lua layout parsing and semantic helpers.
4. Route rotary input using semantic targets.
5. Generalize value feedback publishing beyond shape-defined faders.
6. Add rotary dot/wiper renderers.
7. Update config editor, tooltips, docs, and parser checks.

Each phase should build cleanly and remain manually testable before starting the next
phase.

## Compatibility Notes

- Existing `Shape=Fader` and `Shape=Round` hints should keep working.
- Existing layouts without new metadata should continue through Lua fallback
  heuristics during the transition.
- New behavior should not require users to define `Shape` solely to make OSK input
  work.
- `OSKHidden` should continue hiding cells, but hidden widgets can still serve as
  semantic targets for visible grouped controls.

