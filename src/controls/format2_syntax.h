#pragma once

#include <optional>
#include <string>
#include <vector>

#include "format2_lexer.h"

enum class Format2SyntaxNodeKind {
    Line,
    Block,
};

struct Format2ScalarSyntax {
    std::string text;
    bool quoted = false;
    Format2SourceLocation location;
};

struct Format2ValueSyntax {
    bool list = false;
    Format2ScalarSyntax scalar;
    std::vector<Format2ScalarSyntax> items;
    Format2SourceLocation location;
};

struct Format2PropertySyntax {
    std::string name;
    Format2SourceLocation nameLocation;
    Format2ValueSyntax value;
};

struct Format2SyntaxNode {
    Format2SyntaxNodeKind kind = Format2SyntaxNodeKind::Line;
    Format2SourceLocation location;
    Format2SourceLocation endLocation;
    std::vector<Format2Token> tokens;
    std::vector<Format2Token> positionalTokens;
    std::vector<Format2PropertySyntax> properties;
    std::vector<Format2SyntaxNode> children;
};

enum class Format2WidgetSelectorKind {
    Exact,
    ChannelFamily,
};

struct Format2WidgetSelector {
    Format2WidgetSelectorKind kind = Format2WidgetSelectorKind::Exact;
    std::string source;
    std::string baseName;
    Format2SourceLocation location;
};

struct Format2WidgetSelectorParseResult {
    std::optional<Format2WidgetSelector> selector;
    std::vector<Format2Diagnostic> diagnostics;
};

std::vector<Format2SyntaxNode> ParseFormat2Syntax(Format2LexResult& lexical, std::size_t startTokenIndex);
Format2WidgetSelectorParseResult ParseFormat2WidgetSelector(const Format2Token& token);
bool IsValidFormat2Identifier(const std::string& value);
