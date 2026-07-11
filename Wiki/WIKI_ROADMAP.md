# Wiki Roadmap

This roadmap now tracks only the user-facing pages that are still missing from `Wiki/`.

## Planned Pages

### FX zones guide

Cover how focused FX, selected-track FX, TCP FX, learn mode, and common FX actions fit together.

Primary verification sources:

- [Actions-Reference.md](Actions-Reference.md)
- [Configuration-Format.md](Configuration-Format.md)
- [../src/actions/actions_fx.h](../src/actions/actions_fx.h)
- [../src/actions/actions_display.h](../src/actions/actions_display.h)

### Surface-file guide

Explain `surface.txt`, widget definitions, feedback processors, and surface-level optional blocks.

Primary verification sources:

- [Configuration-Format.md](Configuration-Format.md)
- [../src/controls/surface_parser.cpp](../src/controls/surface_parser.cpp)
- [../src/controls/control_surface.cpp](../src/controls/control_surface.cpp)
- [../todo/SURFACE_TEMPLATE_EXTRACTION_PLAN.md](../todo/SURFACE_TEMPLATE_EXTRACTION_PLAN.md)

### Troubleshooting guide

Collect setup mistakes, MIDI/OSC exclusivity issues, missing files, port conflicts, and common zone/surface errors.

Primary verification sources:

- [02-Installation.md](02-Installation.md)
- [03-QuickStart.md](03-QuickStart.md)
- [Quick-Reference.md](Quick-Reference.md)
- current warning/error strings in `src/`

### Examples and templates guide

Provide copyable configurations that connect the conceptual pages with real zones and surfaces.

Primary verification sources:

- existing examples in the current wiki pages
- `CSI/` surface and zone templates in the repository or support packages
- [Actions-Reference.md](Actions-Reference.md)
- [Configuration-Format.md](Configuration-Format.md)
