# Pseudo-Modifier Plan

## Goal

Allow a widget to become a named modifier only inside zones that declare it:

```text
SomeButton PseudoModifier
SomeButton+Play Play
SomeButton+Stop Stop
Shift+SomeButton+Record Record
```

The modifier name comes from the declaring widget. It does not use a fixed name such as `Pseudo1` and does not replace standard modifiers such as `Shift`.

## Runtime Contract

- `PseudoModifier` is a user-facing action with no required parameter.
- Pseudo-modifier declarations and state are scoped to the owning zone.
- The same widget can be a normal control in every other zone.
- Standard and pseudo-modifiers can be combined in one binding.
- Zone deactivation and reload clear all pseudo-modifier state owned by that zone.
- Standard modifier names and parser keywords are reserved and cannot be pseudo-modifier names.

## Work Plan

### 1. Parser and binding keys

- Parse each zone in two passes so declarations can appear before or after their bindings.
- Replace the integer-only binding key with a structured key containing the standard modifier mask and a normalized set of pseudo-modifier names.
- Validate unknown, duplicate, reserved, and self-conflicting pseudo-modifier names.

### 2. Zone-local state

- Store pressed and locked state in the owning zone, with the source widget recorded.
- Recalculate only the affected zone contexts when pseudo-modifier state changes.
- Keep pseudo-modifier feedback and cleanup separate from physical standard modifier state.

### 3. Editors and serialization

- Add `PseudoModifier` to action discovery and documentation.
- Preserve named pseudo-modifiers in zone serialization, OSK tooltips, and the Bun editor.
- Validate imported snippets and zones against the target surface widgets.

### 4. Latch design checkpoint

Before implementation, compare momentary-only and latch-capable behavior for:

- quick tap and long hold
- lost release events
- multiple pseudo-modifiers held together
- zone changes while a pseudo-modifier is active
- feedback for pressed and locked states

Select and document the latch behavior before runtime code changes begin.

## Validation

- Use the same widget as a pseudo-modifier in one zone and a normal action in another.
- Combine a pseudo-modifier with each supported standard modifier.
- Verify cleanup on zone leave, reload, editor revert, and malformed configuration.
- Verify that physical modifier state is never cleared by pseudo-modifier cleanup.
