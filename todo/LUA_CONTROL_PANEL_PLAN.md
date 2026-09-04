# Lua Control Panel Plan

## Goal

Create one Lua and ReaImGui Control Panel for plugin settings. Use a vertical navigation bar on the left and a stable content area on the right.

The first navigation model has these tabs:

- Devices
- General
- Appearance
- Logging

The Control Panel must make common settings easy to find without duplicating C++ configuration rules or the full standalone configuration editor.

## Current State

- [`../src/ui/config_dialog.cpp`](../src/ui/config_dialog.cpp) is now only the native callback launcher. It opens or focuses the Lua Control Panel and closes the temporary REAPER-owned dialog. It does not parse or write product configuration.
- [`../Scripts/settings_ui.lua`](../Scripts/settings_ui.lua) already edits Product and Surface input settings through [`../Scripts/settings_protocol.lua`](../Scripts/settings_protocol.lua). C++ validates and saves these values.
- [`../Scripts/settings_schema.conf`](../Scripts/settings_schema.conf) is the canonical metadata source for Behavior and Timing settings.
- [`../Scripts/theme_settings.lua`](../Scripts/theme_settings.lua) contains fixed shared style values and separate persistent OSK, OSD, and Notifications schemas.
- [`../Scripts/Notifications.lua`](../Scripts/Notifications.lua) is the dedicated notification script for NOTICE, WARNING, and ERROR log records.
- [`../src/controls/integrator_config_parser.h`](../src/controls/integrator_config_parser.h) already models `surfaceId`, `mainZoneProfileId`, and `fxZoneProfileId` as separate values.
- [`../src/controls/config_parser.cpp`](../src/controls/config_parser.cpp) rejects a surface assignment when its Main zone profile is missing. It creates the User FX directory when necessary.
- The standalone Bun editor in [CONFIGURATION_WORKFLOW_PLAN.md](CONFIGURATION_WORKFLOW_PLAN.md) owns full surface, zone, snippet, import, and batch editing.

## Fixed Architecture Decisions

- Use one new Lua entry script for the Control Panel and small Lua modules for its pages, state, protocol, and reusable controls.
- Keep C++ as the authority for product configuration parsing, validation, atomic saving, runtime reload, device reconnect, and error reporting.
- Do not let Lua write the product INI directly.
- Use a versioned ExtState request and response protocol between the Lua Control Panel and C++.
- Keep a draft separate from the current saved and active configuration. `Save changes` validates, persists, and applies the complete draft without closing the Control Panel. `Revert` restores the last saved state without closing the Control Panel.
- Show validation errors before Save. C++ must validate the complete draft again before it saves anything.
- Preview Appearance changes immediately from the draft, but do not persist them until Save. Revert or discard must restore the saved appearance.
- Keep the small native callback launcher because REAPER expects a native window handle. Configuration editing belongs to the Lua Control Panel.
- Do not add surface, zone, snippet, legacy import, or batch text editing to this Control Panel. Provide a command that opens the standalone editor for these tasks.
- Keep one Appearance page, but do not create one flat settings table. Use separate `OSK`, `OSD`, and `Notifications` setting groups.
- Treat runtime logs as disposable diagnostics. Store them in bounded daily files under the operating system user temporary directory.
- Use daily log ID, segment ID, and record start byte offset as the internal record identity shared by the viewer and Notifications. Keep this identity hidden from the user.
- Register stable REAPER actions in C++. Do not use generated `_RS...` ReaScript IDs as the public action contract.

## Terminology

### Surface template

A Surface template is one `<surface-id>.txt` file under `Surfaces/Vendor` or `Surfaces/User`. It defines widgets, messages, feedback, capabilities, and the OSK layout for one control surface type.

### Zone profile

A Zone profile is one stable `<profile-id>` directory under `Zones/Vendor` or `Zones/User`. It can contain:

```text
<profile-id>/
  Main/
  FX/
```

The product configuration currently stores `ZoneFolder` for Main and `FXZoneFolder` for FX. The GUI must label both values as Zone profiles. It must not present them as arbitrary file-system paths.

