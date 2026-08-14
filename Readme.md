# ReaControlSurface

ReaControlSurface is rewritten fork of the Control Surface Integrator ((CSI)[https://github.com/GeoffAWaddington/CSICode]) plugin for Reaper, designed to help you integrate hardware control surfaces with the DAW. 
The purpose of this separated fork is to make control surfaces in REAPER much more user friendly, intuitive, feature rich, accessible and configurable without need to have a degree in any kind of science, and ultimately without need to read/watch any docs/tutorials.
Since then, many issues have been fixed and new capabilities like functional snippets, OSK (interactive configurable on-screen "keyboard"/surface), and OSD (on-screen display) was added, together with dozens of other features, performance optimizations, and ReaPack support with automated builds for Windows, macOS, and Linux (linux is yet untested). You can view the latest updates in the [releases](/releases).

## Install with ReaPack

1. Install [ReaPack](https://reapack.com/) if it is not installed.
2. In REAPER, open `Extensions > ReaPack > Import repositories...`.
3. Add `https://github.com/gaabora/Reaper-ControlSurfaceIntegrator/releases/latest/download/index.xml`.
4. Open `Extensions > ReaPack > Browse packages...` and search for `ReaControlSurface`.
5. Install the core extension and shared scripts, and your vendor surfaces with their zone profiles. The functional snippets are optional - its basically some common sets of widgets with assigned actions you can easily apply to any zone of any surface in a web-based configuration editor. //FIXME: describe functional snippets better, what, why, where to use and how, etc
6. Apply the changes and restart REAPER.

The standalone configuration editor is available as a platform ZIP in each [GitHub release](/releases). It runs separately from REAPER and is not installed through ReaPack.

## Documentation

### End-user docs

Start in [Wiki/Home.md](Wiki/Home.md).

- [Wiki/01-Overview.md](Wiki/01-Overview.md)
- [Wiki/02-Installation.md](Wiki/02-Installation.md)
- [Wiki/03-QuickStart.md](Wiki/03-QuickStart.md)
- [Wiki/04-Zones-Fundamentals.md](Wiki/04-Zones-Fundamentals.md)
- [Wiki/Quick-Reference.md](Wiki/Quick-Reference.md)
- [Wiki/Actions-Reference.md](Wiki/Actions-Reference.md)
- [Wiki/Configuration-Format.md](Wiki/Configuration-Format.md)
- [Wiki/Migration-Guide.md](Wiki/Migration-Guide.md)

### Developer docs

- [docs/README.md](docs/README.md)
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/LUA_CPP_EXTSTATE_INTERFACE.md](docs/LUA_CPP_EXTSTATE_INTERFACE.md)
- [docs/PRODUCT_IDENTITY.md](docs/PRODUCT_IDENTITY.md)
- [docs/REAPACK.md](docs/REAPACK.md)

### Open work

- [todo/README.md](todo/README.md)

## Build

Windows debug build:

```powershell
cmake --build build --config Debug
```

## Link configuration resources and Lua scripts for development

The plugin reads surface, zone, and snippet configuration from `Data/<ProductResourceDirectory>` in the REAPER resource directory. The Lua runtime is installed below the REAPER `Scripts` directory. For local development, link these paths to the repository so edits are available in REAPER immediately. Set `REAPER_RESOURCE_PATH` to the REAPER resource directory before you run the commands.

The commands read the current product directories from `Scripts/product_identity.conf`. CMake configure is not required. They do not replace existing paths. Move or remove existing `Surfaces`, `Zones`, `Snippets`, and `Scripts/<ProductScriptDirectory>` paths first.

macOS and Linux:

```sh
repository_root="$(pwd)"
identity_file="$repository_root/Scripts/product_identity.conf"
product_resource_directory="$(sed -n 's/^PRODUCT_RESOURCE_DIRECTORY=//p' "$identity_file")"
product_script_directory="$(sed -n 's/^PRODUCT_SCRIPT_DIRECTORY=//p' "$identity_file")"
development_product_root="$REAPER_RESOURCE_PATH/Data/$product_resource_directory"
development_scripts_root="$REAPER_RESOURCE_PATH/Scripts"

mkdir -p "$development_product_root"
mkdir -p "$development_scripts_root"
ln -s "$repository_root/resources/Surfaces" "$development_product_root/Surfaces"
ln -s "$repository_root/resources/Zones" "$development_product_root/Zones"
ln -s "$repository_root/resources/Snippets" "$development_product_root/Snippets"
ln -s "$repository_root/Scripts" "$development_scripts_root/$product_script_directory"
```

Windows PowerShell with Developer Mode enabled:

```powershell
$repositoryRoot = (Get-Location).Path
$identityFile = Join-Path $repositoryRoot "Scripts\product_identity.conf"
$productResourceDirectory = ((Get-Content $identityFile | Select-String "^PRODUCT_RESOURCE_DIRECTORY=").Line -split "=", 2)[1]
$productScriptDirectory = ((Get-Content $identityFile | Select-String "^PRODUCT_SCRIPT_DIRECTORY=").Line -split "=", 2)[1]
$developmentDataRoot = Join-Path $env:REAPER_RESOURCE_PATH "Data"
$developmentProductRoot = Join-Path $developmentDataRoot $productResourceDirectory
$developmentScriptsRoot = Join-Path $env:REAPER_RESOURCE_PATH "Scripts"

New-Item -ItemType Directory -Force -Path $developmentProductRoot
New-Item -ItemType Directory -Force -Path $developmentScriptsRoot
New-Item -ItemType SymbolicLink -Path (Join-Path $developmentProductRoot "Surfaces") -Target (Join-Path $repositoryRoot "resources\Surfaces")
New-Item -ItemType SymbolicLink -Path (Join-Path $developmentProductRoot "Zones") -Target (Join-Path $repositoryRoot "resources\Zones")
New-Item -ItemType SymbolicLink -Path (Join-Path $developmentProductRoot "Snippets") -Target (Join-Path $repositoryRoot "resources\Snippets")
New-Item -ItemType SymbolicLink -Path (Join-Path $developmentScriptsRoot $productScriptDirectory) -Target (Join-Path $repositoryRoot "Scripts")
```

## macOS note

macOS builds are not code-signed. If REAPER blocks or can not see the plugin, remove the quarantine flag by next command running in terminal:

```bash
./allow_use_of_csurf_dylib.sh
```





# Development
based on modified crossplatform automated build template from extension for reaper from https://github.com/ak5k/reaper-sdk-vscode - [Visual Studio Code](https://code.visualstudio.com/) + [CMake](https://cmake.org/) based cross-platform template for developing a [REAPER](https://www.reaper.fm/) [Plug-in Extension](https://www.reaper.fm/sdk/plugin/plugin.php).
Developed and tested on Windows 10-11, MacOS High Sierra 10.13.6-14.5, and Ubuntu 20.04.1. Instructions are based on clean/fresh installation.
## Development environment installation guide
It's recommended to read all steps in advance before beginning installation.
### Windows
Install [Visual Studio Community with Develop C and C++ applications component](https://visualstudio.microsoft.com/vs/features/cplusplus/). Default installation includes MSVC compiler and CMake. Visual Studio installation can be trimmed down before installation or afterwards by cherry-picking only the necessary components from Visual Studio Installer > Individual Components section.
#### Minimal Visual Studio installation
* C++ CMake tools for Windows
* Windows SDK (e.g. Windows 11 SDK or Windows 10 SDK (10.0.19041.0) for Windows 10, version 2004)
### MacOS
Perhaps the easiest way is to install [Homebrew](https://brew.sh/). This will also install Apple XCode Command Line Tools. After Homebrew is installed, install CMake with `brew install cmake`. Git can also be installed from Homebrew with `brew install git`.
### Linux
Mainstream Linux distributions usually include compiler and many of other necessary tools. On Ubuntu, command `sudo apt install build-essential cmake gdb git valgrind` installs all necessary tools.
### All platforms
* Install [Visual Studio Code](https://code.visualstudio.com/) (VSCode).
* Install [Git](https://git-scm.com/downloads), if not already installed. 
* Open the project directory in VSCode
* Install recommended extensions thru extensions panel or recommendations popup or command line or any way you like:
  * [VSCode C/C++ Extension Pack](https://marketplace.visualstudio.com/items/?itemName=ms-vscode.cpptools-extension-pack) `code --verbose --install-extension ms-vscode.cpptools-extension-pack ms-vscode.cmake-tools`.
  * [CMake Tools](https://marketplace.visualstudio.com/items/?itemName=ms-vscode.cmake-tools) with `code --verbose --install-extension ms-vscode.cmake-tools ms-vscode.cmake-tools`. This might take a while.
* VSCode finishes installing the C/C++ Extensions pack. This is indicated in Status Bar.
* Wait, until all dependencies have been downloaded and installed. This might take a while. 
* Then quit VSCode and restart.
* If VSCode notifies about Reload, then Reload.
* If VSCode notifies to `configure project with CMake Tools`, choose `Yes` to configure. CMake Tools can also be set to allow always configure new C/C++ projects.
* Select appropriate build kit for platform and architecture. Visual Studio for Windows, Clang for MacOS and GCC for Linux (e.g. `Visual Studio Community Release 20XX - amd64` for modern Windows PC).
* VSCode should also notify about setting up IntelliSense for current workspace. Allow this.
* If this did not happen, these can be set up by issuing VSCode Command Palette Commands (Ctrl/Cmd + Shift + P) `CMake: Configure` and `CMake: Select a Kit`, or from VSCode Status Bar. 

## First steps on Windows debugging
* Set environment cariables:
  * `REAPER_EXE_PATH` - path to folder with reaper.exe
  * `REAPER_RESOURCE_PATH` - path to folder with your reaper.ini and UserPlugins without trailing \ - there will be installed build .dll and debug files (.ilk and .pdb)
```
setx REAPER_EXE_PATH "c:\apps\MultiMedia\ReaperPortable\ReaperPortable.exe"
setx REAPER_RESOURCE_PATH "c:\apps\MultiMedia\ReaperPortable\Data\ReaperPortableConfig"
```
* By default, VSCode builds a debug version of the plugin it by running `CMake: Build` or keyboard shortcut `F7`.
* Install plugin with VSCode command `CMake: Install`.
* Start debug in REAPER by `F5`.
* Choosing between debug and release builds can be done with `CMake: Select Variant`.

## Other docs
* [VSCode docs](https://code.visualstudio.com/docs/languages/cpp#_tutorials) and [Microsoft C++ docs](https://docs.microsoft.com/en-us/cpp/cpp/) are a helpful resource. And, of course, [ReaScript, JSFX, REAPER Plug-in Extensions, Developer Forum](https://forum.cockos.com/forumdisplay.php?f=3).
