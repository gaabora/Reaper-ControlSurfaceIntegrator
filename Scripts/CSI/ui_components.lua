local imgui = require "imgui" "0.9.3"

local M = {}

local function normalizeId(text, fallback)
    local source = tostring(text or "")
    source = source:gsub("[%s%p]+", "_"):lower()
    if source == "" then source = fallback or "ui" end
    return source
end

function M.Tooltip(ctx, text, wrapped)
    text = tostring(text or "")
    if text == "" then return end
    if imgui.BeginTooltip(ctx) then
        if wrapped and imgui.PushTextWrapPos then
            imgui.PushTextWrapPos(ctx, imgui.GetFontSize(ctx) * 42)
            imgui.Text(ctx, text)
            imgui.PopTextWrapPos(ctx)
        elseif wrapped and imgui.TextWrapped then
            imgui.TextWrapped(ctx, text)
        else
            imgui.Text(ctx, text)
        end
        imgui.EndTooltip(ctx)
    end
end

function M.ItemTooltip(ctx, text, wrapped)
    if imgui.IsItemHovered(ctx) then
        M.Tooltip(ctx, text, wrapped)
    end
end

function M.HelpTooltip(ctx, text)
    M.ItemTooltip(ctx, text, true)
end

function M.Disabled(ctx, disabled, renderFn)
    if imgui.BeginDisabled then imgui.BeginDisabled(ctx, disabled == true) end
    local results = table.pack(renderFn())
    if imgui.EndDisabled then imgui.EndDisabled(ctx) end
    return table.unpack(results, 1, results.n)
end

function M.DirtyActionButton(ctx, label, enabled, onClick)
    local clicked = M.Disabled(ctx, not enabled, function()
        imgui.PushStyleColor(ctx, imgui.Col_Button, 0x8f2424ff)
        imgui.PushStyleColor(ctx, imgui.Col_ButtonHovered, 0xb83232ff)
        imgui.PushStyleColor(ctx, imgui.Col_ButtonActive, 0x701b1bff)
        local pressed = imgui.Button(ctx, label)
        imgui.PopStyleColor(ctx, 3)
        return pressed
    end)
    if clicked and enabled and onClick then onClick() end
    return clicked and enabled
end

function M.SliderWithInput(ctx, label, currentValue, minValue, maxValue, step, options)
    options = options or {}
    local changed = false
    local newValue = currentValue
    local sliderId = options.sliderId or ("##" .. normalizeId(label, "slider"))

    if options.sliderWidth then
        imgui.SetNextItemWidth(ctx, options.sliderWidth)
    end
    local rv
    rv, newValue = imgui.SliderInt(ctx, sliderId, newValue, minValue, maxValue)
    if rv then
        if step and step > 0 then
            newValue = math.floor((newValue + step / 2) / step) * step
        end
        newValue = math.max(minValue, math.min(maxValue, newValue))
        changed = true
    end

    if options.useInput ~= false then
        imgui.SameLine(ctx)
        imgui.SetNextItemWidth(ctx, options.inputWidth or 50)
        local inputValue = tostring(math.floor(newValue))
        rv, inputValue = imgui.InputText(ctx, options.inputId or (sliderId .. "_input"), inputValue, options.inputChars or 5)
        if rv then
            local numericValue = tonumber(inputValue)
            if numericValue then
                newValue = math.max(minValue, math.min(maxValue, numericValue))
                changed = true
            end
        end
    end

    if options.showLabel ~= false then
        imgui.SameLine(ctx)
        imgui.Text(ctx, label)
    end

    return changed, newValue
end

function M.LabelReplacementEditor(ctx, label, value, helpText, options)
    options = options or {}
    imgui.Text(ctx, label)
    if helpText and helpText ~= "" then
        M.HelpTooltip(ctx, helpText)
    end

    local inputId = options.inputId or ("##" .. normalizeId(label, "label_replacements"))
    local changed
    local newValue
    if options.placeholder and imgui.InputTextWithHint then
        changed, newValue = imgui.InputTextWithHint(ctx, inputId, options.placeholder, value or "")
    else
        changed, newValue = imgui.InputText(ctx, inputId, value or "")
    end

    if helpText and helpText ~= "" then
        M.HelpTooltip(ctx, helpText)
    end

    return changed, newValue
end

return M
