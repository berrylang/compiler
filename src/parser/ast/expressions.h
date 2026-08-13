#pragma once
#include "node.h"
#include <string>
#include <memory>
#include <vector>


struct UnaryExprNode : public ASTNode{
    std::string optr;
    std::unique_ptr<ASTNode> operand;

    UnaryExprNode(std::string& opr, std::unique_ptr<ASTNode> expr, int ln):
        optr(opr), operand(std::move(expr)){
            type = NodeType::UNARY_EXPR;
            line = ln;
        }

};

struct GroupedExprNode : public ASTNode{
    std::unique_ptr<ASTNode> expression;

    GroupedExprNode(std::unique_ptr<ASTNode> expr, int ln):
        expression(std::move(expr)){
            type = NodeType::GROUPED_EXPR;
            line = ln;
        }
};

struct BinaryExprNode : public ASTNode{
    std::string optr;
    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
    

    BinaryExprNode(std::string& opr, std::unique_ptr<ASTNode> l,std::unique_ptr<ASTNode> r, int ln):
        optr(opr),
        left(std::move(l)),
        right(std::move(r))
        {
            type = NodeType::BINARY_EXPR;
            line = ln;
            resolvedType = "int";
        }

};

struct BetweenExprNode : public ASTNode{
    std::unique_ptr<ASTNode> value;
    std::unique_ptr<ASTNode> lower;
    std::unique_ptr<ASTNode> upper;
    bool isNegated;
    

    BetweenExprNode(
        std::unique_ptr<ASTNode> val,
        std::unique_ptr<ASTNode> low,
        std::unique_ptr<ASTNode> up,
        bool neg,
        int ln
    )
        : value(std::move(val)),
          lower(std::move(low)),
          upper(std::move(up)),
          isNegated(neg)
    {
        type = NodeType::BETWEEN_EXPR;
        line = ln;
        resolvedType = "int";
    }
};

struct TernaryExprNode : public ASTNode {
    std::unique_ptr<ASTNode> condition;
    std::unique_ptr<ASTNode> trueExpr;
    std::unique_ptr<ASTNode> falseExpr;

    TernaryExprNode(std::unique_ptr<ASTNode> c, std::unique_ptr<ASTNode> te, std::unique_ptr<ASTNode> fe, int ln) :
    condition(std::move(c)), trueExpr(std::move(te)), falseExpr(std::move(fe)) {
        type = NodeType::TERNARY_EXPR;
        line = ln;
        resolvedType = "int";
    }
};

struct AssignmentExprNode : public ASTNode {
    std::unique_ptr<ASTNode> target;
    std::unique_ptr<ASTNode> value;
    std::string op;

    AssignmentExprNode(std::unique_ptr<ASTNode> t, std::unique_ptr<ASTNode> v,std::string op, int ln)
        : target(std::move(t)), value(std::move(v)), op(op) {
        type = NodeType::ASSIGNMENT_EXPR;
        line = ln;
    }
};

struct CastExprNode : public ASTNode {
    std::string targetType;
    std::unique_ptr<ASTNode> expr;
    std::string srcType;

    CastExprNode(std::string t, std::unique_ptr<ASTNode> e, int ln) : targetType(t), expr(std::move(e)) {
        type = NodeType::CAST_EXPR;
        line = ln;
    }
};

struct IndexExprNode : public ASTNode {
    std::string name;
    std::vector<std::unique_ptr<ASTNode>> indices;
    std::vector<std::string> memberChain;

    IndexExprNode(std::string name, std::vector<std::unique_ptr<ASTNode>> idx, int ln)
        : name(name), indices(std::move(idx)) {
        type = NodeType::INDEX_EXPR;
        line = ln;
    }
};

struct NewExprNode : public ASTNode {
    std::string className;
    std::vector<std::unique_ptr<ASTNode>> arguments;

    NewExprNode(std::string cls, std::vector<std::unique_ptr<ASTNode>> args, int ln) : className(cls), arguments(std::move(args)){
        type = NodeType::NEW_EXPR;
        line = ln;
    }
};

struct RefExprNode : public ASTNode{
    std::unique_ptr<ASTNode> target;
    RefExprNode(std::unique_ptr<ASTNode> expr,int ln): target(std::move(expr)){
        type = NodeType::REF_EXPR;
        line = ln;
    }
};