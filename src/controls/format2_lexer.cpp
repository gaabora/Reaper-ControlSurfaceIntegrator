#include "format2_lexer.h"

#include <utility>

class Format2Lexer {
public:
    Format2Lexer(const std::string& source, const std::string& sourcePath) {
        this->result_.sourcePath = sourcePath;
        this->result_.source = source;
        if (this->result_.source.compare(0, 3, "\xEF\xBB\xBF") == 0) this->offset_ = 3;
    }

    Format2LexResult Lex() {
        while (!this->AtEnd()) {
            const char current = this->Current();
            if (current == ' ' || current == '\t') {
                this->AdvanceByte();
                continue;
            }
            if (current == '\r' || current == '\n') {
                this->LexNewLine();
                continue;
            }
            if (current == '/' && this->Peek() == '/') {
                this->SkipComment();
                continue;
            }
            if (current == '"') {
                this->LexQuotedString();
                continue;
            }
            if (this->LexStructuralToken(current)) continue;
            if (static_cast<unsigned char>(current) < 0x20 || current == 0x7F) {
                this->AddDiagnostic("format2.character.invalid", "Unsupported control character");
                this->AdvanceByte();
                continue;
            }
            this->LexBareToken();
        }

        this->AddToken(Format2TokenKind::EndOfFile, "", this->Location(), 0);
        return std::move(this->result_);
    }

private:
    Format2LexResult result_;
    std::size_t offset_ = 0;
    int line_ = 1;
    int column_ = 1;

    bool AtEnd() const { return this->offset_ >= this->result_.source.size(); }

    char Current() const { return this->AtEnd() ? '\0' : this->result_.source[this->offset_]; }

    char Peek() const { return this->offset_ + 1 >= this->result_.source.size() ? '\0' : this->result_.source[this->offset_ + 1]; }

    Format2SourceLocation Location() const { return { this->offset_, this->line_, this->column_ }; }

    void AdvanceByte() {
        if (this->AtEnd()) return;
        this->offset_++;
        this->column_++;
    }

    void AddToken(Format2TokenKind kind, const std::string& text, const Format2SourceLocation& location, std::size_t length) {
        this->result_.tokens.push_back({ kind, text, location, length });
    }

    void AddDiagnostic(const std::string& code, const std::string& message) {
        this->result_.diagnostics.push_back({ code, message, this->Location() });
    }

    void LexNewLine() {
        const Format2SourceLocation location = this->Location();
        const std::size_t startOffset = this->offset_;
        if (this->Current() == '\r') {
            this->AdvanceByte();
            if (this->Current() == '\n') this->AdvanceByte();
        } else {
            this->AdvanceByte();
        }
        this->line_++;
        this->column_ = 1;
        this->AddToken(Format2TokenKind::NewLine, "\n", location, this->offset_ - startOffset);
    }

    void SkipComment() {
        while (!this->AtEnd() && this->Current() != '\r' && this->Current() != '\n') this->AdvanceByte();
    }

    bool LexStructuralToken(char current) {
        Format2TokenKind kind;
        switch (current) {
            case '{': kind = Format2TokenKind::LeftBrace; break;
            case '}': kind = Format2TokenKind::RightBrace; break;
            case '[': kind = Format2TokenKind::LeftBracket; break;
            case ']': kind = Format2TokenKind::RightBracket; break;
            case '(': kind = Format2TokenKind::LeftParenthesis; break;
            case ')': kind = Format2TokenKind::RightParenthesis; break;
            case '=': kind = Format2TokenKind::Equals; break;
            case '+': kind = Format2TokenKind::Plus; break;
            case ',': kind = Format2TokenKind::Comma; break;
            default: return false;
        }
        const Format2SourceLocation location = this->Location();
        this->AdvanceByte();
        this->AddToken(kind, std::string(1, current), location, 1);
        return true;
    }

    void LexQuotedString() {
        const Format2SourceLocation location = this->Location();
        const std::size_t startOffset = this->offset_;
        std::string value;
        bool closed = false;
        this->AdvanceByte();

        while (!this->AtEnd()) {
            const char current = this->Current();
            if (current == '"') {
                this->AdvanceByte();
                closed = true;
                break;
            }
            if (current == '\r' || current == '\n') {
                this->AddDiagnostic("format2.string.newline", "Quoted strings cannot contain a raw newline");
                break;
            }
            if (current != '\\') {
                value.push_back(current);
                this->AdvanceByte();
                continue;
            }

            const Format2SourceLocation escapeLocation = this->Location();
            this->AdvanceByte();
            if (this->AtEnd()) break;
            const char escaped = this->Current();
            switch (escaped) {
                case '\\': value.push_back('\\'); break;
                case '"': value.push_back('"'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default:
                    this->result_.diagnostics.push_back({ "format2.string.escape", "Unknown quoted-string escape", escapeLocation });
                    value.push_back(escaped);
                    break;
            }
            this->AdvanceByte();
        }

        if (!closed && (this->AtEnd() || (this->Current() != '\r' && this->Current() != '\n'))) this->result_.diagnostics.push_back({ "format2.string.unclosed", "Quoted string is not closed before EOF", location });
        this->AddToken(Format2TokenKind::QuotedString, value, location, this->offset_ - startOffset);
    }

    void LexBareToken() {
        const Format2SourceLocation location = this->Location();
        const std::size_t startOffset = this->offset_;
        while (!this->AtEnd()) {
            const char current = this->Current();
            if (current == ' ' || current == '\t' || current == '\r' || current == '\n' || current == '{' || current == '}' || current == '[' || current == ']' || current == '(' || current == ')' || current == '=' || current == '+' || current == ',' || (current == '/' && this->Peek() == '/') || static_cast<unsigned char>(current) < 0x20 || current == 0x7F) break;
            this->AdvanceByte();
        }
        this->AddToken(Format2TokenKind::Bare, this->result_.source.substr(startOffset, this->offset_ - startOffset), location, this->offset_ - startOffset);
    }
};

Format2LexResult LexFormat2Source(const std::string& source, const std::string& sourcePath) {
    Format2Lexer lexer(source, sourcePath);
    return lexer.Lex();
}