A Surface template and a Zone profile are related by an assignment, but they are not the same object. Different surfaces can use the same profile, and a surface can use different Main and FX profile IDs.

## Control Panel Shell

- ✅ Add a resizable ReaImGui window with a fixed-width vertical navigation bar on the left.
- ✅ Open General on the first launch. Restore the last selected tab, scroll position, and window geometry on later launches.
- ✅ Do not duplicate the selected sidebar label as a page heading. Do not show a product-status header or a manual Refresh control.
- ✅ Do not use horizontal separators in Lua UI unless the user requests them. Keep the approved separator between visible notification records.
- ✅ Keep `Save changes`, `Revert`, and status controls in a fixed footer for the combined Control Panel draft. Enable Save and Revert only when at least one page differs from its saved state.
- ✅ Close immediately when the draft is clean. When the draft is dirty, show `Save`, `Don't Save`, and `Cancel`: Save validates, persists, applies, and closes; Don't Save discards the draft and closes; Cancel keeps the window and draft open.
- ✅ Show pending C++ requests without blocking the REAPER UI thread.
- ✅ Use the shared startup, dependency checks, toolbar state, and shutdown behavior from [`../Scripts/script_host.lua`](../Scripts/script_host.lua).
- ✅ Use reusable controls from [`../Scripts/ui_components.lua`](../Scripts/ui_components.lua).
- [ ] Add one command to open the standalone configuration editor for advanced file editing and import tasks.

## Devices Tab

### Scope

The Devices tab replaces the device and assignment parts of the native C++ dialog in stages. It contains three sections:

1. MIDI and OSC I/O definitions.
2. Pages and Surface assignments.
3. Broadcaster and listener relationships.

The first release can show these sections as read-only diagnostics. Editing starts only after the C++ configuration protocol can validate and save the complete model.

- ✅ Present the three sections in workflow order: create an I/O definition, assign it to a Page, then optionally add listeners.
- ✅ Use compact master-detail lists so only the selected I/O definition, Page assignment, or listener opens for editing.
- ✅ Use three columns for Pages and Surface assignments: Pages, assignments on the selected Page, and the selected assignment editor.
- ✅ Keep runtime state outside the editable draft form and use `Reload saved configuration` for an explicit saved-state reload.
- ✅ Put the standalone editor and square `↻` reload actions at the right edge of the internal section row.
- ✅ Render I/O definitions, Pages, Surface assignments, and Relationships in rounded bordered lists with row-local Duplicate and `×` Remove actions.
- ✅ Show one green `●` or red `×` runtime state with a beginner-facing recovery instruction instead of repeating MIDI input and output status text.

### I/O definitions

- ✅ List MIDI definitions with name, channel count, input port, output port, refresh rate, and maximum messages per run.
- ✅ List OSC definitions with name, type, receive port, transmit address, transmit port, channel count, and maximum packets per run.
- ✅ Show whether each configured port is currently available.
- ✅ List only named REAPER MIDI ports, keep `Not selected` first, and preserve only the selected unavailable port as `Unavailable (#X)`.
- ✅ Provide Add, Duplicate, Edit, and Remove operations in the draft.
- ✅ Detect duplicate names and invalid numeric ranges before Save.
- ✅ Confirm Save when a change will disconnect and reconnect an active device.
- ✅ Return a clear C++ error if a configured MIDI or OSC endpoint cannot open.

### Pages and Surface assignments

- ✅ List Pages and their assigned surfaces without hiding skipped or invalid assignments.
- ✅ Edit the Page name and current Page flags.
- ✅ Edit the assignment name, I/O definition, Surface template, start channel, Main Zone profile, FX Zone profile, and Surface-scoped settings.
- ✅ Filter Surface templates by available Vendor and User files. Show the active source and whether User overrides Vendor.
- ✅ Keep the Surface template selector separate from the Zone profile selector.
- ✅ Default to one `Zone profile` selector for Main and FX.
- ✅ Add a `Use a different FX profile` advanced option that exposes separate Main and FX selectors.
- ✅ When the selected Surface template ID has a profile with the same ID, offer it as the default. Do not select it silently if another profile is already saved.
- ✅ Show the resolved Main source and the Vendor plus User FX layers before Save.
- ✅ Show source badges: `Vendor`, `User`, `Vendor + User`, and `Missing Main`.

