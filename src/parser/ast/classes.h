#pragma once
#include "node.h"
#include <string>
#include <vector>
#include <memory>

struct AttributeSectionNode : public ASTNode {
    std::string selfRef;
    std::vector<std::unique_ptr<ASTNode>> attributes;

    AttributeSectionNode(std::string self, std::vector<std::unique_ptr<ASTNode>> attrs, int ln)
        : selfRef(std::move(self)), attributes(std::move(attrs)) {
        type = NodeType::ATTRIBUTE_SECTION;
        line = ln;
    }
};

struct MethodSectionNode : public ASTNode {
    std::vector<std::unique_ptr<ASTNode>> methods;

    MethodSectionNode(std::vector<std::unique_ptr<ASTNode>> m, int ln)
        : methods(std::move(m)) {
        type = NodeType::METHOD_SECTION;
        line = ln;
    }
};

struct ClassDefNode : public ASTNode {
    std::string name;
    std::string parentName;
    std::unique_ptr<AttributeSectionNode> attributes;
    std::unique_ptr<MethodSectionNode> methods;

    ClassDefNode(std::string n, std::string parent, std::unique_ptr<AttributeSectionNode> attr, std::unique_ptr<MethodSectionNode> meth, int ln)
        : name(std::move(n)),parentName(std::move(parent)), attributes(std::move(attr)), methods(std::move(meth)) {
        type = NodeType::CLASS_DEF;
        line = ln;
    }
};