# Current FIXME / TODO Backlog

These are the unresolved items that still appear valid after checking the current source tree.

## Correctness

- Guard the `GetDlgItemText` call in [src/ui/learn_dialog.cpp](../src/ui/learn_dialog.cpp) around line 323 so the learn callback does not hit a null or closed dialog.
- Fix the SubZone modifier hole noted in [src/controls/zone.cpp](../src/controls/zone.cpp) around line 37, where `Shift+WidgetN` in a SubZone can break the inherited unmodified `WidgetN` path.
- Audit [src/controls/control_surface.cpp](../src/controls/control_surface.cpp) `ForceClearTrack()` and decide whether it should stop after the first match or intentionally clear multiple widgets.

## Validation And UX

- Implement a safe close-on-reload path for CSI-owned windows in [src/ui/config_dialog.cpp](../src/ui/config_dialog.cpp), where the `CloseAllWindows()` skeleton is still commented out.
- Add validation for `GoZone`, `GoSubZone`, `LeaveSubZone`, and `GoHome` references during zone preprocessing in [src/controls/zone_manager.cpp](../src/controls/zone_manager.cpp).
- Improve the config version mismatch path so incompatible files are backed up and surfaced clearly instead of only logging and returning.

## Cleanup

- Review the `TrackInvertPolarity` naming carried in [src/actions/actions_track.h](../src/actions/actions_track.h) and [src/actions/actions_display.h](../src/actions/actions_display.h); the source already notes that `TrackInvertPhase` would be clearer.
- Tighten the return-value contract for `GetRestrictedLengthText()` in [src/controls/control_surface.h](../src/controls/control_surface.h), which still carries a TODO about returning the input pointer directly.
- Remove the compatibility shim once the remaining include sites no longer need [src/actions/reaper_actions.h](../src/actions/reaper_actions.h).
