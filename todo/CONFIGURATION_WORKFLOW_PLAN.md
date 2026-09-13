# Configuration Workflow Plan

## Goal

Import correctness, proposed explicit Zone source modes, and runtime failure messages have an open follow-up in [IMPORT_AND_DEVICE_DIAGNOSTICS_PLAN.md](IMPORT_AND_DEVICE_DIAGNOSTICS_PLAN.md). Its source-mode proposal needs review before it changes the existing configuration contract.

Provide safe tools to create, edit, migrate, validate, and distribute surface, zone, and snippet configurations.

Large configuration changes belong in a local Bun and TypeScript editor. The Lua OSK remains a live fine-tuning tool for MIDI surfaces and gains only small zone-file creation support.

## Fixed Decisions

- Use `<ProductName>` and `<ProductRoot>` until the fork has its final product identity.
- Keep runtime names, paths, ExtState sections, script paths, and package names in one product identity manifest that Lua reads directly and CMake consumes.
- Keep OSK layout data in a formal block inside the `<surface-id>.txt` file.
- Support OSK layouts for MIDI surfaces only. Do not add OSK mirroring for OSC surfaces in this plan.
- Use the Bun editor for full surface, zone, snippet, legacy import, and batch workflows.
- Keep Lua OSK focused on live behavior editing and small zone-file creation.
- Treat old CSI installations as read-only import sources. Do not add legacy CSI path fallback to the runtime plugin.
- Keep pseudo-modifier work independent in [PSEUDO_MODIFIER_PLAN.md](PSEUDO_MODIFIER_PLAN.md).

## Target Structure

`<ProductRoot>` is `<REAPER resource>/Data/<ProductResourceDirectory>` so ReaPack data packages can install the configuration with their standard target root.

```text
<ProductRoot>/
  <ProductConfigFile>
  Surfaces/
    Vendor/<surface-id>.txt
    User/<surface-id>.txt
  Zones/
    Vendor/<profile-id>/
      Main/*.zon
      FX/*.zon
    User/<profile-id>/
      Main/*.zon
      FX/*.zon
  Snippets/
    BuiltIn/*.snippet
    User/*.snippet
  Backups/<operation-id>/
    manifest.json
  Generated/
    ZoneRawFXFiles/*.txt
```

- ReaPack owns `Surfaces/Vendor`, `Zones/Vendor`, and `Snippets/BuiltIn`.
- Users own `Surfaces/User`, `Zones/User`, and `Snippets/User`.
- The editor copies a Vendor surface before editing it, copies only Vendor Main before editing Main, and copies only the selected Vendor FX file before editing it.
- OSK requests confirmation before copying Vendor Main or one Vendor FX file. FX Learn and new FX zones write directly to User FX.
- A User surface with the same ID overrides Vendor. User Main is selected as a complete layer when present. Vendor and User FX load together, with an exact User `Zone` name overriding Vendor.
- ReaPack updates must never write to user-owned directories.
- Users own `Generated`; ReaPack must never write to it.
- Local development may link `Surfaces`, `Zones`, and `Snippets` from the product root to the repository resource tree. It may also link the repository `Scripts/` directory to `REAPER/Scripts/<ProductScriptDirectory>`. Installers and ReaPack packages must contain normal files and directories.

## Shared Contracts

### Product identity

- Keep the canonical product identity in one manifest that does not require a C++ build.
- Read the manifest directly from Lua and generate matching C++, TypeScript, CMake/CPack, CI, archive, and ReaPack values from it.
- Include the display name, stable product ID, resource directory, config filename, ExtState prefix, REAPER registration ID, plugin filename, script directory, package prefix, and repository URL.
- Do not require internal C++ class names to change when the user-facing product name changes.
- Keep explicit legacy CSI names only in the Bun importer.

### Identifiers, paths, and versions

- Use stable lowercase ASCII IDs matching `[a-z0-9][a-z0-9_-]*` for surface and profile paths.
- Store user-facing display names separately and allow spaces and Unicode in them.
- Give each public config, surface, zone, and snippet format an explicit version contract.
- Resolve target paths before access. Reject absolute child paths, `..`, and unsupported logical locations. Follow file and directory links while applying ownership and write permissions to the logical configuration path.
- Check duplicate IDs and filenames case-insensitively so the same data works on Windows, macOS, and Linux. Intentional `User` overrides of vendor surface and zone-profile IDs are the only cross-owner exceptions.

