# Preview ReaPack Foundation

## Status

Tagged release CI generates a release-specific preview `index.xml`. It publishes the index, its SHA-256 checksum, platform extension files, and ZIP archives only after all builds and package checks pass.

There is no stable ReaPack repository URL yet. Do not give users a release-specific index URL as a permanent repository. The stable URL stays blocked until clean portable REAPER install, update, downgrade, and uninstall checks are complete.

## Package ownership

| Package | ReaPack target | Content |
| --- | --- | --- |
| `<PackagePrefix> Core` | `UserPlugins/` and `Scripts/<ProductScriptDirectory>/` | One selected platform extension, shared Lua files, and `product_identity.conf` |
| `<PackagePrefix> Surface <surface-id>` | `Data/<ProductResourceDirectory>/Surfaces/Vendor/` | One vendor surface file |
| `<PackagePrefix> Zones <profile-id>` | `Data/<ProductResourceDirectory>/Zones/Vendor/<profile-id>/` | One complete vendor zone profile |
| `<PackagePrefix> Snippet <name>` | `Data/<ProductResourceDirectory>/Snippets/BuiltIn/` | One built-in snippet when `.snippet` files exist |

Packages never target `Surfaces/User`, `Zones/User`, `Snippets/User`, `Backups`, or `Generated`. Runtime-created user directories and files remain after package update or uninstall.

The preview core package uses these platform selectors and release asset suffixes:

| ReaPack platform | Release asset suffix |
| --- | --- |
| `darwin64` | `darwin-x86_64.dylib` |
| `darwin-arm64` | `darwin-arm64.dylib` |
| `win64` | `win64.dll` |
| `linux64` | `linux-x86_64.so` |

All names before these suffixes come from `Scripts/product_identity.conf`.

## Generation and validation

`tools/reapack/repository.py prepare` creates a temporary repository below `.reapack-build/`. Its category paths match the final `Scripts/` and `Data/` destinations. Source URLs point to the exact release tag and exact GitHub Release assets. The release workflow validates vendor surface files before package generation.

The release workflow then:

1. Runs the configuration editor validator against every vendor surface.
2. Runs official `reapack-index` 1.2.6 in strict check mode.
3. Uses official `reapack-index` to generate `index.xml`.
4. Runs `tools/reapack/repository.py finalize` to match every index source to a known local file.
5. Adds a SHA-256 multihash to every source.
6. Writes `index.xml.sha256` and publishes all release files in one gated job.

The core commands used after all four platform assets exist in `release-assets/` are:

```sh
gem install reapack-index --version 1.2.6 --no-document
python3 tools/reapack/repository.py prepare --version <version-without-v> --tag <tag> --asset-dir release-assets
git -C .reapack-build/repository init
git -C .reapack-build/repository config user.name "ReaPack Preview"
git -C .reapack-build/repository config user.email "reapack-preview@example.invalid"
git -C .reapack-build/repository add .
git -C .reapack-build/repository commit -m "Generate preview package metadata"
cd .reapack-build/repository
reapack-index --check --strict .
reapack-index --scan . --strict --no-commit --name "<ProductDisplayName> Preview" --output index.xml .
cd ../..
python3 tools/reapack/repository.py finalize --output-dir release-assets
```

The example sets a temporary identity only inside the staging repository. It does not change the developer's global Git identity.
