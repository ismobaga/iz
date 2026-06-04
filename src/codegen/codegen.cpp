#include "codegen.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include <sstream>

namespace izi {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

CodeGen::CodeGen() {
    builder = std::make_unique<llvm::IRBuilder<>>(ctx);
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
}

// ---------------------------------------------------------------------------
// Generate
// ---------------------------------------------------------------------------

std::unique_ptr<llvm::Module> CodeGen::generate(const Program& prog,
                                                  const std::string& moduleName) {
    auto mod = std::make_unique<llvm::Module>(moduleName, ctx);
    currentModule = mod.get();

    for (auto& fn : prog.functions) {
        genFn(*mod, *fn);
    }

    return mod;
}

// ---------------------------------------------------------------------------
// Type helpers
// ---------------------------------------------------------------------------

llvm::Type* CodeGen::llvmType(IziType t) {
    switch (t) {
        case IziType::INT:    return llvm::Type::getInt64Ty(ctx);
        case IziType::FLOAT:  return llvm::Type::getDoubleTy(ctx);
        case IziType::BOOL:   return llvm::Type::getInt1Ty(ctx);
        case IziType::STRING: return llvm::PointerType::get(llvm::Type::getInt8Ty(ctx), 0);
        case IziType::VOID:   return llvm::Type::getVoidTy(ctx);
        default:              return llvm::Type::getInt64Ty(ctx);
    }
}

llvm::Type* CodeGen::llvmPtrType() {
    return llvm::PointerType::get(llvm::Type::getInt8Ty(ctx), 0);
}

// ---------------------------------------------------------------------------
// Alloca helpers
// ---------------------------------------------------------------------------

llvm::AllocaInst* CodeGen::createEntryAlloca(llvm::Function* func,
                                              const std::string& name,
                                              llvm::Type* type) {
    llvm::IRBuilder<> tmpB(&func->getEntryBlock(),
                           func->getEntryBlock().begin());
    return tmpB.CreateAlloca(type, nullptr, name);
}

llvm::AllocaInst* CodeGen::lookupVar(const std::string& name,
                                      const std::vector<VarMap>& scopes) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return f->second;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// printf / print built-in
// ---------------------------------------------------------------------------

llvm::FunctionCallee CodeGen::getPrintf(llvm::Module& mod) {
    if (auto* f = mod.getFunction("printf")) {
        return llvm::FunctionCallee(f->getFunctionType(), f);
    }
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(ctx),
        {llvmPtrType()},
        /*isVarArg=*/true);
    llvm::Function* f = llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, "printf", mod);
    return llvm::FunctionCallee(ft, f);
}

llvm::Value* CodeGen::genBuiltinPrint(const CallExpr& expr, llvm::Module& mod,
                                       std::vector<VarMap>& scopes) {
    if (expr.args.empty()) {
        // print() with no args — print newline
        llvm::Value* fmtStr = builder->CreateGlobalStringPtr("\n", ".fmt_nl");
        builder->CreateCall(getPrintf(mod), {fmtStr});
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
    }

    for (auto& arg : expr.args) {
        llvm::Value* val = genExpr(*arg, scopes);
        IziType t = arg->resolvedType;

        std::string fmt;
        llvm::Value* printVal = val;

        switch (t) {
            case IziType::INT:
                fmt = "%lld\n";
                break;
            case IziType::FLOAT:
                fmt = "%g\n";
                break;
            case IziType::BOOL:
                // print "true" or "false"
                {
                    llvm::Value* fmtStr = builder->CreateGlobalStringPtr("%s\n", ".fmt_bool");
                    llvm::Value* trueStr  = builder->CreateGlobalStringPtr("true",  ".true");
                    llvm::Value* falseStr = builder->CreateGlobalStringPtr("false", ".false");
                    llvm::Value* str = builder->CreateSelect(val, trueStr, falseStr);
                    builder->CreateCall(getPrintf(mod), {fmtStr, str});
                    continue;
                }
            case IziType::STRING:
                fmt = "%s\n";
                break;
            default:
                fmt = "%lld\n";
                break;
        }

        llvm::Value* fmtStr = builder->CreateGlobalStringPtr(fmt, ".fmt");
        builder->CreateCall(getPrintf(mod), {fmtStr, printVal});
    }

    return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), 0);
}