### Lossless document editing

- Use one lossless document model with a semantic view for validation and file details.
- Preserve comments, line order, whitespace that has user meaning, unknown properties, and unsupported lines.
- Report unknown data as a warning. Never silently remove it during a structured edit.
- Keep one syntax-highlighted text editor as the configuration editing mode.
- Use shared fixture files to keep the TypeScript authoring parser and C++ runtime parser consistent.
- Generate the editor action catalog from the runtime action registry instead of maintaining a second manual list.

### Validation levels

Validate in this order:

1. Syntax and format version.
2. IDs, names, destination paths, and duplicates.
3. Cross-file zone references and structural `IncludedZones` or `SubZones` dependency cycles. `GoZone` and `GoSubZone` return paths are valid navigation, not structural cycles.
4. Surface widget existence and semantic capabilities.
5. Action names and the parameters for which runtime metadata is available.

Warnings may preserve unknown data. Errors must prevent apply or save.

### Safe saves and transactions

- Record the source file hash when a document opens.
- Refuse a save when the current file hash differs, then show reload and conflict options.
- For one file, write a completed temporary file, validate it, and replace the target atomically.
- For multiple files, stage every output first, validate the complete set, create a backup manifest, then commit the set.
- If a multi-file commit fails, restore all changed files from the operation backup.
- Never modify an import source.
- Default conflict behavior is no overwrite. Require an explicit `Rename`, `Replace`, or `Skip` choice.
- Show a final operation report with created, changed, skipped, restored, and failed files.

### Surface OSK layout

Keep the layout in a formal block inside the `<surface-id>.txt` file instead of functional comments:

```text
OSKLayout Version=1
  Row
    Widget Bypass
    Widget Touch
    Spacer Width=0.25
    Widget RotaryBig Width=1.5 Height=1.5
  RowEnd
OSKLayoutEnd
```

- Widget order inside `Row` defines columns.
- Layout validation checks every widget and semantic target against the containing surface definition.
- OSC surface loading and behavior remain unchanged.

## Work Plan

### ✅ Phase 1. Product identity and path resolver

- Add the canonical product identity manifest, read it directly from Lua, and generate other language-specific constants.
- Replace hardcoded runtime, script, installer, CPack, CI, release archive, and ReaPack names and paths.
- Add typed path resolution for vendor surfaces, user surfaces, zone profiles, snippets, backups, and legacy import roots.
- Resolve User Main before Vendor Main. Load Vendor and User FX zones together, with exact User `Zone` names overriding Vendor. Keep Vendor zones read-only, copy only Main before a Main edit, copy only one Vendor FX file before editing it, and write new FX zones directly to User.
- Document manual development resource and Lua runtime links in the main README. Do not create these links during build or install.
- Keep `<ProductRoot>` usable before the final product name is selected.

Ready when a future rename requires one identity change and generated output updates, with no manual runtime path search.

### Phase 2. ReaPack foundation

- ✅ Add `.reapack-index.conf`, generated `index.xml`, and an official `reapack-index` validation command.
- ✅ Run index validation in CI and prevent publication when package metadata, source URLs, checksums, or target paths are invalid.
- ✅ Start with a release-specific preview ReaPack index. Publish the stable repository URL only after installation checks pass.
- ✅ Define one versioned core package with shared Lua scripts and a platform-selected C++ extension payload for Windows, macOS, and Linux.
- ✅ Define platform-independent package groups for vendor surfaces, vendor zone profiles, and built-in snippets.
- ✅ Generate package names, target paths, archive names, and release asset names from the canonical product identity.
- ✅ Install only into product-owned runtime, script, vendor surface, vendor zone-profile, and built-in snippet paths.
- ✅ Never install package content into `Surfaces/User`, `Zones/User`, `Snippets/User`, or `Backups`.
- ✅ Document package ownership so update and uninstall behavior is explicit before publishing user content.
- Check install, update, downgrade, and uninstall in a clean portable REAPER resource directory for every supported platform.