### Missing Zone profile behavior

The GUI must not assume that every Surface template has a matching Vendor profile.

- ✅ Build the profile list from the union of valid Vendor and User profile IDs.
- ✅ If the selected Surface template has no profile with the same ID, keep the assignment incomplete and show `Select or create a Zone profile`.
- ✅ Offer an existing compatible profile first.
- ✅ Offer `Create User profile` for a new profile ID. Create a valid minimal Main scaffold through C++, not by direct Lua file writes.
- ✅ Offer `Copy to User` when a Vendor profile exists and the user wants an editable Main copy.
- ✅ Offer `Open standalone editor` for import, detailed zone editing, and dependency work. Open ReaPack package search when its executable is not installed.
- ✅ Never create or change files under `Zones/Vendor` from the Control Panel.
- ✅ Never silently replace a missing profile with an unrelated profile.
- ✅ Block Save when the Main profile is missing or invalid.
- ✅ Permit an empty FX layer only when the Main profile is valid. Let C++ prepare the User FX path when the assignment is applied.

### Broadcaster and listener relationships

- ✅ Show relationships as Page-local links between configured surface assignments.
- ✅ Edit Go Home, Modifiers, FX Menu, Selected Track FX, Selected Track Sends, and Selected Track Receives categories.
- ✅ Reject links to missing, skipped, or cross-Page assignments.
- ✅ Detect duplicate and circular relationships if the runtime does not support them.

### C++ dialog migration

[`../src/ui/config_dialog.cpp`](../src/ui/config_dialog.cpp) now contains only the launcher required by the REAPER Control/OSC/Web configuration callback. C++ format 2 parsing, validation, serialization, atomic writing, and runtime reload remain outside the dialog.

- ✅ Phase A: add an `Open Control Panel` button to the native dialog. Launch or focus Lua first, then close the REAPER-owned parent dialog as Cancel so it does not remain modal and unfinished native edits are not applied.
- [ ] Manually verify that `Open Control Panel` closes the native parent dialog on macOS and does not save unfinished native edits.
- ✅ Phase B: add a C++ read-only query that returns the complete parsed device, Page, assignment, listener, issue, and active-state model.
- ✅ Phase C: make the Lua Devices tab the preferred read-only view while all editing remains native.
- ✅ Phase D: add C++ draft validation and transactional Save for the complete configuration model.
- ✅ Phase E: enable Lua editing one section at a time, in this order: assignments and profiles, I/O definitions, Pages, then listeners.
- [ ] Phase F: compare native and Lua save output with representative MIDI and OSC configurations.
- ✅ Phase G: replace the native editor with a small launcher. Cross-platform checks and optional recovery information remain incomplete below.
- ✅ Keep the native launcher available because REAPER owns the configuration callback and expects a native window handle.

## General Tab

General contains runtime behavior that is not device routing, appearance, or logging.

- ✅ Move the current Product and Surface settings view from [`../Scripts/settings_ui.lua`](../Scripts/settings_ui.lua) into this tab without changing its C++ authority.
- ✅ Keep the Product and Surface scope selector and show the source of inherited values.
- ✅ Populate the Surface scope selector from C++ runtime assignments and label each option as `Page / Surface`. Do not accept an arbitrary Surface name.
- ✅ Render categories from [`../Scripts/settings_schema.conf`](../Scripts/settings_schema.conf) instead of hardcoding each control.
- ✅ Start with the current `Behavior` and `Timing` categories.
- ✅ Keep compiled defaults, Product values, and Surface values distinct without permanent Override controls. Show the source in a field tooltip. Editing creates the value at the current scope. A saved Product value has a right-click `Reset to default` action, and a saved Surface value has a right-click `Use Product value` action.
- ✅ Keep cross-setting validation, such as `LongHoldDelayMs > HoldDelayMs`, in the schema and C++ validation.
- ✅ Use the left half of the shared two-column page layout. Put labels to the left of equal-width Scope, Surface, Behavior, and Timing controls. Keep the right half available for later settings.
- ✅ Query the current configuration when General opens or its scope changes. Do not expose the internal Reload operation as an unexplained page button.
- [ ] Add future settings here only when they change general runtime behavior for the product or one surface.
- ✅ Do not put device ports, file paths, UI colors, window geometry, notification appearance, or logging values in this tab.

