#include "importer.h"

/*

    Bery Importer,

    it resolves imports by - 
        1. finds .bry file on disk
        2. send it to lexer -> parser to create it's own AST
        3. recursively resolves any imports inside that module too.
        4. collects all global names from the module,
        5. runs mangler on them, like add() function becomes moduleName.add() function.
        6. replaces the 'IMPORT_NODE' on main programs AST with these nodes.

*/

#include "mangler.h"
#include "../lexer/lexer.h"
#include "../parser/parser.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/importer.h"
#include "../parser/ast/functions.h"
#include <iostream>
#include <fstream>
#include <sstream>

void Importer::resolveImports(ProgramNode* mainProgram, const std::string& basePath, DiagnosticEngine& diag) {
    std::vector<std::unique_ptr<ASTNode>> newGlobals;
    
    for (auto& node : mainProgram->globals) {
        if (node->type == NodeType::IMPORT_STMT) {
            auto* imp = static_cast<ImportNode*>(node.get());
            std::string fullPath = basePath + imp->path; 
            loadModule(imp->moduleName, fullPath, basePath, newGlobals, diag);
        } else {
            newGlobals.push_back(std::move(node));
        }
    }
    mainProgram->globals = std::move(newGlobals);
}

void Importer::loadModule(const std::string& modName, const std::string& fullPath, const std::string& basePath, std::vector<std::unique_ptr<ASTNode>>& outGlobals, DiagnosticEngine& diag) {
    if (importedFiles.count(fullPath)) return; 
    importedFiles.insert(fullPath);
    std::ifstream file(fullPath);
    if (!file.is_open()) {
        std::cerr <<"Bery:Error: Cannot find imported module '" << fullPath <<"'\n";
        exit(1);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    Lexer lexer(buffer.str(), diag);
    auto tokens = lexer.tokanize();
    Parser parser(tokens);
    auto ast = parser.parse();
    auto* importedProg = static_cast<ProgramNode*>(ast.get());

    if (parser.hasErrors()) {
        std::cerr <<"Bery: Compilation halted due to syntax errors in imported module '" << modName <<"'.\n";
        exit(1);
    }

    std::vector<std::unique_ptr<ASTNode>> processedGlobals;
    for (auto& node : importedProg->globals) {
        if (node->type == NodeType::IMPORT_STMT) {
            auto* imp = static_cast<ImportNode*>(node.get());
            std::string nextFullPath = basePath + imp->path;
            loadModule(imp->moduleName, nextFullPath, basePath, processedGlobals, diag);
        } else {
            processedGlobals.push_back(std::move(node));
        }
    }
    std::unordered_set<std::string> globalNames;
    for (auto& g : processedGlobals) {
        if (g->type == NodeType::FUNC_DEF) 
            globalNames.insert(static_cast<FunctionDefNode*>(g.get())->name);
        else if (g->type == NodeType::VAR_DECL) 
            globalNames.insert(static_cast<VarDeclNode*>(g.get())->name);
        else if (g->type == NodeType::ARRAY_DECL) 
            globalNames.insert(static_cast<ArrayDeclNode*>(g.get())->name);
        else if (g->type == NodeType::ENUM_DECL) 
            globalNames.insert(static_cast<EnumDeclNode*>(g.get())->name);
    }
    ASTNameMangler mangler(modName, globalNames);
    for (auto& g : processedGlobals) {
        mangler.mangle(g.get());
    }
    for (auto& g : processedGlobals) {
        outGlobals.push_back(std::move(g));
    }
}