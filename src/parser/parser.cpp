#include "parser.h"

#include <sstream>

namespace izi {

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

const Token& Parser::peek(int offset) const {
    size_t idx = pos + static_cast<size_t>(offset);
    if (idx >= tokens.size()) return tokens.back(); // EOF
    return tokens[idx];
}

const Token& Parser::current() const { return peek(0); }

Token Parser::consume() {
    Token t = tokens[pos];
    if (pos + 1 < tokens.size()) ++pos;
    return t;
}

Token Parser::expect(TokenKind kind, const std::string& what) {
    if (!check(kind)) {
        std::ostringstream oss;
        oss << "Expected " << what << ", got '" << current().text
            << "' at line " << current().line << ":" << current().col;
        throw ParseError(oss.str(), current().line, current().col);
    }
    return consume();
}

bool Parser::check(TokenKind kind) const { return current().kind == kind; }

bool Parser::match(TokenKind kind) {
    if (check(kind)) { consume(); return true; }
    return false;
}

// ---------------------------------------------------------------------------
// Type parsing
// ---------------------------------------------------------------------------

IziType Parser::parseType() {
    switch (current().kind) {
        case TokenKind::TY_INT:    consume(); return IziType::INT;
        case TokenKind::TY_FLOAT:  consume(); return IziType::FLOAT;
        case TokenKind::TY_BOOL:   consume(); return IziType::BOOL;
        case TokenKind::TY_STRING: consume(); return IziType::STRING;
        case TokenKind::TY_VOID:   consume(); return IziType::VOID;
        default: {
            std::ostringstream oss;
            oss << "Expected type name, got '" << current().text
                << "' at line " << current().line << ":" << current().col;
            throw ParseError(oss.str(), current().line, current().col);
        }
    }
}

// ---------------------------------------------------------------------------
// Program
// ---------------------------------------------------------------------------

Program Parser::parse() {
    Program prog;
    while (!check(TokenKind::EOF_TOK)) {
        if (check(TokenKind::KW_FN)) {
            prog.functions.push_back(parseFnDecl());
        } else if (check(TokenKind::KW_IMPORT)) {
            prog.imports.push_back(parseImportDecl());
        } else {
            std::ostringstream oss;
            oss << "Unexpected token '" << current().text
                << "' at line " << current().line << ":" << current().col;
            throw ParseError(oss.str(), current().line, current().col);
        }
    }
    return prog;
}

// ---------------------------------------------------------------------------
// Declarations
// ---------------------------------------------------------------------------

std::unique_ptr<FnDecl> Parser::parseFnDecl() {
    auto decl = std::make_unique<FnDecl>();
    decl->line = current().line;
    decl->col  = current().col;

    expect(TokenKind::KW_FN, "fn");
    Token nameToken = expect(TokenKind::IDENT, "function name");
    decl->name = nameToken.text;

    expect(TokenKind::LPAREN, "(");
    while (!check(TokenKind::RPAREN) && !check(TokenKind::EOF_TOK)) {
        Param p;
        Token pname = expect(TokenKind::IDENT, "parameter name");
        p.name = pname.text;
        expect(TokenKind::COLON, ":");
        p.type = parseType();
        decl->params.push_back(std::move(p));
        if (!match(TokenKind::COMMA)) break;
    }
    expect(TokenKind::RPAREN, ")");

    if (match(TokenKind::ARROW)) {
        decl->returnType = parseType();
    } else {
        decl->returnType = IziType::VOID;
    }

    decl->body = parseBlock();
    return decl;
}

std::unique_ptr<ImportDecl> Parser::parseImportDecl() {
    auto decl = std::make_unique<ImportDecl>();
    decl->line = current().line;
    decl->col  = current().col;

    expect(TokenKind::KW_IMPORT, "import");

    // path is dot-separated identifiers
    std::string path;
    Token first = expect(TokenKind::IDENT, "import path");
    path = first.text;
    while (check(TokenKind::DOT)) {
        consume();
        Token part = expect(TokenKind::IDENT, "import path component");
        path += "." + part.text;
    }
    decl->path = path;
    match(TokenKind::SEMICOLON);
    return decl;
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    auto block = std::make_unique<BlockStmt>();
    block->line = current().line;
    block->col  = current().col;

    expect(TokenKind::LBRACE, "{");
    while (!check(TokenKind::RBRACE) && !check(TokenKind::EOF_TOK)) {
        block->stmts.push_back(parseStmt());
    }
    expect(TokenKind::RBRACE, "}");
    return block;
}

StmtPtr Parser::parseStmt() {
    if (check(TokenKind::KW_LET))    return parseVarDecl();
    if (check(TokenKind::KW_RETURN)) return parseReturnStmt();
    if (check(TokenKind::KW_IF))     return parseIfStmt();
    if (check(TokenKind::KW_WHILE))  return parseWhileStmt();

    // Expression statement
    auto stmt = std::make_unique<ExprStmt>();
    stmt->line = current().line;
    stmt->col  = current().col;
    stmt->expr = parseExpr();
    match(TokenKind::SEMICOLON);
    return stmt;
}

StmtPtr Parser::parseVarDecl() {
    auto stmt = std::make_unique<VarDeclStmt>();
    stmt->line = current().line;
    stmt->col  = current().col;

    expect(TokenKind::KW_LET, "let");
    Token name = expect(TokenKind::IDENT, "variable name");
    stmt->name = name.text;

    if (match(TokenKind::COLON)) {
        stmt->declaredType = parseType();
    }

    if (match(TokenKind::EQ)) {
        stmt->init = parseExpr();
    }

    match(TokenKind::SEMICOLON);
    return stmt;
}

StmtPtr Parser::parseReturnStmt() {
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->line = current().line;
    stmt->col  = current().col;

    expect(TokenKind::KW_RETURN, "return");
    if (!check(TokenKind::SEMICOLON) && !check(TokenKind::RBRACE)) {
        stmt->value = parseExpr();
    }
    match(TokenKind::SEMICOLON);
    return stmt;
}

StmtPtr Parser::parseIfStmt() {
    auto stmt = std::make_unique<IfStmt>();
    stmt->line = current().line;
    stmt->col  = current().col;

    expect(TokenKind::KW_IF, "if");
    stmt->condition = parseExpr();
    stmt->thenBlock = parseBlock();

    if (match(TokenKind::KW_ELSE)) {
        stmt->elseBlock = parseBlock();
    }

    return stmt;
}

StmtPtr Parser::parseWhileStmt() {
    auto stmt = std::make_unique<WhileStmt>();
    stmt->line = current().line;
    stmt->col  = current().col;

    expect(TokenKind::KW_WHILE, "while");
    stmt->condition = parseExpr();
    stmt->body = parseBlock();
    return stmt;
}

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

ExprPtr Parser::parseExpr() { return parseAssign(); }

ExprPtr Parser::parseAssign() {
    // Check for assignment: IDENT = expr
    if (check(TokenKind::IDENT) && peek(1).kind == TokenKind::EQ) {
        auto expr = std::make_unique<AssignExpr>();
        expr->line = current().line;
        expr->col  = current().col;
        expr->name = consume().text; // consume IDENT
        consume();                   // consume =
        expr->value = parseAssign();
        return expr;
    }
    return parseOr();
}

ExprPtr Parser::parseOr() {
    auto lhs = parseAnd();
    while (check(TokenKind::PIPEPIPE)) {
        std::string op = consume().text;
        auto rhs = parseAnd();
        auto bin = std::make_unique<BinaryExpr>();
        bin->op  = op;
        bin->lhs = std::move(lhs);
        bin->rhs = std::move(rhs);
        lhs = std::move(bin);
    }
    return lhs;
}

ExprPtr Parser::parseAnd() {
    auto lhs = parseEquality();
    while (check(TokenKind::AMPAMP)) {
        std::string op = consume().text;
        auto rhs = parseEquality();
        auto bin = std::make_unique<BinaryExpr>();
        bin->op  = op;
        bin->lhs = std::move(lhs);
        bin->rhs = std::move(rhs);
        lhs = std::move(bin);
    }
    return lhs;
}

ExprPtr Parser::parseEquality() {
    auto lhs = parseComparison();
    while (check(TokenKind::EQEQ) || check(TokenKind::BANGEQ)) {
        std::string op = consume().text;
        auto rhs = parseComparison();
        auto bin = std::make_unique<BinaryExpr>();
        bin->op  = op;
        bin->lhs = std::move(lhs);
        bin->rhs = std::move(rhs);
        lhs = std::move(bin);
    }
    return lhs;
}

ExprPtr Parser::parseComparison() {
    auto lhs = parseAddSub();
    while (check(TokenKind::LT) || check(TokenKind::GT) ||
           check(TokenKind::LTEQ) || check(TokenKind::GTEQ)) {
        std::string op = consume().text;
        auto rhs = parseAddSub();
        auto bin = std::make_unique<BinaryExpr>();
        bin->op  = op;
        bin->lhs = std::move(lhs);
        bin->rhs = std::move(rhs);
        lhs = std::move(bin);
    }
    return lhs;
}

ExprPtr Parser::parseAddSub() {
    auto lhs = parseMulDiv();
    while (check(TokenKind::PLUS) || check(TokenKind::MINUS)) {
        std::string op = consume().text;
        auto rhs = parseMulDiv();
        auto bin = std::make_unique<BinaryExpr>();
        bin->op  = op;
        bin->lhs = std::move(lhs);
        bin->rhs = std::move(rhs);
        lhs = std::move(bin);
    }
    return lhs;
}

ExprPtr Parser::parseMulDiv() {
    auto lhs = parseUnary();
    while (check(TokenKind::STAR) || check(TokenKind::SLASH) || check(TokenKind::PERCENT)) {
        std::string op = consume().text;
        auto rhs = parseUnary();
        auto bin = std::make_unique<BinaryExpr>();
        bin->op  = op;
        bin->lhs = std::move(lhs);
        bin->rhs = std::move(rhs);
        lhs = std::move(bin);
    }
    return lhs;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenKind::MINUS) || check(TokenKind::BANG)) {
        std::string op = consume().text;
        auto expr = std::make_unique<UnaryExpr>();
        expr->op = op;
        expr->operand = parseUnary();
        return expr;
    }
    return parseCall();
}

