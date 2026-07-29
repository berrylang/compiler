#pragma once 
#include "node.h"
#include <string>
#include <vector>
#include <memory>


struct ArrayDeclNode: public ASTNode{
    std:: string elementType;
    std:: string name;
    std::vector<int> dimensions;  
    std::vector<std::unique_ptr<ASTNode>> initializers;
    std::unique_ptr<ASTNode> valueExpr;
    AccessSpecifier access;
    bool isConst;

    ArrayDeclNode(std::string elementType,std::string name,std::vector<int> dim,  std::vector<std::unique_ptr<ASTNode>> initializers, AccessSpecifier access, bool isConst, int ln): 
    elementType(elementType),name(name), dimensions(dim),    initializers(std::move(initializers)), access(access), isConst(isConst){
        type=NodeType::ARRAY_DECL;
        line = ln;
    }

};
