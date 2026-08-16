# Pseudo-Modifier Plan

## Goal

Allow a physical widget to become a named modifier only inside zones that explicitly declare it:

```text
SomeButton PseudoModifier
[SomeButton]+Play Play
[SomeButton]+Stop Stop
[Shift]+[SomeButton]+Record Record
```

The modifier name comes from the declaring physical widget. It does not use a fixed name such as `Pseudo1` and does not replace standard modifiers such as `Shift`.

This plan uses the explicit binding grammar, timing hierarchy, and validation contract in [ZONE_WIDGET_MODIFIER_VALIDATION.md](ZONE_WIDGET_MODIFIER_VALIDATION.md).

## Runtime Contract

- `PseudoModifier` is a user-facing declaration action with an optional `Mode` override.
- A declaration without `Mode` uses the effective `DefaultPseudoModifierMode`, whose compiled and initial product value is `Latch`.
- Pseudo-modifier declaration, state, feedback, timing, and cleanup are scoped to the owning Zone.
- The same physical widget can be a pseudo-modifier in one Zone and a normal control in another Zone.
- A pseudo-modifier is referenced only as a bracketed selector such as `[SomeButton]`.
- The unwrapped final name in a binding is always the physical target widget.
- Standard and pseudo-modifiers can be combined in one selector set.
- Zone deactivation, reload, live revert, and invalid configuration recovery clear all pseudo-modifier state owned by that Zone.
- Standard modifier names, state selector names, event names, transform names, and parser keywords are reserved and cannot be pseudo-modifier names.
- Pseudo-modifier state must not use or change the shared physical `Widget::isModifier_` flag.
- A release received after the owning Zone deactivates must not reactivate or mutate the old Zone state.

## Explicit Syntax

Declaration:

```text
SomeButton PseudoModifier
OtherButton PseudoModifier Mode=Momentary
```

The first declaration inherits `DefaultPseudoModifierMode`. The second declaration overrides the effective default only for `OtherButton`.

Use as a context selector:

```text
[SomeButton]+Play Play
[Shift]+[SomeButton]+Record Record
[SomeButton]+(Hold)+Play SomeAction
```

The last example means Hold the physical `Play` widget while `[SomeButton]` is active.

An event on the pseudo-modifier source widget names the source without brackets:

```text
(Hold)+SomeButton SomeAction
```

Do not use a pseudo-modifier name as a physical widget alias.

## Modifier Modes

### Momentary

`Mode=Momentary` engages on press and disengages on release. Hold and LongHold actions on the source widget are invalid because normal chord use requires the source button to remain pressed.

```text
SomeButton PseudoModifier Mode=Momentary
(Hold)+SomeButton SomeAction
```

The second line is an error.

### Latch

`Mode=Latch` uses tap-toggle behavior. One Tap engages the pseudo-modifier and the next Tap disengages it. Hold and LongHold on the source widget are allowed because latch state does not use press duration.

```text
SomeButton PseudoModifier
(Hold)+SomeButton SomeAction
```

With the default configuration, the declaration resolves to Latch. The Hold action does not change the latch state.

### Hybrid

`Mode=Hybrid` uses Tap to latch and a longer press for temporary engagement or unlock behavior. Hold and LongHold on the source widget are invalid because the modifier state machine already owns press duration.

Hybrid uses the effective `ModifierTapWindowMs` setting from the main validation and timing contract.

## Zone-Local State

Resolve the pseudo-modifier mode from lowest to highest priority:

```text
compiled fallback Latch < product DefaultPseudoModifierMode < configured Surface instance override < declaration Mode override
```

Changing a configured default must recalculate effective modes and revalidate all inheriting declarations before applying the change.

- Store the source widget, mode, pressed state, latched state, timestamps, and feedback state in the owning Zone.
- Recalculate only the affected Zone action contexts when pseudo-modifier state changes.
- Keep pseudo-modifier state separate from Page and Surface standard modifier state.
- Clear pressed and latched state before a context-changing action deactivates the owning Zone.
- Ignore a late release after cleanup, but record enough input ownership to avoid changing a new Zone that uses the same physical widget.
- Use the deterministic button gesture recognizer for Tap, Hold, LongHold, DoublePress, timeout, and lost-release recovery.

## Binding Key

Replace the integer-only binding key with a structured key containing:

- The standard modifier mask.
- A normalized set of zone-local pseudo-modifier names.
- `[Touch]` and `[Toggle]` channel state.
- The physical target widget.
- The recognized input event, direction, and transforms.

Selector order does not change identity. Preserve canonical order during serialization.

## Validation Rules

Report an error for:

- An unknown, duplicate, empty, or incorrectly wrapped pseudo-modifier selector.
- An invalid `Mode` override or effective `DefaultPseudoModifierMode`.
- A reserved, unsafe, or case-insensitively duplicate pseudo-modifier name.
- A pseudo-modifier declaration on a widget without two-state press and release input.
- A declaration that conflicts with a standard modifier declaration on the same physical widget and Zone context.
- Hold or LongHold on a Momentary or Hybrid source widget.
- A pseudo-modifier state that can survive owning Zone deactivation.
- A dependency or activation cycle caused by a pseudo-modifier action.

Report a warning for:

- DoublePress on a pseudo-modifier source because modifier behavior and DoublePress can be additive.
- More than one pseudo-modifier declared on one physical widget.
- A Latch source Hold action that also changes Zone because cleanup must happen before the transition.

## Cross-Component Contract

- C++ owns runtime state, timing resolution, gesture recognition, and validation.
- Lua displays and edits pseudo-modifier bindings without maintaining independent state.
- OSK labels and tooltips show bracketed pseudo-modifier selectors, the effective source modifier mode, and whether it is inherited or overridden.
- The Bun editor parses declarations before bindings and validates the complete selected profile.
- Snippet requirements can reference named pseudo-modifier slots only when the target Zone declares or creates them explicitly.
- The legacy converter maps supported old modifier aliases to explicit declarations and bracketed selectors. It must report ambiguous aliases instead of guessing.
- Generated metadata keeps mode names, timing fields, reserved words, and diagnostics aligned between C++, Lua, and TypeScript.

## Work Plan

### [ ] 1. Grammar and declarations

- ✅ Define `DefaultPseudoModifierMode=Latch` in the shared settings metadata source and expose it to C++, Lua, and TypeScript consumers.
- [ ] Define `PseudoModifier`, its optional `Mode=Momentary|Latch|Hybrid` override, and bracketed selector grammar.
- Parse each Zone in two passes so declarations can appear before or after their bindings.
- Remove physical widget alias behavior from the new grammar.

### [ ] 2. Structured keys and Zone state

- Replace the integer-only key with the normalized structured binding key.
- Store pseudo-modifier state in the owning Zone.
- Implement cleanup, late-release handling, and context recalculation.

### [ ] 3. Gesture modes and timing

- Connect Momentary, Latch, and Hybrid to the shared deterministic gesture recognizer.
- Enforce source-widget Hold and LongHold rules for each mode.
- Apply product global, configured Surface, declaration mode, and binding timing precedence.

### [ ] 4. Lua, OSK, Bun editor, and converter

- Add declarations and modes to action discovery, serialization, labels, tooltips, settings, snippets, and live apply.
- Add complete-profile parsing and validation to the Bun editor.
- Convert unambiguous legacy modifier aliases and report ambiguous cases.

### [ ] 5. Automated and manual verification

- Add C++ tests for press, release, Tap, Hold, LongHold, DoublePress, lost release, cleanup, and each modifier mode.
- Add Bun tests for grammar, normalization, complete-set validation, conversion, and diagnostics.
- Add Lua serialization and settings-command checks.
- Verify real hardware feedback, multi-surface state, zone changes, reload, and editor revert in REAPER.

## Acceptance Criteria

- [ ] Use the same physical widget as a pseudo-modifier in one Zone and a normal action in another Zone without state leakage.
- [ ] Combine a pseudo-modifier with every supported standard modifier and another pseudo-modifier.
- [ ] Preserve bracketed selectors through Zone load, OSK live edit, save, Bun edit, snippet application, and conversion.
- [ ] Clear all pseudo-modifier state on Zone leave, reload, revert, malformed configuration recovery, and lost release timeout.
- [ ] Keep standard Page or Surface modifier state unchanged during pseudo-modifier cleanup.
- [ ] Enforce Momentary, Latch, and Hybrid Hold rules consistently in C++, Lua, and the Bun editor.
- [ ] Revalidate inheriting declarations when `DefaultPseudoModifierMode` changes and preserve explicit declaration overrides.