ExprPtr Parser::parseCall() {
    if (check(TokenKind::IDENT) && peek(1).kind == TokenKind::LPAREN) {
        auto expr = std::make_unique<CallExpr>();
        expr->line = current().line;
        expr->col  = current().col;
        expr->callee = consume().text; // IDENT
        consume();                     // (
        while (!check(TokenKind::RPAREN) && !check(TokenKind::EOF_TOK)) {
            expr->args.push_back(parseExpr());
            if (!match(TokenKind::COMMA)) break;
        }
        expect(TokenKind::RPAREN, ")");
        return expr;
    }
    return parsePrimary();
}

ExprPtr Parser::parsePrimary() {
    if (check(TokenKind::INT_LIT)) {
        auto expr = std::make_unique<IntLitExpr>();
        expr->line  = current().line;
        expr->col   = current().col;
        expr->value = std::stoll(current().text);
        consume();
        return expr;
    }
    if (check(TokenKind::FLOAT_LIT)) {
        auto expr = std::make_unique<FloatLitExpr>();
        expr->line  = current().line;
        expr->col   = current().col;
        expr->value = std::stod(current().text);
        consume();
        return expr;
    }
    if (check(TokenKind::KW_TRUE)) {
        auto expr = std::make_unique<BoolLitExpr>();
        expr->line  = current().line;
        expr->col   = current().col;
        expr->value = true;
        consume();
        return expr;
    }
    if (check(TokenKind::KW_FALSE)) {
        auto expr = std::make_unique<BoolLitExpr>();
        expr->line  = current().line;
        expr->col   = current().col;
        expr->value = false;
        consume();
        return expr;
    }
    if (check(TokenKind::STRING_LIT)) {
        auto expr = std::make_unique<StringLitExpr>();
        expr->line  = current().line;
        expr->col   = current().col;
        expr->value = current().text;
        consume();
        return expr;
    }
    if (check(TokenKind::IDENT)) {
        auto expr = std::make_unique<IdentExpr>();
        expr->line = current().line;
        expr->col  = current().col;
        expr->name = current().text;
        consume();
        return expr;
    }
    if (match(TokenKind::LPAREN)) {
        auto expr = parseExpr();
        expect(TokenKind::RPAREN, ")");
        return expr;
    }

    std::ostringstream oss;
    oss << "Unexpected token '" << current().text
        << "' at line " << current().line << ":" << current().col;
    throw ParseError(oss.str(), current().line, current().col);
}

} // namespace izi