Ready when the preview repository passes official index validation, installs the correct platform payload and shared scripts, and can update or uninstall without changing user-owned files.

### ✅ Phase 3. Format specifications and editor core

- ✅ Create `tools/config-editor/` as the Bun and TypeScript application boundary with its own local DOX contract.
- ✅ Define the versioned surface, zone, snippet, and product-config contracts before building UI forms.
- ✅ Implement lossless parsers, serializers, semantic document views, validators, and shared fixtures.
- ✅ Add a generated runtime action catalog for editor validation and search.
- ✅ Provide a command-line validation entry point for development and import diagnostics.
- ✅ Run the Bun fixture and CLI checks after explicit approval.

Ready when valid files round-trip without data loss and malformed fixtures produce stable, actionable errors.

### Phase 4. Local browser editor and safe saving

- ✅ Start with REAPER data path selection, discovery, confirmation, and a read-only file tree. Derive the internal product folder from product identity.
- ✅ Add syntax-highlighted text editing for surfaces, all supported zone forms, and snippets.
- ✅ Run the backend only on `127.0.0.1` with a random session token and internal configuration-folder access restrictions.
- ✅ Add hash conflict detection, single-file atomic save, multi-file transactions, backups, rollback, and operation reports.
- ✅ Add standalone compile targets that embed generated product identity and runtime actions.
- ✅ Add platform defaults for the REAPER data path and prefill the path field.
- ✅ Route fixed UI text through a typed i18n catalog and use beginner-facing labels.
- [ ] Build, verify, sign, and publish standalone executables for supported operating systems so users do not install Bun.
- ✅ Add plugin-triggered editor launch and a separate platform-specific ReaPack Configuration Editor package.
- ✅ Reuse the active local editor server when the plugin launch action is pressed again instead of creating another process on a new automatic port.
- ✅ Keep the REAPER Data path centered in a persistent header on the home, configuration edit, and legacy import views.
- ✅ Run the Bun tests and local HTTP checks.
- [ ] Run the standalone build after the planned interface changes.

Ready when a user can safely inspect and edit the new configuration structure from a standalone local application.

### ✅ Phase 5. Repeatable legacy CSI import

- ✅ Let the editor open an old CSI root at any time without changing it.
- ✅ Import one surface, one zone, or a selected set of zones from `Surface.txt` and `Zones/**/*.zon`.
- ✅ Show `Surface.txt` as the first editable import draft above the Zone folder tree.
- ✅ Show `GoZone`, `GoSubZone`, `SubZones`, and `IncludedZones` dependencies before import.
- ✅ Select single unambiguous dependencies by default when the user selects a zone, but allow the user to exclude them.
- ✅ Import legacy surfaces into `Surfaces/User` unless a vendor package is selected separately.
- ✅ Import legacy zone profiles into `Zones/User` unless a vendor package is selected separately.
- ✅ Use semantic widget dropdowns for incompatible or missing target widgets.
- ✅ Show a full source preview and require `Rename`, `Replace`, or `Skip` for every existing target conflict.
- ✅ Validate source and target hashes, validate the final file set, and commit the resolved import as one transaction.
- ✅ Add legacy `FXZones/` discovery after its target mapping and relation to zone profiles are defined. Map legacy `Zones/` to the user profile `Main` tree and `FXZones/` to its `FX` tree.
- ✅ Re-run the focused importer, server, and browser UI tests with the custom `FaderPortV2` data after the `FXZones` and semantic widget mapping changes, then correct any issues found.

Ready when legacy content can be imported repeatedly without runtime legacy support or changes to the source installation.

### ✅ Phase 6. Vendor surfaces, zone profiles, and ReaPack content

- ✅ Add a validated `OSKLayout` block to every vendor MIDI surface file. Runtime support, editor validation, and the first formal `FaderPortV2` layout are verified.
- ✅ Generate versioned surface and matching zone-profile packages through the preview ReaPack foundation from Phase 2. Stable publication remains gated by the clean-install checks documented in `docs/REAPACK.md`.
- ✅ Verify that generated install and update paths touch only `Surfaces/Vendor` and `Zones/Vendor`. Two local preview versions produced identical targets and no user-owned targets.
- ✅ Require the editor to copy a Vendor surface, a Vendor Main directory, or one selected Vendor FX file before customization.

