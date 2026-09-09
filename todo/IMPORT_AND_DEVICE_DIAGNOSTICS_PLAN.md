# Import and Device Diagnostics Plan

## Goal

Make imported profiles pass the same checks as runtime. Show why a configured device cannot run its actions without requiring debug logs. Let the user select which Zone profile sources can run.

Implementation and test runs are approved. The source modes use whole-zone replacement. Gesture input, modifier state changes, and explicit NoAction count as intentional handling; they must not produce an unhandled-input notice solely because no immediate action changes REAPER.

## Current implementation status

- ✅ Import conversion and validation changes pass the focused editor tests. They cover matching Bank contexts, ambiguous exits, empty relations, Home navigation, lifecycle arguments, required destinations, Home count, mixed formats, and final conflict decisions. Tests include Skip retaining an invalid destination, Rename producing a second Home, and a complete User import with an old legacy Vendor file still present.
- [ ] Explicit source modes, Devices Zone readiness, retained failure text, and per-message MIDI OSD are implemented. Debug build and all four C++ test targets pass. REAPER runtime checks remain.

## Confirmed problems

- [Zone loading](../src/controls/zone_manager.cpp) collects Vendor and User Main files, then rejects mixed legacy and format 2 files before it reports document diagnostics. An old Vendor profile can hide errors in imported User files.
- [Legacy conversion](../tools/config-editor/src/legacy-zone-format2.ts) preserves named legacy Bank arguments, can emit empty IncludedZones blocks, leaves GoZone Home unchanged, and converts LeaveSubZone without checking the caller's navigation context.
- [Editor Zone parsing](../tools/config-editor/src/zone.ts) and [set validation](../tools/config-editor/src/validation.ts) do not enforce all rules from the C++ Zone document and profile validators. A successful import is not proof that runtime accepts the profile.
- [Devices status](../src/controls/devices_protocol.cpp) reports a Surface as Active when its runtime object exists. It does not report successful Zone initialization separately.
- MIDI input and widget input logs prove receipt and recognition, not action execution. [MIDI routing](../src/controls/midi/midi_surface.cpp) and [Zone routing](../src/controls/zone_manager.cpp) must supply the result of processing to the status system.
- [OSD publication](../src/shared/daw_display.cpp) and [OSD display](../Scripts/osd_ui.lua) use one current message and an event ID. [The runtime loop](../src/controls/integrator.h) also keeps one pending OSD message. Repeated input cannot rely on that slot to count every event.

## Confirmed user requirements

- Show or refresh OSD for every MIDI message that no action handles, including repeated identical messages. Do not reduce this to one notice at startup or one notice per error.
- Keep the reason for a failed device visible in Devices until the failure is resolved.
- Show a clear next action and a file location when the cause is a configuration error.
- Keep received MIDI distinct from a loaded profile and an action that ran.
- Do not require users to remove Vendor files to use a complete imported User profile.

## [ ] Stage 1: Review explicit source modes

Proposed labels and behavior:

| UI label | Sources | Meaning of User files |
| --- | --- | --- |
| Vendor only | Vendor | User files do not participate |
| Vendor + User changes | Vendor and User | A User zone replaces the complete Vendor zone with the same case-insensitive ID; User can also add zones |
| User only | User | User provides the complete selected collection; Vendor does not participate |

Example: Vendor has Home, Transport, and Sends. User has Transport. Vendor + User changes loads Vendor Home, User Transport, and Vendor Sends. User only loads User Transport and reports the missing Home for a Main profile. It does not return to Vendor automatically.

The patch unit is one complete zone, not one line or one action binding. A changed User Transport zone does not receive later changes to Vendor Transport. Other Vendor zones still receive Vendor updates. Do not add a line-patch format.

