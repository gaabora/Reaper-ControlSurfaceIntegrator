# Architecture Backlog

These are the architectural problem areas that still appear open in the current codebase.

## Large Shared Runtime Objects

- `CSurfIntegrator`, `Page`, `TrackNavigationManager`, and `ActionContext` still own broad cross-cutting behavior and remain the main complexity hotspots under `src/controls/` and `src/actions/`.
- The current runtime still relies on tight object coupling between pages, surfaces, navigators, zones, and action contexts.

## Parsing And Validation

- Zone and surface parsing are split into dedicated files now, but validation is still shallow in places such as zone-navigation references and some config-dialog flows.
- Several string-heavy configuration paths remain contract-sensitive and deserve stronger typed validation or clearer error reporting.

## UI And Tooling Risk

- Native config/learn dialog code still contains defensive TODOs around reload safety, dialog lifetime, and source-of-truth duplication.
- The OSK/OSD runtime has shipped, but the remaining acceptance burden is still manual REAPER verification rather than automated coverage.

## Protocol And Contract Pressure

- The ExtState protocol is now centralized in [../docs/LUA_CPP_EXTSTATE_INTERFACE.md](../docs/LUA_CPP_EXTSTATE_INTERFACE.md), but any future changes still require coordinated edits across `src/controls/`, `src/shared/`, and `Scripts/`.
- User-facing action names and config syntax remain public contracts, so structural refactors have to preserve documentation and migration behavior.
