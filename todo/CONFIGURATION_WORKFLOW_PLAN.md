# Configuration Workflow Plan

## Goal

Provide safe tools to create, edit, import, export, validate, and distribute surface, zone, and snippet configurations.

Large configuration changes belong in a local Bun and TypeScript editor. The Lua OSK remains a live fine-tuning tool for MIDI surfaces and gains only small zone-file creation support.

## Fixed Decisions

- Use `<ProductName>` and `<ProductRoot>` until the fork has its final product identity.
- Generate runtime names, paths, ExtState sections, script paths, and package names from one build-time product identity source.
- Keep OSK layout data in a formal block inside `Surface.txt`.
- Support OSK layouts for MIDI surfaces only. Do not add OSK mirroring for OSC surfaces in this plan.
- Use the Bun editor for full surface, zone, snippet, import, export, and batch workflows.
- Keep Lua OSK focused on live behavior editing and small zone-file creation.
- Treat old CSI installations as read-only import sources. Do not add legacy CSI path fallback to the runtime plugin.
- Keep pseudo-modifier work independent in [PSEUDO_MODIFIER_PLAN.md](PSEUDO_MODIFIER_PLAN.md).

## Target Structure

```text
<ProductRoot>/
  <ProductConfigFile>
  Surfaces/
    Vendor/<surface-id>/Surface.txt
    User/<surface-id>/Surface.txt
  Zones/<profile-id>/
    Main/*.zon
    FX/*.zon
  Snippets/
    BuiltIn/*.snippet
    User/*.snippet
  Backups/<operation-id>/
    manifest.json
```

- ReaPack owns `Surfaces/Vendor` and `Snippets/BuiltIn`.
- Users own `Surfaces/User`, all zone profiles, and `Snippets/User`.
- The editor must clone a vendor surface into `Surfaces/User` before allowing user changes.
- ReaPack updates must never write to user-owned directories.

## Shared Contracts

### Product identity

- Keep the canonical product identity in one build-time source.
- Generate matching C++, Lua, TypeScript, CMake/CPack, CI, archive, and ReaPack values from it.
- Include the display name, stable product ID, resource directory, config filename, ExtState prefix, REAPER registration ID, plugin filename, script directory, and package prefix.
- Do not require internal C++ class names to change when the user-facing product name changes.
- Keep explicit legacy CSI names only in the Bun importer.

### Identifiers, paths, and versions

- Use stable lowercase ASCII IDs matching `[a-z0-9][a-z0-9_-]*` for surface and profile paths.
- Store user-facing display names separately and allow spaces and Unicode in them.
- Give each public config, surface, zone, and snippet format an explicit version contract.
- Resolve and canonicalize every target path before access. Reject absolute child paths, `..`, symlink escapes, and paths outside the selected product or import root.
- Check duplicate IDs and filenames case-insensitively so the same data works on Windows, macOS, and Linux.

### Lossless document editing

- Use one lossless document model with a semantic view for structured editing.
- Preserve comments, line order, whitespace that has user meaning, unknown properties, and unsupported lines.
- Report unknown data as a warning. Never silently remove it during a structured edit.
- Make raw and structured editors operate on the same in-memory document.
- Use shared fixture files to keep the TypeScript authoring parser and C++ runtime parser consistent.
- Generate the editor action catalog from the runtime action registry instead of maintaining a second manual list.

### Validation levels

Validate in this order:

1. Syntax and format version.
2. IDs, names, destination paths, and duplicates.
3. Cross-file zone references and dependency cycles.
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

Keep the layout in a formal `Surface.txt` block instead of functional comments:

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

### Phase 1. Product identity and path resolver

- Add the canonical product identity source and generate language-specific constants.
- Replace hardcoded runtime, script, installer, CPack, CI, release archive, and ReaPack names and paths.
- Add typed path resolution for vendor surfaces, user surfaces, zone profiles, snippets, backups, and legacy import roots.
- Keep `<ProductRoot>` usable before the final product name is selected.

Ready when a future rename requires one identity change and generated output updates, with no manual runtime path search.

### Phase 2. ReaPack foundation

- Add `.reapack-index.conf`, generated `index.xml`, and an official `reapack-index` validation command.
- Run index validation in CI and prevent publication when package metadata, source URLs, checksums, or target paths are invalid.
- Start with a preview ReaPack repository or preview index. Publish the stable repository URL only after installation checks pass.
- Define one versioned core package with shared Lua scripts and a platform-selected C++ extension payload for Windows, macOS, and Linux.
- Define platform-independent package groups for vendor surfaces and built-in snippets.
- Generate package names, target paths, archive names, and release asset names from the canonical product identity.
- Install only into product-owned runtime, script, vendor surface, and built-in snippet paths.
- Never install package content into `Surfaces/User`, `Zones`, `Snippets/User`, or `Backups`.
- Document package ownership so update and uninstall behavior is explicit before publishing user content.
- Check install, update, downgrade, and uninstall in a clean portable REAPER resource directory for every supported platform.

Ready when the preview repository passes official index validation, installs the correct platform payload and shared scripts, and can update or uninstall without changing user-owned files.

