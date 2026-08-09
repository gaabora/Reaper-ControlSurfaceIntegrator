local M = {}

function M.New(imgui, ctx)
    local state = {
        imgui = imgui,
        ctx = ctx,
        cache = {},
    }

    local api = {}

    function api:Get(family, size)
        local numericSize = math.max(8, math.floor((tonumber(size) or 12) + 0.5))
        local cacheKey = table.concat({ tostring(family or "sans-serif"), tostring(numericSize) }, "|")
        if state.cache[cacheKey] then return state.cache[cacheKey] end
        local ok, font = pcall(state.imgui.CreateFont, family or "sans-serif", numericSize)
        if not ok or not font then return nil end
        state.imgui.Attach(state.ctx, font)
        state.cache[cacheKey] = font
        return font
    end

    function api:Build(definitions)
        local fonts = {}
        for index, definition in ipairs(definitions or {}) do
            local font = self:Get(definition.family, definition.size or definition.px)
            fonts[definition.key or index] = font
        end
        return fonts
    end

    return api
end

return M
