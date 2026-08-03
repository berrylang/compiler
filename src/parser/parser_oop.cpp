
#include "parser.h"
#include <stdexcept>
#include <iostream>
#include "ast/classes.h"
#include "ast/functions.h"

std::unique_ptr<ASTNode> Parser::parseClassDecl() {
    advance();
    int line = previous().line;
    Token className = consume(TokenType::TOKEN_IDENT, "Expected class name");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' after class name");
    
    auto attrSection = parseAttributeSection();

    std::unique_ptr<MethodSectionNode> methodSection = nullptr;
    if (check(TokenType::TOKEN_METHODS)) {
        methodSection = parseMethodSection(className.lexeme);
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after class body");
    return std::make_unique<ClassDefNode>(className.lexeme, std::move(attrSection), std::move(methodSection), line);
}

std::unique_ptr<AttributeSectionNode> Parser::parseAttributeSection() {
    consume(TokenType::TOKEN_ATTRIBUTES, "Exptected 'attributes' section");
    consume(TokenType::TOKEN_LBRACKET, "Expected '[' after 'attributes'");
    Token selfToken = consume(TokenType::TOKEN_IDENT, "Expected self-reference identifier");
    consume(TokenType::TOKEN_RBRACKET, "Expected ']' after self-reference");
    consume(TokenType::TOKEN_DCOLON, "Exptected '::' after attributes seciton verbose");

    std::vector<std::unique_ptr<ASTNode>> attributes;
    while(!isAtEnd()&& (isTypeToken(peek().type) || isClassVarDecl() || check(TokenType::TOKEN_PUBLIC) || check(TokenType::TOKEN_PRIVATE) || check(TokenType::TOKEN_PROTECTED))) {

        AccessSpecifier access = AccessSpecifier::PUBLIC;
        if(check(TokenType::TOKEN_PUBLIC)){
            advance();
            access=AccessSpecifier::PUBLIC;
        }
        else if(check(TokenType::TOKEN_PRIVATE)){
            advance();
            access=AccessSpecifier::PRIVATE;
        }
        else if(check(TokenType::TOKEN_PROTECTED)){
            advance();
            access=AccessSpecifier::PROTECTED;
        }
        if(!isTypeToken(peek().type) && !isClassVarDecl()) break;
        
        auto vars= parseVarDecl(access, false);
        for (auto& v : vars) {
            attributes.push_back(std::move(v));
        }
    }
    return std::make_unique<AttributeSectionNode>(selfToken.lexeme, std::move(attributes), selfToken.line);
}

std::unique_ptr<MethodSectionNode> Parser::parseMethodSection(const std::string& className) {
    int line = peek().line;
    consume(TokenType::TOKEN_METHODS, "Expected 'methods' section");
    consume(TokenType::TOKEN_DCOLON, "Exptected '::' after 'methods");

    std::vector<std::unique_ptr<ASTNode>> methods;
    while(!isAtEnd()){
        AccessSpecifier access = AccessSpecifier::PUBLIC;
        if(check(TokenType::TOKEN_PUBLIC)){
            advance();
            access=AccessSpecifier::PUBLIC;
        }
        else if(check(TokenType::TOKEN_PRIVATE)){
            advance();
            access=AccessSpecifier::PRIVATE;
        }
        else if(check(TokenType::TOKEN_PROTECTED)){
            advance();
            access=AccessSpecifier::PROTECTED;
        }

        if (check(TokenType::TOKEN_TILDE)) {
            advance();
            Token nameToken =consume(TokenType::TOKEN_IDENT, "Expected class name after '~'");
            if (nameToken.lexeme!= className) {
                std::cerr <<"Bery:Error [Line " << nameToken.line <<"]: Destructor name '~" << nameToken.lexeme<<"' does not match class '" << className <<"'\n";
                errors = true;
            }
            int declLine = nameToken.line;
            consume(TokenType::TOKEN_LPARAN, "Expected '(' after destructor name");
            std::vector<std::pair<std::string, std::string>> params;
            if (!check(TokenType::TOKEN_RPARAN)) {
                do {
                    Token typeToken = advance();
                    Token pNameToken = consume(TokenType::TOKEN_IDENT, "Expected parameter name");
                    std::string paramType = typeToken.lexeme;
                    if (check(TokenType::TOKEN_LBRACKET)) {
                        advance();
                        consume(TokenType::TOKEN_RBRACKET, "Expected ']' after '[' in array parameter type");
                        paramType = "array<" + paramType + ">";
                    }
                    params.push_back({paramType, pNameToken.lexeme});
                } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
            }
            consume(TokenType::TOKEN_RPARAN, "Expected ')' after parameters");
            consume(TokenType::TOKEN_LBRACE, "Expected '{' before destructor body");
            auto body = parseBlock();
            methods.push_back(std::make_unique<FunctionDefNode>(className, std::move(params), "void", std::move(body), access, declLine, false, true));
            continue;
        }

        if (check(TokenType::TOKEN_IDENT) && peek().lexeme == className &&
            current + 1 < (int)tokens.size() && tokens[current + 1].type == TokenType::TOKEN_LPARAN) {
            Token nameToken = advance();
            int declLine = nameToken.line;
            consume(TokenType::TOKEN_LPARAN, "Expected '(' after constructor name");
            std::vector<std::pair<std::string, std::string>> params;
            if (!check(TokenType::TOKEN_RPARAN)) {  do {
                    Token typeToken = advance();
                    Token pNameToken = consume(TokenType::TOKEN_IDENT, "Expected parameter name");
                    std::string paramType = typeToken.lexeme;
                    if (check(TokenType::TOKEN_LBRACKET)) {
                        advance();
                        consume(TokenType::TOKEN_RBRACKET, "Expected ']' after '[' in array parameter type");
                        paramType = "array<" + paramType + ">";
                    }
                    params.push_back({paramType, pNameToken.lexeme});
                } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
            }
            consume(TokenType::TOKEN_RPARAN, "Expected ')' after parameters");
            consume(TokenType::TOKEN_LBRACE, "Expected '{' before constructor body");
            auto body =parseBlock();
            methods.push_back(std::make_unique<FunctionDefNode>(className, std::move(params), "void", std::move(body), access, declLine, true, false));
            continue;
        }

        if(!check(TokenType::TOKEN_FUNC))
            break;
        methods.push_back(parseFunctionDef(access));
    }

    return std::make_unique<MethodSectionNode>(std::move(methods), line);
}

bool Parser::isClassVarDecl() {
    return peek().type == TokenType::TOKEN_IDENT && current + 1 < (int)tokens.size() && tokens[current + 1].type == TokenType::TOKEN_IDENT;
}