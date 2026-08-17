# Controls Guide

## Purpose

- Implement CSI's runtime model: integrator, pages, surfaces, zones, widgets, navigation, parsing, modifiers, and feedback.

## Ownership

- `CSurfIntegrator` lifecycle and REAPER control-surface callbacks.
- Page and track navigation state.
- Zone and surface-template parsing, zone activation, widgets, feedback, and message generation.
- OSK/OSD ExtState bridges and shared surface behavior.
- Product and Surface setting parsing, effective-value resolution, atomic persistence, and the settings ExtState bridge.

## Local Contracts

- `preamble.h` is the stable include hub and project precompiled header.
- The integrator run loop and REAPER callbacks share `csiMutex_`; preserve reentrant callback safety.
- Zone and surface parser changes affect user configuration formats and require documentation review.
- Treat only `//` as Surface and Zone comments. Preserve single slash tokens as data so OSC addresses such as `/ch/01/mix/fader` reach the OSC widget parser.
- Treat the exact `#WidgetType`, `#DisplayRow`, `#RingStyle`, `#DisplayFont`, and `#SupportsColor` Learn template directives as metadata, not as comments.
- ExtState sections, keys, and serialized payloads shared with `Scripts/` must change atomically.
- Parse Product overrides from `Settings` lines and Surface overrides from their `Surface=` assignment. Resolve compiled defaults, Product, and Surface in that order. Reject one invalid scope as a unit instead of partially applying it.
- Keep `ReaCtrlSurf_SETTINGS_CMD` requests correlated with `ReaCtrlSurf_SETTINGS` responses. Apply must validate before atomic file replacement. Reload must keep current runtime values when validation or runtime matching fails.
- Register bundled OSK and OSD ReaScripts with their absolute resource paths. If REAPER reports an already registered script as an add failure, reuse one unique matching command from the Main action section and reject ambiguous matches.
- Load configuration, surfaces, zones, logs, backups, snippets, and generated files through the generated product identity and `ProductPaths`; do not add runtime fallback to legacy CSI paths.
- Parse the product INI into value-only configuration records before creating runtime objects. Missing files and incompatible versions are fatal. A malformed IO, Page child, Surface, or Listener entry must report its line and must not stop other valid entries from loading. Apply Listener relationships only after all available Surfaces are created.
- OSK configuration batches must be validated before replacing active contexts; file saves use a completed temporary file, timestamped backup, and recovery on replacement failure.
- Vendor zones are read-only. Use User Main when its directory exists, otherwise use Vendor Main. Load Vendor and User FX zones together, with an exact User `Zone` name overriding Vendor. Create the User FX directory during initialization. A Vendor Main edit requires confirmation and an atomic Main-only copy. A Vendor FX edit copies only that file to the matching User FX path. Reload zones after activating either User copy.
- Keep OSK zone creation in `zone_file_creator.*`. Accept only the supported fixed scaffold destinations, create one complete temporary file before rename, reject case-insensitive file or zone-name duplicates, and never edit a parent zone as part of creation.
- Route OSK wheel acceleration through indexed relative actions using the current structured scroll payload only.
- Resolve OSK display labels in this order: explicit KeyLabel, meaningful OSD text, action title, then the surface label or widget name.
- Publish OSK tooltip label maps with distinct NoMod, modifier, Hold, DoublePress, and combined pseudo-modifier names so Lua can present every binding line.
- Publish OSK layout semantic metadata from parsed widget capabilities, including role, input, feedback, widget class, and press/scroll/value/touch targets; visual shape must not be required for fader or rotary behavior.
- Publish OSK continuous-widget state with value availability and volume/pan kind, and prefer current action track colors for meaningful fader/rotary feedback colors.
- Persist OSK enabled state per surface and restore/open enabled OSK surfaces after CSI startup initialization.
- The pre-release Lua bridge uses only documented `ReaCtrlSurf_*` sections and scoped configuration statuses; do not add legacy aliases without a publication requirement.
- Surface-independent behavior belongs here; protocol-specific behavior belongs in `midi/` or `osc/`.

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
