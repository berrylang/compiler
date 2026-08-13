#pragma once
#include <string>
/*


    AST Nodes

    In this file, every node type which is essential for development of Bery is listed, and 
    the parser/ast/ folder will create the header file for each node's strcuture declarations.

    Number of node types = 39


*/


enum class NodeType {
    
    // @blocks
    PROGRAM, RUN_BLOCK, BLOCK,
    
    // @speicals
    IDENT, 

    // @declaration statements
    VAR_DECL, CAST_EXPR, ARRAY_DECL, ASSIGNMENT_EXPR, INDEX_EXPR, ENUM_DECL, IMPORT_STMT, EXTERN_DECL,
    
    // @literals
    INT_LIT,  BOOL_LIT, DECIMAL_LIT, CHAR_LIT, STRING_LIT, NULL_LIT, 

    // @expressions
    TERNARY_EXPR, BINARY_EXPR, BETWEEN_EXPR, UNARY_EXPR, GROUPED_EXPR, 
    
    // @controlflow
    BREAK_STMT, CONTINUE_STMT, PASS_STMT,

    // @loops
    WHILE_STMT, DOWHILE_STMT, FOR_STMT,  FOR_IN_STMT,

    // @conditionals
    IF_STMT,SWITCH_STMT,  
    
    // @functions
    FUNC_DEF,RETURN_STMT,CALL_EXPR, 

    // @oop
    CLASS_DEF,ATTRIBUTE_SECTION,METHOD_SECTION, NEW_EXPR,REF_EXPR,
};


struct ASTNode {
    NodeType type;
    int line;
    std::string resolvedType;
    virtual ~ASTNode() = default;
};

