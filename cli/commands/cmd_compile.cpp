#include "cmd_compile.h"
#include "../../src/common/toolchain.h"
#include "../../src/common/platform.h"
#include "../../src/lexer/lexer.h"
#include "../../src/parser/parser.h"
#include "../../src/sema/sema.h"
#include "../../src/codegen/codegen.h"
#include "../../src/importer/importer.h"
#include "../../src/diagnostic/diagnostic_engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>


static std::string stemOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string filename = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = filename.find_last_of('.');
    return (dot == std::string::npos) ? filename : filename.substr(0, dot);
}

static std::string dirOf(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

std::string getExeDir(const char* argv) {
    std::string path = argv;
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

static std::string findBRELib(const std::string& exeDir) {
    const char* names[] = { "libbre.a", "bre.lib", nullptr };
    const std::string dirs[] = {
        exeDir,
        exeDir + "/../build",
        "build",
        ".",
        ""
    };
    for (int d = 0; !dirs[d].empty(); ++d) {
        for (int n = 0; names[n]; ++n) {
            std::string candidate = dirs[d] + BERY_PATH_SEP + names[n];
            std::ifstream f(candidate);
            if (f.good()) return candidate;
        }
    }
    return "";
}

static int runFrontend(const std::string& sourcePath,   const std::string& irPath) {
    std::ifstream file(sourcePath);
    if (!file.is_open()) {
        std::cerr <<"Bery: Error: cannot open file '" << sourcePath <<"'\n";
        return 3;
    }

    std::string basePath = dirOf(sourcePath) + BERY_PATH_SEP;

    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();
    std::string absPath = std::filesystem::absolute(sourcePath).lexically_normal().string();
    DiagnosticEngine diag(source, absPath);

    Lexer lexer(source, diag);
    auto tokens = lexer.tokanize();

    Parser parser(tokens, diag);
    auto ast = parser.parse();

    diag.printAll();
    if (diag.hasErrors() || parser.hasErrors()) {
        std::cerr <<"Bery: Compilation halted due to syntax errors.\n";
        return 5;
    }

    Importer importer;
    importer.resolveImports(static_cast<ProgramNode*>(ast.get()), basePath, diag);

    SemanticAnalyzer sema(ast.get());
    sema.analyze();

    if (sema.hasErrors()) {
        std::cerr <<"Bery: Compilation halted due to semantic errors.\n";
        return 6;
    }
    CodeGen codegen(ast.get(), sema.symbolTable);
    codegen.generate(irPath);
    return 0;
}

int cmdCompile(const std::string& sourcePath, std::string& outBinaryPath, const std::string& exeDir) {
    BeryToolChain tc = detectToolchain();
    if (!tc.valid) {
        std::cerr <<"Bery: Error: no LLVM toolchain found.\n\tInstall llc and g++ (Linux), clang++ (macOS/Windows).\n";
        return 10;
    }

    std::string breLib = findBRELib(exeDir);
    if (breLib.empty()) {
        std::cerr <<"Bery: Error: cannot find libbre.a.\n\tRun 'cmake --build build' first.\n";
        return 11;
    }

    std::string stem  = stemOf(sourcePath);
    std::string dir   = dirOf(sourcePath);
    std::string irFile  = dir + BERY_PATH_SEP + stem + ".ll";
    std::string objFile = dir + BERY_PATH_SEP + stem + tc.objectExt;
    outBinaryPath       = dir + BERY_PATH_SEP + stem + tc.binaryExt;
    int fe = runFrontend(sourcePath, irFile);
    if (fe != 0) return fe;
    std::string compileCmd = buildCompileCmd(tc, irFile, objFile);
    if (system(compileCmd.c_str()) != 0) {
        std::cerr <<"Bery: Error: llc failed.\n";
        return 12;
    }
    
    std::string linkCmd = buildLinkCmd(tc, objFile, breLib, outBinaryPath);

    if (system(linkCmd.c_str()) != 0) {
        std::cerr <<"Bery: Error: linker failed.\n";
        return 13;
    }
    //remove(irFile.c_str());
    remove(objFile.c_str());
    return 0;
}