## Appearance Tab

### Settings ownership

The Appearance tab is one place to discover and edit visual settings. The code and persistence remain separated by feature.

- Common Control Panel item spacing, rounding, and disabled opacity are fixed at `8`, `4`, and `0.6`. Error color is not configurable.
- `OSK` owns zoom, font size, label case, layout spacing, window opacity, button opacity, LED boost, arrow angle, and title bar behavior.
- `OSD` owns position, alignment, width, height, margins, font size, background colors, and opacity.
- `Notifications` owns notification opacity, placement, width, message duration, maximum visible records, dismiss behavior, and navigation from a notification to its log record.

- ✅ Keep fixed common visual tokens and helpers in the existing shared theme and UI modules.
- ✅ Keep separate namespaced setting schemas for OSK, OSD, and Notifications.
- ✅ Keep Lua-only visual preferences in persistent ExtState. Do not put them in the product INI.
- ✅ Render all appearance groups in the Control Panel from their schemas.
- ✅ Render Appearance in two columns: OSK on the left, Notifications and OSD on the right.
- ✅ Use fixed shared form-control widths, compact drag controls for numbers, and the shared widget-configuration color picker and swatches.
- ✅ Keep OSK appearance and behavior settings on the Appearance page. Keep only Appearance navigation, device resets, MIDI reset, and OSD-bar position in the OSK context menu.
- ✅ Ensure every Control Panel draft change appears immediately in running OSK, OSD, and Notifications through a session-only preview overlay and revision.
- ✅ Put an Open or Show preview action beside each large OSK, Notifications, and OSD group heading. Start the visual script only when it is not already active, then show a real OSD or Notifications preview record.
- ✅ Apply previews to the draft only. Clear the preview overlay and restore the saved appearance after Revert, `Don't Save`, or Control Panel shutdown.
- [ ] Standardize shared visual language, but do not force the same layout or geometry on OSK, OSD, and Notifications.

### Notification behavior

- ✅ Add a Notification opacity setting with the default value `0.8`, which means 80 percent opaque.
- ✅ Add a close button for each visible notification.
- ✅ Use one shared square close-button size and the centered `×` symbol.
- ✅ Make the notification body clickable. A click opens or focuses the Control Panel on Logging, selects the source record, and scrolls it into view.
- ✅ Keep the close button separate from body navigation. Clicking close dismisses the notification and must not open the Control Panel.
- ✅ Identify a source record in the current unsegmented implementation by daily log ID and record start byte offset. The byte offset permits direct file access and supports records that use more than one text line. If the record is unavailable, open Logging and show that result.
- [ ] Add segment ID to the record identity when segmented rotation is implemented.
- ✅ Keep the Notifications script running when one notification is closed. Its close control must dismiss only that visible record so later messages can appear.
- ✅ Stop the Notifications script and set its REAPER toggle state to Off only when the user invokes its registered action to stop it.
- ✅ Keep dismissed records in the log file. Closing or dismissing a notification must not delete log data.
- ✅ Let the registered Notifications action start the script again.

## Logging Tab

### Canonical settings

Move the approved legacy global values into canonical setting metadata and C++ configuration handling. `FXParamsWrite` remains in the native window until a later decision because it creates raw FX diagnostic files instead of log records:

```ini
DebugLevel=Error
SurfaceInDisplay=0
SurfaceOutDisplay=0
SurfaceRawInDisplay=0
FXParamsWrite=0
```

