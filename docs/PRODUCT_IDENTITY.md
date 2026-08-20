# Product Identity and Paths

## Canonical source

[`../Scripts/product_identity.conf`](../Scripts/product_identity.conf) is the only source for public product names, stable product action IDs, and install paths. Lua reads it directly. [`../cmake/ProductIdentity.cmake`](../cmake/ProductIdentity.cmake) validates the same file and derives build and install paths from it.

Current provisional values are:

| Value | Current setting |
| --- | --- |
| Display name | `ReaControlSurface` |
| Stable product ID | `reacontrolsurface` |
| Resource directory | `ReaControlSurface` |
| Resource install directory | `Data/ReaControlSurface` |
| Configuration file | `ReaControlSurface.ini` |
| ExtState prefix | `ReaCtrlSurf` |
| REAPER registration ID | `ReaControlSurface` |
| Plugin filename | `reaper_csurf_integrator` |
| Installed script directory | `Scripts/ReaControlSurface` |
| OSK script | `OSK on-screen keyboard.lua` |
| OSD script | `OSD on-screen display.lua` |
| Notifications script | `Notifications.lua` |
| Control Panel script | `Control Panel.lua` |
| Control Panel action ID | `REACTRLSURF_OPEN_CONTROL_PANEL` |
| Package prefix | `ReaControlSurface` |
| Repository URL | `https://github.com/gaabora/Reaper-ControlSurfaceIntegrator` |

Internal C++ class names and source folder names are not public identity values and do not need to match the display name.

## Generated consumers

CMake configure generates these files under `build/generated/`:

- `product_identity.h` for C++;
- `product_identity.ts` for the future Bun and TypeScript editor;
- `product_identity.env` for CI archive and package steps.

Lua loads the committed `Scripts/product_identity.lua`, which reads the manifest without CMake. Entry scripts also remain committed source files. CI loads the generated `product_identity.env` after configure and uses `cmake --install` to copy the complete Lua runtime into the package tree. The ReaPack staging tool reads the same manifest directly, so it does not require CMake configure.

The C++ action registry uses `PRODUCT_CONTROL_PANEL_ACTION_ID` without a leading underscore. REAPER exposes it to users and scripts as `_REACTRLSURF_OPEN_CONTROL_PANEL`. The ID is stable and does not depend on the public display name.

## Runtime structure

`ProductPaths` resolves paths below the REAPER resource directory:

```text
Data/
  ReaControlSurface/
    ReaControlSurface.ini
    Surfaces/
      Vendor/<surface-id>.txt
      User/<surface-id>.txt
    Zones/
      Vendor/<profile-id>/
        Main/*.zon
        FX/*.zon
      User/<profile-id>/
        Main/*.zon
        FX/*.zon
    Snippets/
      BuiltIn/*.snippet
      User/*.snippet
    Backups/<operation-id>/
    Generated/ZoneRawFXFiles/
```

Surface, profile, and operation IDs use lowercase ASCII letters, digits, `_`, and `-`. A surface filename is its stable ID plus `.txt`. A User surface with the same ID overrides its Vendor file. For zones, User Main overrides Vendor Main only when the User Main directory exists. Vendor and User FX directories are loaded together. A User FX zone with the same exact `Zone` name overrides its Vendor zone. Existing symlinks must not resolve outside the typed root.

Vendor zones are read-only at runtime. The runtime creates `Zones/User/<profile-id>/FX` when it initializes a configured FX profile. FX Learn and new FX zone files always write there. Before OSK edits Vendor Main, it requests confirmation and atomically copies only Main into the matching User profile. Before OSK edits a Vendor FX zone, it requests confirmation and copies only that file to the same relative User FX path. OSK then reloads the layered zones.

The runtime does not read the old `CSI/` root. Legacy names and layouts belong only in the future import workflow.

## Development resources

The manual commands in [`../Readme.md`](../Readme.md) link `resources/Surfaces`, `resources/Zones`, and `resources/Snippets` into `REAPER/Data/<ProductResourceDirectory>`. They also link the repository `Scripts/` directory to `REAPER/Scripts/<ProductScriptDirectory>`. Build and install steps do not create these links. Development links keep edits visible in REAPER without repeated copies. Installers and ReaPack packages must contain normal files and directories. They must not contain links to a source checkout.

## Rename procedure

Change public values in `Scripts/product_identity.conf`. Lua sees the values on its next start. Configure CMake again before building C++, TypeScript data, CI archives, or packages. Do not search and replace generated files or runtime paths manually.
