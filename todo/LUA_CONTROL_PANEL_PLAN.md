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

- [`../src/ui/config_dialog.cpp`](../src/ui/config_dialog.cpp) is the native device configuration window. It edits MIDI and OSC I/O records, pages, surface assignments, zone folder fields, listener relationships, input settings, and logging values. It also writes the complete product INI.
- [`../Scripts/settings_ui.lua`](../Scripts/settings_ui.lua) already edits Product and Surface input settings through [`../Scripts/settings_protocol.lua`](../Scripts/settings_protocol.lua). C++ validates and saves these values.
- [`../Scripts/settings_schema.conf`](../Scripts/settings_schema.conf) is the canonical metadata source for Behavior and Timing settings.
- [`../Scripts/theme_settings.lua`](../Scripts/theme_settings.lua) contains shared style values and separate persistent OSK and OSD schemas.
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
- Keep the native C++ dialog until the Lua Devices page has feature parity and has passed manual runtime checks.
- Do not add surface, zone, snippet, legacy import, or batch text editing to this Control Panel. Provide a command that opens the standalone editor for these tasks.
- Keep one Appearance page, but do not create one flat settings table. Use separate `Common`, `OSK`, `OSD`, and `Notifications` setting groups.
- Treat runtime logs as disposable diagnostics. Store each REAPER process in its own bounded log session under the operating system user temporary directory.
- Use session log ID, segment ID, and record start byte offset as the internal record identity shared by the viewer and Notifications. Keep this identity hidden from the user.
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
- ✅ Show the current product name and configuration status in the header.
- ✅ Keep `Save changes`, `Revert`, and status controls in a fixed footer when the selected page has a draft. Enable Save and Revert only when the draft differs from the saved state.
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

### I/O definitions

- [ ] List MIDI definitions with name, channel count, input port, output port, refresh rate, and maximum messages per run.
- [ ] List OSC definitions with name, type, receive port, transmit address, transmit port, channel count, and maximum packets per run.
- [ ] Show whether each configured port is currently available.
- [ ] Provide Add, Duplicate, Edit, and Remove operations in the draft.
- [ ] Detect duplicate names and invalid numeric ranges before Save.
- [ ] Confirm Save when a change will disconnect and reconnect an active device.
- [ ] Return a clear C++ error if a configured MIDI or OSC endpoint cannot open.

### Pages and Surface assignments

- [ ] List Pages and their assigned surfaces without hiding skipped or invalid assignments.
- [ ] Edit the Page name and current Page flags.
- [ ] Edit the assignment name, I/O definition, Surface template, start channel, Main Zone profile, FX Zone profile, and Surface-scoped settings.
- [ ] Filter Surface templates by available Vendor and User files. Show the active source and whether User overrides Vendor.
- [ ] Keep the Surface template selector separate from the Zone profile selector.
- [ ] Default to one `Zone profile` selector for Main and FX.
- [ ] Add a `Use a different FX profile` advanced option that exposes separate Main and FX selectors.
- [ ] When the selected Surface template ID has a profile with the same ID, offer it as the default. Do not select it silently if another profile is already saved.
- [ ] Show the resolved Main source and the Vendor plus User FX layers before Save.
- [ ] Show source badges: `Vendor`, `User`, `Vendor + User`, and `Missing Main`.

### Missing Zone profile behavior

The GUI must not assume that every Surface template has a matching Vendor profile.

- [ ] Build the profile list from the union of valid Vendor and User profile IDs.
- [ ] If the selected Surface template has no profile with the same ID, keep the assignment incomplete and show `Select or create a Zone profile`.
- [ ] Offer an existing compatible profile first.
- [ ] Offer `Create User profile` for a new profile ID. Create a valid minimal Main scaffold through C++, not by direct Lua file writes.
- [ ] Offer `Copy to User` when a Vendor profile exists and the user wants an editable Main copy.
- [ ] Offer `Open standalone editor` for import, detailed zone editing, and dependency work.
- [ ] Never create or change files under `Zones/Vendor` from the Control Panel.
- [ ] Never silently replace a missing profile with an unrelated profile.
- [ ] Block Save when the Main profile is missing or invalid.
- [ ] Permit an empty FX layer only when the Main profile is valid. Let C++ prepare the User FX path when the assignment is applied.

### Broadcaster and listener relationships

- [ ] Show relationships as Page-local links between configured surface assignments.
- [ ] Edit Go Home, Modifiers, FX Menu, Selected Track FX, Selected Track Sends, and Selected Track Receives categories.
- [ ] Reject links to missing, skipped, or cross-Page assignments.
- [ ] Detect duplicate and circular relationships if the runtime does not support them.

