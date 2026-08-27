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

## [ ] Remaining golden scenarios

- [ ] OSC Device and Surface conversion, including X32 ValueProfile and encoder acknowledgement.
- [ ] Broadcaster and Listener conversion to explicit Link blocks.
- [ ] Every remaining magic Target and BankTarget combination.
- [ ] Deprecated GoZones loading metadata and removal of standalone navigator-name lines.
- [ ] Normal MFT palette output with Companion and command-shaped color detection from a complete selected Surface plus zone set.
- [ ] SCE24 ring color ranges, uniform ring color, push color, and Ring Configure output.
- [ ] XTouch text conversion and shared TrackColor FeedbackGroup output.
- [ ] Fixed text, alignment, inversion, margins, font, constant colors, and state-indexed text/background colors.
- [ ] OSC and MIDI OSKLayout plus ColorCalibration conversion, including `#` color prefixes.
- [ ] Surface and snippet whole-file User overrides plus per-zone Main and FX overlay selection.
- [ ] Saved snippet marker removal while preserving resolved binding content.
- [ ] Action rename registry entries after their final names and parameter transformations are approved.
- [ ] Case-sensitive and ambiguous branches for Widget channel extraction, zone-layer role selection, display pairing, and destination collisions.
