# MIDI Controls Guide

## Purpose

- Translate MIDI hardware input and output into CSI widgets, message generators, and feedback.

## Ownership

- MIDI surface lifecycle and I/O.
- Widget factory and registration.
- Generic MIDI generators plus device-specific feedback for MCU, X-Touch, QCon, ICON, FaderPort, SCE24, Asparion, and Novation devices.

## Local Contracts

- Surface-template widget names and message syntax are public configuration contracts.
- MIDI byte ranges, 7-bit/14-bit values, pitch bend, SysEx framing, and device display formats must remain protocol-correct.
- Register new widget types through the existing factory/registration path.
- Device-specific feedback belongs in the matching `fb_*.h` file; generic behavior belongs in shared MIDI widget/generator code.

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
