#include "lexer/lexer.h"
#include "parser/parser.h"
#include "typechecker/typechecker.h"

#include <cassert>
#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Minimal test harness
// ---------------------------------------------------------------------------

static int passed = 0;
static int failed = 0;

#define ASSERT(cond, msg)                                          \
    do {                                                           \
        if (!(cond)) {                                             \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__  \
                      << "] " << msg << "\n";                      \
            ++failed;                                              \
        } else {                                                   \
            ++passed;                                              \
        }                                                          \
    } while (0)

#define ASSERT_NO_THROW(expr, msg)        \
    do {                                  \
        try { (expr); ++passed; }         \
        catch (...) {                     \
            std::cerr << "FAIL (unexpected throw) [" << __FILE__  \
                      << ":" << __LINE__ << "] " << msg << "\n";  \
            ++failed;                     \
        }                                 \
    } while (0)

#define ASSERT_THROWS(expr, ExcType, msg)     \
    do {                                      \
        bool threw = false;                   \
        try { (expr); }                       \
        catch (const ExcType&) { threw = true; } \
        catch (...) {}                        \
        if (!threw) {                         \
            std::cerr << "FAIL (expected throw of " #ExcType ") [" \
                      << __FILE__ << ":" << __LINE__ << "] " << msg << "\n"; \
            ++failed;                         \
        } else { ++passed; }                  \
    } while (0)

// ---------------------------------------------------------------------------
// Lexer tests
// ---------------------------------------------------------------------------

static void testLexer() {
    using namespace izi;

    // Basic tokens
    {
        Lexer lex("fn main() { }");
        auto toks = lex.tokenize();
        ASSERT(toks[0].kind == TokenKind::KW_FN,     "fn keyword");
        ASSERT(toks[1].kind == TokenKind::IDENT,     "main ident");
        ASSERT(toks[2].kind == TokenKind::LPAREN,    "lparen");
        ASSERT(toks[3].kind == TokenKind::RPAREN,    "rparen");
        ASSERT(toks[4].kind == TokenKind::LBRACE,    "lbrace");
        ASSERT(toks[5].kind == TokenKind::RBRACE,    "rbrace");
        ASSERT(toks[6].kind == TokenKind::EOF_TOK,   "eof");
    }

    // Integer and float literals
    {
        Lexer lex("42 3.14");
        auto toks = lex.tokenize();
        ASSERT(toks[0].kind == TokenKind::INT_LIT,   "int lit");
        ASSERT(toks[0].text == "42",                 "int text");
        ASSERT(toks[1].kind == TokenKind::FLOAT_LIT, "float lit");
        ASSERT(toks[1].text == "3.14",               "float text");
    }

    // String literal
    {
        Lexer lex("\"hello\\nworld\"");
        auto toks = lex.tokenize();
        ASSERT(toks[0].kind == TokenKind::STRING_LIT, "string lit");
        ASSERT(toks[0].text == "hello\nworld",         "string text with escape");
    }

    // Operators
    {
        Lexer lex("-> == != <= >= && ||");
        auto toks = lex.tokenize();
        ASSERT(toks[0].kind == TokenKind::ARROW,    "->");
        ASSERT(toks[1].kind == TokenKind::EQEQ,     "==");
        ASSERT(toks[2].kind == TokenKind::BANGEQ,   "!=");
        ASSERT(toks[3].kind == TokenKind::LTEQ,     "<=");
        ASSERT(toks[4].kind == TokenKind::GTEQ,     ">=");
        ASSERT(toks[5].kind == TokenKind::AMPAMP,   "&&");
        ASSERT(toks[6].kind == TokenKind::PIPEPIPE, "||");
    }

    // Keywords
    {
        Lexer lex("let return if else while true false");
        auto toks = lex.tokenize();
        ASSERT(toks[0].kind == TokenKind::KW_LET,    "let");
        ASSERT(toks[1].kind == TokenKind::KW_RETURN, "return");
        ASSERT(toks[2].kind == TokenKind::KW_IF,     "if");
        ASSERT(toks[3].kind == TokenKind::KW_ELSE,   "else");
        ASSERT(toks[4].kind == TokenKind::KW_WHILE,  "while");
        ASSERT(toks[5].kind == TokenKind::KW_TRUE,   "true");
        ASSERT(toks[6].kind == TokenKind::KW_FALSE,  "false");
    }

    // Line comments
    {
        Lexer lex("42 // comment\n99");
        auto toks = lex.tokenize();
        ASSERT(toks[0].kind == TokenKind::INT_LIT && toks[0].text == "42", "before comment");
        ASSERT(toks[1].kind == TokenKind::INT_LIT && toks[1].text == "99", "after comment");
    }

    // Type names
    {
        Lexer lex("int float bool string void");
        auto toks = lex.tokenize();
        ASSERT(toks[0].kind == TokenKind::TY_INT,    "int type");
        ASSERT(toks[1].kind == TokenKind::TY_FLOAT,  "float type");
        ASSERT(toks[2].kind == TokenKind::TY_BOOL,   "bool type");
        ASSERT(toks[3].kind == TokenKind::TY_STRING, "string type");
        ASSERT(toks[4].kind == TokenKind::TY_VOID,   "void type");
    }
}

