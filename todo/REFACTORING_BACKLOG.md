# Refactoring Backlog

The large `reaper_csurf_integrator/` to `src/` migration has already happened. The remaining refactor work is now about reducing complexity inside the current `src/` layout rather than completing the original folder move.

## Remaining Priorities

- Keep shrinking the biggest control/runtime files under `src/controls/`, especially the logic shared by `integrator.h`, `track_nav_manager.h`, and OSK-related control-surface behavior.
- Continue reducing action duplication across `src/actions/`, especially where track, send, and receive actions still follow near-parallel patterns.
- Replace compatibility includes such as [src/actions/reaper_actions.h](../src/actions/reaper_actions.h) once the split headers are the only entry points still in use.
- Keep teasing apart parsing, validation, and runtime activation so zone/surface loading paths are easier to test and reason about.

## Follow-Up Design Goals

- Reduce circular knowledge between pages, surfaces, zones, and navigators.
- Keep current contracts stable while extracting smaller units from large runtime classes.
- Prefer changes that are mechanically verifiable and easy to review over broad rewrites.