Ready when every vendor MIDI surface has an installable layout and ReaPack cannot overwrite user content.

### ✅ Phase 7. Reusable functional snippets

- ✅ Define semantic slots with required widget roles, inputs, feedback, and target surface widgets.
- ✅ Keep only Edit configuration and Import old CSI as task entry points. Show the snippet selector only when an editable user zone is selected.
- ✅ Resolve the target surface from the selected zone profile and the main config. Show a popup only when the surface, widget mapping, or application conflict needs input.
- ✅ Show a filtered widget dropdown for every binding. Resolve exact compatible name matches automatically and require confirmation for different manual choices.
- ✅ Permit manual selection only when validation explains any capability mismatch and the user enables an explicit override.
- ✅ Support inline preview, `Rename`, `Replace`, `Skip`, text editing, and a small built-in transport starter. Do not provide separate browser snippet import or export workflows.
- ✅ Insert resolved lines after the current cursor line, or before `ZoneEnd` when the cursor is on line 1. Change only the unsaved editor draft.
- ✅ Store shipped snippets in `Snippets/BuiltIn` and editable copies in `Snippets/User`.
- ✅ Add every changed file to Save all automatically, restore it when switching files, and keep one debounced recovery file per logical configuration path in the system temp directory.
- ✅ Keep current-file diagnostics separate from a retained, collapsible All found index. Add Check all with unsaved draft overlays and cross-file validation.
- ✅ Add a registry-based quick-fix workflow that changes only the current draft, starting with the missing zone format marker.
- ✅ Keep the editor inside the viewport, add confirmed Discard changes, keep diagnostic text selectable, and use dismissible semantic notifications outside the workflow header.
- ✅ Restore the selected task, open configuration file, diagnostic line, and bottom panel from a History API query route on reload and browser Back or Forward.
- ✅ Verify the focused parser, workflow, draft recovery, server, and browser UI checks after explicit approval.

Ready when a function group can move between compatible surfaces without fixed vendor widget names or partial file changes.

Format 2 replaces this completed Version 1 semantic-slot workflow with direct zone-fragment snippets. The current `Role`, `Input`, `Feedback`, `Required`, application-ID, conflict-action, and saved-marker implementation is migration input, not the future public snippet format. The replacement is specified and tracked in [ZONE_FORMAT_V2_PLAN.md](ZONE_FORMAT_V2_PLAN.md).

### Phase 8. Small zone-file creation from Lua OSK

- ✅ Add a Lua command that asks C++ to create one minimal valid zone file.
- ✅ Provide scaffold templates for normal, Home, Go, FX, subzone, included, and Learn zone forms.
- ✅ Validate the name, required fields, profile, destination, path containment, and case-insensitive duplicates in C++.
- ✅ Create the file through a completed temporary file, reload zones, and return its path or an actionable error.
- ✅ Do not modify a parent zone automatically in the first version.
- ✅ Treat `Create and link to parent` as a later multi-file operation if it is still useful.
- ✅ Keep full editing, dependency changes, legacy import, and snippet management in the Bun editor.
- Verify the C++ build, Lua self-checks, and focused OSK creation cases after explicit approval.

Ready when OSK can safely create any supported one-file zone scaffold without becoming a second full configuration editor.

### ✅ Phase 9. Visual Surface editor

Use one separate Surface design page inside the Bun editor. Open it from a selected Surface file and add a third task to the home page. Keep `view=surface` and the Surface file path in browser history so Back returns to the text editor.