// ---------------------------------------------------------------------------
// Function generation
// ---------------------------------------------------------------------------

void CodeGen::genFn(llvm::Module& mod, const FnDecl& fn) {
    std::vector<llvm::Type*> paramTypes;
    for (auto& p : fn.params) {
        paramTypes.push_back(llvmType(p.type));
    }

    // The C entry point `main` must return i32
    bool isMain = (fn.name == "main");
    llvm::Type* retLlvmType = isMain ? llvm::Type::getInt32Ty(ctx) : llvmType(fn.returnType);

    llvm::FunctionType* ft = llvm::FunctionType::get(retLlvmType, paramTypes, /*isVarArg=*/false);

    llvm::Function* func = llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, fn.name, mod);

    // Name parameters
    size_t idx = 0;
    for (auto& arg : func->args()) {
        arg.setName(fn.params[idx++].name);
    }

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", func);
    builder->SetInsertPoint(entry);

    // Allocate and store parameters
    std::vector<VarMap> scopes;
    scopes.push_back({});

    idx = 0;
    for (auto& arg : func->args()) {
        llvm::AllocaInst* alloca = createEntryAlloca(func, std::string(arg.getName()),
                                                     arg.getType());
        builder->CreateStore(&arg, alloca);
        scopes.back()[std::string(arg.getName())] = alloca;
        ++idx;
    }

    genBlock(*fn.body, func, scopes);

    // Add implicit return if the block didn't terminate
    if (!builder->GetInsertBlock()->getTerminator()) {
        if (isMain) {
            builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx), 0));
        } else if (fn.returnType == IziType::VOID) {
            builder->CreateRetVoid();
        } else {
            builder->CreateRet(llvm::Constant::getNullValue(retLlvmType));
        }
    }
}

// ---------------------------------------------------------------------------
// Block / Statement generation
// ---------------------------------------------------------------------------

void CodeGen::genBlock(const BlockStmt& block, llvm::Function* func,
                        std::vector<VarMap>& scopes) {
    scopes.push_back({});
    for (auto& stmt : block.stmts) {
        // Stop generating after a terminator
        if (builder->GetInsertBlock()->getTerminator()) break;
        genStmt(*stmt, func, scopes);
    }
    scopes.pop_back();
}

void CodeGen::genStmt(const Stmt& stmt, llvm::Function* func,
                       std::vector<VarMap>& scopes) {
    if (auto* s = dynamic_cast<const VarDeclStmt*>(&stmt)) {
        genVarDecl(*s, func, scopes);
    } else if (auto* s = dynamic_cast<const ReturnStmt*>(&stmt)) {
        // Determine return type from function
        IziType retType = IziType::VOID;
        auto* ft = func->getFunctionType();
        if (!ft->getReturnType()->isVoidTy()) {
            // infer from llvm type
            llvm::Type* rt = ft->getReturnType();
            if (rt->isIntegerTy(64))    retType = IziType::INT;
            else if (rt->isDoubleTy())  retType = IziType::FLOAT;
            else if (rt->isIntegerTy(1))retType = IziType::BOOL;
            else                        retType = IziType::STRING;
        }
        genReturn(*s, func, scopes, retType);
    } else if (auto* s = dynamic_cast<const IfStmt*>(&stmt)) {
        genIf(*s, func, scopes);
    } else if (auto* s = dynamic_cast<const WhileStmt*>(&stmt)) {
        genWhile(*s, func, scopes);
    } else if (auto* s = dynamic_cast<const ExprStmt*>(&stmt)) {
        genExpr(*s->expr, scopes);
    } else if (auto* s = dynamic_cast<const BlockStmt*>(&stmt)) {
        genBlock(*s, func, scopes);
    }
}

