# Migration golden coverage

Each scenario compares every file under `legacy/` with `expected/` and any `diagnostics.txt` or `notices.txt` file.

## ✅ Covered scenarios

- `product-config`: removes `Version=7.0` and converts root Settings, MIDI Device, Page, and Surface assignment records.
- `zone-structure`: converts magic Home and Track behavior, `Zone...ZoneEnd`, `IncludedZones`, `SubZones`, channel `|`, lifecycle activation, `GoSubZone`, `LeaveSubZone`, single-slash comments, Range, Delta, AccelerationDeltas, and state colors.
- `actions-and-values`: converts every lifecycle pseudo-widget name, `GoZone SelectedTrackFX`, ordinary `GoZone`, StepValues, and TicksPerStep.
- `surface-and-zone`: converts Widget blocks, Fader14Bit, Touch, WidgetClass, StepSize, AccelerationValues, ignored inline encoder ranges, RingProfile, BarProfile, explicit Channel, FaderPort value bars, repeated scribble-strip mode, and OSKLayout.
- `learn-fx`: converts FXWidgetLayout, FXPrologue, FXEpilogue, and supported hash directives, and drops FXRowLayout with notices.
- `snippet`: converts a version 1 semantic snippet to a direct format 2 zone fragment.
- `unresolved`: covers a non-standard encoder range, invalid FPVUMeter data, missing channel metadata, command-shaped MFT colors, conflicting per-zone display modes, and a file used as both a layer and an independent zone.
- `osc-x32`: converts an OSCX32 Device, Page assignment, X32 fader curve, rotary delta scaling and acknowledgement, and an OSC OSK layout.
- `links`: converts an order-dependent Broadcaster and Listener record to one explicit same-Page Link with only enabled share categories.
- `magic-targets`: converts every current magic Main zone name to explicit Role, Target, and optional BankTarget metadata.
- `gozones`: drops the deprecated loading manifest, converts recognized navigator metadata, removes ineffective standalone navigator lines, preserves magic-name precedence, and reports missing, unknown, unsupported, or conflicting entries.
- `mft-palette`: converts the complete legacy MFT palette and per-widget mode Companion, converts normal zone colors, and detects the raw-command branch only after resolving the selected Surface widget.
- `sce24-ring`: converts SCE24 ring value and color output to a universal Ring profile and Configure block, expands range colors, and restores the declared push color in the first three physical segments.
- `xtouch-text`: splits legacy XTouch display behavior into normal Text feedback and one shared TrackColor FeedbackGroup with exact channels, slots, members, and hue-range color mapping.

## [ ] Remaining golden scenarios

- [ ] Fixed text, alignment, inversion, margins, font, constant colors, and state-indexed text/background colors.
- [ ] OSC and MIDI OSKLayout plus ColorCalibration conversion, including `#` color prefixes.
- [ ] Surface and snippet whole-file User overrides plus per-zone Main and FX overlay selection.
- [ ] Saved snippet marker removal while preserving resolved binding content.
- [ ] Action rename registry entries after their final names and parameter transformations are approved.
- [ ] Case-sensitive and ambiguous branches for Widget channel extraction, zone-layer role selection, display pairing, and destination collisions.
