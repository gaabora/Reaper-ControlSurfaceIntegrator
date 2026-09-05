# Controls Guide

## Purpose

- Implement CSI's runtime model: integrator, pages, surfaces, zones, widgets, navigation, parsing, modifiers, and feedback.

## Ownership

- `CSurfIntegrator` lifecycle and REAPER control-surface callbacks.
- Page and track navigation state.
- Zone and surface-template parsing, zone activation, widgets, feedback, and message generation.
- OSK/OSD ExtState bridges and shared surface behavior.
- Product and Device setting parsing, effective-value resolution, atomic persistence, and the settings and Devices ExtState bridges.
- Shared format 2 lexical analysis and typed document parsing before runtime object construction.

## Local Contracts

- `preamble.h` is the stable include hub and project precompiled header.
- The integrator run loop and REAPER callbacks share `csiMutex_`; preserve reentrant callback safety.
- Zone and surface parser changes affect user configuration formats and require documentation review.
- Treat only `//` as Surface and Zone comments. Preserve single slash tokens as data so OSC addresses such as `/ch/01/mix/fader` reach the OSC widget parser.
- Treat the exact `#WidgetType`, `#DisplayRow`, `#RingStyle`, `#DisplayFont`, and `#SupportsColor` Learn template directives as metadata, not as comments.
- ExtState sections, keys, and serialized payloads shared with `Scripts/` must change atomically.
- Parse Product overrides from the root `Settings` block and Device overrides from its nested `Settings` block. Resolve compiled defaults, Product, and Device in that order. Reject one invalid scope as a unit instead of partially applying it.
- Apply canonical Product `DebugLevel`, `WriteLogFile`, `ShowLogInReaperConsole`, `SurfaceRawInDisplay`, `SurfaceInDisplay`, and `SurfaceOutDisplay` values to the runtime logger and legacy diagnostic globals after initial configuration load, Apply, and Reload.
- Route runtime debug-level actions through the canonical Product `DebugLevel` writer so the Control Panel, configuration file, and active logger stay synchronized.
- Keep `ReaCtrlSurf_SETTINGS_CMD` requests correlated with `ReaCtrlSurf_SETTINGS` responses. Apply must validate before atomic file replacement. Reload must keep current runtime values when validation or runtime matching fails.
- A missing product configuration is an initial-setup state for the Settings and Devices bridges, not a read failure. Devices Query must return an empty editable model plus discovery options and an empty-source revision so Apply can create the parent directory and first valid file. Settings Query must identify the missing configuration and direct Lua to Devices. Existing unreadable files remain errors.
- Include every configured Device ID in each successful settings Query response so Lua can select a valid Device scope without reading the product configuration.
- Keep `ReaCtrlSurf_DEVICES_CMD` Query responses complete. Include saved MIDI and OSC definitions, currently named REAPER MIDI port options, separate MIDI input/output open states, standalone editor availability, Pages, Surface assignments, Zone profile availability, listeners, parser issues, and matching runtime status without hiding valid assignments that have missing resources. Validate complete Lua drafts in C++, reject stale revisions, replace the configuration atomically, and reconnect CSI only after a successful Apply. Zone profile operations may create or copy only under the User root. Launch the optional standalone editor only from its typed product Tools path.
- Register bundled OSK, OSD, and Notifications ReaScripts through the shared ReaScript command resolver.
- Consume the OSK `Open` command without changing enabled surfaces when at least one is enabled. If none is enabled, enable every current-Page surface, republish its data, and open the OSK ReaScript.
- Start the Notifications ReaScript before configuration loading so it can display new NOTICE, WARNING, and ERROR log entries without opening the REAPER console.
- Respect the session-only Notifications Enabled value during later CSI initialization so a deliberate stable-action stop is not immediately reversed.
- Publish the selected track name with its adjacent track names and selected track color to OSD after track selection. Publish edit cursor position as `[bar/beat]` during CSI cursor movement, rewind, and fast-forward.
- Treat an explicit action-line OSD value as authoritative text and mark it in the shared payload. Do not interpret template variable names in C++; Lua owns their definitions and expansion.
- Load configuration, surfaces, zones, logs, backups, snippets, and generated files through the generated product identity and `ProductPaths`; do not add runtime fallback to legacy CSI paths.
- Parse the brace-format product configuration into value-only records before creating runtime objects. A missing file or unrecoverable document structure is fatal. A malformed Device, Page child, Surface, or Link must report its line and must not stop other valid entries from loading. Apply Link relationships only after all available Surfaces are created.
- The product `.conf` brace parser returns the shared `IntegratorConfig` model and reuses the common lexer, syntax tree, and delimiter validation. Do not add a second line parser or legacy runtime fallback.
- Serialize the format 2 product configuration through `SerializeFormat2IntegratorConfig`; do not add a second canonical C++ writer for individual consumers.
- In the format 2 product configuration, resolve settings as compiled defaults, Product, then Device. A Page Surface assignment cannot contain settings.
- Read the positive channel count from Surface `@Meta Channels`, not from Device. Resolve assigned Device channel counts before runtime I/O construction and reject one Device assigned to Surface templates with different counts.
- OSK configuration batches must be validated before replacing active contexts; file saves use a completed temporary file, timestamped backup, and recovery on replacement failure.
- Vendor zones are read-only. Use User Main when its directory exists, otherwise use Vendor Main. Load Vendor and User FX zones together, with an exact User `Zone` name overriding Vendor. Create the User FX directory during initialization. A Vendor Main edit requires confirmation and an atomic Main-only copy. A Vendor FX edit copies only that file to the matching User FX path. Reload zones after activating either User copy.
- Keep OSK zone creation in `zone_file_creator.*`. Accept only the supported fixed scaffold destinations, create one complete temporary file before rename, reject case-insensitive file or zone-name duplicates, and never edit a parent zone as part of creation.
- Route OSK wheel acceleration through indexed relative actions using the current structured scroll payload only.
- Resolve OSK display labels in this order: explicit KeyLabel, meaningful OSD text, action title, then the surface label or widget name.
- Publish OSK tooltip label maps with distinct NoMod, modifier, Hold, DoublePress, and combined pseudo-modifier names so Lua can present every binding line.
- Publish OSK layout semantic metadata from parsed widget capabilities, including role, input, feedback, widget class, and press/scroll/value/touch targets; visual shape must not be required for fader or rotary behavior.
- Publish OSK continuous-widget state with value availability and volume/pan kind, and prefer current action track colors for meaningful fader/rotary feedback colors.
- Persist OSK enabled state per surface and restore/open enabled OSK surfaces after CSI startup initialization.
- Publish OSK lifecycle state, terminate the script when the final enabled Surface closes, and support explicit per-Surface plus all-Surface toggles so a failed instance can be restarted.
- The pre-release Lua bridge uses only documented `ReaCtrlSurf_*` sections and scoped configuration statuses; do not add legacy aliases without a publication requirement.
- Surface-independent behavior belongs here; protocol-specific behavior belongs in `midi/` or `osc/`.
- Keep the format 2 lexer independent from legacy `GetTokens()` and preserve source offsets plus one-based line and column locations for every token and diagnostic.
- In legacy `GetTokens()`, keep whitespace inside a named list property such as `RingColors=[ #FF0000, #000000 ]` in one property token. Do not combine positional action parameters such as `[ 0.716 ]`.
- Build format 2 document-specific parsers on the shared syntax tree. Do not tokenize or scan the same source again inside Surface, Zone, Learn FX, OSK, or snippet consumers.
- Keep a compiled format 2 action-context specification as an index into its immutable source binding vector. Expand only terminal `#` channel-placeholder specifications and do not clone the typed zone or channel-neutral bindings.
- Store the effective Navigator and optional slot override in each `ActionContext`. Actions must use `ActionContext::GetNavigator()`, `GetTrack()`, and `GetSlotIndex()` instead of reading the Zone navigator directly, because one format 2 Zone can contain channel contexts for different tracks or slots.
- Create format 2 binding contexts through `format2_zone_runtime.*` from the existing typed document and compiled binding specifications. Resolve every Widget, Action, Navigator, selector, and banked slot before mutating the runtime Zone, so one invalid binding cannot leave a partially loaded Zone.
- Resolve format 2 Main and FX Vendor/User sources per case-insensitive zone ID before profile validation. A User source blocks the matching Vendor source even when the User document is invalid, and same-layer duplicates must not select a source by directory order.
- Validate the resolved active Main profile before runtime Zone construction. Require exactly one valid `Role=Home`, reject unavailable structural references and cycles, require `ZoneLayers` targets to use `Role=Layer`, and reject Role=Layer targets in `IncludedZones`. Keep an invalid unreferenced non-required zone unavailable without disabling independent valid zones.
- Keep format 2 action parameter semantics in `format2_action_metadata.*`. Zone document parsers consume typed navigation-reference metadata and must not recognize `GoZone` or `EnterZoneLayer` with local action-name branches.
- Load complete format 2 Zone profiles through `format2_zone_profile_loader.*`. Keep parsed documents and source descriptors in the same deterministic order so resolver indices select the existing typed document without reopening or reparsing its file.
- Let `ZoneManager` select the typed initialization path only when its profile contains format 2 documents. Reject a mixed legacy and format 2 profile, resolve the complete profile before creating Zones, re-resolve after Surface-specific binding validation, and retain the typed documents for later FX activation without reopening their files.
- Build format 2 `IncludedZones` and `ZoneLayers` from retained typed documents. Included zones keep their own effective target, each layer gets a separate runtime instance per parent and inherits that parent's effective target and bank context, and only exact current format 2 contexts consume input or feedback ownership.
- Store each format 2 zone's derived runtime target on the `Zone` object. Use it for track-collection mode, selected-track lifetime, Page-scope navigation, and format 2 `GoZone` routing. Derive selected-track Link routing from the typed bank target and filter it through the configured Link categories. Filename checks and legacy special-name routing must stay inside the legacy path.
- Store the derived format 2 bank target with the runtime target. Resolve banked action slot indexes when the action runs, and let format 2 `Bank Amount` use those typed values. String bank names are legacy-only.
- Use explicit manager actions for format 2 FX contexts. `ToggleSelectedTrackFX` is not a zone navigation target, clear actions call typed manager methods, and active FX-menu zones are identified by their runtime bank target. Keep `SelectedTrackFX` and FX-menu filename checks in legacy navigation only.
- Resolve unqualified format 2 two-state button bindings through `DefaultButtonTrigger`; do not apply the button default to continuous inputs. Keep explicit Press, Tap, Release, Hold, LongHold, and DoublePress event identity on `ActionContext`, but keep active gesture timing and captured press contexts in one Zone-level recognizer per physical Widget. Use Product or Device timing values with binding delay overrides and apply each standard modifier declaration's effective Momentary, Latch, or Hybrid mode. Keep PseudoModifier runtime work in its separate plan.
- Read format 2 primitive names, capabilities, encoding compatibility, properties, value and relationship rules, nested transport blocks, reusable profiles, and Surface-level block schemas from the CMake-generated view of `Scripts/surface_io_schema.conf`. Do not add a second C++ catalog.
- Load format 2 MIDI and OSC Surfaces through `Format2SurfaceDocument` before runtime Widget construction. The MIDI runtime bridge supports MIDI7, MIDI14, and MIDISplit Value input and feedback with optional shared ValueProfile conversion, the converted FaderPort primitive set, MIDI7 Bar and Meter feedback, MIDIRGB state brightness, MIDIPalette color feedback with one optional fixed Companion message, MIDISysEx State, Value, Color, Ring, Bar, Meter, and Text feedback through their closed typed payload fields, and Surface-level MIDI initialization messages. The OSC runtime bridge supports OSCFloat and OSCInt Press and Touch inputs with typed Match behavior, State feedback with explicit Off and On values, Value input and feedback with optional ValueProfile conversion, Encoder input with Scale or a decode ValueProfile and optional constant Acknowledge output, OSCString Text feedback with an optional TextProfile, OSCInt palette Color feedback, OSCString HexRGBA Color feedback, OSCFloat or OSCInt Ring feedback with separate value and optional style addresses, and OSCFloat or OSCInt Meter feedback. Feedback Value InitialValue belongs to common Surface initialization and must not be sent during parsing. Both bridges must report unsupported typed primitives instead of passing format 2 source through the legacy text parser. Apply typed OSK layout and color calibration data directly without reparsing the Surface file.
- Runtime Surface loading accepts only format 2 documents. A missing file or a document without format 2 metadata is a focused error. Legacy Surface conversion belongs only to the Bun importer.
- Construct format 2 MIDI runtime generators and feedback processors directly from typed primitives. Do not restore the removed string-token `MidiWidgetRegistry` adapter.
- Resolve Ring and Bar style in this order: explicit binding style, the ActionContext internal FeedbackShape, then the profile default or supported fallback. Do not expose FeedbackShape as a zone property.

## Work Guidance

- Keep ownership explicit: `unique_ptr` for owned objects and documented raw pointers for non-owning references.
- Preserve the flow from hardware input to widget, zone/action context, action, and feedback.
- Keep parser errors actionable and avoid accepting ambiguous configuration silently.

## Verification

- Build the plugin.
- Load CSI in REAPER and exercise the affected lifecycle, parser, navigation, zone, widget, or ExtState path.
- For configuration changes, test both valid input and representative malformed input.

## Child DOX Index

- `midi/AGENTS.md` - MIDI surfaces, message generators, widget registration, and device feedback formats.
- `osc/AGENTS.md` - OSC surfaces, widgets, network input/output, and OSC feedback.
