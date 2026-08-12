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
- Text and guided editing with the same lossless document.
- Hash conflict checks, atomic single-file saves, and multi-file transactions.
- Backup manifests, rollback, and operation reports.
- Repeatable read-only import from legacy `Surface.txt`, `Zones/**/*.zon`, and `FXZones/**/*.zon` files.
- Legacy dependency preview, semantic widget mapping, selected-zone import, and required `Rename`, `Replace`, or `Skip` conflict resolution.
- A loopback-only browser server with a random session token.
- A typed English UI text catalog with parameter replacement.
- Standalone compile targets for Windows, macOS, and Linux.

Functional snippet application remains a later phase. Plugin-triggered launch is optional and is not part of the first local editor.

## Commands

Run these commands from `tools/config-editor/`:

```sh
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

The browser opens no configuration files until the user clicks `Open`. The tree marks vendor surfaces, vendor zone profiles, and built-in snippets as read-only. Use `Make editable copy` before editing them. A vendor zone copy includes its complete profile.

The editor follows linked files and directories. This lets a development product root link `Surfaces`, `Zones`, and `Snippets` directly to repository resources. Access and write permissions still use the logical configuration path, so linked vendor content remains read-only and only supported paths below `User` are writable. Directory link cycles are shown as unavailable instead of being traversed.

## Legacy CSI import

Open the target REAPER `Data` directory first. Expand `Import old CSI configuration`, enter either an old `CSI/` directory or its parent directory, and click `Open`. The source remains read-only and the importer follows linked files and directories.

Choose a surface, include or exclude its `Surface.txt`, and select any `.zon` files from its legacy `Zones` and `FXZones` directories. Selecting a zone also selects its single unambiguous `GoZone`, `GoSubZone`, `IncludedZones`, and `SubZones` dependencies. You can clear a dependency after automatic selection. Files whose names do not end exactly in `.zon`, such as `.zon~20260101` and `.zon1` backup files, are not imported.

The preview shows migrated source text, target paths, syntax diagnostics, dependencies, widget mappings, and conflicts. Missing format markers are added as `// @format surface 1` or `// @format zone 1`. A legacy surface named `FaderPortV2` maps to `Surfaces/User/faderportv2.txt`. Its `Zones` files map below `Zones/User/faderportv2/Main`, and its `FXZones` files map below `Zones/User/faderportv2/FX`. Relative subdirectories are preserved.

The importer compares each selected binding with the target surface. If `Surface.txt` is included and will be created or replaced, this is the imported surface. If the surface conflict is set to `Rename` or `Skip`, or `Surface.txt` is excluded, the importer uses the existing user surface and then the vendor surface with the same stable ID. Changing this conflict choice refreshes the widget preview. Missing or incompatible widgets require a compatible replacement from the dropdown. The dropdown uses the same press, touch, relative, absolute, value, toggle, color, text, and meter capabilities as the runtime OSK metadata. Channel placeholders stay channel placeholders, so a legacy `Fader|` can map to a compatible family such as `Rotary|`, but not to one fixed `Rotary1`. The resolved widget names are written into the preview source before hashes are calculated.

Every existing target requires `Rename`, `Replace`, or `Skip`. Import is disabled while selected content has errors, a widget mapping is unresolved, or a conflict has no decision. The server checks source and target hashes again, validates the final file set, and saves it through the normal multi-file transaction. The final report lists changed, created, failed, restored, and skipped paths.

`actions:generate` writes `generated/action-catalog.json`. Generated output is not source-controlled. The catalog is rebuilt from `src/shared/types.h` and action documentation, so action names are not duplicated in TypeScript.

The repository rule requires explicit permission before running tests or build commands. The commands above describe the available checks; they are not automatic.

## Safe saves

Each opened file includes a SHA-256 hash. Save fails with a conflict when the file changed after it opened. A single save writes and validates a complete temporary file beside its target before atomic rename.

`Save all` stages and validates every file, then writes `Backups/<operation-id>/manifest.json`, copies original files into the operation backup, and commits each staged file. A commit failure restores every file already changed and records the rollback in the manifest and operation report.

Only the main config and files below `Surfaces/User`, `Zones/User`, and `Snippets/User` are writable. API paths are relative to the app configuration folder. Absolute paths, `..`, backslash separators, and unsupported logical locations are rejected. Symlink targets use the permissions of the logical API path.

## Local server

The server binds to `127.0.0.1` on a random port. It creates a new 256-bit session token for every launch. The token starts in the URL fragment, moves into browser session storage, and is sent only in the `X-Session-Token` request header. API requests from another browser origin are rejected.

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

## Lossless model

Every parsed document keeps each original line and its exact line ending. The serializer joins those stored values without normalization. Format parsers add file details and checks but do not remove unknown data.

Unknown lines and unsupported properties produce warnings when the surrounding syntax remains safe. Missing block ends, invalid versions, unsafe stable IDs, and case-insensitive duplicates produce errors.

See [docs/FORMATS.md](docs/FORMATS.md) for the grammar contracts.