- ✅ Add a `Logging` category to the canonical settings schema with Product scope.
- ✅ Define `DebugLevel` as an enum that matches the current C++ levels: Error, Warning, Notice, Info, and Debug.
- ✅ Do not add an `Off` value. The current numeric runtime value `0` means Error, not Off.
- ✅ Define Surface input, Surface output, and raw Surface input values as booleans with clear user labels.
- ✅ Document the exposed flags: raw input writes three-byte MIDI input, Surface input writes processed MIDI widget and OSC values, and Surface output writes MIDI and OSC output.
- ✅ Remove direct native ownership of `DebugLevel`, Surface input, Surface output, and raw Surface input after connecting schema load, Apply, Reload, and runtime updates.
- ✅ `SetDebugLevel` and `CycleDebugLevel` persist and immediately apply the canonical Product `DebugLevel`; Logging no longer needs to display a temporary runtime override.
- [ ] Decide whether `FXParamsWrite` should later become a canonical Boolean setting. Until then it remains only in the native window.
- ✅ Route all normal C++ and Lua logging through the shared product logger. Keep daily-file output enabled by default and provide a separate optional REAPER-console output setting.
- ✅ Prefix each log record with local time as `[HH:MM:SS]` without a calendar date.
- ✅ Keep popup notifications limited to NOTICE, WARNING, and ERROR records.

### Log viewer

- ✅ Store disposable runtime logs under the operating system user temporary directory, with one shared file per local day inside a monthly directory: `<temp>/<stable-product-id>/logs/<extstate-prefix>_logs_YYYY-MM/<extstate-prefix>_YYYY-MM-DD.log`.
- ✅ Resolve the log directory in C++ and give Lua the resolved path. C++ and Lua do not build the platform path independently.
- ✅ Document that the operating system can remove temporary logs at any time. Logs do not contain required product configuration or user data.
- ✅ Let simultaneous REAPER processes append to the same daily file and publish the active daily ID, resolved path, and initial reader offset to Lua.
- [ ] Start a new numbered segment when one daily log reaches a documented size. Apply a documented total size or retained-day limit across the product log directory so repeated launches cannot use unlimited temporary storage.
- ✅ Tail the current product log without blocking the REAPER UI thread.
- [ ] Show timestamp, severity, source, and message columns when the record format contains these fields.
- [ ] Add severity filters, text search, Pause, Resume, and Auto-scroll.
- [ ] Handle `Ctrl+C` explicitly in ReaImGui and copy the selected records. Do not require a dedicated Copy selected button.
- ✅ Use a read-only multiline text field in the reduced viewer so normal text selection and `Ctrl+C` work now. Keep record-aware copy behavior deferred with the full viewer.
- ✅ Add `Open log file` and `Open log folder` actions to the reduced Logging toolbar.
- ✅ Open the active log and its directory through the operating system default file and folder associations on Windows, macOS, and Linux.
- ✅ Keep Log level, logging checkboxes, and file or folder actions on one row. Do not show the full log path in the page.
- ✅ Show consecutive records on consecutive lines without an extra blank line.
- [ ] Add `Insert separator`. It writes a timestamped structured marker with an optional short label. Notifications must ignore this marker.
- [ ] Add `Delete all logs...`. Show the resolved directory, file count, and total size, require confirmation, close active readers and writers, remove the active and rotated log files, then create a new empty active log.
- [ ] Detect when the active segment becomes smaller, is replaced, or a new numbered segment becomes active. Open the correct segment, reset its byte offset when required, and continue reading without duplicate records or a permanent stopped state.
- ✅ Give each loaded record a daily log ID and record start byte offset in the current unsegmented implementation.
- [ ] Add segment ID when rotation is implemented.
- ✅ Accept a session-only navigation request from Notifications, open the Logging tab, select the matching record, and scroll it into view.
- [ ] Limit the in-memory view and load older records only on request.
- [ ] Make `Clear view` clear only the current GUI buffer.
- ✅ Show a useful empty state when file logging is disabled, the active file is unavailable, or it has no supported records.

## Stable REAPER Actions

Use the registration pattern from Reasonus-Native as a design reference:

