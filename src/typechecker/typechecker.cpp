#include "typechecker.h"

#include <sstream>

namespace izi {

namespace {
bool isNumeric(IziType t) {
    return t == IziType::INT || t == IziType::FLOAT;
}
} // namespace

void TypeChecker::check(Program& prog) {
    // Register all function signatures first (forward declarations)
    for (auto& fn : prog.functions) {
        functions[fn->name] = fn.get();
    }

    // Check each function body
    for (auto& fn : prog.functions) {
        checkFn(*fn);
    }
}

void TypeChecker::checkFn(FnDecl& fn) {
    currentReturnType = fn.returnType;
    scope.push();

    for (auto& p : fn.params) {
        scope.define(p.name, p.type);
    }

    checkBlock(*fn.body);
    scope.pop();
}

void TypeChecker::checkBlock(BlockStmt& block) {
    scope.push();
    for (auto& stmt : block.stmts) {
        checkStmt(*stmt);
    }
    scope.pop();
}

void TypeChecker::checkStmt(Stmt& stmt) {
    if (auto* s = dynamic_cast<VarDeclStmt*>(&stmt)) {
        checkVarDecl(*s);
    } else if (auto* s = dynamic_cast<ReturnStmt*>(&stmt)) {
        checkReturn(*s);
    } else if (auto* s = dynamic_cast<IfStmt*>(&stmt)) {
        checkIf(*s);
    } else if (auto* s = dynamic_cast<WhileStmt*>(&stmt)) {
        checkWhile(*s);
    } else if (auto* s = dynamic_cast<ExprStmt*>(&stmt)) {
        checkExpr(*s->expr);
    } else if (auto* s = dynamic_cast<BlockStmt*>(&stmt)) {
        checkBlock(*s);
    }
}

void TypeChecker::checkVarDecl(VarDeclStmt& stmt) {
    IziType resolved = IziType::UNKNOWN;

    if (stmt.init) {
        resolved = checkExpr(*stmt.init);
    }

    if (stmt.declaredType.has_value()) {
        IziType declared = stmt.declaredType.value();
        if (stmt.init && resolved != IziType::UNKNOWN && resolved != declared) {
            // Allow int -> float promotion
            if (!(declared == IziType::FLOAT && resolved == IziType::INT)) {
                std::ostringstream oss;
                oss << "Type mismatch in declaration of '" << stmt.name
                    << "': declared " << typeToString(declared)
                    << " but got " << typeToString(resolved)
                    << " at line " << stmt.line;
                throw TypeError(oss.str(), stmt.line, stmt.col);
            }
        }
        resolved = declared;
    }

    if (resolved == IziType::UNKNOWN) {
        std::ostringstream oss;
        oss << "Cannot infer type for variable '" << stmt.name
            << "' at line " << stmt.line;
        throw TypeError(oss.str(), stmt.line, stmt.col);
    }

    scope.define(stmt.name, resolved);
}

void TypeChecker::checkReturn(ReturnStmt& stmt) {
    if (stmt.value) {
        IziType t = checkExpr(*stmt.value);
        if (t != currentReturnType) {
            // Allow int -> float
            if (!(currentReturnType == IziType::FLOAT && t == IziType::INT)) {
                std::ostringstream oss;
                oss << "Return type mismatch: expected "
                    << typeToString(currentReturnType)
                    << " but got " << typeToString(t)
                    << " at line " << stmt.line;
                throw TypeError(oss.str(), stmt.line, stmt.col);
            }
        }
    } else if (currentReturnType != IziType::VOID) {
        std::ostringstream oss;
        oss << "Expected return value of type "
            << typeToString(currentReturnType)
            << " at line " << stmt.line;
        throw TypeError(oss.str(), stmt.line, stmt.col);
    }
}

void TypeChecker::checkIf(IfStmt& stmt) {
    IziType condType = checkExpr(*stmt.condition);
    (void)condType; // any type can be used as condition
    checkBlock(*stmt.thenBlock);
    if (stmt.elseBlock) checkBlock(*stmt.elseBlock);
}

void TypeChecker::checkWhile(WhileStmt& stmt) {
    checkExpr(*stmt.condition);
    checkBlock(*stmt.body);
}

IziType TypeChecker::checkExpr(Expr& expr) {
    IziType t = IziType::UNKNOWN;

    if (dynamic_cast<IntLitExpr*>(&expr)) {
        t = IziType::INT;
    } else if (dynamic_cast<FloatLitExpr*>(&expr)) {
        t = IziType::FLOAT;
    } else if (dynamic_cast<BoolLitExpr*>(&expr)) {
        t = IziType::BOOL;
    } else if (dynamic_cast<StringLitExpr*>(&expr)) {
        t = IziType::STRING;
    } else if (auto* e = dynamic_cast<IdentExpr*>(&expr)) {
        const Symbol* sym = scope.lookup(e->name);
        if (!sym) {
            std::ostringstream oss;
            oss << "Undefined variable '" << e->name
                << "' at line " << e->line;
            throw TypeError(oss.str(), e->line, e->col);
        }
        t = sym->type;
    } else if (auto* e = dynamic_cast<BinaryExpr*>(&expr)) {
        t = checkBinary(*e);
    } else if (auto* e = dynamic_cast<UnaryExpr*>(&expr)) {
        t = checkUnary(*e);
    } else if (auto* e = dynamic_cast<CallExpr*>(&expr)) {
        t = checkCall(*e);
    } else if (auto* e = dynamic_cast<AssignExpr*>(&expr)) {
        t = checkAssign(*e);
    }

    expr.resolvedType = t;
    return t;
}

IziType TypeChecker::checkBinary(BinaryExpr& expr) {
    IziType lhs = checkExpr(*expr.lhs);
    IziType rhs = checkExpr(*expr.rhs);

    const std::string& op = expr.op;

    // Logical operators return bool
    if (op == "&&" || op == "||") {
        return IziType::BOOL;
    }

    // Comparison operators return bool
    if (op == "==" || op == "!=" || op == "<" || op == ">" ||
        op == "<=" || op == ">=") {
        return IziType::BOOL;
    }

    // Arithmetic: string + string is valid (concatenation)
    if (op == "+") {
        if (lhs == IziType::STRING && rhs == IziType::STRING) return IziType::STRING;
    }

    // Numeric arithmetic
    if (isNumeric(lhs) && isNumeric(rhs)) {
        // Float wins over int
        if (lhs == IziType::FLOAT || rhs == IziType::FLOAT) return IziType::FLOAT;
        return IziType::INT;
    }

    std::ostringstream oss;
    oss << "Invalid operand types for '" << op << "': "
        << typeToString(lhs) << " and " << typeToString(rhs)
        << " at line " << expr.line;
    throw TypeError(oss.str(), expr.line, expr.col);
}

IziType TypeChecker::checkUnary(UnaryExpr& expr) {
    IziType t = checkExpr(*expr.operand);
    if (expr.op == "-") {
        if (!isNumeric(t)) {
            throw TypeError("Unary '-' requires numeric operand", expr.line, expr.col);
        }
        return t;
    }
    if (expr.op == "!") {
        return IziType::BOOL;
    }
    return t;
}

IziType TypeChecker::checkCall(CallExpr& expr) {
    // Built-in: print
    if (expr.callee == "print") {
        for (auto& arg : expr.args) checkExpr(*arg);
        return IziType::VOID;
    }

    auto it = functions.find(expr.callee);
    if (it == functions.end()) {
        std::ostringstream oss;
        oss << "Undefined function '" << expr.callee << "' at line " << expr.line;
        throw TypeError(oss.str(), expr.line, expr.col);
    }

    FnDecl* fn = it->second;
    if (expr.args.size() != fn->params.size()) {
        std::ostringstream oss;
        oss << "Function '" << expr.callee << "' expects " << fn->params.size()
            << " arguments but got " << expr.args.size()
            << " at line " << expr.line;
        throw TypeError(oss.str(), expr.line, expr.col);
    }

    for (size_t i = 0; i < expr.args.size(); ++i) {
        IziType argType = checkExpr(*expr.args[i]);
        IziType paramType = fn->params[i].type;
        if (argType != paramType) {
            // Allow int -> float promotion
            if (!(paramType == IziType::FLOAT && argType == IziType::INT)) {
                std::ostringstream oss;
                oss << "Argument " << (i + 1) << " of '" << expr.callee
                    << "' expects " << typeToString(paramType)
                    << " but got " << typeToString(argType)
                    << " at line " << expr.line;
                throw TypeError(oss.str(), expr.line, expr.col);
            }
        }
    }

    return fn->returnType;
}

IziType TypeChecker::checkAssign(AssignExpr& expr) {
    const Symbol* sym = scope.lookup(expr.name);
    if (!sym) {
        std::ostringstream oss;
        oss << "Undefined variable '" << expr.name << "' at line " << expr.line;
        throw TypeError(oss.str(), expr.line, expr.col);
    }
    IziType valType = checkExpr(*expr.value);
    if (valType != sym->type) {
        if (!(sym->type == IziType::FLOAT && valType == IziType::INT)) {
            std::ostringstream oss;
            oss << "Cannot assign " << typeToString(valType)
                << " to variable '" << expr.name << "' of type "
                << typeToString(sym->type) << " at line " << expr.line;
            throw TypeError(oss.str(), expr.line, expr.col);
        }
    }
    return sym->type;
}

} // namespace izi
