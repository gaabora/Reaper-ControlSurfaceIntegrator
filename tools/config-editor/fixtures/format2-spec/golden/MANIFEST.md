# Migration golden coverage

Migration scenarios compare every file under `legacy/` with `expected/` and any `diagnostics.txt` or `notices.txt` file. Resolver scenarios compare a complete `input/` tree with the active files under `expected/`. Diagnostic-only scenarios keep all relevant source and destination files and list the required diagnostics without an expected converted file.

## ✅ Covered scenarios

- `product-config`: removes `Version=7.0` and converts root Settings, MIDI Device, Page, and Surface assignment records.
- `zone-structure`: converts magic Home and Track behavior, `Zone...ZoneEnd`, `IncludedZones`, `SubZones`, channel `|`, lifecycle activation, `GoSubZone`, `LeaveSubZone`, single-slash comments, Range, Delta, AccelerationDeltas, and state colors.
- `actions-and-values`: converts every lifecycle pseudo-widget name, `GoZone SelectedTrackFX`, ordinary `GoZone`, StepValues, and TicksPerStep.
- `surface-and-zone`: converts Widget blocks, Fader14Bit, Touch, WidgetClass, StepSize, AccelerationValues, ignored inline encoder ranges, RingProfile, BarProfile, explicit Channel, FaderPort value bars and peak meters, repeated scribble-strip mode, and OSKLayout.
- `learn-fx`: converts FXWidgetLayout, FXPrologue, FXEpilogue, and supported hash directives, and drops FXRowLayout with notices.
- `snippet`: converts a version 1 semantic snippet to a direct format 2 zone fragment.
- `unresolved`: covers a non-standard encoder range, missing channel metadata, command-shaped MFT colors, conflicting per-zone display modes, and a file used as both a layer and an independent zone.
- `osc-x32`: converts an OSCX32 Device, Page assignment, X32 fader curve, exact rotary delta profile and acknowledgement, and an OSC OSK layout. A focused check rejects an acknowledgement without its paired rotary input.
- `osc-basic`: converts OSC `AnyPress`, `Touch`, and integer feedback to universal typed primitives while preserving every explicit address and the OSC OSK layout.
- `links`: converts an order-dependent Broadcaster and Listener record to one explicit same-Page Link with only enabled share categories.
- `magic-targets`: converts every current magic Main zone name to explicit Role, Target, and optional BankTarget metadata.
- `gozones`: drops the deprecated loading manifest, converts recognized navigator metadata, removes ineffective standalone navigator lines, preserves magic-name precedence, and reports missing, unknown, unsupported, or conflicting entries.
- `mft-palette`: converts the complete legacy MFT palette and per-widget mode Companion, converts normal zone colors, and detects the raw-command branch only after resolving the selected Surface widget.
- `sce24-ring`: converts SCE24 ring value and color output to a universal Ring profile and Configure block, expands range colors, and restores the declared push color in the first three physical segments.
- `sce24-state`: converts SCE24 LED buttons and their Off/On colors to universal State feedback and an ordered `StateColors` list.
- `xtouch-text`: splits legacy XTouch display behavior into normal Text feedback and one shared TrackColor FeedbackGroup with exact channels, slots, members, and hue-range color mapping.
- `icon-track-color`: splits iCON display behavior into normal Text feedback and one shared RGB7 TrackColor FeedbackGroup with exact channels, slots, members, and blue correction.
- `scribble-strip-mode`: moves one confirmed legacy FaderPort scribble mode into a universal Surface Value initial message.
- `text-feedback`: converts SCE24 text and OLED output plus FaderPort scribble text to universal Text feedback, including fixed text, alignment, inversion, margins, font, constant colors, and state-indexed colors.
- `osk-color-calibration`: converts MIDI and OSC OSK layouts without a layout version, normalizes layout colors to opaque `#RRGGBB`, removes ignored legacy alpha bytes, and converts an enabled legacy device color calibration block.
- `overrides`: selects complete User Surface and snippet documents by ID while resolving Main and FX zones per ID without hiding unrelated Vendor zones.
- `saved-snippet`: removes saved snippet application markers while preserving the resolved bindings in their original order.
- `widget-channel`: extracts the complete trailing Widget number only for a legacy processor that depends on the Widget channel.
- `shared-layer`: converts one legacy SubZone referenced by several parents to one reusable Layer without reporting an ambiguity.
- `ambiguous-display`: rejects two possible upper-display sources for the same shared display-color channel.
- `destination-collision`: rejects an imported zone ID that already exists in the same User profile and collection, including case-only spelling differences.
- `faderport-classic-split`: converts the two-message FaderPort Classic fader to one symmetric 10-bit MIDISplit input and feedback codec.
- `mcu-character-displays`: converts MCU time characters, mode lights, and assignment letters to universal MIDICharacters and MIDIExact State feedback.
- `mcu-meter-display`: converts MCU, MCUXT, and C4 display rows together with MCU and MCUXT meters, their shared scale, and Surface initialization.
- `asparion-feedback`: converts Asparion RGB, ring, text, and meter processors while keeping explicit channel metadata.
- `qcon-meter`: converts both QCon Pro X master-meter sides without treating the side selector as a Surface channel.
- `icon-displays`: converts both Icon display protocols and their upper and lower row offsets.
- `faderport-feedback`: converts value bars and both FaderPort peak-meter status ranges, including channels 1, 8, 9, and 16.

## [ ] Executable golden coverage

The Bun tests compare complete generated output for `xtouch-text`, `icon-track-color`, `icon-displays`, `scribble-strip-mode`, `mcu-character-displays`, `mcu-meter-display`, `faderport-classic-split`, `faderport-feedback`, `mft-palette`, `sce24-ring`, `sce24-state`, `asparion-feedback`, `qcon-meter`, `osc-basic`, `osc-x32`, the MIDI and OSC Surfaces in `osk-color-calibration`, and the FaderPort and SCE24 Surfaces in `text-feedback`. The remaining scenarios are normative examples until their related migration stages have executable comparisons. Every future registry entry must add its legacy input, expected output, context restrictions, and ambiguous branch here in the same change.
