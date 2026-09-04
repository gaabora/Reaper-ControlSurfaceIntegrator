# OSC Controls Guide

## Purpose

- Connect OSC network endpoints to CSI surfaces, widgets, actions, and feedback.

## Ownership

- OSC surface setup, packet processing, widget definitions, and outgoing feedback.

## Local Contracts

- OSC addresses, argument types, and surface-template declarations are public integration contracts.
- Packet encoding/decoding uses the shared `oscpkt` and UDP facilities in `src/shared`.
- Network failures and malformed packets must not destabilize the REAPER control-surface run loop.
- Load format 2 OSC Surface files through `format2_osc_runtime.*`. OSCFloat and OSCInt Press and Touch inputs use the catalog-defined Match behavior. State feedback sends its explicit OffValue or OnValue. Value input can decode a referenced ValueProfile before dispatch, and Value feedback can apply its inverse, an echo guard, touch suppression, and common Surface InitialValue. Encoder input applies either Scale or a decode ValueProfile, then sends an optional constant Acknowledge only after accepted input. Apply Widget Channel before constructing processors. Typed State, Value, Text, and Color feedback processors must respond only to their matching Widget update type. OSCString Color with `Format=HexRGBA` applies the Surface ColorCalibration, sends lower-case `#RRGGBBAA` text to its explicit Address, and does not invent a `/Color` suffix at runtime.
- Keep OSC device behavior in the universal format 2 runtime. Do not restore the removed legacy OSC Widget processor classes or pointer-specific SendOSCMessage overloads.

## Work Guidance

- Keep address matching and type conversion explicit.
- Preserve surface-independent action and feedback behavior in the parent controls layer.
- Bound incoming data before conversion or dispatch.

## Verification

- Build the plugin.
- Send and receive representative OSC messages with the configured endpoint.
- Test malformed addresses/types and reconnect or shutdown behavior when affected.

## Child DOX Index

- None.
