#include "format2_zone_compiler.h"

Format2ZoneCompileResult CompileFormat2ZoneBindings(const std::vector<Format2ZoneBinding>& bindings, int surfaceChannelCount) {
    Format2ZoneCompileResult result;

    for (std::size_t bindingIndex = 0; bindingIndex < bindings.size(); bindingIndex++) {
        const Format2ZoneBinding& binding = bindings[bindingIndex];
        if (binding.widget.kind != Format2WidgetSelectorKind::ChannelFamily) {
            result.actionContexts.push_back({bindingIndex, binding.widget.source, std::nullopt});
            continue;
        }
        if (surfaceChannelCount < 1) {
            result.diagnostics.push_back({"format2.zone.channel-count.required", "An @CH binding requires a positive Surface channel count", binding.widget.location});
            continue;
        }
        for (int surfaceChannelOffset = 0; surfaceChannelOffset < surfaceChannelCount; surfaceChannelOffset++) {
            result.actionContexts.push_back({bindingIndex, binding.widget.baseName + std::to_string(surfaceChannelOffset + 1), surfaceChannelOffset});
        }
    }

    return result;
}
