#include "sema.h"

/*

    Semantic Analyzer,
    this is main file, where we implemented the analyze() function, which has two passes, and
    delegate type checking to the TypeChecker.

    
*/
#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/functions.h"
#include "../parser/ast/classes.h"
#include <iostream>

SemanticAnalyzer::SemanticAnalyzer(ASTNode* root)
   : root(root), errors(false), typeChecker(symbolTable, functions, errors, classes, currentClassContext) {}

void SemanticAnalyzer::analyze() {
    auto* program = static_cast<ProgramNode*>(root);
    for (auto& node : program->globals) {
        if (node->type == NodeType::FUNC_DEF) {
            auto* func = static_cast<FunctionDefNode*>(node.get());
            FunctionSignature sig;
            sig.returnType = func->returnType;
            for (auto& p : func->parameters) sig.parameterTypes.push_back(p.first);
            std::vector<FunctionSignature>& overload = functions[func->name];
            for(auto& existing : overload){
                if(existing.parameterTypes == sig.parameterTypes){
                    std::cerr << "Bery:Error [Line " << func->line << "]: Function '" << func->name <<"' is already defined with same parameter types.\n";
                    errors = true;
                    break; 
                }
            } 
            overload.push_back(sig);
    
        } else if (node->type == NodeType::CLASS_DEF) {
            auto* cls = static_cast<ClassDefNode*>(node.get());
            classes[cls->name] = cls;
        } 
        else if (node->type == NodeType::EXTERN_DECL) {
            auto* extern_node = static_cast<ExternDeclNode*>(node.get());
            FunctionSignature sig;
            sig.returnType = extern_node->returnType;
            for (auto& p : extern_node->parameters) sig.parameterTypes.push_back(p.first);
            functions[extern_node->name].push_back(sig);
        }
        else if (node->type == NodeType::ENUM_DECL) {
            analyzeEnumDecl(node.get());
        }
    }
    for (auto& node : program->globals)
        if (node->type != NodeType::ENUM_DECL) {
            analyzeNode(node.get());
        }
    if (program->runBlock) {
        symbolTable.pushScope();
        for (auto& node : program->runBlock->statements)
            analyzeNode(node.get());
        symbolTable.popScope();
    }
}

void SemanticAnalyzer::analyzeNode(ASTNode* node) {
    if (node->type == NodeType::VAR_DECL)               analyzeVarDecl(node);
    else if (node->type == NodeType::ARRAY_DECL)        analyzeArrayDecl(node);
    else if (node->type == NodeType::ASSIGNMENT_EXPR || 
             node->type == NodeType::CALL_EXPR || 
             node->type == NodeType::UNARY_EXPR)   typeChecker.analyzeExpression(node);
    else if(node->type == NodeType::IF_STMT)            analyzeIfStmt(node);
    else if (node->type == NodeType::WHILE_STMT)        analyzeWhileStmt(node);
    else if (node->type == NodeType::DOWHILE_STMT)      analyzeDoWhileStmt(node);
    else if (node->type == NodeType::FOR_STMT)          analyzeForStmt(node);
    else if (node->type == NodeType::FUNC_DEF)          analyzeFuncDef(node);
    else if (node->type == NodeType::RETURN_STMT)       analyzeReturnStmt(node);
    else if (node->type == NodeType::CONTINUE_STMT)     analyzeContinueStmt(node);
    else if (node->type == NodeType::SWITCH_STMT)       analyzeSwitchStmt(node);
    else if (node->type == NodeType::ENUM_DECL)         analyzeEnumDecl(node);
    else if (node->type == NodeType::FOR_STMT)          analyzeForStmt(node);
    else if (node->type == NodeType::FOR_IN_STMT)       analyzeForInStmt(node);
    else if (node->type == NodeType::CLASS_DEF)         analyzeClassDecl(node);
    else if (node->type == NodeType::PASS_STMT){}
    else if (node->type == NodeType::EXTERN_DECL){}
} 


bool SemanticAnalyzer::hasErrors() { return errors; }