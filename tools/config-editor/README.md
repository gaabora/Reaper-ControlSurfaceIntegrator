# ReaControlSurface Conf Editor

This Bun and TypeScript application provides a local browser editor and its lossless configuration core.

## Current scope

- Lossless line model and serializer.
- Product config, surface, zone, and functional snippet parsers.
- File details and clear syntax checks.
- Case-insensitive duplicate checks and zone dependency checks.
- Runtime action catalog loaded from C++ `ACTION_TYPE_LIST` and `//!` metadata, including short descriptions where available.
- CLI validation and JSON action-catalog generation.
- Automatic REAPER data path discovery and manual selection.
- CodeMirror text editing with line numbers and configuration syntax highlighting.
- Debounced recovery drafts stored as one temporary file per logical configuration path.
- Hash conflict checks, atomic single-file saves, and multi-file transactions.
- Backup manifests, rollback, and operation reports.
- Repeatable read-only import from legacy `Surface.txt`, `Zones/**/*.zon`, and `FXZones/**/*.zon` files.
- Legacy dependency preview, semantic widget mapping, selected-zone import, and required `Rename`, `Replace`, or `Skip` conflict resolution.
- Inline functional snippet insertion with semantic widget filtering, automatic exact matches, confirmed manual mappings, mismatch explanations, and application conflict resolution.
- A loopback-only browser server with a random session token.
- A typed English UI text catalog with parameter replacement.
- Standalone compile targets for Windows, macOS, and Linux.

Plugin-triggered launch is optional and is not part of the first local editor.

## Commands

Run these commands from `tools/config-editor/`:

```sh
bun install
bun run validate -- fixtures/valid
bun run validate -- --json fixtures/invalid
bun run actions
bun run actions:generate
bun run start
bun test
```

The editor fills the REAPER data path from `REAPER_RESOURCE_PATH` or the current platform default:

- macOS: `~/Library/Application Support/REAPER/Data`
- Windows: `%APPDATA%/REAPER/Data`
- Linux: `~/.config/REAPER/Data`, or `$XDG_CONFIG_HOME/REAPER/Data` when set

Enter only the REAPER `Data` directory. The editor adds the product configuration folder from `Scripts/product_identity.conf`. Use an explicit path when automatic discovery is not correct:

```sh
bun run start -- --data-path "/absolute/REAPER/resource/Data"
```

The browser opens the first valid discovered REAPER data path automatically. The main page shows the selected path above the `Edit configuration` and `Import old CSI` tasks. It keeps `Open` as a fallback when the discovered path is wrong or a different REAPER installation is needed. The tree marks vendor surfaces, vendor zone profiles, and built-in snippets with a dim read-only style. Use `Make editable copy` before editing them. A vendor zone copy includes its complete profile.

The text editor stays inside the available viewport and scrolls independently. `Problems` separates diagnostics for the current file from the collapsible `All found` index. Opening or checking a file updates its entry without removing previously found problems from other files. `Check all` validates every available configuration file, uses unsaved drafts for changed files, and adds cross-file dependency and duplicate diagnostics. File and line links navigate without turning the diagnostic text into a button, so the message remains selectable.

The browser URL stores the current task, open configuration file, diagnostic line, and bottom panel. Reload restores that editor state and its recovery draft. Browser Back and Forward move between previously opened tasks and files. The REAPER data path and session token are not stored in the URL.

Some diagnostics offer one or more quick-fix links. A quick fix is validated against the current source, changes only the unsaved draft, and still requires Save or Save all. Quick fixes are registered by stable IDs in `src/quick-fixes.ts`; the first registered fix adds a missing `// @format zone 1` marker.

The editor follows linked files and directories. This lets a development product root link `Surfaces`, `Zones`, and `Snippets` directly to repository resources. Access and write permissions still use the logical configuration path, so linked vendor content remains read-only and only supported paths below `User` are writable. Directory link cycles are shown as unavailable instead of being traversed.

## Legacy CSI import

The editor checks the standard old `CSI` directory beside the selected `Data` directory automatically. For example, `/REAPER/Data` maps to `/REAPER/CSI`. If that folder is missing or the old installation is elsewhere, open `Advanced details`, enter either an old `CSI/` directory or its parent directory, and click `Open`. The source remains read-only and the importer follows linked files and directories.