### Phase 3. Format specifications and editor core

- Create `tools/config-editor/` as the Bun and TypeScript application boundary with its own local DOX contract.
- Define the versioned surface, zone, snippet, and product-config contracts before building UI forms.
- Implement lossless parsers, serializers, semantic document views, validators, and shared fixtures.
- Add a generated runtime action catalog for editor validation and search.
- Provide a command-line validation entry point for development and import diagnostics.

Ready when valid files round-trip without data loss and malformed fixtures produce stable, actionable errors.

### Phase 4. Local browser editor and safe saving

- Start with product-root selection, discovery, confirmation, and a read-only file tree.
- Add structured and raw editing for surfaces, all supported zone forms, and snippets.
- Run the backend only on `127.0.0.1` with a random session token and product-root access restrictions.
- Add hash conflict detection, single-file atomic save, multi-file transactions, backups, rollback, and operation reports.
- Build signed or packaged standalone executables for supported operating systems so users do not install Bun.
- Keep plugin-triggered editor launch as an optional later integration, not a requirement for the first editor release.

Ready when a user can safely inspect and edit the new configuration structure from a standalone local application.

### Phase 5. Repeatable legacy CSI import

- Let the editor open an old CSI root at any time without changing it.
- Import one surface, one zone, or a selected set of zones.
- Show `GoZone`, `SubZones`, `IncludedZones`, and other discovered dependencies before import.
- Select dependencies by default but allow the user to exclude them.
- Import legacy surfaces into `Surfaces/User` unless a curated vendor package is selected separately.
- Use semantic widget dropdowns for incompatible or missing target widgets.
- Show a full preview and require `Rename`, `Replace`, or `Skip` for every conflict.
- Validate and commit the resolved import as one transaction.

Ready when legacy content can be imported repeatedly without runtime legacy support or changes to the source installation.

### Phase 6. Vendor surfaces and ReaPack content

- Start with surfaces that this project owns or has verified permission to redistribute.
- Add a validated `OSKLayout` block to every curated MIDI `Surface.txt`.
- Publish one versioned package per surface through the ReaPack foundation from Phase 2.
- Verify that package install and update touch only `Surfaces/Vendor`.
- Require the editor to clone a vendor surface before customization.
- Keep vendor inventory, compatibility, provenance, and redistribution status visible in package metadata.

Ready when every curated MIDI surface has an installable layout and ReaPack cannot overwrite user content.

### Phase 7. Reusable functional snippets

- Define semantic slots with required widget roles, inputs, feedback, and targets.
- Show a filtered widget dropdown for every binding and require confirmation before apply.
- Permit manual selection only when validation explains any capability mismatch.
- Support preview, `Rename`, `Replace`, `Skip`, raw editing, import, export, and a small built-in starter set.
- Apply a resolved snippet as one validated transaction.
- Store shipped snippets in `Snippets/BuiltIn` and exports in `Snippets/User`.

Ready when a function group can move between compatible surfaces without fixed vendor widget names or partial file changes.

### Phase 8. Small zone-file creation from Lua OSK

- Add a Lua command that asks C++ to create one minimal valid zone file.
- Provide scaffold templates for normal, FX, subzone, included, and other supported zone forms.
- Validate the name, required fields, profile, destination, path containment, and case-insensitive duplicates in C++.
- Create the file through a completed temporary file, reload zones, and return its path or an actionable error.
- Do not modify a parent zone automatically in the first version.
- Treat `Create and link to parent` as a later multi-file operation if it is still useful.
- Keep full editing, dependency changes, legacy import, and snippet management in the Bun editor.

Ready when OSK can safely create any supported one-file zone scaffold without becoming a second full configuration editor.

## Verification

- Round-trip representative surface, zone, and snippet files without losing comments or unknown data.
- Reject malformed versions, unsafe IDs, absolute child paths, `..`, symlink escapes, and case-only duplicates.
- Detect an external file change made after the editor opened it.
- Force a failure during a multi-file commit and verify complete rollback from the manifest.
- Confirm legacy import leaves every source file unchanged.
- Confirm semantic dropdowns reject widgets without required capabilities.
- Confirm a standalone editor starts without an installed Bun runtime and exposes no non-local network listener.
- Validate the generated ReaPack index in CI before publication.
- Install, update, downgrade, and uninstall the core package in a clean portable REAPER resource directory for each supported platform.
- Confirm every core package installs the correct C++ extension and shared Lua scripts without changing user-owned files.
- Install and update a ReaPack surface package without changes under `Surfaces/User`, `Zones`, or `Snippets/User`.
- Create each supported one-file zone scaffold from OSK and verify the returned path and reload status.
- Confirm OSC runtime behavior is unchanged.

## Non-Goals

- OSK mirroring for OSC surfaces.
- Runtime fallback to legacy CSI directories or formats.
- A full surface, zone, import, or snippet manager inside Lua OSK.
- Automatic parent-zone edits during the first OSK zone-file creation pass.
- Pseudo-modifier implementation, which is owned by [PSEUDO_MODIFIER_PLAN.md](PSEUDO_MODIFIER_PLAN.md).
