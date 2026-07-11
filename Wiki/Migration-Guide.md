# CSI Action Migration Guide: Old Wiki → Current Version

**Last Updated**: March 14, 2026  
**Purpose**: Help users migrate from old CSI wiki-based configuration to the current Actions-Reference.md specification

---

## Overview of Changes

The CSI action system has been restructured for clarity and flexibility. Major changes include:

- **14 categories → 11 focused categories** with clearer organization
- **26 actions removed/deprecated** (mostly display variants and banking features)
- **8 new actions added** (JSFX support, accessibility, transport improvements)
- **Full documentation** with parameters, values, and examples (was wiki-only)
- **Modifier-based selection** replaces multi-select actions

---

## Quick Migration Checklist

- [ ] Review removed actions list (below) - search zone files for deprecated actions
- [ ] Update multi-select actions to use modifiers (`Shift`, `Control`, `Alt`)
- [ ] Replace banking actions with `GoZone` navigation
- [ ] Update display actions to use consolidated formats
- [ ] Test all modifier combinations in updated zones
- [ ] Verify FX actions use correct slot parameters

---

## Removed/Deprecated Actions

### Transport & Timeline (Removed)

| Old Action | Replacement | Migration Notes |
|---|---|---|
| `CycleTimeline` | `GoHome` or `GoZone` | Use zone navigation instead |
| `CycleTimeDisplayModes` | `TimecodeDisplay` | Display format now centralized |
| `MCUTimeDisplay` | `TimecodeDisplay` | Generic 7-segment display |
| `OSCTimeDisplay` | `TimecodeDisplay` + OSC | Use display with OSC messaging |
| `MoveEditCursor` | Reaper action + `Reaper` prefix | Use Reaper action syntax |

**Migration Example**:
```
OLD:
Widget Display1 MCUTimeDisplay
Widget Display2 OSCTimeDisplay

NEW:
Widget Display1 TimecodeDisplay
Widget Display2 TimecodeDisplay
  Properties SendOSC /timecode
```

---

### Track Selection (Replaced by Modifiers)

| Old Action | New Approach | Migration Example |
|---|---|---|
| `TrackSelect` | `TrackSelect` (normal) | `Widget Button1 TrackSelect` |
| `TrackUniqueSelect` | `TrackSelect` + normal click | Same as TrackSelect alone |
| `TrackRangeSelect` | `Shift+TrackSelect` | `Widget Button1+Shift TrackSelect` |

**Migration Example**:
```
OLD:
Zone "Mixer"
  Widget Button1 TrackSelect
  Widget Button1 TrackUniqueSelect
  Widget Button2 TrackRangeSelect
ZoneEnd

NEW:
Zone "Mixer"
  Widget Button1 TrackSelect                ; Normal: select this track
  Widget Button1+Shift TrackSelect          ; Shift: add to selection
  Widget Button1+Control TrackSelect        ; Control: toggle selection
ZoneEnd
```

---

### Track Display (Consolidated)

| Old Action | New Approach | Notes |
|---|---|---|
| `TrackNumberDisplay` | `TrackNameDisplay` | Combined into single display |
| `TrackVolumeDB` | `TrackVolumeDisplay` | dB format automatic |
| `TrackVolumeWithMeterAverageLR` | `TrackVolumeDisplay` + `TrackOutputMeter` | Split into separate widgets |
| `TrackVolumeWithMeterMaxPeakLR` | `TrackVolumeDisplay` + `TrackOutputMeter` | Split into separate widgets |

**Migration Example**:
```
OLD:
Widget Display1 TrackNumberDisplay
Widget Display2 TrackVolumeDB
Widget Display3 TrackVolumeWithMeterAverageLR

NEW:
Widget Display1 TrackNameDisplay
Widget Display2 TrackVolumeDisplay          ; Outputs dB automatically
Widget Display3 TrackVolumeDisplay          ; Use separate meter widget
Widget Meter1 TrackOutputMeter
```

---

### Pan Display Variants (Consolidated)

| Old Action | Migration | Notes |
|---|---|---|
| `TrackPanLeftDisplay` | `TrackPanDisplay` | All pan displays unified |
| `TrackPanRightDisplay` | `TrackPanDisplay` | Use single display |
| `TrackPanPercent` | `TrackPanDisplay` | Format auto-detected |
| `TrackPanWidthPercent` | `TrackPanDisplay` | Use with `TrackPanWidth` action |
| `TrackPanLPercent` | `TrackPanDisplay` | Use with `TrackPanLeft` action |
| `TrackPanRPercent` | `TrackPanDisplay` | Use with `TrackPanRight` action |

