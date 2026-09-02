# ReaControlSurface Conf Editor Guide

## Purpose

- Define and validate product configuration, surface, zone, and functional snippet formats.
- Provide the lossless core, local browser editor, safe file service, and standalone compile workflow.

## Ownership

- Bun package and TypeScript configuration in this directory.
- Lossless source model, tokenization, parsers, semantic views, and diagnostics under `src/`.
- Runtime action catalog generation from C++ registry and documentation source.
- REAPER data path discovery, contained configuration access, temporary recovery drafts, safe saves, transactions, backups, rollback, and operation reports.
- Read-only legacy CSI discovery, dependency preview, conflict resolution, and transactional import.
- Loopback HTTP API under `src/`, browser asset sources and CodeMirror configuration language support under `src/ui/`, and their loader and browser bundle entry in `src/ui.ts`.
- Standalone executable generation under `scripts/`.
- Format examples and malformed input under `fixtures/`.
- Parser and validator checks under `tests/`.

## Local Contracts

- Preserve original line text, line endings, comments, whitespace, unsupported properties, and unknown lines.
- Unknown data produces a warning and remains serializable. Syntax or unsafe identifiers produce errors.
- The product `.conf` uses unversioned brace blocks. Format 2 Surface files use `@Meta { Version=2 ... }`; current Zone files still use `// @format zone 1` until their format 2 migration is implemented.
- Treat only `//` as a comment in current Surface and Zone files. Report a leading single `/` or unsupported `#` line as an error with a quick fix. Keep the exact `#WidgetType`, `#DisplayRow`, `#RingStyle`, `#DisplayFont`, and `#SupportsColor` Learn FX directives as metadata.
- Functional snippets use semantic bindings and must not store fixed hardware widget names.
- Functional snippet `Role`, `Input`, and `Feedback` requirements use the same capability rules as runtime OSK metadata. Formal layout metadata overrides inferred surface metadata when present.
- Resolve an exact compatible semantic-slot and widget-name match automatically. Let legacy Widget mapping accept a compatible widget from its suggestions or a manual modifier-plus-widget expression whose final widget exists and has the required capabilities. Require explicit confirmation for a different manual choice. Allow an incompatible choice only after showing its role or capability mismatch and receiving an explicit override.
- Keep the browser editor task-based with only `Edit configuration` and `Import old CSI` on the main page. Keep the REAPER data path control above these tasks, and use the compact task title, task instructions, and SVG back arrow inside each workflow.
- Generate resolved snippets only for writable user zones. Use stable application IDs and the `// @snippet Application=<id> Source=<snippet-id>` and matching `// @snippet-end Application=<id>` markers.
- Show the snippet selector only while a writable user zone is open in the text editor. Resolve its surface from the zone profile and the main product config, and show the mapping dialog only when surface selection, manual widget confirmation, or an application conflict needs input.
- Insert a new resolved snippet block after the current cursor line. Insert it before `ZoneEnd` when the cursor is on line 1 or outside the zone body. Change only the current unsaved draft.
- Require `Replace`, `Rename`, or `Skip` when an application marker already exists.
- Keep CodeMirror text mode as the only configuration editing mode. Constrain it to the available viewport and keep its own scrolling. Show Problems and File details as collapsible bottom tabs with an open height of 20 percent of the viewport.
- Keep browser navigation in the History API query route. Use `view` for the task, `file` for an open configuration file, `line` for diagnostic navigation, and `panel` for the bottom panel. Restore these values on reload and Back or Forward without putting the REAPER data path, session token, dialogs, or temporary form state in the URL. Remember the last successfully opened REAPER data path in the server-side per-user Conf Editor settings, not in browser storage.
- Keep Current file problems separate from the collapsible All found index. Update the index when a file opens, retain other indexed files during navigation, and let Check all replace it with complete file and cross-file validation results that include unsaved drafts.
- Report a structural `IncludedZones` or `SubZones` dependency cycle on the exact dependency line that closes it. Do not treat `GoZone` or `GoSubZone` navigation return paths as dependency cycles. Offer a quick fix that comments out only the structural dependency line in the unsaved configuration or legacy import draft, then revalidate the legacy preview.
- Render diagnostic messages as selectable text. Use separate navigation links for primary and related file and line targets, and separate quick-fix links for writable sources.
- Register quick fixes in the TypeScript quick-fix registry. Give every fix a stable ID, calculate its available variants from the current diagnostic and source, validate it again before applying it, and change only the unsaved draft.
- Suggest no more than three runtime action replacements whose case-insensitive normalized Levenshtein distance is at most `0.4`. Keep each suggestion as a separate quick fix in current Zone files and legacy import drafts.
- Add every writable text change to Save all automatically. Preserve each unsaved file as one debounced temporary draft keyed by the SHA-256 of its full logical path, restore it across browser and editor restarts, mark it with bold text and `*` in the tree, and remove it after a successful save or a return to the saved source.
- Let Discard changes restore the current file from disk and remove its recovery draft only after confirmation.
- Reject automatic recovery when the source hash changed after draft creation. Require an explicit Use draft or Discard draft choice.
- Show field errors directly below their related path controls with semantic `danger` styling. Show other operation messages as dismissible notifications outside the workflow header. Route structured validation details into All found and keep notifications short instead of printing diagnostic JSON. Remove success notifications after five seconds, and keep info, warning, and danger notifications until the user closes them. Use `primary`, `secondary`, `success`, `warning`, `danger`, and `info` for visual state names, and keep operation reports inside their workflow instead of a global status footer.
- Read action names from `src/shared/types.h` `ACTION_TYPE_LIST`. Do not add a manual action-name list.
- Read setting metadata from `Scripts/settings_schema.conf`. Embed the parsed schema in standalone builds and do not add a separate TypeScript setting list. Read user-selected values from the product `.conf`, not from the schema.
- Read format 2 Surface primitive and representation metadata from `Scripts/surface_io_schema.conf`. Keep the public legacy Surface processor coverage report tied to explicit converter targets from that catalog. Count processors only inside valid legacy Widget blocks, report malformed block boundaries separately, show processors waiting for an approved runtime as planned, and treat every other unclassified processor as incomplete migration work.
- Parse Product values from the root `Settings` block and Device values from nested `Device Settings` blocks. Validate both scopes atomically with the shared schema and inherited cross-setting constraints.
- Require positive `Channels` in Surface `@Meta`, omit it from product Device blocks and I/O forms, and report when one Device is assigned to Surface templates with different channel counts.
- Keep parsers independent from the browser UI and file-writing service.
- Bind the editor server only to `127.0.0.1` and require a random session token for every API request. Deliver the token in the generated initial HTML, not in the URL or persistent browser storage.
- Let the user select only the REAPER `Data` directory. Derive the internal product configuration folder from product identity before file access.
- Open the first valid discovered REAPER `Data` directory automatically. Derive the old CSI default as the sibling `CSI` directory and keep manual path selection as an advanced fallback.
- Reject absolute child paths, `..`, and unsupported logical locations. Follow configuration file and directory links while applying ownership and write permissions to their logical paths.
- Keep all fixed user-interface text in the typed English catalog in `src/i18n.ts`. Derive `TranslationKey` from this catalog, type new locale files as `TranslationOverrides`, and use English values as the fallback for missing locale entries.
- Resolve ordinary `{{translation.key}}` HTML placeholders directly from the English catalog. Keep only placeholders with runtime parameters or non-translation values as explicit template overrides, and reject unknown placeholders.
- Write only the product config and user-owned surface, zone, and snippet paths. Copy Vendor Main as one directory before editing it. Copy only the selected Vendor FX file to its matching User path. Check all must select User Main when present and validate Vendor and User FX zones as layers where an exact User `Zone` name overrides Vendor.
- Keep a selected legacy CSI source separate from the writable product-root guard. Accept the legacy `CSI/` directory or its parent, follow source links, and never modify source files.
- Keep a visible old CSI reload action on the import page. Re-read surface folders and source files while preserving the selected surface, selected zones, target choices, conflict choices, and in-memory import drafts. Keep the prior preview and drafts if a changed source cannot be refreshed safely.
- Keep the legacy import workspace in two columns. Put a folder tree with independent file-open buttons and import-selection checkboxes beside the import draft editor, with its Problems panel below. Put source selection and resolution in two compact accordion steps in the right column. Stack the columns on narrow screens.
- Keep the legacy Import action in a permanently visible footer below the right-column steps. Show one exact blocking reason beside it: missing surface or file selection, unresolved widget mappings, validation errors, or unresolved target conflicts. Highlight every unresolved target conflict with danger styling, name the existing target path, show explicit Replace, Rename, and Skip actions, and make the footer reason navigate to the first conflict.
- Validate selected legacy zone references against other selected zones and active zones already present in the target profile. When a matching legacy zone is not selected and no active target exists, name that state and link to the matching source file.
- Keep one legacy FX-zone selection checkbox that selects or clears all `FXZones/` sources without changing the selection of ordinary zones.
- Import legacy `Surface.txt` into `Surfaces/User`, `Zones/**/*.zon` into the matching `Zones/User/<profile>/Main` tree, and `FXZones/**/*.zon` into its `FX` tree. Let the user change the stable target profile ID and each zone path inside that user profile. Reject duplicate selected targets. Ignore backup names that do not end in `.zon`, add missing surface and zone format markers, and preserve zone subdirectories. Convert leading single-slash and unsupported hash comment lines to `//`, and preserve exact Learn FX directives with their `#` prefix.
- Convert imported legacy Surface files to format 2 instead of adding a format 1 marker. When no explicit OSK layout exists, generate an editable layout from usable Input widgets. Reserve seven rows for each fader and fill the remaining cells in stable source order. Report unsupported legacy processors as blocking diagnostics instead of preserving old processor lines.
- Convert legacy `FB_Encoder` to a shared `RotaryRing` profile and explicit `Feedback Ring` output. Preserve its historical output-address offset and RingStyle encoding without retaining a device-specific processor name.
- Convert generic legacy OSC `Control` to OSCFloat Input Value. Split `FB_Processor` into OSCFloat Value, OSCString Text, and OSCString HexRGBA Color feedback at the explicit legacy address and `<address>/Color`. Do not infer one feedback type from Widget names or selected zones.
- Convert legacy FaderPort value bars to one shared `BarProfile` and explicit value plus style messages. Convert state-scaled RGB to MIDIRGB brightness metadata. Convert MFT palette output to one generated ColorProfile and MIDIPalette Companion output. Treat command-shaped RGB values as unresolved only when their resolved target Widget uses that palette output.
- Convert legacy MCU, MCUXT, and C4 upper and lower displays to one shared seven-character TextProfile and constant-prefix MIDISysEx Text payloads. Encode row, channel, and upper/lower offsets as literal Surface data instead of adding device branches to the runtime.
- Convert legacy MCU and MCUXT meters to one Surface MeterProfile, generic MIDI7 Meter feedback, and a Surface-level Initialize block. Infer the old MeterMode from the legacy zones, preserve its scale, and reject unknown or conflicting modes instead of choosing silently.
- Convert legacy dynamic text and OLED button displays to generic MIDISysEx Text Payload fields. Move the processor margins, font, and black color defaults into each Feedback block and keep the hardware address as a literal byte.
- Convert legacy SCE24 encoder feedback to a generic RingProfile and nested MIDISysEx Configure block. Expand `LEDRingColor`, `LEDRingColors`, and `PushColor` into one 18-entry `RingColors` value. Move a standalone RotaryPush `PushColor` to the paired Rotary binding, and reject invalid or overlapping ranges without changing that binding.
- Treat a legacy file named `GoZones.zon` with a `Zone GoZones` header as deprecated loading metadata, not as an imported zone. Move its navigator values into matching zone headers as `NavType` properties.
- Keep legacy import source editing in memory. Recheck the original source hash before every preview and import, and never write an import draft back to the old CSI directory.
- Resolve missing or incompatible legacy zone widgets through capability-filtered target widget mappings. Preserve `Widget|` channel-family semantics and map them only to another compatible channel family. Treat `Touch` in `Touch+Widget` as a modifier, not as a required touch-input capability on `Widget`. Apply mappings to preview source before source hashes and transactional saves are calculated.
- Require explicit `Rename`, `Replace`, or `Skip` decisions for existing import targets. Recheck source and target hashes before one validated multi-file transaction.
- Show the complete legacy import operation report with changed, created, failed, restored, and skipped paths.
- Render actionable diagnostics as links. Open the related editor, move the cursor to the reported line, center it in the viewport, and focus the CodeMirror editor.
- Render each legacy widget-mapping occurrence as a link that opens its source file and line in Import draft.
- Show the selected file in the configuration tree with an accent background and accent edge.
- Check the open-file SHA-256 before save. Stage and validate complete files before atomic rename.
- Back up and roll back multi-file operations and keep their manifest below the product `Backups` directory.

