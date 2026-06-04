#include "lexer.h"

#include <stdexcept>
#include <unordered_map>

namespace izi {

Lexer::Lexer(std::string source, std::string filename)
    : src(std::move(source)), filename(std::move(filename)) {}

char Lexer::peek(int offset) const {
    size_t idx = pos + static_cast<size_t>(offset);
    if (idx >= src.size()) return '\0';
    return src[idx];
}

char Lexer::advance() {
    char c = src[pos++];
    if (c == '\n') {
        ++line;
        col = 1;
    } else {
        ++col;
    }
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (pos < src.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Line comment
            while (pos < src.size() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            // Block comment
            advance(); advance(); // consume /*
            while (pos + 1 < src.size()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); advance(); // consume */
                    break;
                }
                advance();
            }
        } else {
            break;
        }
    }
}

Token Lexer::readNumber() {
    int startLine = line, startCol = col;
    std::string text;
    bool isFloat = false;
    while (pos < src.size() && (std::isdigit(peek()) || peek() == '.')) {
        if (peek() == '.') {
            if (isFloat) break; // second dot — stop
            isFloat = true;
        }
        text += advance();
    }
    Token tok;
    tok.kind = isFloat ? TokenKind::FLOAT_LIT : TokenKind::INT_LIT;
    tok.text = text;
    tok.line = startLine;
    tok.col = startCol;
    return tok;
}

Token Lexer::readString() {
    int startLine = line, startCol = col;
    advance(); // consume opening "
    std::string text;
    while (pos < src.size() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            char esc = advance();
            switch (esc) {
                case 'n':  text += '\n'; break;
                case 't':  text += '\t'; break;
                case '\\': text += '\\'; break;
                case '"':  text += '"';  break;
                default:   text += '\\'; text += esc; break;
            }
        } else {
            text += advance();
        }
    }
    if (pos < src.size()) advance(); // consume closing "
    return Token{TokenKind::STRING_LIT, text, startLine, startCol};
}

Token Lexer::readIdentOrKeyword() {
    int startLine = line, startCol = col;
    std::string text;
    while (pos < src.size() && (std::isalnum(peek()) || peek() == '_')) {
        text += advance();
    }

    static const std::unordered_map<std::string, TokenKind> keywords = {
        {"fn",     TokenKind::KW_FN},
        {"let",    TokenKind::KW_LET},
        {"return", TokenKind::KW_RETURN},
        {"if",     TokenKind::KW_IF},
        {"else",   TokenKind::KW_ELSE},
        {"while",  TokenKind::KW_WHILE},
        {"for",    TokenKind::KW_FOR},
        {"import", TokenKind::KW_IMPORT},
        {"struct", TokenKind::KW_STRUCT},
        {"true",   TokenKind::KW_TRUE},
        {"false",  TokenKind::KW_FALSE},
        {"int",    TokenKind::TY_INT},
        {"float",  TokenKind::TY_FLOAT},
        {"bool",   TokenKind::TY_BOOL},
        {"string", TokenKind::TY_STRING},
        {"void",   TokenKind::TY_VOID},
    };

    auto it = keywords.find(text);
    TokenKind kind = (it != keywords.end()) ? it->second : TokenKind::IDENT;
    return Token{kind, text, startLine, startCol};
}

Token Lexer::makeToken(TokenKind kind, std::string text) {
    return Token{kind, std::move(text), line, col};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();

        if (pos >= src.size()) {
            tokens.push_back(Token{TokenKind::EOF_TOK, "", line, col});
            break;
        }

        int startLine = line, startCol = col;
        char c = peek();

        if (std::isdigit(c)) {
            tokens.push_back(readNumber());
            continue;
        }

        if (c == '"') {
            tokens.push_back(readString());
            continue;
        }

        if (std::isalpha(c) || c == '_') {
            tokens.push_back(readIdentOrKeyword());
            continue;
        }

        // Operators and delimiters
        advance();
        switch (c) {
            case '+': tokens.push_back({TokenKind::PLUS,     "+", startLine, startCol}); break;
            case '-':
                if (peek() == '>') { advance(); tokens.push_back({TokenKind::ARROW, "->", startLine, startCol}); }
                else tokens.push_back({TokenKind::MINUS, "-", startLine, startCol});
                break;
            case '*': tokens.push_back({TokenKind::STAR,     "*", startLine, startCol}); break;
            case '/': tokens.push_back({TokenKind::SLASH,    "/", startLine, startCol}); break;
            case '%': tokens.push_back({TokenKind::PERCENT,  "%", startLine, startCol}); break;
            case '=':
                if (peek() == '=') { advance(); tokens.push_back({TokenKind::EQEQ,  "==", startLine, startCol}); }
                else tokens.push_back({TokenKind::EQ, "=", startLine, startCol});
                break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back({TokenKind::BANGEQ, "!=", startLine, startCol}); }
                else tokens.push_back({TokenKind::BANG, "!", startLine, startCol});
                break;
            case '<':
                if (peek() == '=') { advance(); tokens.push_back({TokenKind::LTEQ, "<=", startLine, startCol}); }
                else tokens.push_back({TokenKind::LT, "<", startLine, startCol});
                break;
            case '>':
                if (peek() == '=') { advance(); tokens.push_back({TokenKind::GTEQ, ">=", startLine, startCol}); }
                else tokens.push_back({TokenKind::GT, ">", startLine, startCol});
                break;
            case '&':
                if (peek() == '&') { advance(); tokens.push_back({TokenKind::AMPAMP,   "&&", startLine, startCol}); }
                else tokens.push_back({TokenKind::UNKNOWN, "&", startLine, startCol});
                break;
            case '|':
                if (peek() == '|') { advance(); tokens.push_back({TokenKind::PIPEPIPE, "||", startLine, startCol}); }
                else tokens.push_back({TokenKind::UNKNOWN, "|", startLine, startCol});
                break;
            case '(': tokens.push_back({TokenKind::LPAREN,    "(", startLine, startCol}); break;
            case ')': tokens.push_back({TokenKind::RPAREN,    ")", startLine, startCol}); break;
            case '{': tokens.push_back({TokenKind::LBRACE,    "{", startLine, startCol}); break;
            case '}': tokens.push_back({TokenKind::RBRACE,    "}", startLine, startCol}); break;
            case '[': tokens.push_back({TokenKind::LBRACKET,  "[", startLine, startCol}); break;
            case ']': tokens.push_back({TokenKind::RBRACKET,  "]", startLine, startCol}); break;
            case ':': tokens.push_back({TokenKind::COLON,     ":", startLine, startCol}); break;
            case ',': tokens.push_back({TokenKind::COMMA,     ",", startLine, startCol}); break;
            case '.': tokens.push_back({TokenKind::DOT,       ".", startLine, startCol}); break;
            case ';': tokens.push_back({TokenKind::SEMICOLON, ";", startLine, startCol}); break;
            default:
                tokens.push_back({TokenKind::UNKNOWN, std::string(1, c), startLine, startCol});
                break;
        }
    }

    return tokens;
}

} // namespace izi
