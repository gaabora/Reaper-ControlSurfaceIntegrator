# OSK / OSD Remaining Work

The major OSK/OSD implementation plans were completed and removed from the backlog. The durable contracts now live in:

- [../docs/LUA_CPP_EXTSTATE_INTERFACE.md](../docs/LUA_CPP_EXTSTATE_INTERFACE.md)
- [../Scripts/AGENTS.md](../Scripts/AGENTS.md)

## Remaining Work

- Run the manual REAPER verification checklist in [../Scripts/AGENTS.md](../Scripts/AGENTS.md) after UI or protocol changes.
- Verify press, release, hold, double-press, wheel, fader drag, touch, and config-editor flows against real surfaces.
- Re-check multi-surface behavior, especially surfaces that share widget names.
- Re-check standalone OSD timeout refresh, positioning, and settings-window behavior in REAPER.