**Migration Example**:
```
OLD:
Zone "Pan"
  Widget VPot1 TrackPan
  Widget Display1 TrackPanLeftDisplay
  Widget Display2 TrackPanRightDisplay
ZoneEnd

NEW:
Zone "Pan"
  Widget VPot1 TrackPan
  Widget Display1 TrackPanDisplay
ZoneEnd
```

---

### Navigation/Banking (Major Restructure)

| Old Action | New Approach | Migration Notes |
|---|---|---|
| `Bank` | `GoZone` or `NextTrack`/`PreviousTrack` | Use explicit zone navigation |
| `GoPage` | `GoZone` | Pages replaced by zones |
| `NextPage` | `GoZone` with zone name | Explicit zone navigation |
| `PageNameDisplay` | `FixedTextDisplay` | Zone now shows via display |
| `ToggleSynchPageBanking` | N/A | Banking model deprecated |
| `ToggleScrollLink` | N/A | Scroll sync removed |

**Migration Example**:
```
OLD:
Widget Button1 Bank 8              ; Bank by 8 tracks
Widget Display1 PageNameDisplay    ; Show page name
Widget Button2 NextPage            ; Go to next page

NEW:
Widget Button1+Shift NextTrack     ; Shift+button navigates tracks
Widget Button2 GoZone MixerZone    ; Explicit zone navigation
Widget Display1 TrackNameDisplay   ; Show current track name
```

---

### FX Display Variants (Consolidated)

| Old Action | New Approach | Notes |
|---|---|---|
| `FXParamNameDisplay` | `FXParamNameDisplay` | Still available (no change) |
| `FXParamValueDisplay` | `FXParamValueDisplay` | Still available (no change) |
| `LastTouchedFXParamNameDisplay` | `LastTouchedFXParam` + display | Use with FXParamNameDisplay |
| `LastTouchedFXParamValueDisplay` | `LastTouchedFXParam` + display | Use with FXParamValueDisplay |
| `TCPFXParamNameDisplay` | `TCPFXParam` + display | Use with display widget |
| `TCPFXParamValueDisplay` | `TCPFXParam` + display | Use with display widget |

**Migration Example**:
```
OLD:
Zone "FX"
  Widget VPot1 LastTouchedFXParam
  Widget Display1 LastTouchedFXParamNameDisplay
  Widget Display2 LastTouchedFXParamValueDisplay
ZoneEnd

NEW:
Zone "FX"
  Widget VPot1 LastTouchedFXParam
  Widget Display1+TrackNameDisplay         ; Or use dedicated display
  Widget Display2 TrackVolumeDisplay       ; FX param value feedback
  
  ; Alternative: explicit TCP
  Widget VPot2 TCPFXParam 0
  Widget Display3 FXParamNameDisplay 0
  Widget Display4 FXParamValueDisplay 0
ZoneEnd
```

---

### Output Metering (Removed)

| Old Action | Replacement | Notes |
|---|---|---|
| `TrackOutputMeter` | `TrackOutputMeter` | **Still available** |
| `TrackOutputMeterAverageLR` | N/A | Use `TrackOutputMeter` alone |
| `TrackOutputMeterMaxPeakLR` | N/A | Use `TrackOutputMeter` alone |

**Migration Example**:
```
OLD:
Widget MeterL TrackOutputMeterAverageLR
Widget MeterR TrackOutputMeterMaxPeakLR

NEW:
Widget MeterL TrackOutputMeter              ; Combined meter display
Widget MeterR TrackOutputMeter              ; Use same widget reordered
```

---

### Automation & Special Modes

| Old Action | Removed? | Migration |
|---|---|---|
| `TrackAutoMode` | ❌ Removed | Use zone-based context instead |
| `GlobalAutoMode` | ❌ Removed | Use `GoHome` + explicit zone |
| `CycleTrackAutoMode` | ❌ Removed | Replaced by modifier zones |
| `ToggleUseLocalModifiers` | ❌ Removed | N/A |
| `ToggleUseLocalFXSlot` | ❌ Removed | N/A |
| `SetHoldTime` | ❌ Removed | Zone-level parameter |
| `SetLatchTime` | ❌ Removed | Zone-level parameter |

