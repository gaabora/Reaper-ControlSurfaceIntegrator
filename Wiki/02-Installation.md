# 02 - Installation Guide

> This is the legacy upstream installation guide. The fork is not published yet. Current CI packages install `UserPlugins/reaper_csurf_integrator.*` and `Scripts/ReaControlSurface/`; the configuration root is `Data/ReaControlSurface/`. Tagged builds create a release-specific preview ReaPack index, but there is no stable user repository URL yet. //FIXME before release

**Purpose**: Get CSI installed and verified  
**Time Required**: 15-20 minutes  
**Difficulty**: Beginner

> ⚠️ **IMPORTANT**: Before starting, note that CSI requires exclusive access to your MIDI ports. This means these ports must be disabled in Reaper's Preferences. For this reason, you cannot use MIDI controllers for note data AND CSI simultaneously on the same port.

---

## Requirements

### System Requirements
- **Reaper v7.0 or later** (check via Help > About REAPER)
- **Windows**: 64-bit Windows 10 or later  
- **macOS**: 10.15 (Catalina) or later  
- **Linux**: Ubuntu 20.04+ or equivalent

### Hardware
- Any MIDI controller with drivers installed (Windows/Mac)
- OR OSC-capable device (iPad, Android, networked PC)

---

## Step 1: Download CSI

