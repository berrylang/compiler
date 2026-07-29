#include "parser.h"

/*

    Parsing conditional statements

    This file include if-else_if-else statements, and switch-case blocks
    normal syntax.

*/

#include "ast/controlflow.h"
#include <iostream>


std::unique_ptr<ASTNode> Parser::parseIfStmt() {
    advance();
    int line = previous().line;

    consume(TokenType::TOKEN_LPARAN, "Expected '(' after if");
    auto condition = parseExpression();
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after condition");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' before if Body");
    auto ifBranch = parseBlock();    
    std::unique_ptr<ASTNode> elseBranch = nullptr;
    if(check(TokenType::TOKEN_ELSE)){
        advance();
        if(check(TokenType::TOKEN_IF)){
            elseBranch = parseIfStmt();
        }else{
            consume(TokenType::TOKEN_LBRACE, "Expected '{' before if else Body");
            elseBranch = parseBlock();
        }
    }

    return std::make_unique<IfStmtNode>(std::move(condition),std::move(ifBranch),std::move(elseBranch),line); 

}

std::unique_ptr<ASTNode> Parser::parseSwitchStmt() {
    advance();
    int line = previous().line;

    consume(TokenType::TOKEN_LPARAN, "Expected '(' after switch");
    auto expr = parseExpression();
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after condition");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' before switch-body");

    auto sw = std::make_unique<SwitchStmtNode>(line);
    sw->condition = std::move(expr);

    while (!isAtEnd() && !check(TokenType::TOKEN_RBRACE)) {
        if (check(TokenType::TOKEN_CASE)) {
            advance();
            CaseBlock cb;
            cb.value = parseExpression();
            consume(TokenType::TOKEN_COLON, "expected ':' after case");
            while (!isAtEnd() && !check(TokenType::TOKEN_CASE) &&
                   !check(TokenType::TOKEN_DEFAULT) && !check(TokenType::TOKEN_RBRACE)) {
                for (auto& stmt : parseStatement()) cb.statements.push_back(std::move(stmt));
            }
            sw->cases.push_back(std::move(cb));
        }
        else if (check(TokenType::TOKEN_DEFAULT)) {
            advance();
            consume(TokenType::TOKEN_COLON, "Expected ':'");
            sw->hasDefault = true;

            while (!isAtEnd() && !check(TokenType::TOKEN_CASE) && !check(TokenType::TOKEN_RBRACE)) {
                for (auto& stmt : parseStatement()) sw->defaultBlock.push_back(std::move(stmt));
            }
        }
        else {
            std::cerr << "Bery:Error [Line " << peek().line << "]: Expected 'case' or 'default'\n";
            errors = true;
            advance();
        }
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after switch body");
    return sw;
}