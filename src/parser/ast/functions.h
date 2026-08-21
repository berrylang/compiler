#pragma once
#include "node.h"
#include "blocknode.h"
#include "accessSpecifier.h"
#include <string>
#include <vector>
#include <memory>
#include <utility>

struct FunctionDefNode : public ASTNode {
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters; 
    std::string returnType;
    std::unique_ptr<BlockNode> body;
    AccessSpecifier access;
    bool isConstructor;
    bool isDestructor;

    FunctionDefNode(const std::string& name, std::vector<std::pair<std::string, std::string>> params, const std::string& retType, std::unique_ptr<BlockNode> b,AccessSpecifier access, int ln,bool isConstruct = false, bool isDestructor = false)
        : name(name), parameters(std::move(params)), returnType(retType), body(std::move(b)), access(access), isConstructor(isConstruct), isDestructor(isDestructor){
        type = NodeType::FUNC_DEF;
        line = ln;
    }
};

struct ReturnStmtNode : public ASTNode {
    std::unique_ptr<ASTNode> value;

    ReturnStmtNode(std::unique_ptr<ASTNode> val, int ln)
        : value(std::move(val)) {
        type = NodeType::RETURN_STMT;
        line = ln;
    }
};

struct CallExprNode : public ASTNode {
    std::string callee; 
    std::vector<std::unique_ptr<ASTNode>> arguments;
    std::vector<std::string> resolvedParamTypes;

    CallExprNode(const std::string& callee, std::vector<std::unique_ptr<ASTNode>> args, int ln)
        : callee(callee), arguments(std::move(args)) {
        type = NodeType::CALL_EXPR;
        line = ln;
    }
};

struct ExternDeclNode : public ASTNode {
    std::string name;
    std::string returnType;
    std::vector<std::pair<std::string, std::string>> parameters;

    ExternDeclNode(const std::string& n, const std::string& rt, std::vector<std::pair<std::string, std::string>> params, int ln) :
    name(n), returnType(rt), parameters(std::move(params)) {
        type = NodeType::EXTERN_DECL;
        line = ln;
    }
};