### C++ dialog migration

Direct replacement of [`../src/ui/config_dialog.cpp`](../src/ui/config_dialog.cpp) is possible, but it must not be a direct Lua port. The native dialog is tied to the REAPER Control/OSC/Web configuration callback and currently owns complete INI serialization.

- ✅ Phase A: add an `Open Control Panel` button to the native dialog. Opening the Lua Control Panel keeps the native dialog open with all current controls available.
- [ ] Phase B: add a C++ read-only query that returns the complete parsed device, Page, assignment, listener, issue, and active-state model.
- [ ] Phase C: make the Lua Devices tab the preferred read-only view while all editing remains native.
- [ ] Phase D: add C++ draft validation and transactional Save for the complete configuration model.
- [ ] Phase E: enable Lua editing one section at a time, in this order: assignments and profiles, I/O definitions, Pages, then listeners.
- [ ] Phase F: compare native and Lua save output with representative MIDI and OSC configurations.
- [ ] Phase G: replace the native editor with a small launcher and status dialog only after feature parity and manual checks on Windows, macOS, and Linux.
- [ ] Keep the native launcher available because REAPER owns the configuration callback and expects a native window handle.

## General Tab

General contains runtime behavior that is not device routing, appearance, or logging.

- [ ] Move the current Product and Surface settings view from [`../Scripts/settings_ui.lua`](../Scripts/settings_ui.lua) into this tab without changing its C++ authority.
- [ ] Keep the Product and Surface scope selector and show the source of inherited values.
- [ ] Render categories from [`../Scripts/settings_schema.conf`](../Scripts/settings_schema.conf) instead of hardcoding each control.
- [ ] Start with the current `Behavior` and `Timing` categories.
- [ ] Keep compiled defaults, Product overrides, and Surface overrides visible and distinct.
- [ ] Keep cross-setting validation, such as `LongHoldDelayMs > HoldDelayMs`, in the schema and C++ validation.
- [ ] Add future settings here only when they change general runtime behavior for the product or one surface.
- [ ] Do not put device ports, file paths, UI colors, window geometry, notification appearance, or logging values in this tab.

## Appearance Tab

### Settings ownership

The Appearance tab is one place to discover and edit visual settings. The code and persistence remain separated by feature.

- `Common` owns shared font choices, spacing, rounding, base colors, disabled colors, error colors, and common window behavior.
- `OSK` owns zoom, font size, label case, layout spacing, window opacity, button opacity, LED boost, arrow angle, and title bar behavior.
- `OSD` owns position, alignment, width, height, margins, font size, background colors, and opacity.
- `Notifications` owns notification opacity, placement, width, message duration, maximum visible records, dismiss behavior, and navigation from a notification to its log record.

- [ ] Extract common visual tokens and helpers from feature code into the existing shared theme and UI modules.
- [ ] Keep separate namespaced setting schemas for Common, OSK, OSD, and Notifications.
- [ ] Keep Lua-only visual preferences in persistent ExtState. Do not put them in the product INI.
- [ ] Render all appearance groups in the Control Panel from their schemas.
- [ ] Keep the existing OSK context menu and OSD right-click settings as quick access views of the same schemas.
- [ ] Ensure a change made in one view appears in every other view after reload.
- [ ] Add a small preview area for common controls, OSD text, and notification appearance.
- [ ] Apply previews to the draft only. Restore the saved appearance after Revert or `Don't Save`.
- [ ] Standardize shared visual language, but do not force the same layout or geometry on OSK, OSD, and Notifications.

### Notification behavior

- [ ] Add a Notification opacity setting with the default value `0.8`, which means 80 percent opaque.
- [ ] Add a close button for each visible notification.
- [ ] Make the notification body clickable. A click must open or focus the Control Panel on Logging, select the source record, and scroll it into view.
- [ ] Keep the close button separate from body navigation. Clicking close dismisses the notification and must not open the Control Panel.
- [ ] Identify a notification source record by session log ID, segment ID, and record start byte offset. The byte offset permits direct file access and supports records that use more than one text line. If retention already removed that record, open Logging and show that the source record is no longer available.
- [ ] Keep the Notifications script running when one notification is closed. Its close control must dismiss only that visible record so later messages can appear.
- [ ] Stop the Notifications script and set its REAPER toggle state to Off only when the user deliberately disables Notifications or invokes its registered action to stop it.
- [ ] Keep dismissed records in the log file. Closing or dismissing a notification must not delete log data.
- [ ] Let the registered Notifications action start the script again.

## Logging Tab

### Canonical settings

