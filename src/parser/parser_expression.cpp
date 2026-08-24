#include "parser.h"
/*
    In Bery, each expression is made up of smaller expressions.

    the precedence of operators in Bery : (lowese to highest):
    01. ternary
    02. logical
    03. equality
    04. relational
    05. between
    06. bitwise
    07. shift
    08. additives
    09. multiplicative
    10. unary
    11. postfix
    12. primary

    the parsing flow goes from lowest to highest. and because 12th level (primary) level can have
    grouped expressions it should call the praseExpression() again, making it -
        Recursive Descent Parsing
*/

#include "ast/literals.h"
#include <stdexcept>
#include <iostream>
#include "ast/expressions.h"


std::unique_ptr<ASTNode> Parser::parseExpression(){
    auto expr= parseTernary();
    auto isassignmentOperator= [](TokenType t){
        return t==TokenType::TOKEN_EQUAL || t== TokenType::TOKEN_ADD_ASSIGN || t== TokenType::TOKEN_SUB_ASSIGN 
        || t== TokenType::TOKEN_MUL_ASSIGN || t== TokenType::TOKEN_DIV_ASSIGN  || t== TokenType::TOKEN_DSTAR_ASSIGN 
        || t== TokenType::TOKEN_MODULE_ASSIGN || t== TokenType::TOKEN_AND_ASSIGN || t== TokenType::TOKEN_OR_ASSIGN 
        || t== TokenType::TOKEN_XOR_ASSIGN || t== TokenType::TOKEN_LSHIFT_ASSIGN || t== TokenType::TOKEN_RSHIFT_ASSIGN ;
    };
    if(isassignmentOperator(peek().type)) {
        Token optoken = advance();
        auto value = parseExpression();
        
        if(expr->type == NodeType::IDENT || expr->type == NodeType::INDEX_EXPR) {
            return std::make_unique<AssignmentExprNode>(std::move(expr), std::move(value), optoken.lexeme, optoken.line);
        }
        std::cerr<<"Bery:Error [Line "<< optoken.line<<"]: Invalid Assignment Target \n";
        errors= true;
        throw ParseError();
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseTernary() {
    auto expr = parseLogicalOr();
    if (check(TokenType::TOKEN_QUESTION)) {
        advance();
        int opLine = previous().line;

        auto trueExpr = parseExpression();
        consume(TokenType::TOKEN_COLON, "Expected ':' in ternary operator");
        auto falseExpr = parseTernary();

        return std::make_unique<TernaryExprNode>(std::move(expr), std::move(trueExpr), std::move(falseExpr), opLine);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parseLogicalOr(){
    auto left = parseLogicalAnd();
    while(check(TokenType::TOKEN_OR)){
        std::string op=peek().lexeme;
        advance();
        auto right=parseLogicalAnd();
        left=std::make_unique<BinaryExprNode>(op,std::move(left),std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseLogicalAnd(){
    auto left = parseEquality();
    while(check(TokenType::TOKEN_AND)){
        std::string op=peek().lexeme;
        advance();
        auto right=parseEquality();
        left=std::make_unique<BinaryExprNode>(op,std::move(left),std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseEquality(){
    auto left = parseRelational();
    while(check(TokenType::TOKEN_EQUAL_EQUAL)|| check(TokenType::TOKEN_NOT_EQUAL)){
        std::string op=peek().lexeme;
        advance();
        auto right=parseRelational();
        left=std::make_unique<BinaryExprNode>(op,std::move(left),std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseRelational(){
    auto left =parseBetween();
   
    while(check(TokenType::TOKEN_GTHAN)|| check(TokenType::TOKEN_LTHAN)||check(TokenType::TOKEN_GTEQUAL)||check(TokenType::TOKEN_LTEQUAL)){
        std::string op= peek().lexeme;
        advance();
        auto right=parseBetween();
        left=std::make_unique<BinaryExprNode>(op,std::move(left),std::move(right), previous().line);    
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseBetween(){
    auto value = parseBitwise();
    if(check(TokenType::TOKEN_BETWEEN) || check(TokenType::TOKEN_NOT_BETWEEN)){
        bool isNegated = check(TokenType::TOKEN_NOT_BETWEEN);
       advance();
       int opLine = previous().line;
        auto lower = parseBitwise();
        consume(TokenType::TOKEN_COMMA, "Expected ',' after lower bound");
        auto upper = parseBitwise();
        return std::make_unique<BetweenExprNode>(
            std::move(value),
            std::move(lower),
            std::move(upper),
            isNegated, 
            opLine
        );
    }
    return value;
}

std::unique_ptr<ASTNode> Parser::parseBitwise(){
    auto left = parseShift();

    while(check(TokenType::TOKEN_AMPERSAND) || check(TokenType::TOKEN_CARET) || check(TokenType::TOKEN_PIPE)){
        std::string op = peek().lexeme;
        advance();
        auto right = parseShift();
        left = std::make_unique<BinaryExprNode>(op, std::move(left), std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseShift(){
    auto left = parseAdditive();

    while(check(TokenType::TOKEN_LSHIFT) || check(TokenType::TOKEN_RSHIFT)){
        std::string op = peek().lexeme;
        advance();
        auto right = parseAdditive();
        left = std::make_unique<BinaryExprNode>(op,std::move(left), std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseAdditive(){
    auto left = parseMultiplicative();
    
    while(check(TokenType::TOKEN_MINUS) ||check(TokenType::TOKEN_PLUS)){
        std::string optr = peek().lexeme;
        advance();
        auto right = parseMultiplicative();
        left = std::make_unique<BinaryExprNode>(optr,std::move(left),std::move(right), previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseMultiplicative(){
    auto left = parseUnary();

    while(check(TokenType::TOKEN_STAR) || check(TokenType::TOKEN_FSLASH)||
        check(TokenType::TOKEN_PERCENT)|| check(TokenType::TOKEN_POWER)){
        std::string optr = peek().lexeme;
        advance();
        auto right = parseUnary();
        left = std::make_unique<BinaryExprNode>(optr, std::move(left), std::move(right),previous().line);
    }
    return left;
}

std::unique_ptr<ASTNode> Parser::parseUnary(){
    if (check(TokenType::TOKEN_LPARAN) &&
        current + 1 < (int)tokens.size() && isTypeToken(tokens[current + 1].type) && 
        current + 2 < (int)tokens.size() && tokens[current + 2].type == TokenType::TOKEN_RPARAN) {
            advance();
            Token typeToken = advance();
            advance();

            auto expr = parseUnary();
            return std::make_unique<CastExprNode>(typeToken.lexeme, std::move(expr), typeToken.line);
    } 

    if(check(TokenType::TOKEN_BANG) ||check(TokenType::TOKEN_TILDE)||
        check(TokenType::TOKEN_INC)||check(TokenType::TOKEN_DEC)||
        check(TokenType::TOKEN_MINUS)){
            advance();
            std::string optr = previous().lexeme;
            auto operand = parsePostfix();
            return std::make_unique<UnaryExprNode>(optr, std::move(operand), previous().line);
    }
    return parsePostfix();
}

std::unique_ptr<ASTNode> Parser::parsePostfix(){
    auto expr = parsePrimary();
    if(check(TokenType::TOKEN_INC) || check(TokenType::TOKEN_DEC)){
        advance();
        std::string optr = "post"+previous().lexeme;
        return std::make_unique<UnaryExprNode>(optr, std::move(expr), previous().line);
    }
    return expr;
}

std::unique_ptr<ASTNode> Parser::parsePrimary(){
    Token t = peek();
    if(t.type == TokenType::TOKEN_REF){
        advance();
        auto target = parsePrimary();
        return std::make_unique<RefExprNode>(std::move(target),t.line);
    }
    if (t.type == TokenType::TOKEN_NEW) {
        advance();
        Token className = consume(TokenType::TOKEN_IDENT, "Expected class name after 'new'");
        consume(TokenType::TOKEN_LPARAN, "Expected '(' after class name");
        std::vector<std::unique_ptr<ASTNode>> arguments;
        if (!check(TokenType::TOKEN_RPARAN)) {
            do {
                arguments.push_back(parseExpression());
            } while (check(TokenType::TOKEN_COMMA) && (advance(), true));
        }
        consume(TokenType::TOKEN_RPARAN, "Expected ')' after constructor arguments");
        return std::make_unique<NewExprNode>(className.lexeme, std::move(arguments), t.line);
    }
    if (t.type == TokenType::TOKEN_IDENT || t.type == TokenType::TOKEN_SUPER) {
        advance();
        std::string fullName = t.lexeme;
        while (check(TokenType::TOKEN_DOT)) {
            advance();
            Token nextIdent = consume(TokenType::TOKEN_IDENT, "Expected identifier after '.'");
            fullName += "." + nextIdent.lexeme;
        }
        if (check(TokenType::TOKEN_LPARAN)) {
            Token stitchedToken = t;
            stitchedToken.lexeme = fullName;
            return parseCallExpr(stitchedToken);
        }
        if (check(TokenType::TOKEN_LBRACKET)) {
            std::vector<std::unique_ptr<ASTNode>> indices;
            while (check(TokenType::TOKEN_LBRACKET)) {
                advance();
                indices.push_back(parseExpression());
                consume(TokenType::TOKEN_RBRACKET, "Expected ']' after array index");
            }
            auto idxExpr = std::make_unique<IndexExprNode>(fullName, std::move(indices), t.line);
            while (check(TokenType::TOKEN_DOT)) {
                advance();
                Token member = consume(TokenType::TOKEN_IDENT, "Expected identifier after '.'");
                idxExpr->memberChain.push_back(member.lexeme);
            }
            return idxExpr;
        }
        return std::make_unique<IdentNode>(fullName, "", t.line);
    }
    if (t.type == TokenType::TOKEN_LPARAN) {
        advance();
        auto expression = parseExpression();
        consume(TokenType::TOKEN_RPARAN, "Expected ')'");
        return std::make_unique<GroupedExprNode>(std::move(expression), t.line);
    }
    return parseLiteral();
}