local section = "ReaCtrlSurf_OSK"
local surfaces = reaper.GetExtState(section, "Surfaces")
reaper.ShowConsoleMsg("Surfaces: " .. tostring(surfaces) .. "\n")

for surf in tostring(surfaces):gmatch("[^|]+") do
  reaper.ShowConsoleMsg("\n[" .. surf .. "]\n")
  reaper.ShowConsoleMsg("State: " .. reaper.GetExtState(section, "State_" .. surf) .. "\n")
  reaper.ShowConsoleMsg("Labels: " .. reaper.GetExtState(section, "Labels_" .. surf) .. "\n")
  reaper.ShowConsoleMsg("LabelMap: " .. reaper.GetExtState(section, "LabelMap_" .. surf) .. "\n")
end