1. Go to [CSI v7 Install & Support Files](https://github.com/FunkybotsEvilTwin/CSI_Install)
2. Click the green **Code** button
3. Select **Download ZIP**
4. Extract the zip file to a temporary location

You'll have:
- `reaper_csurf_integrator64.dll` (Windows)
- `reaper_csurf_integrator.dylib` (macOS)
- `reaper_csurf_integrator.so` (Linux)
- `CSI/` folder with surface templates

---

## Step 2: Locate Reaper Resource Path

This is where Reaper stores all plugins and configuration.

**Windows**:
1. Open Reaper
2. Go **Options > Preferences**
3. Click the button: **"Show REAPER resource path in explorer"**
4. A folder window opens - this is your resource path

**macOS**:
1. Open Reaper
2. Go **Reaper > Preferences**
3. Click the button: **"Show REAPER resource path in Finder"**
4. Finder opens to your resource path

**Resource Path Typical Locations**:
- **Windows**: `C:\Users\YourName\AppData\Roaming\REAPER\`
- **macOS**: `/Users/YourName/Library/Application Support/REAPER/`
- **Linux**: `~/.config/REAPER/`

---

## Step 3: Install Plugin

### A) Copy Plugin File

1. In your Reaper resource path, locate the `UserPlugins` folder
   - If it doesn't exist, create it
   
2. Copy the plugin file there:
   - **Windows**: Copy `reaper_csurf_integrator64.dll` to `UserPlugins/`
   - **macOS**: Copy `reaper_csurf_integrator.dylib` to `UserPlugins/`
   - **Linux**: Copy `reaper_csurf_integrator.so` to `UserPlugins/`

### B) macOS Security (if applicable)

If you see a security warning on macOS 10.15+:

```bash
# Remove quarantine flag
xattr -d com.apple.quarantine ~/Library/Application\ Support/REAPER/UserPlugins/reaper_csurf_integrator.dylib

# Restart Reaper
```

If you still  get issues, go to **System Settings > Security & Privacy** and allow the plugin.

---

## Step 4: Install Support Files

Support files include pre-configured surface files and zone templates.

1. Navigate to your Reaper resource path
2. Look for the `CSI` folder (it may not exist yet)
3. Copy the downloaded `CSI` folder into your resource path
   - This creates: `Reaper Resource Path/CSI/`

Verify structure:
```
Reaper Resource Path/
├── CSI/
│   ├── csi.ini (will be created next)
│   ├── Surfaces/
│   │   ├── XTouch/
│   │   ├── Behringer_MCU_Pro/
│   │   └── ... (many more templates)
│   └── ZoneRawFXFiles/
└── UserPlugins/
    └── reaper_csurf_integrator64.dll (or .dylib/ .so)
```

---

## Step 5: Disable MIDI Ports for Your Controller

This is **critical**. CSI requires exclusive access to MIDI ports.

1. Open Reaper **Preferences > Audio > MIDI Inputs**
2. Find your controller in the list
3. Uncheck all columns:
   - ☐ Input
   - ☐ All
   - ☐ Control

4. Now go to **Preferences > Audio > MIDI Outputs**
5. Find your controller
6. Uncheck the **Enabled** column

**Result**: Your controller is now available only to CSI, not to Reaper's built-in MIDI mapping.

---

## Step 6: Add CSI as Control Surface

1. Go to **Preferences > Control/OSC/Web**
2. Click **Add**
3. Select **Control Surface Integrator** from the dropdown
4. Default settings are fine - click **OK**

You should now see CSI in your control surfaces list.

---

## Step 7: Configure Your First Surface

1. In CSI settings, click the **Edit** button next to CSI
2. A configuration window opens

### A) Create a Page

1. In the **Pages** section, click **Add**
2. Enter: `HomePage`
3. Click **OK**
4. Set any page options (defaults are fine for now)

### B) Add a MIDI Surface

1. Go to **Surfaces** section, click **Add MIDI**
2. Enter:
   - **Name**: (e.g., "My_Controller")
   - **MIDI In**: Select your controller's input
   - **MIDI Out**: Select your controller's output
   - **Number of Channels**: 1 (for most controllers)
3. Click **OK**

### C) Assign Surface to Page

1. Go to **Assignments** section, click **Add**
2. **Surface Selector**: Choose the surface you just created
3. **Surface Folder**: Select matching folder name (e.g., "XTouch" for X-Touch)
4. Click **OK**

---

## Step 8: Test Installation

1. Close the CSI settings window (click **OK**)
2. Restart Reaper (File > Close or just quit and restart)
3. Watch for any error messages

### Verify CSI Is Running

1. Go back to **Preferences > Control/OSC/Web**
2. CSI should still be listed
3. No red error indicators

If you see errors, check:
- Plugin file is in `UserPlugins/` folder
- `CSI/` folder structure is correct
- MIDI ports are disabled for your controller

---

## Troubleshooting Installation

### "CSI doesn't appear in Control Surface dropdown"

**Causes & Solutions**:
1. Plugin file in wrong location - verify `UserPlugins/` has the .dll/.dylib
2. Reaper version too old - upgrade to v7.0+
3. Windows: Missing Visual C++ Runtime - [download here](https://support.microsoft.com/en-us/help/2977003/)
4. macOS: Security settings - run the xattr command above

### "Surface won't respond to fader movements"

**Most Common**: MIDI ports still enabled in Reaper!

1. Go to **Preferences > Audio > MIDI Inputs**
2. Verify your controller input is completely unchecked
3. Same for **MIDI Outputs**

### "CSI crashes when I adjust a control"

**Likely Issues**:
1. Surface folder doesn't exist - verify folder name matches
2. Zone files missing - check `CSI/Surfaces/YourSurface/Zones/`
3. Corrupted config - delete `csi.ini` and reconfigure from scratch

---

## Next Steps

✅ **Installation complete!**

→ **Ready to test?** Go to [03 - Quick Start](03-QuickStart.md)  
→ **Want details?** Read [04 - Zones Fundamentals](04-Zones-Fundamentals.md)  
→ **Troubleshooting?** See [09 - Troubleshooting](09-Troubleshooting.md)  

---

## Did It Work?

**Yes?** Amazing! Go to [03 - Quick Start](03-QuickStart.md) to test your setup.

**No?** Check [09 - Troubleshooting](09-Troubleshooting.md) for solutions.

---

**Last Updated**: March 2026  
**Time to Install**: ~15 minutes  
**Difficulty**: Beginner