**Migration Example**:
```
OLD:
Zone "AutomationMode"
  Widget Button1 TrackAutoMode              ; Cycle automation
  Widget Display1 TrackAutoModeDisplay
ZoneEnd

NEW:
Zone "AutomationMode"
  ; Use modifiers instead
  Widget Button1 Reaper 40047              ; Toggle auto-latch
  Widget Button1+Shift Reaper 40048         ; Toggle auto-touch
ZoneEnd
```

---

### Speech & Display Actions

| Old Action | Replacement | Notes |
|---|---|---|
| `Speak` | `SpeakOSARAMessage` | Now OSARA-integrated |
| `SpeakFXMenuName` | `SpeakOSARAMessage "FX: " + param` | Manual composition |
| `SpeakTrackSendDestination` | `SpeakOSARAMessage` | Manual composition |
| `SpeakTrackReceiveSource` | `SpeakOSARAMessage` | Manual composition |
| `FixedTextDisplay` | `FixedTextDisplay` | **Still available** |
| `FixedRGBColorDisplay` | `SetXTouchDisplayColors` | X-Touch specific |
| `NoAction` | N/A | Use empty zone or comment |
| `CSINameDisplay` | `FixedTextDisplay "CSI"` | Use fixed text |
| `CSIVersionDisplay` | `FixedTextDisplay "v6.x"` | Use fixed text |
| `ToggleRestrictTextLength` | Zone-level parameter | Configure in zone definition |

**Migration Example**:
```
OLD:
Widget Button1 Speak "Play button pressed"
Widget Button2 SpeakFXMenuName

NEW:
Widget Button1 SpeakOSARAMessage "Play button"
Widget Button2 SpeakOSARAMessage "Opening FX"
```

---

### Modifier & Navigation Cleanup

| Old Action | Status | Migration |
|---|---|---|
| `Bank` | ❌ Removed | Use `GoZone` + track navigation |
| `ClearFXSlot` | ❌ Removed | Use `GoHome` or zone switch |
| `ClearFocusedFX` | ❌ Removed | Use `GoHome` |
| `ClearLastTouchedFXParam` | ❌ Removed | Focus different FX |
| `ClearSelectedTrackFX` | ❌ Removed | Use zone navigation |
| `AllSurfacesGoHome` | ❌ Removed | Use `GoHome` (single surface) |
| `CycleTrackInputMonitor` | ❌ Removed | Use Reaper actions |
| `TrackInputMonitorDisplay` | ❌ Removed | N/A |

---

## New Actions (v6.0+)

### Transport Improvements

**`PlayAndStopAtMarker`** - Play until next marker
```
Widget Button1 PlayAndStopAtMarker
```

**`PlayAndStopAtRegion`** - Play until region end
```
Widget Button1 PlayAndStopAtRegion
```

**`Pause`** - Pause (toggle) playback
```
Widget Button1 Pause
```

### Project Management

**`NewProject`** - Create new project
```
Widget Button1 NewProject
```

**`OpenProject`** - Open project dialog
```
Widget Button1 OpenProject
```

### FX Enhancements

**`JSFXParam`** - JSFX-specific parameter control
```
Widget VPot1 JSFXParam 0                    ; JSFX slot 0
```

**`TCPFXParam`** - TCP (Track Control Panel) FX parameter
```
Widget VPot1 TCPFXParam 0                   ; TCP-resident FX
```

### Navigation Additions

**`NextTrack` / `PreviousTrack`** - Sequential track navigation
```
Widget Button1 NextTrack
Widget Button2 PreviousTrack
```

**`NextSend` / `PreviousSend`** - Sequential send navigation
```
Widget Button1 NextSend
Widget Button2 PreviousSend
```

**`SelectedTrackFX`** - Navigate to selected track FX
```
Widget Button1 SelectedTrackFX
```

### Accessibility

**`SpeakOSARAMessage`** - OSARA-integrated text-to-speech
```
Widget Button1 SpeakOSARAMessage "Playing track"
```

### Display

**`TimecodeDisplay`** - Generic timeline/timecode display
```
Widget Display1 TimecodeDisplay
```

---

## Category Restructuring

### Old Structure → New Structure

**Track-Related Actions (OLD)**
- Track Actions (11 items)
- Track Sends (8 items)
- Track Receives (8 items)

**Track-Related Actions (NEW)**
- Volume Actions (7 items: TrackVolume, SendVolume, RecvVolume, Master*)
- Pan Actions (8 items: TrackPan, SendPan, RecvPan, Master*)
- Track Control Actions (5 items: Mute, Solo, RecArm, Select, Name)

**FX Actions (OLD)**
- FX (18 items, mixed parameter & display)

