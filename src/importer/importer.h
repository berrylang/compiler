#pragma once

/*

    Bery Importer,

    It resolves every 'import' statements by replacing them with the actual AST nodes of the imported file,
    before semantic analyzer runs.


*/

#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include "../parser/ast/node.h"
#include "../parser/ast/programnode.h"
#include "../diagnostic/diagnostic_engine.h"

class Importer {
public:
    void resolveImports(ProgramNode* mainProgram, const std::string& basePath, DiagnosticEngine& diag);

private:
    std::unordered_set<std::string> importedFiles;
    void loadModule(const std::string& modName, const std::string& fullPath, const std::string& basePath, std::vector<std::unique_ptr<ASTNode>>& outGlobals, DiagnosticEngine& diag);
};