Move the following legacy global values into canonical setting metadata and C++ configuration handling:

```ini
DebugLevel=Error
SurfaceInDisplay=0
SurfaceOutDisplay=0
SurfaceRawInDisplay=0
FXParamsWrite=0
```

- [ ] Add a `Logging` category to the canonical settings schema with Product scope.
- [ ] Define `DebugLevel` as an enum that matches the current C++ levels: Error, Warning, Notice, Info, and Debug.
- [ ] Do not add an `Off` value until C++ has an explicit no-logging level. The current numeric value `0` means Error, not Off.
- [ ] Define Surface input, Surface output, raw Surface input, and FX parameter write values as booleans with clear user labels.
- [ ] Audit each flag and document exactly which records it enables before exposing it in the GUI.
- [ ] Remove direct native control ownership only after the new schema values load, save, reload, and affect runtime behavior.
- [ ] Keep all normal C++ and Lua logging in the product log file. Do not open the REAPER console for automatic messages.
- [ ] Keep popup notifications limited to NOTICE, WARNING, and ERROR records.

### Log viewer

- [ ] Store disposable runtime logs under the operating system user temporary directory in a stable product subdirectory, with one session directory for each REAPER process, such as `<temp>/<stable-product-id>/logs/<session-id>`.
- [ ] Resolve the log directory in C++ and give Lua the resolved path. Do not let C++ and Lua build the platform path independently.
- [ ] Document that the operating system can remove temporary logs at any time. Logs must not contain required product configuration or user data.
- [ ] Give each REAPER process a unique log session so simultaneous REAPER instances never share one writable log file.
- [ ] Start a new numbered segment when one session log reaches a documented size. Apply a documented total size or retained-session limit across the product log directory so a long-running process and repeated launches cannot use unlimited temporary storage.
- [ ] Tail the current product log without blocking the REAPER UI thread.
- [ ] Show timestamp, severity, source, and message columns when the record format contains these fields.
- [ ] Add severity filters, text search, Pause, Resume, and Auto-scroll.
- [ ] Handle `Ctrl+C` explicitly in ReaImGui and copy the selected records. Do not require a dedicated Copy selected button.
- [ ] Add `Open current log file`, and `Open log folder` actions to the Logging toolbar or menu.
- [ ] Add `Insert separator`. It writes a timestamped structured marker with an optional short label. Notifications must ignore this marker.
- [ ] Add `Delete all logs...`. Show the resolved directory, file count, and total size, require confirmation, close active readers and writers, remove the active and rotated log files, then create a new empty active log.
- [ ] Detect when the active segment becomes smaller, is replaced, or a new numbered segment becomes active. Open the correct segment, reset its byte offset when required, and continue reading without duplicate records or a permanent stopped state.
- [ ] Give each loaded record a session log ID, segment ID, and record start byte offset so Notifications can request exact navigation to it.
- [ ] Accept a session-only navigation request from Notifications, open the Logging tab, select the matching record, and scroll it into view.
- [ ] Limit the in-memory view and load older records only on request.
- [ ] Make `Clear view` clear only the current GUI buffer.
- [ ] Show a useful empty state when the log file does not exist.

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
| `REACTRLSURF_OPEN_CONTROL_PANEL` | `_REACTRLSURF_OPEN_CONTROL_PANEL` | Open or focus the Control Panel | On while open |
| `REACTRLSURF_OPEN_OSK` | `_REACTRLSURF_OPEN_OSK` | Open or focus the OSK | On while at least one OSK window is active |
| `REACTRLSURF_TOGGLE_OSD` | `_REACTRLSURF_TOGGLE_OSD` | Start or stop the standalone OSD | On while active |
| `REACTRLSURF_TOGGLE_NOTIFICATIONS` | `_REACTRLSURF_TOGGLE_NOTIFICATIONS` | Start or stop Notifications | On while active |

Register `custom_action_register_t.idStr` without a leading underscore. REAPER exposes the named command with a leading underscore to `NamedCommandLookup`, scripts, and user configuration. The exact public label can contain the configured product name. The stable ID must not change during a product rename. Invoking an active Open action focuses its window and does not close it. Invoking a Toggle action changes its active state.

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

- ✅ Keep the Phase 1 protocol limited to versioned Open, Focus, and Select Tab requests plus the window lifecycle state required by the stable action.
- ✅ Launch the Lua entry script when the Control Panel is not active. When it is already active, send a Focus request instead of starting a second script instance.
- ✅ Do not design the complete Devices payload in Phase 1. Reuse the existing settings protocol in Phase 2 and add the Devices model in Phase 4.

### Device configuration operations