**FX Actions (NEW)**
- FX Parameter Actions (7 items: FXParam, JSFXParam, TCPFXParam, LastTouchedFXParam, Bypass, Offline, GainReduction)

**Navigation (OLD)**
- Navigation (11 items, including Bank, Page, FX slot)

**Navigation (NEW)**
- Navigation Actions (7 items: GoZone, GoHome, GoSubZone, NextTrack, GoFXSlot, etc.)

**Removed Categories**
- Automation (moved to Reaper actions)
- VCA and Folder (moved to Reaper actions)

---

## Common Migration Patterns

### Pattern 1: Multi-Page Banking → Multi-Zone Navigation

**OLD**:
```
Zone "Mixer"
  Widget Bank1 Bank 8                       ; Bank 8 tracks at a time
  Widget Display1 PageNameDisplay
ZoneEnd
```

**NEW**:
```
Zone "Mixer"
  Widget Bank1+Shift NextTrack              ; Shift+button for next track
  Widget Bank1+Control PreviousTrack        ; Control+button for prev
  Widget Display1 TrackNameDisplay
ZoneEnd

; Or use explicit zones:
Zone "Mixer1"
  ; Tracks 1-8
ZoneEnd

Zone "Mixer2"
  ; Tracks 9-16
ZoneEnd

Zone "Navigation"
  Widget Button1 GoZone Mixer1
  Widget Button2 GoZone Mixer2
ZoneEnd
```

---

### Pattern 2: Display Consolidation

**OLD**:
```
Zone "ChannelStrip"
  Widget Fader1 TrackVolume
  Widget VPot1 TrackPan
  Widget Display1 TrackNameDisplay
  Widget Display2 TrackNumberDisplay
  Widget Display3 TrackVolumeDB
  Widget Display4 TrackPanLeftDisplay
  Widget Display5 TrackPanRightDisplay
  Widget Meter1 TrackOutputMeterAverageLR
  Widget Meter2 TrackOutputMeterMaxPeakLR
ZoneEnd
```

**NEW**:
```
Zone "ChannelStrip"
  Widget Fader1 TrackVolume
  Widget VPot1 TrackPan
  Widget Display1 TrackNameDisplay
  Widget Display2 TrackVolumeDisplay        ; Auto dB format
  Widget Display3 TrackPanDisplay           ; Unified pan display
  Widget Meter1 TrackOutputMeter            ; Combined meter
ZoneEnd
```

---

### Pattern 3: Multi-Select Actions → Modifiers

**OLD**:
```
Zone "TrackSelect"
  Widget Button1 TrackSelect
  Widget Button1 TrackUniqueSelect
  Widget Button2 TrackRangeSelect
ZoneEnd
```

**NEW**:
```
Zone "TrackSelect"
  Widget Button1 TrackSelect                ; Normal select
  Widget Button1+Shift TrackSelect          ; Add to selection
  Widget Button1+Control TrackSelect        ; Toggle selection
  Widget Button2 TrackSelect                ; Alternative button
ZoneEnd
```

---

### Pattern 4: Automation Zone → Reaper Actions

**OLD**:
```
Zone "AutomationMode"
  Widget Button1 TrackAutoMode
  Widget Button1 CycleTrackAutoMode
  Widget Display1 TrackAutoModeDisplay
ZoneEnd
```

**NEW**:
```
Zone "AutomationMode"
  ; Use Reaper action IDs for automation
  Widget Button1 Reaper 40047              ; Toggle auto-latch
  Widget Button1+Shift Reaper 40048         ; Toggle auto-touch
  Widget Button1+Control Reaper 40076       ; Toggle auto-off
ZoneEnd
```

Find Reaper action IDs in REAPER's Action List (Alt+?).

---

### Pattern 5: FX Display → Dual Action

**OLD**:
```
Zone "FXMapping"
  Widget VPot1 LastTouchedFXParam
  Widget Display1 LastTouchedFXParamNameDisplay
  Widget Display2 LastTouchedFXParamValueDisplay
ZoneEnd
```

**NEW**:
```
Zone "FXMapping"
  Widget VPot1 LastTouchedFXParam
  Widget Display1 FXParamNameDisplay
  Widget Display2 FXParamValueDisplay
ZoneEnd

; Or for explicit FX:
Zone "FXMapping"
  Widget VPot1 FXParam 0
  Widget Display1 FXParamNameDisplay 0
  Widget Display2 FXParamValueDisplay 0
ZoneEnd
```

---