Use `Reload old CSI` after files or surface folders change on disk. Reload keeps the selected surface, selected zones, target and conflict choices, and in-memory import drafts. If a changed source cannot be refreshed safely, the existing preview and drafts remain available and the editor reports the conflict.

The import workspace uses two columns. Open a source file from Import preview to edit its import draft in the left column. Problems stay below that editor. Target profile settings, target paths, conflict actions, Import, and the operation report stay in the independently scrolling right column. Narrow screens stack these columns.

Choose a surface, include or exclude its `Surface.txt`, and select any `.zon` files from its legacy `Zones` and `FXZones` directories. Selecting a zone also selects its single unambiguous `GoZone`, `GoSubZone`, `IncludedZones`, and `SubZones` dependencies. You can clear a dependency after automatic selection. Files whose names do not end exactly in `.zon`, such as `.zon~20260101` and `.zon1` backup files, are not imported. A deprecated `GoZones.zon` manifest is not imported. When it assigns a navigator to a zone, the importer adds that navigator to the matching zone header as `NavType`.

The preview shows migrated source text, target paths, syntax diagnostics, dependencies, widget mappings, and conflicts. Missing format markers are added as `// @format surface 1` or `// @format zone 1`. Click a source file or diagnostic to open its import draft at the related line. Draft changes stay in memory and are written only to the new target during import. The old CSI file is never changed.

Legacy lines whose first non-space character is one `/` or an unsupported `#` are converted to `//` comments in the import preview. OSC address tokens after a widget type, such as `X32Fader /ch/01/mix/fader`, and exact Learn FX directives such as `#WidgetType` stay unchanged. Current Surface and Zone files accept only `//` comments, apart from these reserved Learn FX directives. A leading single-slash or unsupported hash comment in an editable current file is an error with a `Convert to // comment` quick fix.

A legacy surface named `FaderPortV2` initially maps to `Surfaces/User/faderportv2.txt`. Its `Zones` files map below `Zones/User/faderportv2/Main`, and its `FXZones` files map below `Zones/User/faderportv2/FX`. Change the target profile ID to rename this common profile root. You can also edit each selected target path inside that profile. The preview reports existing files and rejects duplicate targets. Relative subdirectories are preserved unless you change them.

The importer compares each selected binding with the target surface. If `Surface.txt` is included and will be created or replaced, this is the imported surface. If the surface conflict is set to `Rename` or `Skip`, or `Surface.txt` is excluded, the importer uses the existing user surface and then the vendor surface with the target profile ID. Changing this conflict choice refreshes the widget preview. Missing or incompatible widgets require a compatible replacement from the dropdown. The dropdown uses the same press, touch, relative, absolute, value, toggle, color, text, and meter capabilities as the runtime OSK metadata. A binding such as `Touch+DisplayLower|` uses `Touch` as a modifier, so the `DisplayLower|` family does not need touch input. Channel placeholders stay channel placeholders, so a legacy `Fader|` can map to a compatible family such as `Rotary|`, but not to one fixed `Rotary1`. The resolved widget names are written into the preview source before hashes are calculated.

Every existing target requires `Rename`, `Replace`, or `Skip`. Import is disabled while selected content has errors, a widget mapping is unresolved, or a conflict has no decision. The server checks source and target hashes again, validates the final file set, and saves it through the normal multi-file transaction. The final report lists changed, created, failed, restored, and skipped paths.

## Functional snippets

Choose `Edit configuration` and open an editable zone below `Zones/User`. The snippet selector appears only for this zone file. Choose a built-in or user snippet and click `Insert snippet`. The editor resolves the surface through the selected zone profile and the matching `ZoneFolder` or `FXZoneFolder` assignment in the main product config. If no unique assignment exists, the mapping dialog shows a surface selector.

An exact compatible widget name is accepted automatically, and the generated block is inserted without opening the dialog. The dialog opens when a surface, widget mapping, or existing application decision is required. To skip a binding with `Required=No`, leave its widget empty and confirm the skip. An incompatible widget is not accepted by default. If the manual choice is intentional, read the displayed mismatch, enable the mismatch override, and confirm the widget.

The application ID identifies one generated block inside the zone. Inserting the same ID again requires `Replace`, `Rename`, or `Skip`. Replace updates only that block. Rename creates another block under a new ID. Skip does not change the draft. A new block is inserted after the current cursor line. When the cursor is on line 1 or outside the zone body, the block is inserted before `ZoneEnd`. Snippet insertion changes only the current recovery draft. It does not save the zone.