- ✅ Use the Surface text document as the only source of truth. Visual changes produce normal lossless draft edits and use the existing validation, recovery, Save, and Save all workflow. No second JSON layout format exists.
- ✅ Show an `OSKLayout` grid in the center, only Widgets not yet added to the layout in a left panel, and properties for the selected layout cell in a right sidebar.
- ✅ Let the user drag an existing Widget into the grid, move it between cells or rows, change Width and Height, add rows, and remove a layout reference without deleting its Widget definition.
- ✅ Let the right sidebar edit only applicable `OSKLayout` properties from `surface_io_schema.conf`. Keep Input, Feedback, Widget names, and profiles in Code view. The browser does not own a second property catalog.
- ✅ Keep a Code view on the same page. Visual and code edits update the same unsaved CodeMirror draft, share Undo and Redo history, and preserve source lines that the visual operation did not change.
- ✅ If code has a blocking syntax error, keep the text editable, highlight the exact problem, and temporarily disable structural visual operations. Never replace invalid source with a regenerated document.
- ✅ Require Make editable copy before changing a Vendor Surface. Keep the visual page read-only while a Vendor file is selected.
- ✅ Add keyboard insertion and Alt plus Arrow-key movement for drag operations, clear drop targets, and a compact layout for narrow screens.
- ✅ Replace the separate Home and workflow headers with one persistent header, add the three-page navigation drawer with a local server stop action, and remove the Back button.
- ✅ Make the legacy import preview column collapsible and automatically remove short success and information notifications after four seconds.
- ✅ Clear legacy file and folder selection recursively without waiting for preview refresh, do not let an unselected draft target block selection changes, and open `Surface.txt` through the same converted import-draft flow as Zone files.
- ✅ Remove per-row action buttons from Surface Designer, render controls close to Lua OSK shapes, and show each same-Row `Group` such as Rotary plus RotaryPush as one movable combined control without changing the Surface file format.
- ✅ Give Surface Designer cells fixed Lua OSK proportions from button height, button aspect, Width, Height, and Top instead of stretching them across each web row. Size the visible body of a round control separately from its aspect-wide layout slot.
- ✅ Match Lua OSK Spacer defaults and round-button geometry, detect rotary controls from their Widget input, and reserve occupied columns below multi-row faders so later rows align with the Lua layout.
- ✅ Limit the visual Surface Designer inspector to `OSKLayout` placement and appearance properties. Keep Widget definitions, Input, Feedback, names, and profiles in Code view only.
- ✅ Make Surface Designer Properties context-aware from the shared Surface schema, including dropdowns for fixed choices and capability-filtered Widget target dropdowns.
- ✅ Add focused checks for text-to-grid mapping, grid-to-text edits, shared draft history, comments and unknown blocks, invalid source, Vendor files, and schema-driven property controls after approval to run checks.

Ready when a Surface can move between Code and visual layout editing without losing source text or creating a second save model.

## Verification

- Round-trip representative surface, zone, and snippet files without losing comments or unknown data.
- Reject malformed versions, unsafe IDs, absolute child paths, `..`, unsupported logical locations, and case-only duplicates. Follow linked resources without recursive directory cycles.
- Detect an external file change made after the editor opened it.
- Force a failure during a multi-file commit and verify complete rollback from the manifest.
- Confirm legacy import leaves every source file unchanged.
- Confirm semantic dropdowns block widgets without required capabilities unless the user sees the mismatch, enables the explicit override, and confirms the choice.
- Confirm a standalone editor starts without an installed Bun runtime and exposes no non-local network listener.
- Validate the generated ReaPack index in CI before publication.
- Install, update, downgrade, and uninstall the core package in a clean portable REAPER resource directory for each supported platform.
- Confirm every core package installs the correct C++ extension and shared Lua scripts without changing user-owned files.
- Install and update a ReaPack surface or zone-profile package without changes under `Surfaces/User`, `Zones/User`, or `Snippets/User`.
- Create each supported one-file zone scaffold from OSK and verify the returned path and reload status.
- Confirm OSC runtime behavior is unchanged.

## Non-Goals

- OSK mirroring for OSC surfaces.
- Runtime fallback to legacy CSI directories or formats.
- A full surface, zone, import, or snippet manager inside Lua OSK.
- Automatic parent-zone edits during the first OSK zone-file creation pass.
- Pseudo-modifier implementation, which is owned by [PSEUDO_MODIFIER_PLAN.md](PSEUDO_MODIFIER_PLAN.md).
