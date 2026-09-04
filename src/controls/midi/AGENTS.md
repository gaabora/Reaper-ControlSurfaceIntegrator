# MIDI Controls Guide

## Purpose

- Translate MIDI hardware input and output into CSI widgets, message generators, and feedback.

## Ownership

- MIDI surface lifecycle and I/O.
- Typed format 2 Widget construction and universal MIDI input and feedback codecs.

## Local Contracts

- Surface-template widget names and message syntax are public configuration contracts.
- MIDI byte ranges, 7-bit/14-bit values, pitch bend, SysEx framing, and device display formats must remain protocol-correct.
- Configured MIDI ports must recover from offline-to-online transitions without resetting unrelated surfaces.
- Add new protocol behavior through the canonical Surface I/O schema and the typed format 2 runtime. Do not add a string-token Widget registry or device-specific feedback class.
- `format2_midi_runtime.*` owns typed format 2 MIDI Surface validation and runtime Widget construction. Keep MIDI I/O lifecycle code in `midi_surface.*` and do not pass format 2 source through the legacy Surface text parser.
- Format 2 `MIDIRGB` feedback must use its declared Enable, Red, Green, and Blue messages through the generic codec. EncoderProfile lookup must use the referenced profile ID and must not assume `RotaryWidgetClass`.
- Format 2 MIDI Value, Encoder, and Ring runtime behavior belongs to `format2_midi_generators.h` and `fb_format2.h`. Apply the shared ValueProfile outside MIDI7, MIDI14, or MIDISplit packing. Keep `SignedBit`, `SignedBitFixed`, and `Relative7Bit` explicit. Resolve Ring value and style through its referenced `RingProfile`; apply ValueBase, Combine, StyleTarget, StyleShift, and StyleCombine only where the Surface schema permits them.
- Format 2 `MIDIPrefix` Press matches its declared two-byte prefix and emits a press for every matching third byte without inventing release behavior.
- Send each validated format 2 `Initialize` MIDI message once after Surface loading and again after MIDI I/O reconnect. Keep complete channel and system messages plus complete SysEx framing in the Surface file.
- Resolve format 2 `MIDI7` Meter values through the referenced MeterProfile. Keep scale thresholds, channel value prefixes, combination mode, and continuous refresh settings in Surface metadata instead of device-named feedback processors.
- Resolve format 2 MIDISysEx Text Payload items from the closed canonical field set. Apply Surface defaults and binding overrides for margins, font, presentation, text color, background color, and state colors before sending one complete packet.
- Resolve format 2 MIDISysEx State Payload items from a constant prefix followed by `State7` or the `Red7`, `Green7`, `Blue7` set. Resolve RGB state output from the binding `StateColors` list and apply Surface color calibration before sending one complete packet.
- Resolve format 2 Ring Configure Payload items through the closed segment-mask and RGB field set. Expand masks from RingProfile Segments, group equal colors, apply ColorCalibration, and send configuration when the Ring binding becomes active.
- Construct MIDIExact Press and Touch plus MIDI14 Value input and feedback directly from typed properties. Do not convert them back to legacy token lines.

## Work Guidance

- Validate message lengths and status/data bytes before indexing payloads.
- Keep device quirks isolated from generic MIDI behavior.
- Avoid heap work in high-frequency message and feedback paths where existing value types suffice.

## Verification

- Build the plugin.
- Test affected input and feedback with the target MIDI device or a MIDI monitor/simulator.
- Verify surface-template parsing and boundary values for the changed message type.

## Child DOX Index

- None.
