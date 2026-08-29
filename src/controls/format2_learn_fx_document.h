#pragma once

#include <cstddef>
#include <vector>

#include "format2_document.h"
#include "format2_zone_document.h"

enum class Format2LearnFxWidgetRole {
    Parameter,
    NameDisplay,
    ValueDisplay,
};

struct Format2LearnFxWidget {
    Format2LearnFxWidgetRole role = Format2LearnFxWidgetRole::Parameter;
    Format2SourceLocation location;
    Format2WidgetSelector selector;
    std::vector<Format2PropertySyntax> defaults;
};

enum class Format2LearnFxGeneratedKind {
    Binding,
    Lifecycle,
};

struct Format2LearnFxGeneratedStatement {
    Format2LearnFxGeneratedKind kind = Format2LearnFxGeneratedKind::Binding;
    std::size_t index = 0;
    Format2SourceLocation location;
};

struct Format2LearnFxDocument {
    std::vector<Format2LearnFxWidget> widgets;
    std::vector<Format2ZoneBinding> generatedBindings;
    std::vector<Format2LifecycleBlock> generatedLifecycleBlocks;
    std::vector<Format2LearnFxGeneratedStatement> generatedOrder;
};

struct Format2LearnFxParseResult {
    Format2DocumentParseResult document;
    Format2LearnFxDocument learnFx;

    bool IsValid() const { return this->document.IsValid(); }
};

Format2LearnFxParseResult ParseFormat2LearnFxDocumentSource(const std::string& source, const std::string& sourcePath);
