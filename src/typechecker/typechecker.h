#pragma once

#include "../ast/ast.h"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace izi {

class TypeError : public std::runtime_error {
public:
    int line, col;
    TypeError(const std::string& msg, int line, int col)
        : std::runtime_error(msg), line(line), col(col) {}
};

// Symbol table entry
struct Symbol {
    std::string name;
    IziType type;
};

// Scope is a stack of symbol maps
class Scope {
public:
    void push() { frames.push_back({}); }
    void pop()  { frames.pop_back(); }

    void define(const std::string& name, IziType type) {
        frames.back()[name] = Symbol{name, type};
    }

    // Returns nullptr if not found
    const Symbol* lookup(const std::string& name) const {
        for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
            auto f = it->find(name);
            if (f != it->end()) return &f->second;
        }
        return nullptr;
    }

private:
    std::vector<std::unordered_map<std::string, Symbol>> frames;
};

class TypeChecker {
public:
    void check(Program& prog);

private:
    Scope scope;
    std::unordered_map<std::string, FnDecl*> functions;
    IziType currentReturnType{IziType::VOID};

    void checkFn(FnDecl& fn);
    void checkBlock(BlockStmt& block);
    void checkStmt(Stmt& stmt);
    void checkVarDecl(VarDeclStmt& stmt);
    void checkReturn(ReturnStmt& stmt);
    void checkIf(IfStmt& stmt);
    void checkWhile(WhileStmt& stmt);

    IziType checkExpr(Expr& expr);
    IziType checkBinary(BinaryExpr& expr);
    IziType checkUnary(UnaryExpr& expr);
    IziType checkCall(CallExpr& expr);
    IziType checkAssign(AssignExpr& expr);
};

} // namespace izi