- [ ] Approve the three labels and whole-zone replacement rule. Existing format 2 overlay rules continue to define Vendor + User changes; this proposal adds explicit selection around them.
- [ ] Prefer storing the mode with each configured profile selection in the product INI. Offer it beside the Main profile, and beside a separately selected FX profile. Inherit the Main selection when no separate FX selection is configured. Confirm this scope before choosing property names or changing the configuration schema.
- [ ] Select User only for a complete legacy import intended to replace a profile. Show the proposed selection in the import result and apply it only through an explicit assignment update. Importing files alone must not silently change existing assignments.
- [ ] Use Vendor only for a Vendor preset and Vendor + User changes when the user explicitly creates an override. Define the default for a missing saved mode before implementation; never infer the mode from directory existence.
- [ ] Resolve source selection before format, duplicate, role, reference, and runtime checks. Files in an excluded source cannot block the selected profile.
- [ ] In the combined mode, apply User replacement before checking effective runtime content. Report inactive source problems separately from failures in files that will run. Do not silently use Vendor when a selected User replacement is invalid.
- [ ] Reject an effective profile that still needs legacy files. Show which selected source requires conversion. Do not silently drop needed Vendor zones to make the check pass.
- [ ] Apply the same selection to LearnFX.fxzon. Keep Main and FX ID collections separate and preserve existing file ownership rules.
- [ ] Show the selected mode and each effective zone's source in the editor. Preview missing dependencies and Home errors before saving a mode change.
- [ ] Define the edit path for Vendor only: preview creation of the required User override and the mode change together. Define FX Learn behavior when the selected FX mode excludes User; do not report a saved User FX file as active in that mode.

Acceptance: all three modes select the documented files. An empty User directory does not change the mode. User only accepts a valid converted profile while an old Vendor copy remains on disk. Combined mode accepts valid replacements and reports legacy files that remain effective.

## [ ] Stage 2: Correct legacy conversion

- [ ] Convert GoZone Home to GoHome when the resolved destination has the Home role.
- [ ] Omit empty IncludedZones and ZoneLayers blocks while preserving their comments.
- [ ] Convert named legacy Bank arguments only when the new zone context has the same target and bank meaning. Validate amount syntax and range.
- [ ] Resolve Bank calls in layers against their parent contexts. LinkLock calls Bank SelectedTrackFXMenu from a Home layer; removing the target name alone changes behavior. Block import with an explanation if the old meaning has no supported conversion.
- [ ] Determine LeaveSubZone conversion from the complete navigation graph. Metronome is entered through GoZone in the reported profile; blindly adding Role=Layer would make the entry invalid. Present a concrete correction when intent is ambiguous.
- [ ] Check remaining converted action and modifier declarations, including declarations with feedback properties, against runtime rules. Do not stop after fixing the first reported errors.
- [ ] Keep unsupported or ambiguous conversions visible with a source file, line, explanation, and proposed next action. Never silently remove an action to obtain a valid profile.

Acceptance: faderportv2 and xtouchminimc either produce valid profiles with preserved action meaning or show exact blocking conversion issues before import.

## [ ] Stage 3: Align import and runtime validation

- [ ] Check the complete effective profile and selected Surface using the same source mode as runtime. Include converted drafts, retained destination files, and dependencies.
- [ ] Match C++ checks for action arguments, layer-only actions, role and target rules, empty relations, Home count, navigation targets, duplicate IDs, and widget compatibility. Include lifecycle actions.
- [ ] Repeat final validation after Replace, Rename, and Skip choices. Recheck file hashes before writing and keep the existing transaction and backup behavior.
- [ ] Make dependencies required by runtime blocking import errors. Do not treat a known but invalid destination zone as available.
- [ ] Add common valid and invalid examples for both C++ and TypeScript checks. Agreement means the same accept or reject result and equivalent source-linked reasons.
- [ ] Keep primary errors first and mark dependent failures as consequences when their cause is known. Collect useful errors even if a format mismatch is present.
- [ ] Distinguish Files imported and validated from Profile loaded by device. Show the second result only after runtime confirms it.

Acceptance: a profile accepted by import passes equivalent runtime configuration checks. Port failures and later external file changes remain separate runtime conditions with their own messages.

## [ ] Stage 4: Report device and input results

