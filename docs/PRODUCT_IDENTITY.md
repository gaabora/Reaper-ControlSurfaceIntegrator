# Product Identity and Paths

## Canonical source

[`../cmake/ProductIdentity.cmake`](../cmake/ProductIdentity.cmake) is the only source for public product names and install paths.

Current provisional values are:

| Value | Current setting |
| --- | --- |
| Display name | `ReaControlSurface` |
| Stable product ID | `reacontrolsurface` |
| Resource directory | `ReaControlSurface` |
| Configuration file | `ReaControlSurface.ini` |
| ExtState prefix | `ReaCtrlSurf` |
| REAPER registration ID | `ReaControlSurface` |
| Plugin filename | `reaper_csurf_integrator` |
| Installed script directory | `Scripts/ReaControlSurface` |
| Package prefix | `ReaControlSurface` |

Internal C++ class names and source folder names are not public identity values and do not need to match the display name.

## Generated consumers

CMake configure generates these files under `build/generated/`:

- `product_identity.h` for C++;
- `product_identity.ts` for the future Bun and TypeScript editor;
- `Scripts/ReaControlSurface/product_identity.lua` for installed Lua scripts;
- `product_identity.env` for CI archive and package steps.

CMake also configures the installed OSK, OSD, and OSK debug entry scripts with the current display name. CI loads `product_identity.env` after configure and uses `cmake --install` to create the package tree.

## Runtime structure

`ProductPaths` resolves paths below the REAPER resource directory:

```text
ReaControlSurface/
  ReaControlSurface.ini
  Surfaces/
    Vendor/<surface-id>/Surface.txt
    User/<surface-id>/Surface.txt
  Zones/<profile-id>/
    Main/*.zon
    FX/*.zon
  Snippets/
    BuiltIn/*.snippet
    User/*.snippet
  Backups/<operation-id>/
  Generated/ZoneRawFXFiles/
```

Surface, profile, and operation IDs use lowercase ASCII letters, digits, `_`, and `-`. When the same surface ID exists in both `Vendor` and `User`, the runtime uses the user copy. Existing symlinks must not resolve outside the typed root.

The runtime does not read the old `CSI/` root. Legacy names and layouts belong only in the future import workflow.

## Rename procedure

Change public values in `cmake/ProductIdentity.cmake`, then configure CMake again. Do not search and replace generated files or runtime paths manually.
