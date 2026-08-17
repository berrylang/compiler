#pragma once

/*

    Bery Type Checker,

    it traverse the AST after parsing and checks : 
        what type does this produce?
    
    analyzeExpression() handles control to specific checker by a node type. 
    Each checker recursively calls analyzeExpression() on its children and takes their types back finally validates — 
        are these resolved types compatible for this operator? 
    
    If yes, return the result type. 
    If no, print error and return "unknown".

*/

#include "../parser/ast/node.h"
#include "symboltable.h"
#include <string>
#include <vector>
#include <unordered_map>
#include "../parser/ast/classes.h"
#include "../parser/ast/accessSpecifier.h"
#include "../parser/ast/vardecl.h"


struct FunctionSignature {
    std::string returnType;
    std::vector<std::string> parameterTypes;
};

class TypeChecker {
public:
    TypeChecker(SymbolTable& symbolTable, std::unordered_map<std::string, FunctionSignature>& funcs, bool& errorsFlag, std::unordered_map<std::string, ClassDefNode*>& classesMap, std::string& currentClassRef);    std::string analyzeExpression(ASTNode* node);
    bool typeMatchesLiteral(const std::string& type, NodeType litType);

private:
    SymbolTable& symbolTable;
    std::unordered_map<std::string, FunctionSignature>& functions;
    std::unordered_map<std::string, ClassDefNode*>& classes;

    // @todo : need chnages after Error Handler is added.
    bool& errors;

    std::string checkBinaryExpr(ASTNode* node);
    std::string checkUnaryExpr(ASTNode* node);
    std::string checkTernaryExpr(ASTNode* node);
    std::string checkBetweenExpr(ASTNode* node);
    std::string checkCallExpr(ASTNode* node);
    std::string checkIndexExpr(ASTNode* node);
    std::string checkAssignmentExpr(ASTNode* node);
    std::string checkCastExpr(ASTNode* node);
    std::string checkIdentifier(ASTNode* node);
    std::string checkLiteral(ASTNode* node);
    std::string checkNewExpr(ASTNode* node);
    std::string checkRefExpr(ASTNode* node);
    std::string resolveFieldType(ClassDefNode* cls, const std::string& fieldName);
    std::string resolveChainType(const std::vector<std::string>& parts, int line);
    std::string resolveFieldChainFrom(std::string currentType, const std::vector<std::string>& parts, int line);
    std::string resolveNumericPromotion(const std::string& lType, const std::string& rType);

    // @oop
    std::string& currentClass;
    ASTNode* findField(ClassDefNode* cls, const std::string& fieldName);
    FunctionDefNode* findMethod(ClassDefNode* cls, const std::string& methodName);
    bool checkMemberAccess(AccessSpecifier access, const std::string& className, const std::string& memberName, const std::string& type, int line);
};