void CodeGen::genVarDecl(const VarDeclStmt& stmt, llvm::Function* func,
                          std::vector<VarMap>& scopes) {
    // Determine resolved type
    IziType t = stmt.declaredType.value_or(
        stmt.init ? stmt.init->resolvedType : IziType::INT);

    llvm::AllocaInst* alloca = createEntryAlloca(func, stmt.name, llvmType(t));
    scopes.back()[stmt.name] = alloca;

    if (stmt.init) {
        llvm::Value* val = genExpr(*stmt.init, scopes);
        // Promote int to float if needed
        if (t == IziType::FLOAT && stmt.init->resolvedType == IziType::INT) {
            val = builder->CreateSIToFP(val, llvm::Type::getDoubleTy(ctx));
        }
        builder->CreateStore(val, alloca);
    }
}

void CodeGen::genReturn(const ReturnStmt& stmt, llvm::Function* /*func*/,
                         std::vector<VarMap>& scopes, IziType retType) {
    if (stmt.value) {
        llvm::Value* val = genExpr(*stmt.value, scopes);
        if (retType == IziType::FLOAT && stmt.value->resolvedType == IziType::INT) {
            val = builder->CreateSIToFP(val, llvm::Type::getDoubleTy(ctx));
        }
        builder->CreateRet(val);
    } else {
        builder->CreateRetVoid();
    }
}

void CodeGen::genIf(const IfStmt& stmt, llvm::Function* func,
                     std::vector<VarMap>& scopes) {
    llvm::Value* cond = genExpr(*stmt.condition, scopes);

    // Convert to i1 if not already bool
    if (!cond->getType()->isIntegerTy(1)) {
        if (cond->getType()->isIntegerTy()) {
            cond = builder->CreateICmpNE(
                cond, llvm::ConstantInt::get(cond->getType(), 0));
        } else if (cond->getType()->isDoubleTy()) {
            cond = builder->CreateFCmpONE(
                cond, llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), 0.0));
        }
    }

    llvm::BasicBlock* thenBB  = llvm::BasicBlock::Create(ctx, "if.then",  func);
    llvm::BasicBlock* elseBB  = llvm::BasicBlock::Create(ctx, "if.else",  func);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(ctx, "if.merge", func);

    builder->CreateCondBr(cond, thenBB, elseBB);

    // then
    builder->SetInsertPoint(thenBB);
    genBlock(*stmt.thenBlock, func, scopes);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(mergeBB);

    // else
    builder->SetInsertPoint(elseBB);
    if (stmt.elseBlock) {
        genBlock(*stmt.elseBlock, func, scopes);
    }
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(mergeBB);

    // merge
    builder->SetInsertPoint(mergeBB);
}

void CodeGen::genWhile(const WhileStmt& stmt, llvm::Function* func,
                        std::vector<VarMap>& scopes) {
    llvm::BasicBlock* condBB  = llvm::BasicBlock::Create(ctx, "while.cond",  func);
    llvm::BasicBlock* bodyBB  = llvm::BasicBlock::Create(ctx, "while.body",  func);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(ctx, "while.after", func);

    builder->CreateBr(condBB);

    // condition
    builder->SetInsertPoint(condBB);
    llvm::Value* cond = genExpr(*stmt.condition, scopes);
    if (!cond->getType()->isIntegerTy(1)) {
        if (cond->getType()->isIntegerTy()) {
            cond = builder->CreateICmpNE(
                cond, llvm::ConstantInt::get(cond->getType(), 0));
        } else if (cond->getType()->isDoubleTy()) {
            cond = builder->CreateFCmpONE(
                cond, llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), 0.0));
        }
    }
    builder->CreateCondBr(cond, bodyBB, afterBB);

    // body
    builder->SetInsertPoint(bodyBB);
    genBlock(*stmt.body, func, scopes);
    if (!builder->GetInsertBlock()->getTerminator())
        builder->CreateBr(condBB);

    builder->SetInsertPoint(afterBB);
}

// ---------------------------------------------------------------------------
// Expression generation
// ---------------------------------------------------------------------------

