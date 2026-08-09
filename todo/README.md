# Engineering Backlog

This folder keeps the work that is still open.

## Active Backlog

- [FIXME_TODO.md](FIXME_TODO.md) - current correctness, validation, and cleanup follow-ups pulled from the old mixed TODO plans
- [ARCHITECTURE_BACKLOG.md](ARCHITECTURE_BACKLOG.md) - architectural issues that still need design or implementation work
- [REFACTORING_BACKLOG.md](REFACTORING_BACKLOG.md) - remaining structural refactor targets after the `src/` migration and current file split
- [SURFACE_TEMPLATE_EXTRACTION_PLAN.md](SURFACE_TEMPLATE_EXTRACTION_PLAN.md) - open plan for moving more surface-specific behavior into `Surface.txt`
- [OSK_REMAINING_WORK.md](OSK_REMAINING_WORK.md) - remaining manual REAPER verification and follow-up checks for the shipped OSK/OSD work
- [CONFIGURATION_WORKFLOW_PLAN.md](CONFIGURATION_WORKFLOW_PLAN.md) - plan for product paths, the local configuration editor, legacy import, vendor layouts, snippets, and OSK zone-file creation
- [PSEUDO_MODIFIER_PLAN.md](PSEUDO_MODIFIER_PLAN.md) - independent plan for zone-scoped modifiers named after their source widgets

## What Was Removed

Historical plan snapshots that described already-implemented OSK/OSD work were removed during this reorganization. Their durable contracts now live in:

- [../docs/LUA_CPP_EXTSTATE_INTERFACE.md](../docs/LUA_CPP_EXTSTATE_INTERFACE.md)
- [../Scripts/CSI/AGENTS.md](../Scripts/CSI/AGENTS.md)
