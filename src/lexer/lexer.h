#pragma once

#include <string>
#include <vector>

namespace izi {

enum class TokenKind {
    // Literals
    INT_LIT,
    FLOAT_LIT,
    STRING_LIT,

    // Identifiers
    IDENT,

    // Keywords
    KW_FN,
    KW_LET,
    KW_RETURN,
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_FOR,
    KW_IMPORT,
    KW_STRUCT,
    KW_TRUE,
    KW_FALSE,

    // Built-in type names
    TY_INT,
    TY_FLOAT,
    TY_BOOL,
    TY_STRING,
    TY_VOID,

    // Operators
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    PERCENT,    // %
    EQ,         // =
    EQEQ,       // ==
    BANG,       // !
    BANGEQ,     // !=
    LT,         // <
    GT,         // >
    LTEQ,       // <=
    GTEQ,       // >=
    AMPAMP,     // &&
    PIPEPIPE,   // ||
    ARROW,      // ->

    // Delimiters
    LPAREN,    // (
    RPAREN,    // )
    LBRACE,    // {
    RBRACE,    // }
    LBRACKET,  // [
    RBRACKET,  // ]
    COLON,     // :
    COMMA,     // ,
    DOT,       // .
    SEMICOLON, // ;

    // Special
    EOF_TOK,
    UNKNOWN,
};

struct Token {
    TokenKind kind;
    std::string text;
    int line;
    int col;
};

class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<input>");
    std::vector<Token> tokenize();

private:
    std::string src;
    std::string filename;
    size_t pos{0};
    int line{1};
    int col{1};

    char peek(int offset = 0) const;
    char advance();
    void skipWhitespaceAndComments();
    Token readNumber();
    Token readString();
    Token readIdentOrKeyword();
    Token makeToken(TokenKind kind, std::string text);
};

} // namespace izi
