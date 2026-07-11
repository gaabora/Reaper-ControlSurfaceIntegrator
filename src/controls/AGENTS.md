# Controls Guide

## Purpose

- Implement CSI's runtime model: integrator, pages, surfaces, zones, widgets, navigation, parsing, modifiers, and feedback.

## Ownership

- `CSurfIntegrator` lifecycle and REAPER control-surface callbacks.
- Page and track navigation state.
- Zone and surface-template parsing, zone activation, widgets, feedback, and message generation.
- OSK/OSD ExtState bridges and shared surface behavior.

## Local Contracts

- `preamble.h` is the stable include hub and project precompiled header.
- The integrator run loop and REAPER callbacks share `csiMutex_`; preserve reentrant callback safety.
- Zone and surface parser changes affect user configuration formats and require documentation review.
- ExtState sections, keys, and serialized payloads shared with `Scripts/CSI` must change atomically.
- OSK configuration batches must be validated before replacing active contexts; file saves use a completed temporary file, timestamped backup, and recovery on replacement failure.
- Route OSK wheel acceleration through indexed relative actions while retaining compatibility with legacy `Inc`/`Dec` scroll payloads.
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