// ---------------------------------------------------------------------------
// Parser tests
// ---------------------------------------------------------------------------

static izi::Program parseSource(const std::string& src) {
    izi::Lexer lex(src);
    auto toks = lex.tokenize();
    izi::Parser parser(std::move(toks));
    return parser.parse();
}

static void testParser() {
    using namespace izi;

    // Empty main
    {
        ASSERT_NO_THROW(parseSource("fn main() {}"), "empty main");
    }

    // Function with params and return type
    {
        auto prog = parseSource("fn add(a: int, b: int) -> int { return a + b }");
        ASSERT(prog.functions.size() == 1,          "one function");
        ASSERT(prog.functions[0]->name == "add",    "function name");
        ASSERT(prog.functions[0]->params.size() == 2, "two params");
        ASSERT(prog.functions[0]->returnType == IziType::INT, "return int");
    }

    // Let declaration
    {
        auto prog = parseSource("fn f() { let x: int = 42 }");
        auto& stmts = prog.functions[0]->body->stmts;
        ASSERT(stmts.size() == 1, "one stmt");
        auto* decl = dynamic_cast<VarDeclStmt*>(stmts[0].get());
        ASSERT(decl != nullptr,        "var decl");
        ASSERT(decl->name == "x",      "var name");
    }

    // If statement
    {
        ASSERT_NO_THROW(
            parseSource("fn f() { if x { let y: int = 1 } }"),
            "if statement");
    }

    // While statement
    {
        ASSERT_NO_THROW(
            parseSource("fn f() { while x < 10 { x = x + 1 } }"),
            "while statement");
    }

    // Import
    {
        auto prog = parseSource("import ai.tensor\nfn main() {}");
        ASSERT(prog.imports.size() == 1,              "one import");
        ASSERT(prog.imports[0]->path == "ai.tensor",  "import path");
    }

    // Error recovery — bad token
    {
        ASSERT_THROWS(parseSource("???"), ParseError, "bad token throws ParseError");
    }
}

// ---------------------------------------------------------------------------
// Type checker tests
// ---------------------------------------------------------------------------

static void typeCheck(const std::string& src) {
    auto prog = parseSource(src);
    izi::TypeChecker tc;
    tc.check(prog);
}

static void testTypeChecker() {
    using namespace izi;

    ASSERT_NO_THROW(typeCheck("fn main() { let x: int = 1 }"), "int decl");
    ASSERT_NO_THROW(typeCheck("fn main() { let x: float = 3.14 }"), "float decl");
    ASSERT_NO_THROW(typeCheck("fn main() { let x: bool = true }"), "bool decl");
    ASSERT_NO_THROW(typeCheck("fn main() { let x: string = \"hi\" }"), "string decl");
    ASSERT_NO_THROW(typeCheck("fn main() { let x: float = 1 }"), "int->float promotion");

    // Return type mismatch
    ASSERT_THROWS(typeCheck("fn f() -> int { return 3.14 }"), TypeError, "float return as int");

    // Undefined variable
    ASSERT_THROWS(typeCheck("fn f() { print(x) }"), TypeError, "undefined var");

    // Undefined function
    ASSERT_THROWS(typeCheck("fn f() { g() }"), TypeError, "undefined fn");

    // Correct function call
    ASSERT_NO_THROW(typeCheck("fn add(a: int, b: int) -> int { return a + b }\nfn main() { let r: int = add(1, 2) }"), "fn call");

    // Arg count mismatch
    ASSERT_THROWS(typeCheck("fn f(a: int) {}\nfn main() { f(1, 2) }"), TypeError, "arg count mismatch");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "Running izi tests...\n";

    testLexer();
    testParser();
    testTypeChecker();

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    return failed > 0 ? 1 : 0;
}
