#pragma once

#include "../ast/ast.h"
#include "../lexer/lexer.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace izi {

class ParseError : public std::runtime_error {
public:
    int line, col;
    ParseError(const std::string& msg, int line, int col)
        : std::runtime_error(msg), line(line), col(col) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();

private:
    std::vector<Token> tokens;
    size_t pos{0};

    // Token access
    const Token& peek(int offset = 0) const;
    const Token& current() const;
    Token consume();
    Token expect(TokenKind kind, const std::string& what);
    bool check(TokenKind kind) const;
    bool match(TokenKind kind);

    // Type parsing
    IziType parseType();

    // Declaration parsing
    std::unique_ptr<FnDecl> parseFnDecl();
    std::unique_ptr<ImportDecl> parseImportDecl();

    // Statement parsing
    std::unique_ptr<BlockStmt> parseBlock();
    StmtPtr parseStmt();
    StmtPtr parseVarDecl();
    StmtPtr parseReturnStmt();
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();

    // Expression parsing (precedence climbing)
    ExprPtr parseExpr();
    ExprPtr parseAssign();
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseAddSub();
    ExprPtr parseMulDiv();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();
};

} // namespace izi