## Work Guidance

- Use Bun-compatible TypeScript and Node standard-library APIs. Use CodeMirror only for browser text editing and bundle it locally without a CDN. Do not add another package dependency when the standard library or the existing editor dependencies are sufficient.
- Keep parsers line-oriented until a format feature requires a more complex grammar.
- Add a valid and malformed fixture when a public grammar rule changes.
- Keep browser assets embedded so compiled executables do not need adjacent UI files.
- Keep HTML, CSS, and browser JavaScript in their native file types under `src/ui/`. Bundle the browser JavaScript and CodeMirror dependencies through `src/ui.ts` so standalone builds still embed them.
- Generate standalone identity and action data from repository contracts during compile. Do not source-control generated compile input or `dist/` output.
- Build tagged standalone release archives and raw executables for Windows x64, macOS Intel, macOS ARM, and Linux x64 independently from the C++ build. Publish raw executables through the separate platform-specific ReaPack Configuration Editor package, not through the Core package.

## Verification

- Run `bun test` from this directory.
- Run `bun run validate -- fixtures/valid` and confirm there are no errors.
- Run `bun run validate -- fixtures/invalid` and confirm the command reports errors.
- Run `bun run actions` and compare the catalog count with `ACTION_TYPE_LIST`.
- Run focused store tests for conflicts, rollback, cloning, symlink traversal, and directory link cycles.
- Run focused legacy import tests for source discovery, `Zones` and `FXZones` mapping, backup-file filtering, dependency selection, semantic widget mapping, repeat conflicts, source immutability, and transactional writes.
- Run `bun run surface-coverage` and review every unresolved public legacy Surface processor before declaring migration coverage complete.
- Run focused snippet workflow tests for semantic widget filtering, surface resolution, explicit confirmation, mismatch override, cursor insertion, and repeat application conflicts.
- Run focused temporary draft tests for path isolation, recovery, discard, source-hash conflicts, and save cleanup.
- Run focused validation-set and quick-fix registry tests for draft overlays, cross-file diagnostics, writable ownership, stale diagnostics, and lossless source changes.
- Start the server with `--no-open` and confirm unauthenticated API access fails.
- Build and launch each standalone target in its matching operating system before release.

## Child DOX Index

- None.
