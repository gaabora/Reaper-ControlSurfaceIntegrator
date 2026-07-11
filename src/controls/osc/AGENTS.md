# OSC Controls Guide

## Purpose

- Connect OSC network endpoints to CSI surfaces, widgets, actions, and feedback.

## Ownership

- OSC surface setup, packet processing, widget definitions, and outgoing feedback.

## Local Contracts

- OSC addresses, argument types, and surface-template declarations are public integration contracts.
- Packet encoding/decoding uses the shared `oscpkt` and UDP facilities in `src/shared`.
- Network failures and malformed packets must not destabilize the REAPER control-surface run loop.

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
