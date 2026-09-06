#include "format2_zone_source_editor.h"

#include <algorithm>
#include <map>
#include <set>

#include "format2_zone_document.h"

static std::string JoinFormat2ZoneSourceLines(const std::vector<std::string>& lines) {
    std::string source;
    for (const std::string& line : lines) source += line + "\n";
    return source;
}

static bool MatchesFormat2ChannelFamily(const std::string& baseName, const std::string& widgetName, int surfaceChannelCount) {
    if (baseName.empty() || widgetName.size() <= baseName.size() || widgetName.compare(0, baseName.size(), baseName) != 0) return false;
    int channel = 0;
    for (std::size_t characterIdx = baseName.size(); characterIdx < widgetName.size(); ++characterIdx) {
        const char character = widgetName[characterIdx];
        if (character < '0' || character > '9') return false;
        channel = channel * 10 + character - '0';
    }
    return channel >= 1 && channel <= surfaceChannelCount;
}

static std::string ReplaceGeneratedFormat2Widget(const std::string& line, const std::string& widgetName, const std::string& replacementWidget) {
    std::size_t searchOffset = 0;
    while (searchOffset < line.size()) {
        const std::size_t widgetOffset = line.find(widgetName, searchOffset);
        if (widgetOffset == std::string::npos) return line;
        const bool startsToken = widgetOffset == 0 || line[widgetOffset - 1] == '+';
        const std::size_t widgetEnd = widgetOffset + widgetName.size();
        const bool endsToken = widgetEnd == line.size() || line[widgetEnd] == ' ' || line[widgetEnd] == '\t';
        if (startsToken && endsToken) return line.substr(0, widgetOffset) + replacementWidget + line.substr(widgetEnd);
        searchOffset = widgetOffset + widgetName.size();
    }
    return line;
}

static std::string FindFormat2InlineComment(const std::string& line) {
    bool insideQuote = false;
    bool escaped = false;
    for (std::size_t characterIdx = 0; characterIdx + 1 < line.size(); ++characterIdx) {
        if (insideQuote && escaped) {
            escaped = false;
            continue;
        }
        if (insideQuote && line[characterIdx] == '\\') {
            escaped = true;
            continue;
        }
        if (line[characterIdx] == '"') insideQuote = !insideQuote;
        if (!insideQuote && line[characterIdx] == '/' && line[characterIdx + 1] == '/') return line.substr(characterIdx);
    }
    return "";
}

static void ApplyFormat2InlineComments(std::vector<std::string>& replacementLines, const std::vector<std::string>& comments) {
    const std::size_t sharedCount = (std::min)(replacementLines.size(), comments.size());
    for (std::size_t commentIdx = 0; commentIdx < sharedCount; ++commentIdx) if (!comments[commentIdx].empty()) replacementLines[commentIdx] += " " + comments[commentIdx];
    for (std::size_t commentIdx = sharedCount; commentIdx < comments.size(); ++commentIdx) if (!comments[commentIdx].empty()) replacementLines.push_back(comments[commentIdx]);
}

static Format2ZoneParseResult ParseFormat2ZoneEditorSource(const std::string& source, const std::string& sourcePath, Format2DocumentKind& kind) {
    Format2ZoneParseResult mainResult = ParseFormat2ZoneDocumentSource(source, sourcePath, Format2DocumentKind::MainZone);
    if (!mainResult.document.metadata.matchFx) {
        kind = Format2DocumentKind::MainZone;
        return mainResult;
    }
    kind = Format2DocumentKind::FxZone;
    return ParseFormat2ZoneDocumentSource(source, sourcePath, kind);
}

static std::string FormatFirstZoneEditDiagnostic(const Format2ZoneParseResult& parsed, const std::string& fallback) {
    if (parsed.document.lexical.diagnostics.empty()) return fallback;
    const Format2Diagnostic& diagnostic = parsed.document.lexical.diagnostics.front();
    return diagnostic.code + " at line " + std::to_string(diagnostic.location.line) + ": " + diagnostic.message;
}

