# 03 - Quick Start (15 Minutes)

**Purpose**: Get working with CSI immediately  
**Time**: 15 minutes  
**Prerequisites**: [Installation complete](02-Installation.md)

> Assuming you've completed installation, let's test your setup and make your first zone modification.

---

## Phase 1: Verify Installation (5 min)

### 1. Launch Reaper

1. Open Reaper
2. Go to **Preferences > Control/OSC/Web**
3. Verify you see **Control Surface Integrator** in the list
4. Click **Edit** to open CSI settings

### 2. Check Configuration

In the CSI settings window, you should see:
- **Pages** section: "HomePage" listed
- **Assignments** section: Your surface assigned to HomePage
- **Surfaces** section: Your MIDI surface configured

If all three ✓, configuration is good.

---

## Phase 2: Test Basic Control (5 min)

### 3. Load a Track with Audio

1. Close CSI settings (click **OK**)
2. Create a new track in Reaper
3. Import audio or generate a test tone
4. Select the track

### 4. Test Your First Widget

Assuming you're using a MIDI controller with a fader:

1. **Move the first fader on your controller** (typically becomes Fader 1)
2. **Expected**: The first track's fader in Reaper moves
3. **If it works**: ✅ CSI is functioning!
4. **If nothing happens**: Check [Troubleshooting](09-Troubleshooting.md)

### 5. Test Display Feedback (if your device has a display)

1. Move the fader on your controller
2. **Expected**: Display shows something like "-3.4 dB"
3. If you see volume feedback, CSI is properly bidirectional

---

## Phase 3: Make Your First Modification (5 min)

Now let's customize a zone file so you understand the basics.

### 6. Locate Your Zone Files

Navigate to: `Reaper Resource Path/CSI/Surfaces/YourSurface/Zones/HomeZones/`

This folder contains `.zon` files - these define what your controls do.

**Common approach**: Look for a file like:
- `Home.zon` or
- `Mixer.zon` or  
- `default.zon`

### 7. Open a Zone File

Right-click on `.zon` file → **Edit with Notepad** (or your text editor)

You'll see something like:
```
Zone "Mixer" "8-Channel Mixer"
    Widget Fader1 TrackVolume
    Widget Fader2 TrackVolume
    Widget Button1 Mute
    Widget Button2 Solo
    Widget Display1 TrackNameDisplay
ZoneEnd
```

This is a zone definition! It maps:
- `Fader1` → `TrackVolume` (control track volume)
- `Button1` → `Mute` (toggle mute)
- `Display1` → Shows track name

### 8. Make a Simple Change

Let's modify Button 1 to do something different:

**Find this line**:
```
Widget Button1 Mute
```

**Add a new line after it**:
```
Widget Button1+Shift Solo
```

This means:
- Button1 alone = Mute
- Button1 + Shift = Solo

**Save the file** (Ctrl+S)

### 9. Reload CSI

For CSI to recognize the change:

1. Go to **Preferences > Control/OSC/Web > Edit** (CSI)
2. Look for a **Refresh** or **Reset** button (varies by version)
3. OR: Close Reaper and reopen it (always works)

### 10. Test Your Change

1. Press Button 1 on your controller → Track mutes/unmutes
2. Press Button 1 + Shift → Track solos/unsolos
3. ✅ You've successfully customized CSI!

---

## What You Just Learned

✅ **Zones** - Files that map hardware to actions  
✅ **Widgets** - Individual controls (Fader1, Button1, etc.)  
✅ **Actions** - What you want widgets to do (TrackVolume, Mute, etc.)  
✅ **Modifiers** - Using Shift, Control to create alternate mappings  

---

## Common Next Steps

### Option A: Add More Controls
```
Zone "Mixer"
    Widget Fader1 TrackVolume
    Widget Fader2 TrackVolume
    Widget VPot1 TrackPan            ; Add pot control
    Widget Button1 Mute
    Widget Button1+Shift Solo
    Widget Button2 RecArm             ; Add record arm
    Widget Display1 TrackNameDisplay
ZoneEnd
```

### Option B: Add Secondary Zone for FX

Create a file `FX.zon` in the same folder:
```
Zone "FX" "FX Parameter Control"
    Widget VPot1 FXParam 0           ; Control FX slot 0 parameter
    Widget VPot2 FXParam 1           ; Control FX slot 1 parameter
    Widget Button1 GoZone Mixer      ; Button to switch back to Mixer
ZoneEnd
```

Then in Mixer.zon, add navigation:
```
Zone "Mixer"
    ... (existing mappings)
    Widget Button3 GoZone FX         ; Press to enter FX zone
ZoneEnd
```

### Option C: Add Banking for More Tracks

```
Zone "Mixer"
    Widget Fader1 TrackVolume
    Widget Fader1+Shift TrackVolume  ; Same as normal?
    Widget Button1 Mute
    Widget Button2 NextTrack         ; Navigate to next track
    Widget Button3 PreviousTrack     ; Navigate to previous track
ZoneEnd
```

---

## Key Actions to Try

| Action | What It Does |
|--------|-------------|
| `TrackVolume` | Control track volume |
| `TrackPan` | Control track pan |
| `Mute` | Toggle mute |
| `Solo` | Toggle solo |
| `RecArm` | Toggle record arm |
| `FXParam N` | Control FX slot N parameter |
| `GoZone ZoneName` | Switch to another zone |
| `GoHome` | Return to home zone |
| `NextTrack` / `PreviousTrack` | Navigate tracks |
| `Play` / `Stop` / `Record` | Transport controls |

**Complete list**: [07 - Complete Actions Reference](07-Complete-Actions-Reference.md)

---

## Troubleshooting This Step

### Changes aren't taking effect
- Restart Reaper after editing zone files
- Verify zone file was actually saved
- Check syntax (missing `ZoneEnd`)

### Modifier isn't working
- Syntax must be: `Widget Button1+Shift Mute` (plus sign, no spaces around +)
- Some controllers don't support modifiers natively

### Display shows wrong info
- Double-check widget name (Fader1 vs Fader2?)
- Verify action name (TrackVolume vs Track Volume - no spaces)

---

## What Now?

🎉 **You understand the basics!**

→ **Make it your own**: Keep experimenting with zone files  
→ **Learn zones deeply**: [04 - Zones Fundamentals](04-Zones-Fundamentals.md)  
→ **Add FX control**: [05 - FX Zones](05-FX-Zones.md)  
→ **Control multiple surfaces**: [Advanced Setup](08-Configuration-Format.md)  
→ **Problems?**: [09 - Troubleshooting](09-Troubleshooting.md)  

---

## Common Mistakes to Avoid

❌ **Missing `ZoneEnd`** - Syntax error
❌ **Action names with spaces** - Should be `TrackVolume` not `Track Volume`
❌ **Wrong widget name** - Widget must match surface.txt exactly
❌ **Forgetting to reload** - Changes don't take effect until refresh/restart
❌ **Not disabling MIDI ports** - Controller won't respond

---

**Time Invested**: ~15 minutes  
**Concepts Learned**: 4  
**Next Level**: Intermediate (~30 minutes to intermediate proficiency)

**Ready?** → [04 - Zones Fundamentals](04-Zones-Fundamentals.md)