Open a snippet from the configuration tree to edit its text. Built-in snippets stay read-only until `Make editable copy` creates the matching user file.

`actions:generate` writes `generated/action-catalog.json`. Generated output is not source-controlled. The catalog is rebuilt from `src/shared/types.h` and action documentation, so action names are not duplicated in TypeScript.

The repository rule requires explicit permission before running tests or build commands. The commands above describe the available checks; they are not automatic.

## Safe saves

Each opened file includes a SHA-256 hash. Save fails with a conflict when the file changed after it opened. A single save writes and validates a complete temporary file beside its target before atomic rename.

Every text change automatically enters the Save all set. Changed files use bold text and `*` in the tree. Switching files restores their unsaved text. The browser writes each changed source after a 300 ms debounce to `<system-temp>/<product-id>-conf-editor/drafts/<path-sha256>.json`. The hash includes the full logical product root and relative configuration path. Drafts survive browser and editor restarts, but the operating system can clean temporary files. If the source file changed outside the editor, choose whether to use or discard the recovered draft.

`Discard changes` asks for confirmation, restores the current file from disk, and removes its recovery draft. Successful operations use green notifications that close after five seconds. Info, warning, and error notifications stay visible until closed. Errors related to a specific input stay below that input.

`Save all` stages and validates every draft, then writes `Backups/<operation-id>/manifest.json`, copies original files into the operation backup, and commits each staged file. A commit failure restores every file already changed and records the rollback in the manifest and operation report. A successful `Save` or `Save all` removes the related recovery draft.

Only the main config and files below `Surfaces/User`, `Zones/User`, and `Snippets/User` are writable. API paths are relative to the app configuration folder. Absolute paths, `..`, backslash separators, and unsupported logical locations are rejected. Symlink targets use the permissions of the logical API path.

## Local server

The server binds to `127.0.0.1` on a random port. It creates a new 256-bit session token for every launch. The initial HTML contains the token in a generated metadata value, and the browser sends it in the `X-Session-Token` request header. The token is not stored in the URL or persistent browser storage. API requests from another browser origin are rejected. CodeMirror and its configuration language support are bundled into the served JavaScript. The editor does not load browser code from a CDN.

Use `--no-open` to prevent automatic browser launch and `--port <number>` to select a fixed loopback port.

## VS Code

The workspace recommends the official `oven.bun-vscode` extension. Use these commands from VS Code:

- `Run Task: Config Editor: Start` starts the local editor.
- `Run Test Task` runs the complete Bun suite.
- `Run Task: Config Editor: Test watch` reruns tests after file changes.
- `Run Task: Config Editor: Validate path` asks for one file or directory to validate.
- `Run and Debug: Config Editor: Debug server` starts the server under the Bun debugger without opening a browser.
- `Run and Debug: Config Editor: Debug current test` debugs the currently open Bun test file.

## Standalone executables

The compile workflow reads `Scripts/product_identity.conf` and the C++ action registry before compiling. It embeds those generated values in the executable, so the executable does not need the repository or Bun at runtime.

```sh
bun run compile:local
bun run compile:macos-arm64
bun run compile:macos-x64
bun run compile:windows-x64
bun run compile:linux-x64
bun run compile:linux-arm64
```

Outputs go to `dist/`, which is not source-controlled. Cross-target compilation can require Bun to download the matching runtime. Release signing and archive packaging must run in the release environment after the executable checks pass.

Tagged release CI builds and publishes these standalone ZIP archives:

- `config-editor-<ProductId>-windows-x64.zip`
- `config-editor-<ProductId>-darwin-x64.zip`
- `config-editor-<ProductId>-darwin-arm64.zip`
- `config-editor-<ProductId>-linux-x64.zip`

The editor runs separately from REAPER and is not installed through ReaPack.

## Lossless model

Every parsed document keeps each original line and its exact line ending. The serializer joins those stored values without normalization. Format parsers add file details and checks but do not remove unknown data.

Unknown lines and unsupported properties produce warnings when the surrounding syntax remains safe. Missing block ends, invalid versions, unsafe stable IDs, and case-insensitive duplicates produce errors.

See [docs/FORMATS.md](docs/FORMATS.md) for the grammar contracts.
