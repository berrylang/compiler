#pragma once

/*

PARSER DEFINITION

This is header file served for parser's definition.
it contains every helper functions which eventually helps the 'parse()' method.

*/
#include "ast/node.h"
#include "ast/blocknode.h"
#include "../lexer/token.h"
#include "ast/classes.h"
#include "ast/accessSpecifier.h"
#include <vector>
#include <memory>
#include <exception>

// @panic-mode trigger for error recovery
class ParseError : public std::exception {};


class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    std::unique_ptr<ASTNode> parse();

    // @todo : change it to the Error Handler after it's implementation as independent unit of compiler.
    bool hasErrors();

private:
    // @ast-node data and token list
    std::vector<Token> tokens;
    int current;
    bool errors;

    // @pointers inside the token list
    Token advance();
    Token peek();
    Token previous();
    bool isAtEnd();
    bool isTypeToken(TokenType t);

    // @check for peek() matches the required token type
    bool check(TokenType type);

    // @actively consume the required token; if absent throw error
    Token consume(TokenType type, const std::string& msg);
    
    // @panic-mode recovery
    void synchronize();
    

    // @every parse function
    // @statements
    std::vector<std::unique_ptr<ASTNode>> parseVarDecl(AccessSpecifier access,bool isConst);
    std::unique_ptr<ASTNode> parseArrayDeclTail(const std::string& elementType, const Token& nameToken, AccessSpecifier access, bool isConst);
    std::unique_ptr<ASTNode> parseLiteral();
    void parseArrayInitializer(std::vector<std::unique_ptr<ASTNode>>& initializers);
    std::unique_ptr<ASTNode> parseEnumDecl();
    std::unique_ptr<ASTNode> parseImportDecl();
    std::unique_ptr<ASTNode> parseExternDecl();
    static std::vector<std::unique_ptr<ASTNode>> single(std::unique_ptr<ASTNode> node);
    
    //@expressions - recursive descent
    std::unique_ptr<ASTNode> parseExpression();
    std::unique_ptr<ASTNode> parsePrimary();
    std::unique_ptr<ASTNode> parsePostfix();
    std::unique_ptr<ASTNode> parseUnary();
    std::unique_ptr<ASTNode> parseMultiplicative();
    std::unique_ptr<ASTNode> parseAdditive();
    std::unique_ptr<ASTNode> parseShift();
    std::unique_ptr<ASTNode> parseBitwise();
    std::unique_ptr<ASTNode> parseBetween();
    std::unique_ptr<ASTNode> parseRelational();
    std::unique_ptr<ASTNode> parseEquality();
    std::unique_ptr<ASTNode> parseLogicalAnd();
    std::unique_ptr<ASTNode> parseLogicalOr();
    std::unique_ptr<ASTNode> parseTernary();
    
    // @repititive blocks
    std::vector<std::unique_ptr<ASTNode>> parseStatement();
    std::unique_ptr<BlockNode> parseBlock();

    // @controlflow
    std::unique_ptr<ASTNode> parseIfStmt();
    std::unique_ptr<ASTNode> parseSwitchStmt();

    // @controlflow
    std::unique_ptr<ASTNode> parseContinueStmt();
    std::unique_ptr<ASTNode> parsePassStmt();
    std::unique_ptr<ASTNode> parseBreakStmt();

    // @loops
    std::unique_ptr<ASTNode> parseWhileStmt();
    std::unique_ptr<ASTNode> parseDoWhileStmt();
    std::unique_ptr<ASTNode> parseForStmt();

    // @functions
    std::unique_ptr<ASTNode> parseFunctionDef(AccessSpecifier access);
    std::unique_ptr<ASTNode> parseCallExpr(const Token& identifierToken);
    std::unique_ptr<ASTNode> parseReturnStmt();

    // @oops
    std::unique_ptr<ASTNode> parseClassDecl();
    std::unique_ptr<AttributeSectionNode> parseAttributeSection();
    std::unique_ptr<MethodSectionNode> parseMethodSection(const std::string& className);
    bool isClassVarDecl();
};

