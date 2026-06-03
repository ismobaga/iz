#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace izi {

// ---------------------------------------------------------------------------
// Type system
// ---------------------------------------------------------------------------

enum class IziType {
    INT,
    FLOAT,
    BOOL,
    STRING,
    VOID,
    UNKNOWN,
};

inline std::string typeToString(IziType t) {
    switch (t) {
        case IziType::INT:     return "int";
        case IziType::FLOAT:   return "float";
        case IziType::BOOL:    return "bool";
        case IziType::STRING:  return "string";
        case IziType::VOID:    return "void";
        default:               return "unknown";
    }
}

// ---------------------------------------------------------------------------
// Node base
// ---------------------------------------------------------------------------

struct Node {
    int line{0};
    int col{0};
    virtual ~Node() = default;
};

// ---------------------------------------------------------------------------
// Expressions
// ---------------------------------------------------------------------------

struct Expr : public Node {
    IziType resolvedType{IziType::UNKNOWN};
};

using ExprPtr = std::unique_ptr<Expr>;

struct IntLitExpr : public Expr {
    long long value;
};

struct FloatLitExpr : public Expr {
    double value;
};

struct BoolLitExpr : public Expr {
    bool value;
};

struct StringLitExpr : public Expr {
    std::string value;
};

struct IdentExpr : public Expr {
    std::string name;
};

struct BinaryExpr : public Expr {
    std::string op; // "+", "-", "*", "/", "%", "==", "!=", "<", ">", "<=", ">=", "&&", "||"
    ExprPtr lhs;
    ExprPtr rhs;
};

struct UnaryExpr : public Expr {
    std::string op; // "-", "!"
    ExprPtr operand;
};

struct CallExpr : public Expr {
    std::string callee;
    std::vector<ExprPtr> args;
};

struct AssignExpr : public Expr {
    std::string name;
    ExprPtr value;
};

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

struct Stmt : public Node {};
using StmtPtr = std::unique_ptr<Stmt>;

struct ExprStmt : public Stmt {
    ExprPtr expr;
};

struct VarDeclStmt : public Stmt {
    std::string name;
    std::optional<IziType> declaredType; // nullopt means inferred
    ExprPtr init;                        // may be null
};

struct ReturnStmt : public Stmt {
    ExprPtr value; // may be null for void returns
};

struct BlockStmt : public Stmt {
    std::vector<StmtPtr> stmts;
};

struct IfStmt : public Stmt {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> thenBlock;
    std::unique_ptr<BlockStmt> elseBlock; // may be null
};

struct WhileStmt : public Stmt {
    ExprPtr condition;
    std::unique_ptr<BlockStmt> body;
};

// ---------------------------------------------------------------------------
// Top-level declarations
// ---------------------------------------------------------------------------

struct Param {
    std::string name;
    IziType type;
};

struct FnDecl : public Node {
    std::string name;
    std::vector<Param> params;
    IziType returnType{IziType::VOID};
    std::unique_ptr<BlockStmt> body;
};

struct ImportDecl : public Node {
    std::string path; // e.g. "ai.tensor"
};

// ---------------------------------------------------------------------------
// Program
// ---------------------------------------------------------------------------

struct Program {
    std::vector<std::unique_ptr<FnDecl>> functions;
    std::vector<std::unique_ptr<ImportDecl>> imports;
};

} // namespace izi