## Step-by-Step Migration Guide

### Step 1: Audit Existing Zones

Search your `.zon` files for deprecated actions:

```powershell
# PowerShell: Find all deprecated actions
$deprecated = @(
  'Bank', 'GoPage', 'NextPage', 'PageNameDisplay',
  'TrackUniqueSelect', 'TrackRangeSelect',
  'TrackNumberDisplay', 'TrackVolumeDB',
  'TrackVolumeWithMeterAverageLR', 'TrackVolumeWithMeterMaxPeakLR',
  'TrackPanLeftDisplay', 'TrackPanRightDisplay',
  'TrackPanPercent', 'TrackPanWidthPercent', 'TrackPanLPercent', 'TrackPanRPercent',
  'Speak ', 'SpeakFXMenuName', 'SpeakTrackSendDestination', 'SpeakTrackReceiveSource'
)

Get-ChildItem *.zon -Recurse | ForEach-Object {
  $deprecated | ForEach-Object {
    Select-String -Path $_.FullName -Pattern $_ | ForEach-Object {
      Write-Host "$($_.Path): $($_.LineNumber): $($_.Line.Trim())"
    }
  }
}
```

### Step 2: Back Up Zone Files

```powershell
Copy-Item ".\CSI\" ".\CSI.backup\" -Recurse
```

### Step 3: Update Zone Files

For each deprecated action found:

1. **Identify the action** - Check the tables above
2. **Find replacement** - Use the "Replacement" column
3. **Update zone file** - Replace action with new approach
4. **Test in REAPER** - Verify functionality

### Step 4: Test Thoroughly

- [ ] All widgets respond to input
- [ ] Displays show correct values
- [ ] Zone navigation works
- [ ] Modifier combinations function correctly
- [ ] FX parameter control works
- [ ] No REAPER errors in console

### Step 5: Document Changes

Add comments to zone files noting migration:

```
Zone "Mixer"
  ; MIGRATED: TrackNumberDisplay → TrackNameDisplay (consolidated)
  Widget Display1 TrackNameDisplay
  
  ; MIGRATED: TrackVolumeDB → TrackVolumeDisplay (auto dB)
  Widget Display2 TrackVolumeDisplay
ZoneEnd
```

---

## Troubleshooting Migration Issues

### Issue: "Unknown action: Bank"
**Solution**: Replace with `GoZone` or `NextTrack`/`PreviousTrack`

### Issue: "Action requires parameter: FXParam"
**Solution**: Ensure FX slot index is provided:
```
Widget VPot1 FXParam 0              ; ✓ Correct
Widget VPot1 FXParam                ; ✗ Missing parameter
```

### Issue: Display shows wrong value after migration
**Solution**: Verify display action matches control action:
```
Widget Fader1 TrackVolume
Widget Display1 TrackVolumeDisplay  ; ✓ Matches
Widget Display1 TrackPanDisplay     ; ✗ Doesn't match
```

### Issue: Multi-select broken after migration
**Solution**: Use modifiers instead of separate actions:
```
Widget Button1 TrackSelect           ; Normal select
Widget Button1+Shift TrackSelect     ; Shift: add to selection
```

### Issue: "Cannot find zone: PageNameDisplay"
**Solution**: PageNameDisplay is removed; create explicit zone or use FixedTextDisplay:
```
Zone "Navigation"
  Widget Display1 FixedTextDisplay "Mixer Zone"
ZoneEnd
```

---

## Version Compatibility

| Version | Status | Migration Needed? |
|---------|--------|-------------------|
| ≤ v5.x | ❌ Unsupported | Yes - full rewrite |
| v6.0+ | ✅ Current | Check this guide |
| Dev | ⚠️ Check changelog | Refer to Actions-Reference.md |

---

## Additional Resources

- [Actions-Reference.md](Actions-Reference.md) - Complete current action reference
- [Configuration-Format.md](Configuration-Format.md) - Zone file syntax
- [../docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) - System design overview
- [../Readme.md](../Readme.md) - Project overview

---

## Getting Help

If you encounter migration issues:

1. **Search Actions-Reference.md** for the current action syntax
2. **Review examples** in the "Complete Example Zone" section
3. **Check error logs** in REAPER console (Ctrl+Alt+?)
4. **Test incrementally** - update one zone at a time

---

**Last Updated**: June 2025  
**Changelog**: [10 - Change Log](https://github.com/FunkybotsEvilTwin/CSIUserGuide/wiki/10-%E2%80%90-Change-Log)







