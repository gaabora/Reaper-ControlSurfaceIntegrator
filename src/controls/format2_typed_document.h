#pragma once

#include <variant>

#include "format2_learn_fx_document.h"
#include "format2_surface_document.h"
#include "format2_zone_document.h"

using Format2TypedDocument = std::variant<std::monostate, Format2SurfaceDocument, Format2ZoneDocument, Format2LearnFxDocument>;

struct Format2TypedDocumentParseResult {
    Format2DocumentParseResult document;
    Format2TypedDocument model;

    bool IsValid() const { return this->document.IsValid(); }
    const std::vector<Format2Diagnostic>& Diagnostics() const { return this->document.lexical.diagnostics; }
};

Format2TypedDocumentParseResult ParseFormat2TypedDocumentSource(const std::string& source, const std::string& sourcePath, Format2DocumentKind kind);
