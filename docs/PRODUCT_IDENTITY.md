# Product Identity and Paths

## Canonical source

[`../Scripts/product_identity.conf`](../Scripts/product_identity.conf) is the only source for public product names and install paths. Lua reads it directly. [`../cmake/ProductIdentity.cmake`](../cmake/ProductIdentity.cmake) validates the same file and derives build and install paths from it.

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
| Package prefix | `ReaControlSurface` |
| Repository URL | `https://github.com/gaabora/Reaper-ControlSurfaceIntegrator` |

Internal C++ class names and source folder names are not public identity values and do not need to match the display name.

## Generated consumers

CMake configure generates these files under `build/generated/`:

- `product_identity.h` for C++;
- `product_identity.ts` for the future Bun and TypeScript editor;
- `product_identity.env` for CI archive and package steps.

Lua loads the committed `Scripts/product_identity.lua`, which reads the manifest without CMake. Entry scripts also remain committed source files. CI loads the generated `product_identity.env` after configure and uses `cmake --install` to copy the complete Lua runtime into the package tree. The ReaPack staging tool reads the same manifest directly, so it does not require CMake configure.

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

Surface, profile, and operation IDs use lowercase ASCII letters, digits, `_`, and `-`. A surface filename is its stable ID plus `.txt`. When the same surface or zone-profile ID exists in both `Vendor` and `User`, the runtime uses the user source. Existing symlinks must not resolve outside the typed root.

Vendor zone profiles are read-only at runtime. Before OSK or FX Learn writes into a vendor profile, it requests confirmation and atomically clones the complete profile into `Zones/User`. OSK then reloads the user profile. All file writes use the user copy.

The runtime does not read the old `CSI/` root. Legacy names and layouts belong only in the future import workflow.

## Development resources

The manual commands in [`../Readme.md`](../Readme.md) link `resources/Surfaces`, `resources/Zones`, and `resources/Snippets` into `REAPER/Data/<ProductResourceDirectory>`. They also link the repository `Scripts/` directory to `REAPER/Scripts/<ProductScriptDirectory>`. Build and install steps do not create these links. Development links keep edits visible in REAPER without repeated copies. Installers and ReaPack packages must contain normal files and directories. They must not contain links to a source checkout.

## Rename procedure

Change public values in `Scripts/product_identity.conf`. Lua sees the values on its next start. Configure CMake again before building C++, TypeScript data, CI archives, or packages. Do not search and replace generated files or runtime paths manually.