llvm::Value* CodeGen::genExpr(const Expr& expr, std::vector<VarMap>& scopes) {
    if (auto* e = dynamic_cast<const IntLitExpr*>(&expr)) {
        return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx), e->value, true);
    }
    if (auto* e = dynamic_cast<const FloatLitExpr*>(&expr)) {
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx), e->value);
    }
    if (auto* e = dynamic_cast<const BoolLitExpr*>(&expr)) {
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx), e->value ? 1 : 0);
    }
    if (auto* e = dynamic_cast<const StringLitExpr*>(&expr)) {
        return builder->CreateGlobalStringPtr(e->value, ".str");
    }
    if (auto* e = dynamic_cast<const IdentExpr*>(&expr)) {
        llvm::AllocaInst* alloca = lookupVar(e->name, scopes);
        if (!alloca) {
            throw CodeGenError("Undefined variable: " + e->name);
        }
        return builder->CreateLoad(alloca->getAllocatedType(), alloca, e->name);
    }
    if (auto* e = dynamic_cast<const BinaryExpr*>(&expr)) {
        return genBinary(*e, scopes);
    }
    if (auto* e = dynamic_cast<const UnaryExpr*>(&expr)) {
        return genUnary(*e, scopes);
    }
    if (auto* e = dynamic_cast<const CallExpr*>(&expr)) {
        return genCall(*e, *currentModule, scopes);
    }
    if (auto* e = dynamic_cast<const AssignExpr*>(&expr)) {
        return genAssign(*e, scopes);
    }

    throw CodeGenError("Unknown expression type");
}

llvm::Value* CodeGen::genBinary(const BinaryExpr& expr, std::vector<VarMap>& scopes) {
    const std::string& op = expr.op;

    // Short-circuit for logical ops
    // For simplicity, we evaluate both sides (no short-circuit)
    llvm::Value* lhs = genExpr(*expr.lhs, scopes);
    llvm::Value* rhs = genExpr(*expr.rhs, scopes);

    bool lhsFloat = lhs->getType()->isDoubleTy();
    bool rhsFloat = rhs->getType()->isDoubleTy();
    bool isFloat  = lhsFloat || rhsFloat;

    // Promote int to float if mixed
    if (isFloat) {
        if (!lhsFloat) lhs = builder->CreateSIToFP(lhs, llvm::Type::getDoubleTy(ctx));
        if (!rhsFloat) rhs = builder->CreateSIToFP(rhs, llvm::Type::getDoubleTy(ctx));
    }

    if (op == "+") {
        if (isFloat) return builder->CreateFAdd(lhs, rhs);
        return builder->CreateAdd(lhs, rhs);
    }
    if (op == "-") {
        if (isFloat) return builder->CreateFSub(lhs, rhs);
        return builder->CreateSub(lhs, rhs);
    }
    if (op == "*") {
        if (isFloat) return builder->CreateFMul(lhs, rhs);
        return builder->CreateMul(lhs, rhs);
    }
    if (op == "/") {
        if (isFloat) return builder->CreateFDiv(lhs, rhs);
        return builder->CreateSDiv(lhs, rhs);
    }
    if (op == "%") {
        return builder->CreateSRem(lhs, rhs);
    }
    if (op == "==") {
        if (isFloat) return builder->CreateFCmpOEQ(lhs, rhs);
        return builder->CreateICmpEQ(lhs, rhs);
    }
    if (op == "!=") {
        if (isFloat) return builder->CreateFCmpONE(lhs, rhs);
        return builder->CreateICmpNE(lhs, rhs);
    }
    if (op == "<") {
        if (isFloat) return builder->CreateFCmpOLT(lhs, rhs);
        return builder->CreateICmpSLT(lhs, rhs);
    }
    if (op == ">") {
        if (isFloat) return builder->CreateFCmpOGT(lhs, rhs);
        return builder->CreateICmpSGT(lhs, rhs);
    }
    if (op == "<=") {
        if (isFloat) return builder->CreateFCmpOLE(lhs, rhs);
        return builder->CreateICmpSLE(lhs, rhs);
    }
    if (op == ">=") {
        if (isFloat) return builder->CreateFCmpOGE(lhs, rhs);
        return builder->CreateICmpSGE(lhs, rhs);
    }
    if (op == "&&") {
        // Convert both to bool
        if (!lhs->getType()->isIntegerTy(1))
            lhs = builder->CreateICmpNE(lhs, llvm::Constant::getNullValue(lhs->getType()));
        if (!rhs->getType()->isIntegerTy(1))
            rhs = builder->CreateICmpNE(rhs, llvm::Constant::getNullValue(rhs->getType()));
        return builder->CreateAnd(lhs, rhs);
    }
    if (op == "||") {
        if (!lhs->getType()->isIntegerTy(1))
            lhs = builder->CreateICmpNE(lhs, llvm::Constant::getNullValue(lhs->getType()));
        if (!rhs->getType()->isIntegerTy(1))
            rhs = builder->CreateICmpNE(rhs, llvm::Constant::getNullValue(rhs->getType()));
        return builder->CreateOr(lhs, rhs);
    }

    throw CodeGenError("Unknown binary operator: " + op);
}