- [ ] Define versioned `Query`, `Validate`, `Apply`, `Reload`, and `Status` operations for the complete device configuration model.
- [ ] Treat the internal `Apply` operation as the implementation of the user-facing `Save changes` command. Do not show a separate Apply button.
- [ ] Return a configuration revision or source hash with every query.
- [ ] Reject Apply when the saved file changed after the draft was opened.
- [ ] Return structured field errors plus full parser issues.
- [ ] Write a completed temporary file, validate it, replace the target atomically, and reload only after validation succeeds.
- [ ] Keep the active runtime configuration unchanged when validation or saving fails.
- [ ] Report devices that failed to reconnect and keep the saved versus active state clear.

### General and Logging operations

- [ ] Extend the existing settings protocol instead of creating a second Product and Surface settings protocol.
- [ ] Keep setting names, types, defaults, scopes, categories, ranges, and constraints in the canonical schema.
- [ ] Let C++ return effective values, explicit overrides, inherited values, and sources.

### Appearance operations

- [ ] Keep appearance settings in Lua ExtState schemas.
- [ ] Add a small revision notification so running OSK, OSD, and Notifications scripts can reload changed appearance values.
- [ ] Do not use the device configuration protocol for Lua-only appearance values.

## Implementation Phases

### Phase 1. Contracts and shell - implementation complete, manual verification pending

- ✅ Confirm the stable action IDs, tab names, setting ownership, Phase 1 lifecycle protocol, and native fallback behavior.
- ✅ Add the Control Panel entry script, shell, navigation, page module interface, draft status, and close warning.
- ✅ Add stable C++ registration for the Control Panel action.
- ✅ Open the Control Panel from the native configuration dialog without removing current controls.
- [ ] Manually verify in REAPER that the stable action and native button open or focus one window and that the action state follows the Lua lifecycle.

Ready when REAPER can open or focus one Control Panel window and its action state is correct.

### Phase 2. General and Appearance

- [ ] Move the existing schema-driven Product and Surface settings UI into General.
- [ ] Add schema-driven Common, OSK, OSD, and Notifications groups to Appearance.
- [ ] Add Notification opacity `0.8` and dismiss behavior.
- [ ] Keep quick settings views synchronized with the Control Panel.

Ready when all existing input and visual settings are available from the Control Panel and old quick views still use the same values.

### Phase 3. Logging and Notifications

- [ ] Add canonical Logging settings and migrate current native values.
- [ ] Move the product log to the resolved temporary product log directory and add bounded rotation.
- [ ] Add the non-blocking log viewer and filters.
- [ ] Add notification-to-record navigation, separator insertion, and confirmed deletion of all active and rotated logs.
- [ ] Register the Notifications action and synchronize its toggle state.
- [ ] Verify that normal logging never opens the REAPER console.

Ready when the user can change logging behavior, inspect the log, and control notifications from stable REAPER actions.

### Phase 4. Read-only Devices

- [ ] Add the complete C++ device configuration query and runtime status response.
- [ ] Render I/O definitions, Pages, Surface assignments, Zone profiles, listeners, and parser issues.
- [ ] Show missing devices, Surface templates, and Main Zone profiles without hiding invalid assignments.

Ready when the Lua view explains the complete saved and active device configuration better than the native dialog.

### Phase 5. Device editing and Zone profiles

- [ ] Add complete draft editing and C++ validation.
- [ ] Add Zone profile selection, advanced separate Main and FX selection, User profile creation, and Vendor-to-User copy operations.
- [ ] Add transactional save, reload, reconnect status, conflict detection, and Revert.
- [ ] Add Page-local listener editing.

Ready when representative MIDI and OSC configurations produce the same valid result from the native and Lua editors.

### Phase 6. Native dialog reduction

- [ ] Complete manual feature-parity checks on Windows, macOS, and Linux.
- [ ] Replace the native configuration editor body with a Control Panel launcher, current status, and recovery information.
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
- [ ] Stable REAPER actions open or focus every product GUI and report correct toggle state.
- [ ] The standalone Bun editor remains the only full editor for surface, zone, snippet, import, and batch work.
- [ ] The native configuration window remains available until Lua feature parity is verified.

## Manual Verification

- [ ] Open the Control Panel from the REAPER Action List and from the native Control/OSC/Web configuration entry.
- [ ] Open the same action again and confirm that it focuses the existing window instead of creating a second one.
- [ ] Change a Product setting and a Surface override, then verify inheritance after reload.
- [ ] Change an appearance value in the Control Panel and confirm that its quick settings view shows the same value.
- [ ] Dismiss one visible notification and confirm that the script remains active and a later notification still appears.
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
