#include "lexer/lexer.h"
#include "parser/parser.h"
#include "typechecker/typechecker.h"
#include "codegen/codegen.h"

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Read entire file into string
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "izi: cannot open file '" << path << "'\n";
        std::exit(1);
    }
    return {std::istreambuf_iterator<char>(f), {}};
}

static void usage() {
    std::cerr << "Usage:\n"
              << "  izi build <file.izi> [-o <output>]   compile to native binary\n"
              << "  izi emit-ir <file.izi>               print LLVM IR\n"
              << "  izi lex <file.izi>                   print tokens\n";
    std::exit(1);
}

int main(int argc, char** argv) {
    if (argc < 3) usage();

    std::string cmd   = argv[1];
    std::string input = argv[2];

    std::string source = readFile(input);

    // -----------------------------------------------------------------------
    // LEX
    // -----------------------------------------------------------------------
    if (cmd == "lex") {
        izi::Lexer lexer(source, input);
        auto tokens = lexer.tokenize();
        for (auto& tok : tokens) {
            std::cout << tok.line << ":" << tok.col
                      << " [" << static_cast<int>(tok.kind) << "] "
                      << tok.text << "\n";
        }
        return 0;
    }

    // -----------------------------------------------------------------------
    // PARSE
    // -----------------------------------------------------------------------
    izi::Lexer lexer(source, input);
    auto tokens = lexer.tokenize();

    izi::Program prog;
    try {
        izi::Parser parser(std::move(tokens));
        prog = parser.parse();
    } catch (const izi::ParseError& e) {
        std::cerr << input << ":" << e.line << ":" << e.col
                  << ": parse error: " << e.what() << "\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // TYPE CHECK
    // -----------------------------------------------------------------------
    try {
        izi::TypeChecker checker;
        checker.check(prog);
    } catch (const izi::TypeError& e) {
        std::cerr << input << ":" << e.line << ":" << e.col
                  << ": type error: " << e.what() << "\n";
        return 1;
    }

    // -----------------------------------------------------------------------
    // CODEGEN — produce LLVM IR
    // -----------------------------------------------------------------------
    izi::CodeGen cg;
    auto mod = cg.generate(prog, input);

    // Verify IR
    std::string verifyErr;
    llvm::raw_string_ostream errStream(verifyErr);
    if (llvm::verifyModule(*mod, &errStream)) {
        std::cerr << "Internal error: invalid LLVM IR generated:\n"
                  << verifyErr << "\n";
        return 1;
    }

    if (cmd == "emit-ir") {
        mod->print(llvm::outs(), nullptr);
        return 0;
    }

    if (cmd == "build") {
        // Determine output paths
        std::string outBinary = "a.out";
        std::string objFile   = "/tmp/izi_out.o";

        for (int i = 3; i < argc; ++i) {
            if (std::string(argv[i]) == "-o" && i + 1 < argc) {
                outBinary = argv[i + 1];
                ++i;
            }
        }

        // Emit object file
        if (!cg.emitObjectFile(*mod, objFile)) {
            std::cerr << "izi: failed to emit object file\n";
            return 1;
        }

        // Link with clang
        std::string linkCmd = "clang " + objFile + " -o " + outBinary + " -lm";
        int ret = std::system(linkCmd.c_str());
        if (ret != 0) {
            std::cerr << "izi: linker failed\n";
            return 1;
        }

        std::cout << "Built: " << outBinary << "\n";
        return 0;
    }

    usage();
    return 1;
}