- [Reasonus Control Panel action registration](https://github.com/navelpluisje/Reasonus-Native/blob/development/src/actions/action_ui_fp_8_control_panel.cpp)
- [Reasonus Control Panel window](https://github.com/navelpluisje/Reasonus-Native/blob/development/src/ui/windows/csurf_ui_fp_8_control_panel.cpp)

- ✅ Add the first C++ registry module for the Control Panel action. Register it once during plugin startup and unregister it during plugin shutdown.
- ✅ Register the Control Panel action with `custom_action`, handle it through `hookcommand2`, and expose its lifecycle state through `toggleaction`.
- ✅ Keep the Control Panel command ID stable and independent from the public product display name.
- ✅ Generate the Control Panel public action label from product identity.
- ✅ Resolve and launch the installed Control Panel ReaScript through the shared script command resolver.
- [ ] Avoid one global registration block for every action. Use one small registry table and shared handlers.
- ✅ Keep the Control Panel action state synchronized with the actual Lua window lifecycle.

Initial action contract:

| C++ `idStr` | External named command | Public purpose | Toggle |
| --- | --- | --- | --- |
| `REACTRLSURF_OPEN_CONTROL_PANEL` | `_REACTRLSURF_OPEN_CONTROL_PANEL` | Open or close the Control Panel | On while open |
| `REACTRLSURF_OPEN_OSK` | `_REACTRLSURF_OPEN_OSK` | Open or focus the OSK | On while at least one OSK window is active |
| `REACTRLSURF_TOGGLE_OSD` | `_REACTRLSURF_TOGGLE_OSD` | Start or stop the standalone OSD | On while active |
| `REACTRLSURF_TOGGLE_NOTIFICATIONS` | `_REACTRLSURF_TOGGLE_NOTIFICATIONS` | Start or stop Notifications | On while active |

Register `custom_action_register_t.idStr` without a leading underscore. REAPER exposes the named command with a leading underscore to `NamedCommandLookup`, scripts, and user configuration. The exact public label can contain the configured product name. The stable ID must not change during a product rename. The Control Panel user action toggles its window. Programmatic Open, Focus, and Select Tab calls do not toggle an existing panel closed. Other Open actions focus an existing window unless their own contract says that they toggle.

## Reasonus-Native Assessment

Reasonus-Native uses native C++ ReaImGui, not Lua. Its C++ component files cannot be reused directly by the Lua Control Panel without a port.

Use these ideas:

- Stable native REAPER actions that open and track GUI windows.
- A vertical page navigation bar.
- Separate page, component, style, color, and asset responsibilities.
- One draft with clear `Save changes` and `Revert` controls.
- Central style tokens instead of colors and dimensions inside each page.

Do not copy these parts:

- Native C++ ReaImGui components that duplicate existing [`../Scripts/ui_components.lua`](../Scripts/ui_components.lua) behavior.
- Large page-local setting state when schema-driven rendering can provide the same controls.
- Repeated action registration globals for each window.
- Reasonus product names, command IDs, assets, fonts, or branding.

If code is copied instead of reimplemented, verify its MIT license notice and attribution requirements before the change. Prefer architecture ideas and a clean Lua implementation.

## C++ and Lua Protocol

### Control Panel lifecycle operations

- ✅ Keep the lifecycle protocol limited to versioned Open, Close, Focus, and Select Tab requests plus the window lifecycle state required by the stable action.
- ✅ Launch the Lua entry script when the Control Panel is not active. When it is already active, send a Focus request instead of starting a second script instance.
- ✅ Do not design the complete Devices payload in Phase 1. Reuse the existing settings protocol in Phase 2 and add the Devices model in Phase 4.

### Device configuration operations

- [ ] Define versioned `Query`, `Validate`, `Apply`, `Reload`, and `Status` operations for the complete device configuration model.
- ✅ Treat the internal `Apply` operation as the implementation of the user-facing `Save changes` command. Do not show a separate Apply button.
- ✅ Return a configuration revision or source hash with every query.
- ✅ Reject Apply when the saved file changed after the draft was opened.
- [ ] Return structured field errors plus full parser issues.
- ✅ Write a completed temporary file, validate it, replace the target atomically, and reload only after validation succeeds.
- ✅ Keep the active runtime configuration unchanged when validation or saving fails.
- ✅ Report devices that failed to reconnect and keep the saved versus active state clear.

### General and Logging operations

- ✅ Extend the existing settings protocol instead of creating a second Product and Surface settings protocol.
- ✅ Keep setting names, types, defaults, scopes, categories, ranges, and constraints in the canonical schema.
- ✅ Let C++ return effective values, explicit overrides, inherited values, and sources.

### Appearance operations

- ✅ Keep appearance settings in Lua ExtState schemas.
- ✅ Add a small revision notification so running OSK, OSD, and Notifications scripts can reload changed appearance values.
- ✅ Do not use the device configuration protocol for Lua-only appearance values.

## Implementation Phases

### Phase 1. Contracts and shell - implementation complete, manual verification pending

- ✅ Confirm the stable action IDs, tab names, setting ownership, Phase 1 lifecycle protocol, and native fallback behavior.
- ✅ Add the Control Panel entry script, shell, navigation, page module interface, draft status, and close warning.
- ✅ Add stable C++ registration for the Control Panel action.
- ✅ Open the Control Panel from the native configuration dialog without removing current controls.
- [ ] Manually verify in REAPER that the stable action opens a closed panel, closes an open panel, shows the dirty-draft close prompt, and follows the Lua lifecycle state.
- [ ] Manually verify that the native button opens or focuses Lua, closes the blocking native parent, and does not apply unfinished native edits.

Ready when REAPER can toggle one Control Panel window, programmatic callers can open or focus it, and its action state is correct.

### ✅ Phase 2. General and Appearance - complete

- ✅ Move the existing schema-driven Product and Surface settings UI into General.
- ✅ Add schema-driven OSK, OSD, and Notifications groups to Appearance and keep common Control Panel styling fixed.
- ✅ Add Notification opacity `0.8` and dismiss behavior.
- ✅ Keep running visual scripts synchronized with both saved Appearance revisions and each session-only draft preview revision.
- ✅ Manually verify Product and Surface Save/Revert, Appearance preview/Save/Revert, quick-view synchronization, and per-record notification dismiss behavior in REAPER.

Ready when all existing input and visual settings are available from the Control Panel and old quick views still use the same values.

### Phase 3. Logging and Notifications - reduced implementation complete, deferred viewer work pending

- ✅ Add canonical Product settings for `DebugLevel`, `SurfaceRawInDisplay`, `SurfaceInDisplay`, and `SurfaceOutDisplay`, then apply them to runtime after load, Apply, and Reload.
- ✅ Keep `FXParamsWrite` native until its future ownership is decided.
- ✅ Move the product log to one resolved temporary daily file in a monthly directory.
- [ ] Add bounded segment rotation and retained-session cleanup later.
- ✅ Add a simple current-session NOTICE, WARNING, and ERROR text view with native file and folder opening.
- ✅ Make the reduced text view read-only but selectable, with normal text copy and source-record selection after notification navigation.
- [ ] Add the full bounded non-blocking viewer, filters, search, Pause, Resume, Auto-scroll, explicit `Ctrl+C`, and older-record loading later.
- ✅ Add notification-to-record navigation for the current unsegmented daily log.
- [ ] Add separator insertion and confirmed deletion of active and rotated logs later.
- ✅ Register the Notifications action and synchronize its toggle state with the Lua lifecycle.
- [ ] Manually verify that normal logging never opens the REAPER console.

Ready when the user can change logging behavior, inspect the log, and control notifications from stable REAPER actions.

### ✅ Phase 4. Read-only Devices

- ✅ Add the complete C++ device configuration query and runtime status response.
- ✅ Render I/O definitions, Pages, Surface assignments, Zone profiles, listeners, and parser issues.
- ✅ Show missing devices, Surface templates, and Main Zone profiles without hiding invalid assignments.

Ready when the Lua view explains the complete saved and active device configuration better than the native dialog.

### Phase 5. Device editing and Zone profiles

- ✅ Add complete draft editing and C++ validation.
- ✅ Add Zone profile selection, advanced separate Main and FX selection, User profile creation, and Vendor-to-User copy operations.
- ✅ Add transactional save, reload, reconnect status, conflict detection, and Revert.
- ✅ Add Page-local listener editing.
- ✅ Replace the expanded diagnostic forms with the three-section master-detail Devices workflow.
- ✅ Populate MIDI port selectors with current REAPER device names while preserving only an unavailable saved port index.
- ✅ Add a separate platform-specific ReaPack package for the standalone configuration editor executable and add its Control Panel launcher.
- [ ] Compare native and Lua save output with representative MIDI and OSC configurations.

Ready when representative MIDI and OSC configurations produce the same valid result from the native and Lua editors.

### Phase 6. Native dialog reduction

- [ ] Complete manual feature-parity checks on Windows, macOS, and Linux.
- ✅ Replace the native configuration editor body with the Control Panel launcher.
- [ ] Keep a safe fallback path for missing ReaImGui or missing Lua scripts.
- [ ] Remove obsolete native control resources only after the replacement is stable.

Ready when normal configuration uses Lua and the REAPER native callback still gives a clear recovery path.

## Acceptance Criteria

- [ ] The Control Panel has a vertical left navigation bar and opens as one window.
- [ ] Devices, General, Appearance, and Logging have clear ownership and no duplicate persistence rules.
- [ ] Lua never writes the product INI directly.
- [ ] A Surface template can select any valid Zone profile.
- [ ] A missing Vendor profile gives clear choices and never creates Vendor data.
- [ ] Save is blocked when Main Zones are missing.
- [ ] Existing Product and Surface inheritance works from General.
- [ ] OSK, OSD, and Notifications share visual tokens but keep feature-specific settings.
- [ ] Notifications default to opacity `0.8` and can be dismissed or stopped.
- [ ] Logging uses the product log and does not open the REAPER console automatically.
- [ ] Stable REAPER actions follow each GUI's documented Open or Toggle behavior and report correct toggle state.
- [ ] The standalone Bun editor remains the only full editor for surface, zone, snippet, import, and batch work.
- [ ] The native configuration window remains available until Lua feature parity is verified.

## Manual Verification

- [ ] Open the Control Panel from the REAPER Action List and from the native Control/OSC/Web configuration entry.
- [ ] Confirm that the native parent closes after `Open Control Panel` and that its unfinished changes are discarded.
- [ ] Open the same stable action again and confirm that it closes the existing window after the normal unsaved-change prompt. Confirm that internal notification, OSK, and native-dialog requests still open or focus one existing window instead of toggling it closed.
- [ ] Confirm that General lists every configured assignment as `Page / Surface`, then change a Product setting and a Surface override and verify inheritance after reload.
- [ ] Change values in every Appearance group and confirm that running OSK, OSD, and Notifications preview each change before Save.
- [ ] Preview an Appearance change, use Revert and `Don't Save`, and confirm that the persisted quick-view value is restored.
- [ ] Confirm that each notification uses a square `×` button, then dismiss one record and confirm that the script remains active and a later notification still appears.
- [ ] Stop and restart Notifications with its registered action and confirm that the action toggle state follows the script lifecycle.
- [ ] Click a notification and confirm that Logging opens, selects the exact source record, and scrolls it into view.
- [ ] Change every Logging setting and confirm the expected log records.
- [ ] Insert a labeled separator and confirm that it appears in the viewer but not as a notification.
- [ ] Rotate, truncate, and replace the active log while the viewer is open and confirm that tailing continues without duplicate records.
- [ ] Delete all logs after confirmation and confirm that the viewer and Notifications continue from a new empty active log.
- [ ] Select a Surface template with a matching profile, a different valid profile, a User override, and no matching profile.
- [ ] Confirm that a missing Main profile blocks Save and gives recovery actions.
- [ ] Save a MIDI and an OSC change and verify reconnect status.
- [ ] Change the product INI outside the Control Panel and confirm conflict detection.
- [ ] Remove ReaImGui or the installed Lua entry script and confirm that the native launcher shows recovery information.

## Non-Goals

- Full zone, surface template, snippet, legacy import, or batch text editing in Lua.
- Direct Lua serialization of the product INI.
- Automatic creation or modification of Vendor content.
- A required one-to-one relation between a Surface template ID and a Zone profile ID.
- Removal of the native configuration callback before Lua feature parity.
- Direct reuse of Reasonus C++ UI components in Lua.
