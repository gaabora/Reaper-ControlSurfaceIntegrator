#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class Format2TokenKind {
    EndOfFile,
    NewLine,
    Bare,
    QuotedString,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    LeftParenthesis,
    RightParenthesis,
    Equals,
    Plus,
    Comma,
};

struct Format2SourceLocation {
    std::size_t offset = 0;
    int line = 1;
    int column = 1;
};

struct Format2Token {
    Format2TokenKind kind = Format2TokenKind::EndOfFile;
    std::string text;
    Format2SourceLocation location;
    std::size_t length = 0;
};

enum class Format2DiagnosticSeverity {
    Error,
    Warning,
};

struct Format2Diagnostic {
    std::string code;
    std::string message;
    Format2SourceLocation location;
    Format2DiagnosticSeverity severity = Format2DiagnosticSeverity::Error;
};

inline bool HasFormat2DiagnosticErrors(const std::vector<Format2Diagnostic>& diagnostics) {
    for (const Format2Diagnostic& diagnostic : diagnostics) if (diagnostic.severity == Format2DiagnosticSeverity::Error) return true;
    return false;
}

struct Format2LexResult {
    std::string sourcePath;
    std::string source;
    std::vector<Format2Token> tokens;
    std::vector<Format2Diagnostic> diagnostics;
};

Format2LexResult LexFormat2Source(const std::string& source, const std::string& sourcePath = "");
