#pragma once

#include "../ast/ast.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace izi {

class CodeGenError : public std::runtime_error {
public:
    CodeGenError(const std::string& msg) : std::runtime_error(msg) {}
};

// Per-scope variable table: name -> alloca pointer
using VarMap = std::unordered_map<std::string, llvm::AllocaInst*>;

class CodeGen {
public:
    CodeGen();

    // Generate LLVM IR for the whole program. Returns the module.
    std::unique_ptr<llvm::Module> generate(const Program& prog,
                                           const std::string& moduleName = "izi");

    // Compile to object file (for `izi build`)
    bool emitObjectFile(llvm::Module& mod, const std::string& outPath);

private:
    llvm::LLVMContext ctx;
    std::unique_ptr<llvm::IRBuilder<>> builder;

    // Helpers for LLVM types
    llvm::Type* llvmType(IziType t);
    llvm::Type* llvmPtrType(); // i8*

    // Code generation
    void genFn(llvm::Module& mod, const FnDecl& fn);
    void genBlock(const BlockStmt& block, llvm::Function* func,
                  std::vector<VarMap>& scopes);
    void genStmt(const Stmt& stmt, llvm::Function* func,
                 std::vector<VarMap>& scopes);
    void genVarDecl(const VarDeclStmt& stmt, llvm::Function* func,
                    std::vector<VarMap>& scopes);
    void genReturn(const ReturnStmt& stmt, llvm::Function* func,
                   std::vector<VarMap>& scopes, IziType retType);
    void genIf(const IfStmt& stmt, llvm::Function* func,
               std::vector<VarMap>& scopes);
    void genWhile(const WhileStmt& stmt, llvm::Function* func,
                  std::vector<VarMap>& scopes);

    llvm::Value* genExpr(const Expr& expr, std::vector<VarMap>& scopes);
    llvm::Value* genBinary(const BinaryExpr& expr, std::vector<VarMap>& scopes);
    llvm::Value* genUnary(const UnaryExpr& expr, std::vector<VarMap>& scopes);
    llvm::Value* genCall(const CallExpr& expr, llvm::Module& mod,
                         std::vector<VarMap>& scopes);
    llvm::Value* genAssign(const AssignExpr& expr, std::vector<VarMap>& scopes);

    // Lookup variable alloca across scopes (innermost first)
    llvm::AllocaInst* lookupVar(const std::string& name,
                                const std::vector<VarMap>& scopes) const;

    // Create alloca in function entry block
    llvm::AllocaInst* createEntryAlloca(llvm::Function* func,
                                        const std::string& name,
                                        llvm::Type* type);

    // Declare or retrieve the printf function
    llvm::FunctionCallee getPrintf(llvm::Module& mod);
    // Declare or retrieve the built-in print wrapper
    llvm::Value* genBuiltinPrint(const CallExpr& expr, llvm::Module& mod,
                                  std::vector<VarMap>& scopes);

    // Current module (used inside nested generators)
    llvm::Module* currentModule{nullptr};
};

} // namespace izi
