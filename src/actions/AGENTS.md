# Actions Guide

## Purpose

- Define the actions that bind CSI widgets and zones to REAPER behavior and feedback.

## Ownership

- The `Action` interface and action name/type registry contract.
- `ActionContext`, value transforms, timing, colors, and common action helpers.
- Track, transport, FX, send/receive, display, meter, and manager action implementations.

## Local Contracts

- Action names are user-facing configuration keys used by zone files, Lua action search, and documentation.
- New action types must be represented in `ACTION_TYPE_LIST`, registered by the integrator, and documented when user-visible.
- Declare a confirmed normalized action shape with `GetFeedbackShape()` and the matching `//! @feedback_shape` value. Leave dynamic, raw-unit, text, state, color, and semantically unknown actions without a shape.
- `Do`, `RequestUpdate`, `Touch`, and normalized-value behavior must agree for each action.
- Stateless `Reaper` actions with explicit action-line colors must still provide inactive color feedback unless the mapping explicitly uses `Feedback=No` or `NoFeedback`.
- Action feedback must use the owning `ActionContext` and widget contracts rather than bypassing them.
- An explicit action-line `OSD` value is authoritative event text. Queue it after the action runs and mark it as explicit instead of replacing it with automatic action-specific text. Lua owns template variable expansion.
- `SetDebugLevel` and `CycleDebugLevel` must persist the canonical Product `DebugLevel` setting before changing the active runtime level. They must not create a session-only logging override.

## Work Guidance

- Put an action in the narrowest existing category header.
- Reuse `action_base.h` and `action_helpers.h` patterns before adding another abstraction.
- Preserve release handling, modifier behavior, soft takeover, and feedback semantics when refactoring.

## Verification

- Build the plugin.
- In REAPER, exercise the changed action from a zone mapping and verify input, feedback, modifiers, and release/touch behavior as applicable.
- Check user-facing action names against `Wiki/Actions-Reference.md` and related wiki/reference pages.

## Child DOX Index

- None.
