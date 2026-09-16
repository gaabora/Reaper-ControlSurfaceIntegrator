# [ ] Legacy Import Editor and Conversion Plan

## Goal and status

Make old CSI import easier to review and correct. Preserve source text that needs attention, keep repeated Widget declarations easy to compare, and separate source preparation from result editing.

This document records the agreed design. Implementation is not complete. It extends [CONFIGURATION_WORKFLOW_PLAN.md](CONFIGURATION_WORKFLOW_PLAN.md). Runtime device diagnostics remain in [IMPORT_AND_DEVICE_DIAGNOSTICS_PLAN.md](IMPORT_AND_DEVICE_DIAGNOSTICS_PLAN.md).

## Agreed workflow

Each file has two stages. Store its stage with its drafts and restore it after a browser or editor restart. Original files in the old CSI directory remain unchanged.

| Stage | Left editor | Right editor | Conversion |
| --- | --- | --- | --- |
| Prepare source | Editable old-format draft | Read-only converted preview | Update after source changes |
| Edit result | Read-only old-format draft | Editable converted import draft | No automatic conversion |

The user fixes source problems or comments out unwanted source commands in Prepare source. When no blocking source or conversion errors remain, the user can select Move to result editing. Warnings do not block this transition. Errors that belong to the generated format can be corrected in Edit result and still block final import when required by validation.

Move to result editing captures the current converted preview as the result draft. Changes to the result must never start another conversion.

Back to source preparation makes the left editor editable again. If the result has manual changes, show a warning that conversion will reset those changes and require confirmation. Cancel leaves both drafts and the stage unchanged. Do not implement partial block regeneration or merging of source changes into a manually edited result.

## ✅ Stage 1: Preserve conversion content and compact Widget groups

- ✅ Preserve standalone and inline comments in Surface conversion, close to the related commands. Keep source order and comment order where conversion permits it.
- ✅ Preserve every unconverted or invalid command as `//! original line` in the result, with an explanation in Problems. Audit both Surface and Zone conversion paths for lines that are currently omitted.
- ✅ Keep unresolved conversion problems visible. An automatically generated `//!` marker must not silently turn failed conversion into success. An explicit user edit that comments out the unwanted command in the source draft must allow the conversion problem to clear after revalidation.
- ✅ Write adjacent numbered Widgets from the same family on consecutive lines when their structure matches and only names, addresses, or values differ. Preserve order; do not sort or gather Widgets from separate parts of the file.
- ✅ Allow one compact Widget to contain both Input and Feedback. Several commands alone do not require a multiline block.
- ✅ Keep multiline output for nested tables, long sets of parameters, or comments between commands when compact output would hide the structure. A preceding comment or trailing comment does not by itself prevent a compact Widget.
- ✅ Define the compact-output rule through representative fixtures before implementation. Do not insert file newlines only because the editor viewport is narrow; visual wrapping is separate from file formatting.
- ✅ Check all Surface readers used by the editor and runtime before emitting compact output. Ensure the Bun semantic model retains inline Widgets and their input and feedback capabilities.

Expected compact group:

```text
Widget TrackSelectB1 { Input Press { Encoding=OSCFloat Address="/-stat/selidx/16" Match=Any } }
Widget TrackSelectB2 { Input Press { Encoding=OSCFloat Address="/-stat/selidx/17" Match=Any } }
```

Acceptance: a numbered family is easy to compare line by line. Comments survive conversion. Unsupported source content remains visible. Compact and multiline forms describe the same Widgets and capabilities.

## ✅ Stage 2: Store source and result drafts separately

- ✅ Add a persistent old-format source draft, separate from the original file and the converted import draft. Reuse the existing recovery-draft service where practical.
- ✅ Save source edits after a debounce and regenerate the read-only preview. Ignore stale conversion responses when a newer source edit exists.
- ✅ Implement the two stages and transition rules above for each file. A completed Surface must not force all Zones into result editing.
- ✅ Save and restore both drafts, the current stage, and the source identity needed for existing source-change checks. Do not recover a draft against a changed original without the existing explicit conflict choice.
- ✅ Keep manually edited result drafts fixed during preview refreshes, navigation, source reload, and edits to other files. Revalidate dependencies without overwriting those results.
- ✅ Require selected files to reach result editing before final import. Continue to validate the complete selected set and destination conflicts before writing.
- ✅ Make discard controls state which draft they affect. Do not silently discard a source draft when discarding result edits.

