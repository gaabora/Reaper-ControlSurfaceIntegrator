#include "format2_syntax.h"

#include <utility>

bool IsValidFormat2Identifier(const std::string& value) {
    if (value.empty()) return false;
    const char first = value[0];
    if (!((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z') || first == '_')) return false;
    for (std::size_t characterIdx = 1; characterIdx < value.size(); characterIdx++) {
        const char character = value[characterIdx];
        if (!((character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_' || character == '-')) return false;
    }
    return true;
}

class Format2SyntaxParser {
public:
    Format2SyntaxParser(Format2LexResult& lexical, std::size_t startTokenIndex) : lexical_(lexical), tokenIndex_(startTokenIndex) {}

    std::vector<Format2SyntaxNode> Parse() { return this->ParseSequence(false); }

private:
    Format2LexResult& lexical_;
    std::size_t tokenIndex_;

    const Format2Token& CurrentToken() const { return this->lexical_.tokens[this->tokenIndex_]; }

    bool AtEnd() const { return this->CurrentToken().kind == Format2TokenKind::EndOfFile; }

    void Advance() {
        if (!this->AtEnd()) this->tokenIndex_++;
    }

    void AddDiagnostic(const std::string& code, const std::string& message, const Format2SourceLocation& location) {
        this->lexical_.diagnostics.push_back({ code, message, location });
    }

    std::vector<Format2SyntaxNode> ParseSequence(bool stopAtRightBrace) {
        std::vector<Format2SyntaxNode> nodes;
        while (!this->AtEnd()) {
            while (this->CurrentToken().kind == Format2TokenKind::NewLine) this->Advance();
            if (this->AtEnd()) break;
            if (this->CurrentToken().kind == Format2TokenKind::RightBrace) {
                if (stopAtRightBrace) break;
                this->Advance();
                continue;
            }

            Format2SyntaxNode node;
            node.location = this->CurrentToken().location;
            while (!this->AtEnd() && this->CurrentToken().kind != Format2TokenKind::NewLine && this->CurrentToken().kind != Format2TokenKind::LeftBrace && this->CurrentToken().kind != Format2TokenKind::RightBrace) {
                node.tokens.push_back(this->CurrentToken());
                this->Advance();
            }

            if (this->CurrentToken().kind == Format2TokenKind::LeftBrace) {
                node.kind = Format2SyntaxNodeKind::Block;
                if (node.tokens.empty()) this->AddDiagnostic("format2.block.header", "A brace block requires a name", this->CurrentToken().location);
                this->AnalyzeTokens(node);
                this->Advance();
                node.children = this->ParseSequence(true);
                if (this->CurrentToken().kind == Format2TokenKind::RightBrace) {
                    node.endLocation = this->CurrentToken().location;
                    this->Advance();
                } else {
                    node.endLocation = this->CurrentToken().location;
                }
                nodes.push_back(std::move(node));
                continue;
            }

            if (!node.tokens.empty()) {
                node.kind = Format2SyntaxNodeKind::Line;
                node.endLocation = node.tokens.back().location;
                this->AnalyzeTokens(node);
                nodes.push_back(std::move(node));
            }
            if (this->CurrentToken().kind == Format2TokenKind::NewLine) this->Advance();
        }
        return nodes;
    }

    void AnalyzeTokens(Format2SyntaxNode& node) {
        std::size_t tokenIdx = 0;
        while (tokenIdx < node.tokens.size()) {
            const Format2Token& token = node.tokens[tokenIdx];
            if (token.kind != Format2TokenKind::Bare || tokenIdx + 1 >= node.tokens.size() || node.tokens[tokenIdx + 1].kind != Format2TokenKind::Equals) {
                node.positionalTokens.push_back(token);
                tokenIdx++;
                continue;
            }

            Format2PropertySyntax property;
            property.name = token.text;
            property.nameLocation = token.location;
            tokenIdx += 2;
            if (tokenIdx >= node.tokens.size()) {
                this->AddDiagnostic("format2.property.value", "Property " + property.name + " requires a value", property.nameLocation);
                node.properties.push_back(std::move(property));
                continue;
            }

            const Format2Token& valueToken = node.tokens[tokenIdx];
            property.value.location = valueToken.location;
            if (valueToken.kind == Format2TokenKind::LeftBracket) {
                tokenIdx = this->ParseList(node.tokens, tokenIdx, property);
            } else if (valueToken.kind == Format2TokenKind::Bare || valueToken.kind == Format2TokenKind::QuotedString) {
                property.value.scalar = { valueToken.text, valueToken.kind == Format2TokenKind::QuotedString, valueToken.location };
                tokenIdx++;
            } else {
                this->AddDiagnostic("format2.property.value", "Property " + property.name + " requires a scalar or list value", valueToken.location);
                tokenIdx++;
            }
            node.properties.push_back(std::move(property));
        }
    }

    std::size_t ParseList(const std::vector<Format2Token>& tokens, std::size_t tokenIdx, Format2PropertySyntax& property) {
        property.value.list = true;
        tokenIdx++;
        bool expectValue = true;
        bool hadValue = false;
        while (tokenIdx < tokens.size() && tokens[tokenIdx].kind != Format2TokenKind::RightBracket) {
            const Format2Token& token = tokens[tokenIdx];
            if (expectValue) {
                if (token.kind != Format2TokenKind::Bare && token.kind != Format2TokenKind::QuotedString) {
                    this->AddDiagnostic("format2.list.value", "List property " + property.name + " requires a value after [ or comma", token.location);
                    tokenIdx++;
                    continue;
                }
                property.value.items.push_back({ token.text, token.kind == Format2TokenKind::QuotedString, token.location });
                hadValue = true;
                expectValue = false;
                tokenIdx++;
                continue;
            }
            if (token.kind != Format2TokenKind::Comma) {
                this->AddDiagnostic("format2.list.comma", "List property " + property.name + " requires a comma between values", token.location);
                tokenIdx++;
                continue;
            }
            expectValue = true;
            tokenIdx++;
        }

        if (tokenIdx >= tokens.size() || tokens[tokenIdx].kind != Format2TokenKind::RightBracket) {
            this->AddDiagnostic("format2.list.unclosed", "List property " + property.name + " is not closed on this statement", property.value.location);
            return tokenIdx;
        }
        if (!hadValue) this->AddDiagnostic("format2.list.empty", "List property " + property.name + " cannot be empty", property.value.location);
        else if (expectValue) this->AddDiagnostic("format2.list.trailing-comma", "List property " + property.name + " cannot end with a comma", tokens[tokenIdx].location);
        return tokenIdx + 1;
    }
};

std::vector<Format2SyntaxNode> ParseFormat2Syntax(Format2LexResult& lexical, std::size_t startTokenIndex) {
    Format2SyntaxParser parser(lexical, startTokenIndex);
    return parser.Parse();
}

Format2WidgetSelectorParseResult ParseFormat2WidgetSelector(const Format2Token& token, bool allowPattern) {
    Format2WidgetSelectorParseResult result;
    if (token.kind != Format2TokenKind::Bare) {
        result.diagnostics.push_back({ "format2.widget-selector.token", "Widget selector must be one unquoted token", token.location });
        return result;
    }

    Format2WidgetSelector selector;
    selector.source = token.text;
    selector.location = token.location;
    const std::size_t qualifierPosition = token.text.find('@');
    const std::size_t patternPosition = token.text.find('*');
    if (qualifierPosition != std::string::npos && patternPosition != std::string::npos) {
        result.diagnostics.push_back({ "format2.widget-selector.mixed", "A Widget selector cannot combine @CH and *", token.location });
        return result;
    }
    if (qualifierPosition != std::string::npos) {
        if (qualifierPosition == 0 || token.text.substr(qualifierPosition) != "@CH" || token.text.find('@', qualifierPosition + 1) != std::string::npos) {
            result.diagnostics.push_back({ "format2.widget-selector.channel", "The only channel qualifier is terminal @CH", token.location });
            return result;
        }
        selector.kind = Format2WidgetSelectorKind::ChannelFamily;
        selector.baseName = token.text.substr(0, qualifierPosition);
    } else if (patternPosition != std::string::npos) {
        if (!allowPattern) {
            result.diagnostics.push_back({ "format2.widget-selector.pattern-context", "A wildcard is not allowed in this Widget position", token.location });
            return result;
        }
        if (patternPosition == 0 || patternPosition != token.text.size() - 1 || token.text.find('*', patternPosition + 1) != std::string::npos) {
            result.diagnostics.push_back({ "format2.widget-selector.pattern", "A Widget pattern must contain one terminal * after an identifier", token.location });
            return result;
        }
        selector.kind = Format2WidgetSelectorKind::Pattern;
        selector.baseName = token.text.substr(0, patternPosition);
    } else {
        selector.kind = Format2WidgetSelectorKind::Exact;
        selector.baseName = token.text;
    }

    if (!IsValidFormat2Identifier(selector.baseName)) {
        result.diagnostics.push_back({ "format2.widget-selector.identifier", "Widget selector base must be a valid identifier", token.location });
        return result;
    }
    result.selector = selector;
    return result;
}
