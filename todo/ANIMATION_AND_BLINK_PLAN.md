# Animation and Blink Plan

## Current Base

- ✅ Format 2 separates the blink condition from the animation preset: `Blink=Active|Inactive|Always Animation=<preset>`.
- ✅ Built-in presets are `BlinkFast`, `BlinkNormal`, `BlinkSlow`, `BlinkDouble`, and `BlinkHeartbeat`.
- ✅ Product and Device settings control the Fast, Normal, and Slow intervals.
- ✅ `Feedback=None` can use `Blink=Always`, while `Blink=Active` and `Blink=Inactive` follow the resolved feedback state.
- ✅ The Lua widget editor uses choice buttons and migrates the unpublished numeric blink format to named presets.

## Remaining Work

### [ ] 1. Shared preset metadata

- Put preset names, frame sequences, timing references, and UI labels in one shared metadata source.
- Make C++, Lua, and the Bun editor read the same preset list instead of keeping separate hard-coded lists.
- Keep clear validation messages when a preset is missing or invalid.

### [ ] 2. User presets

- Define the user preset file format and its location in the product configuration directory.
- Define name conflicts, invalid files, reload behavior, and what happens when a Zone refers to a removed preset.
- Support about ten user presets without requiring changes to the C++, Lua, or Bun code.

### [ ] 3. RGB animation frames

- Extend an animation frame from an on/off value to an optional color and brightness value.
- Keep binary buttons on the same preset system by reducing each frame to on or off.
- Add a small built-in RGB preset set only after the supported hardware output rules are defined.

### [ ] 4. Editor and preview

- Show built-in and user presets in the Lua editor with a short preview.
- Show whether timing comes from Product settings, Device settings, or the preset itself.
- Keep `BlinkIntervalMs` as migration input only and always save the named preset syntax.

### [ ] 5. Verification and documentation

- Verify `Active`, `Inactive`, and `Always` during feedback changes, Zone changes, reload, and device reconnect.
- Verify normal actions, actions with `Feedback=None`, modifiers, binary LEDs, and RGB widgets.
- Document the final preset file format and user examples after the contract is stable.

## Related Current Code

- [C++ animation registry](../src/controls/animation_registry.cpp)
- [Lua widget editor](../Scripts/osk_config_view.lua)
- [Shared timing settings](../Scripts/settings_schema.conf)
- [Lua and C++ interface contract](../docs/LUA_CPP_EXTSTATE_INTERFACE.md)