Acceptance: source edits update the preview only in the first stage. Result edits survive navigation and restart. Returning to source preparation warns before losing manual result changes. No draft write changes the old CSI files.

## [ ] Stage 3: Add comparison and adjustable panels

- [ ] Add Show source to switch between result-only view and two editors, with the source draft on the left and the converted result on the right.
- [ ] Show the current stage and read-only state in the editor headings. Keep Move to result editing and Back to source preparation easy to find.
- [ ] Add a draggable width divider between the editors.
- [ ] Make the left file tree and right steps sidebar resizable and collapsible as complete panels. Keep an accessible way to reopen each panel.
- [ ] Keep useful minimum widths and editor scrolling when space is limited. Provide keyboard controls for width changes as well as pointer dragging.
- [ ] Show the original source path, intended import destination, and actual recovery-draft folder as separate values. Identify stored JSON recovery records as drafts, not ready-to-use configuration files. Get their locations from the server rather than constructing paths in the browser.

Acceptance: the user can compare a source and result, give more width to either editor, hide either sidebar, and find the draft location without confusing it with the final import destination.

## [ ] Stage 4: Correct Problems locations and linked navigation

- [ ] Separate source problems, conversion problems, and result problems. Label the file and version to which each problem belongs.
- [ ] Open the correct editor and line from each diagnostic. Reveal the source panel when navigating to a source problem. Do not use an original Surface line number as a result line number.
- [ ] Record source-to-result block and line relationships during conversion, including one-to-many output and retained `//!` lines.
- [ ] Add Show related block in both directions. Use the conversion relationships rather than equal line numbers or matching text alone. Do not require continuous synchronized scrolling.
- [ ] Adjust result locations after manual edits where the editor can retain the relationship. If an edited or removed block no longer has a reliable match, show that no exact match remains instead of navigating to an unrelated line.
- [ ] Keep diagnostic fixes and editor permissions consistent with the current stage. A result fix must not edit a read-only preview or silently change the source draft.

Acceptance: clicking a problem in Surface.txt reaches the text that caused it. Related-block navigation still works for unchanged blocks after edits elsewhere. Lost relationships never produce a false exact location.

## [ ] Stage 5: Verification and documentation

- [ ] Add focused conversion fixtures for numbered families, Input plus Feedback, nested blocks, inline comments, comments between commands, malformed source, and unsupported commands. Include a family of 32 Widgets.
- [ ] Add focused checks for compact Surface parsing and capability discovery, source/result diagnostic locations, and one-to-many conversion relationships.
- [ ] Check both stage transitions, cancelled return to source, restart recovery, stale preview responses, original-file conflicts, and dependency revalidation without overwriting manual results.
- [ ] Check the browser flow with Surface, ordinary Zone, FX Zone, and Learn FX sources where supported. Verify resize controls, sidebar reopening, long paths, and diagnostic navigation in both stages.
- [ ] Run builds and tests only after explicit user permission. Record actual results separately from planned checks.
- [ ] Update the editor documentation and its owning AGENTS.md when the workflow is implemented. Mark completed plan items with the repository completion convention.

Acceptance: the implemented workflow preserves original files and manual result edits, restores drafts, and gives correct file and line navigation. Final import still uses the existing validated transaction and conflict decisions.

## Implementation references

- [Surface conversion](../tools/config-editor/src/legacy-surface-format2.ts), [Zone conversion](../tools/config-editor/src/legacy-zone-format2.ts), and [Surface parsing](../tools/config-editor/src/surface.ts).
- [Import workflow](../tools/config-editor/src/legacy-import.ts), [draft storage](../tools/config-editor/src/draft-store.ts), and [server API](../tools/config-editor/src/server.ts).
- [Browser workflow](../tools/config-editor/src/ui/app.js), [editor integration](../tools/config-editor/src/ui/code-editor.js), [page structure](../tools/config-editor/src/ui/index.html), [styles](../tools/config-editor/src/ui/app.css), and [UI text](../tools/config-editor/src/i18n.ts).
- [Editor contract](../tools/config-editor/AGENTS.md). Read the applicable contracts again before implementation.