- [ ] Preserve structured initialization results instead of only logging and returning. Include device assignment, selected sources, failure stage, diagnostic code, file, line, and next action.
- [ ] Extend the Devices response and view with separate input/output open state, observed input, Surface load state, Zone load state, and action routing result. A Surface object alone must not imply readiness.
- [ ] Keep configured assignments visible when initialization fails, including failures before a usable Surface object exists.
- [ ] Show profile errors through a details action with selectable text and editor navigation. Keep the current issue visible until successful reload or removal of the assignment; closing an OSD notice must not erase it.
- [ ] Do not treat absence of recent MIDI activity as proof of disconnection. An action can be valid yet cause no visible change, for example Play when playback has already started.

Acceptance: with a failed Home zone, Devices shows that MIDI can arrive while the profile cannot run. A corrected and reloaded profile clears the stale failure without affecting other assignments.

## [ ] Stage 5: OSD on every unhandled MIDI message

- [ ] Carry an input result through raw MIDI decoding, widget recognition, active Zone selection, and action dispatch. Generate the notice after the relevant processing result is known, not from the presence of a debug log.
- [ ] Produce a diagnostic event for every unhandled message, including identical messages. No per-error suppression, cooldown, or notice-once rule.
- [ ] Refresh the current diagnostic OSD on each such event. Include device name, reason, raw bytes, and widget name when known. Include a monotonic event number or count so bursts remain visible even when the screen cannot draw each event separately. Do not create a backlog of delayed popups.
- [ ] Preserve every event in the producer's result/count before publishing the current display state. Do not claim that one ExtState slot or one render frame displays every message separately.
- [ ] Differentiate Unknown MIDI message, No action for this input in the current zone, and Profile not loaded. For profile failures, show the primary cause and the action to open Devices details.
- [ ] Publish configuration failure notices without depending on an action in the failed Home zone or its EnableOSD setting. Make the diagnostic display work when normal action OSD cannot start. Keep Devices available if the Lua OSD cannot open.
- [ ] Define priority between diagnostic OSD and normal action OSD so a routine message cannot immediately hide the reason for the unhandled event.
- [ ] Review the exact meaning of handled before implementation. Pending Hold/DoublePress recognition, release messages, modifier state updates, explicit NoAction, compound MIDI inputs, and protocol-only traffic need explicit decisions. Do not silently add exclusions from the user's every-message rule.
- ✅ Approved handling rule: input consumed by a valid gesture or modifier is pending or handled; explicit NoAction is an intentional assignment. A message that is truly unhandled always generates an event. Implementation and verification remain open.
- [ ] Define attribution when one physical input serves multiple assignments or listeners. Report the affected current assignment and avoid counting one raw message more than once for that assignment when several widget decoders participate.

Example display for a failed profile:

```text
fp2: Zone profile not loaded
MIDI received: 90 5e 7f. Widget: Play.
Home.zon: empty IncludedZones block.
Open Devices > fp2 > Errors.
```

Acceptance: repeated unhandled input refreshes OSD and advances the event count each time. Normal input cannot be called unhandled solely because it waits for a gesture decision. The final rule for intentional NoAction, release, modifiers, and protocol traffic matches the reviewed decision.

## [ ] Stage 6: Verification and documentation

- [ ] After approval to run checks, run focused C++ and TypeScript cases for all source modes, ignored sources, invalid replacements, import conflict choices, the reported conversion failures, and repeated input results.
- [ ] Verify in REAPER: old Vendor plus complete User import, failed Home with live MIDI, repaired profile reload, each source mode, repeated button messages, fader bursts, gesture timing, and unavailable OSD.
- [ ] Update the product configuration contract, Devices protocol, editor help, source ownership guides, and affected AGENTS.md files when behavior is implemented. Reconcile old whole-directory Main selection text with the approved explicit mode contract.
- [ ] Keep this plan linked from the configuration workflow and format 2 plans. Mark implemented steps only after their acceptance checks are complete.

## Related plans

- [Configuration workflow](CONFIGURATION_WORKFLOW_PLAN.md)
- [Format 2 zones](ZONE_FORMAT_V2_PLAN.md)
- [Widget and gesture validation](ZONE_WIDGET_MODIFIER_VALIDATION.md)
- [Control Panel](LUA_CONTROL_PANEL_PLAN.md)
