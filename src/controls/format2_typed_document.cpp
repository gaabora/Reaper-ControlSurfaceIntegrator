#include "format2_typed_document.h"

#include <utility>

Format2TypedDocumentParseResult ParseFormat2TypedDocumentSource(const std::string& source, const std::string& sourcePath, Format2DocumentKind kind) {
    Format2TypedDocumentParseResult result;

    switch (kind) {
        case Format2DocumentKind::Surface: {
            Format2SurfaceParseResult parsed = ParseFormat2SurfaceSource(source, sourcePath);
            result.document = std::move(parsed.document);
            result.model = std::move(parsed.surface);
            return result;
        }
        case Format2DocumentKind::MainZone:
        case Format2DocumentKind::FxZone:
        case Format2DocumentKind::Snippet: {
            Format2ZoneParseResult parsed = ParseFormat2ZoneDocumentSource(source, sourcePath, kind);
            result.document = std::move(parsed.document);
            result.model = std::move(parsed.zone);
            return result;
        }
        case Format2DocumentKind::LearnFx: {
            Format2LearnFxParseResult parsed = ParseFormat2LearnFxDocumentSource(source, sourcePath);
            result.document = std::move(parsed.document);
            result.model = std::move(parsed.learnFx);
            return result;
        }
    }

    result.document.kind = kind;
    result.document.lexical.source = source;
    result.document.lexical.sourcePath = sourcePath;
    result.document.lexical.diagnostics.push_back({"format2.document.kind.unsupported", "Unsupported format 2 document kind", {}});
    return result;
}