llvm::Value* CodeGen::genUnary(const UnaryExpr& expr, std::vector<VarMap>& scopes) {
    llvm::Value* val = genExpr(*expr.operand, scopes);
    if (expr.op == "-") {
        if (val->getType()->isDoubleTy()) return builder->CreateFNeg(val);
        return builder->CreateNeg(val);
    }
    if (expr.op == "!") {
        if (!val->getType()->isIntegerTy(1)) {
            val = builder->CreateICmpNE(val, llvm::Constant::getNullValue(val->getType()));
        }
        return builder->CreateNot(val);
    }
    throw CodeGenError("Unknown unary operator: " + expr.op);
}

llvm::Value* CodeGen::genCall(const CallExpr& expr, llvm::Module& mod,
                               std::vector<VarMap>& scopes) {
    if (expr.callee == "print") {
        return genBuiltinPrint(expr, mod, scopes);
    }

    llvm::Function* fn = mod.getFunction(expr.callee);
    if (!fn) {
        throw CodeGenError("Undefined function: " + expr.callee);
    }

    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < expr.args.size(); ++i) {
        llvm::Value* val = genExpr(*expr.args[i], scopes);
        llvm::Type* paramType = fn->getFunctionType()->getParamType(i);
        // Promote int to float if needed
        if (paramType->isDoubleTy() && val->getType()->isIntegerTy()) {
            val = builder->CreateSIToFP(val, llvm::Type::getDoubleTy(ctx));
        }
        args.push_back(val);
    }

    return builder->CreateCall(fn, args);
}

llvm::Value* CodeGen::genAssign(const AssignExpr& expr, std::vector<VarMap>& scopes) {
    llvm::AllocaInst* alloca = lookupVar(expr.name, scopes);
    if (!alloca) {
        throw CodeGenError("Undefined variable in assignment: " + expr.name);
    }
    llvm::Value* val = genExpr(*expr.value, scopes);
    // Promote int -> float
    if (alloca->getAllocatedType()->isDoubleTy() && val->getType()->isIntegerTy()) {
        val = builder->CreateSIToFP(val, llvm::Type::getDoubleTy(ctx));
    }
    builder->CreateStore(val, alloca);
    return val;
}

// ---------------------------------------------------------------------------
// Object file emission
// ---------------------------------------------------------------------------

bool CodeGen::emitObjectFile(llvm::Module& mod, const std::string& outPath) {
    auto targetTriple = llvm::sys::getDefaultTargetTriple();
    mod.setTargetTriple(targetTriple);

    std::string error;
    const llvm::Target* target =
        llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target) {
        llvm::errs() << "Error looking up target: " << error << "\n";
        return false;
    }

    llvm::TargetOptions opt;
    auto rm = std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_);
    std::unique_ptr<llvm::TargetMachine> targetMachine(
        target->createTargetMachine(targetTriple, "generic", "", opt, rm));

    mod.setDataLayout(targetMachine->createDataLayout());

    std::error_code ec;
    llvm::raw_fd_ostream dest(outPath, ec, llvm::sys::fs::OF_None);
    if (ec) {
        llvm::errs() << "Could not open file: " << ec.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager pass;
    if (targetMachine->addPassesToEmitFile(
            pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
        llvm::errs() << "Target machine cannot emit object file\n";
        return false;
    }

    pass.run(mod);
    dest.flush();
    return true;
}

} // namespace izi
