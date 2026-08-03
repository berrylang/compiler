#include "mangler.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/functions.h"
#include "../parser/ast/expressions.h"
#include "../parser/ast/controlflow.h"
#include "../parser/ast/blocknode.h"
#include "../parser/ast/literals.h"

ASTNameMangler::ASTNameMangler(std::string p, std::unordered_set<std::string>& g) : prefix(p + "."), globals(g) {}

void ASTNameMangler::mangle(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NodeType::VAR_DECL: {
            auto* n = static_cast<VarDeclNode*>(node);
            if (globals.count(n->name)) n->name = prefix + n->name;
            mangle(n->value.get());
            break;
        }
        case NodeType::ARRAY_DECL: {
            auto* n = static_cast<ArrayDeclNode*>(node);
            if (globals.count(n->name)) n->name = prefix + n->name;
            for (auto& init : n->initializers) mangle(init.get());
            break;
        }
        case NodeType::FUNC_DEF: {
            auto* n = static_cast<FunctionDefNode*>(node);
            if (globals.count(n->name)) n->name = prefix + n->name;
            mangle(n->body.get());
            break;
        }
        case NodeType::ENUM_DECL: {
            auto* n = static_cast<EnumDeclNode*>(node);
            if (globals.count(n->name)) n->name = prefix + n->name;
            break;
        }
        case NodeType::IDENT: {
            auto* n = static_cast<IdentNode*>(node);
            if (globals.count(n->name)) n->name = prefix + n->name;
            break;
        }
        case NodeType::CALL_EXPR: {
            auto* n = static_cast<CallExprNode*>(node);
            if (globals.count(n->callee)) n->callee = prefix + n->callee;
            for (auto& arg : n->arguments) mangle(arg.get());
            break;
        }
        case NodeType::INDEX_EXPR: {
            auto* n = static_cast<IndexExprNode*>(node);
            if (globals.count(n->name)) n->name = prefix + n->name;
            for (auto& idx : n->indices) mangle(idx.get());
            break;
        }
        case NodeType::ASSIGNMENT_EXPR: {
            auto* n = static_cast<AssignmentExprNode*>(node);
            mangle(n->target.get());
            mangle(n->value.get());
            break;
        }
        case NodeType::BINARY_EXPR: {
            auto* n = static_cast<BinaryExprNode*>(node);
            mangle(n->left.get());
            mangle(n->right.get());
            break;
        }
        case NodeType::UNARY_EXPR: {
            auto* n = static_cast<UnaryExprNode*>(node);
            mangle(n->operand.get());
            break;
        }
        case NodeType::TERNARY_EXPR: {
            auto* n = static_cast<TernaryExprNode*>(node);
            mangle(n->condition.get());
            mangle(n->trueExpr.get());
            mangle(n->falseExpr.get());
            break;
        }
        case NodeType::GROUPED_EXPR: {
            auto* n = static_cast<GroupedExprNode*>(node);
            mangle(n->expression.get());
            break;
        }
        case NodeType::BETWEEN_EXPR: {
            auto* n = static_cast<BetweenExprNode*>(node);
            mangle(n->value.get());
            mangle(n->lower.get());
            mangle(n->upper.get());
            break;
        }
        case NodeType::CAST_EXPR: {
            auto* n = static_cast<CastExprNode*>(node);
            mangle(n->expr.get());
            break;
        }
        case NodeType::BLOCK: {
            auto* n = static_cast<BlockNode*>(node);
            for (auto& statement : n->statements) mangle(statement.get());
            break;
        }
        case NodeType::IF_STMT: {
            auto* n = static_cast<IfStmtNode*>(node);
            mangle(n->conditions.get());
            mangle(n->ifBranch.get());
            if (n->elseBranch) mangle(n->elseBranch.get());
            break;
        }
        case NodeType::WHILE_STMT: {
            auto* n = static_cast<WhileStmtNode*>(node);
            mangle(n->condition.get());
            mangle(n->body.get());
            break;
        }
        case NodeType::DOWHILE_STMT: {
            auto* n = static_cast<DoWhileStmtNode*>(node);
            mangle(n->condition.get());
            mangle(n->body.get());
            break;
        }
        case NodeType::FOR_STMT: {
            auto* n = static_cast<ForStmtNode*>(node);
            for(auto& statement :n->init){
                mangle(statement.get());
            }
            if (n->condition) mangle(n->condition.get());
            for(auto& statement : n->update){
                mangle(statement.get());
            }
            mangle(n->body.get());
            break;
        }
        case NodeType::FOR_IN_STMT: {
            auto* n = static_cast<ForInNode*>(node);
            mangle(n->iterableOrStart.get());
            if (n->rangeEnd) mangle(n->rangeEnd.get());
            if (n->step) mangle(n->step.get());
            mangle(n->body.get());
            break;
        }
        case NodeType::SWITCH_STMT: {
            auto* n = static_cast<SwitchStmtNode*>(node);
            mangle(n->condition.get());
            for (auto& c : n->cases) {
                if (c.value) mangle(c.value.get());
                for (auto& statement : c.statements) mangle(statement.get());
            }
            for (auto& statement : n->defaultBlock) mangle(statement.get());
            break;
        }
        case NodeType::RETURN_STMT: {
            auto* n = static_cast<ReturnStmtNode*>(node);
            if (n->value) mangle(n->value.get());
            break;
        }
        default:
            break;
    }
}