Format2ZoneWidgetEditResult EditFormat2ZoneWidgetSource(const std::string& sourcePath, const std::vector<std::string>& originalLines,
    const std::string& widgetName, int surfaceChannelCount, const std::vector<std::string>& requestedReplacementLines) {
    Format2ZoneWidgetEditResult result;
    Format2DocumentKind kind = Format2DocumentKind::MainZone;
    const std::string originalSource = JoinFormat2ZoneSourceLines(originalLines);
    const Format2ZoneParseResult parsed = ParseFormat2ZoneEditorSource(originalSource, sourcePath, kind);
    if (!parsed.IsValid() || parsed.document.metadata.version != 2) {
        result.message = FormatFirstZoneEditDiagnostic(parsed, "OSK can save only a valid format 2 zone");
        return result;
    }

    std::set<int> matchingLines;
    std::map<int, std::string> commentsByLine;
    std::string channelFamilySelector;
    bool foundExactBinding = false;
    for (const Format2ZoneBinding& binding : parsed.zone.bindings) {
        const bool exactMatch = binding.widget.kind == Format2WidgetSelectorKind::Exact && binding.widget.source == widgetName;
        const bool familyMatch = binding.widget.kind == Format2WidgetSelectorKind::ChannelFamily && MatchesFormat2ChannelFamily(binding.widget.baseName, widgetName, surfaceChannelCount);
        if (!exactMatch && !familyMatch) continue;
        matchingLines.insert(binding.location.line);
        foundExactBinding = foundExactBinding || exactMatch;
        if (familyMatch && !channelFamilySelector.empty() && channelFamilySelector != binding.widget.source) {
            result.message = "The selected Widget is produced by more than one channel-family selector";
            return result;
        }
        if (familyMatch) channelFamilySelector = binding.widget.source;
        if (binding.location.line >= 1 && static_cast<std::size_t>(binding.location.line) <= originalLines.size()) commentsByLine[binding.location.line] = FindFormat2InlineComment(originalLines[binding.location.line - 1]);
    }
    if (foundExactBinding && !channelFamilySelector.empty()) {
        result.message = "The selected Widget has both exact and channel-family bindings; resolve this conflict in the configuration editor";
        return result;
    }
    bool foundModifierDeclaration = false;
    for (const Format2ModifierDeclaration& declaration : parsed.zone.modifiers) {
        if (declaration.widget.kind != Format2WidgetSelectorKind::Exact || declaration.widget.source != widgetName) continue;
        foundModifierDeclaration = true;
        matchingLines.insert(declaration.location.line);
        if (declaration.location.line >= 1 && static_cast<std::size_t>(declaration.location.line) <= originalLines.size()) commentsByLine[declaration.location.line] = FindFormat2InlineComment(originalLines[declaration.location.line - 1]);
    }
    if (foundModifierDeclaration && !channelFamilySelector.empty()) {
        result.message = "The selected Widget combines an exact Modifier declaration with channel-family bindings; resolve this conflict in the configuration editor";
        return result;
    }

    std::vector<std::string> comments;
    for (const auto& commentEntry : commentsByLine) comments.push_back(commentEntry.second);

    std::vector<std::string> replacementLines = requestedReplacementLines;
    if (!channelFamilySelector.empty()) {
        result.editedChannelFamily = true;
        result.channelFamilyBaseName = channelFamilySelector.substr(0, channelFamilySelector.size() - 1);
        for (std::string& replacementLine : replacementLines) replacementLine = ReplaceGeneratedFormat2Widget(replacementLine, widgetName, channelFamilySelector);
    }
    ApplyFormat2InlineComments(replacementLines, comments);
    bool insertedReplacement = false;
    for (std::size_t lineIdx = 0; lineIdx < originalLines.size(); ++lineIdx) {
        const int lineNumber = static_cast<int>(lineIdx + 1);
        const auto matchingLine = matchingLines.find(lineNumber);
        if (matchingLine == matchingLines.end()) {
            result.lines.push_back(originalLines[lineIdx]);
            continue;
        }
        if (!insertedReplacement) {
            result.lines.insert(result.lines.end(), replacementLines.begin(), replacementLines.end());
            insertedReplacement = true;
        }
    }

    if (!insertedReplacement && !replacementLines.empty()) {
        if (!result.lines.empty() && !result.lines.back().empty()) result.lines.push_back("");
        result.lines.insert(result.lines.end(), replacementLines.begin(), replacementLines.end());
    }

    const Format2ZoneParseResult updated = ParseFormat2ZoneDocumentSource(JoinFormat2ZoneSourceLines(result.lines), sourcePath, kind);
    if (!updated.IsValid()) {
        result.message = FormatFirstZoneEditDiagnostic(updated, "The edited format 2 zone is invalid");
        result.lines.clear();
        return result;
    }
    result.parsed = updated;
    result.success = true;
    return result;
}
