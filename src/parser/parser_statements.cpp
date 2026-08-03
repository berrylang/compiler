#include "parser.h"

/*

    Parsing declaration statements

    Bery supports variable delcrations, constants declarations,
    any dimentional array declarations, 
    enumerate datatype declaration, 
    import statements and 
    FFI functions using extern keyword.

    maintained by this parser_statements.cpp file

*/

#include "ast/literals.h"
#include "ast/vardecl.h"
#include "ast/arraydeclare.h"
#include <stdexcept>
#include <iostream>
#include "ast/expressions.h"
#include "ast/functions.h"
#include "ast/importer.h"


std::vector<std::unique_ptr<ASTNode>> Parser::parseVarDecl(AccessSpecifier access,bool isConst) {
    std::vector<std::unique_ptr<ASTNode>> decls;
    Token typeToken = advance();
    std::string varType = typeToken.lexeme;
    do {
        Token name = consume(TokenType::TOKEN_IDENT, "Expected identifier");
        if (check(TokenType::TOKEN_LBRACKET)) {
            decls.push_back(parseArrayDeclTail(varType, name, access, isConst));
            continue;
        }

        std::unique_ptr<ASTNode> value = nullptr;
        if (check(TokenType::TOKEN_EQUAL)) {
            advance();
            value = parseExpression();
        }
        decls.push_back(std::make_unique<VarDeclNode>(varType, name.lexeme, std::move(value), access,name.line, isConst));
    } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';'");
    return decls;
}
std::unique_ptr<ASTNode> Parser::parseLiteral() {
    Token t = peek();

    switch(t.type) {
        case TokenType::TOKEN_INT_LIT: 
            advance();
            return std::make_unique<IntLitNode>(std::stoll(t.lexeme), t.line);
        case TokenType::TOKEN_DECIMAL_LIT: 
            advance();
            return std::make_unique<DecimalLitNode>(std::stod(t.lexeme), t.line);
        case TokenType::TOKEN_TRUE:
            advance();
            return std::make_unique<BoolLitNode>(true, t.line);
        case TokenType::TOKEN_FALSE:
            advance();
            return std::make_unique<BoolLitNode>(false, t.line);
        case TokenType::TOKEN_CHAR_LIT:
            advance();
            return std::make_unique<CharLitNode>(t.lexeme[0], t.line);
        case TokenType::TOKEN_STRING_LIT:
            advance();
            return std::make_unique<StringLitNode>(t.lexeme, t.line);
        case TokenType::TOKEN_NULL:
            advance();
            return std::make_unique<NullLitNode>(t.line);
        default:
            errors = true;
            std::cerr <<"Bery:Error [Line " << t.line <<"]: Expected valid expression or literal\n";
            throw ParseError();
    }
}

std::unique_ptr<ASTNode> Parser::parseArrayDeclTail(const std::string& elementType, const Token& nameToken, AccessSpecifier access, bool isConst) {
    std::string name = nameToken.lexeme;

    std::vector<int> dimensions;
    while (check(TokenType::TOKEN_LBRACKET)) {
        advance();
        if (check(TokenType::TOKEN_INT_LIT)) {
            Token sizeToken = advance();
            dimensions.push_back(std::stoi(sizeToken.lexeme));
        } else {
            dimensions.push_back(-1); 
        }
        consume(TokenType::TOKEN_RBRACKET, "Expected ']'");
    }

    std::vector<std::unique_ptr<ASTNode>> initializers;
    std::unique_ptr<ASTNode> valueExpr = nullptr;
    bool isDynamic = dimensions.size() ==1 && dimensions[0]==-1;

    if (check(TokenType::TOKEN_EQUAL)) {
        advance(); 
        if (check(TokenType::TOKEN_LBRACE)) {parseArrayInitializer(initializers);} 
        else if(isDynamic) {
            valueExpr = parseExpression();
        } else {
            std::cerr <<"Bery:Error: [Line " << peek().line <<"]: Arrays must be initialized with list inside '{}'\n";
            errors = true;
            while (!isAtEnd() && !check(TokenType::TOKEN_SEMICOLON)) advance();
            return std::make_unique<ArrayDeclNode>(elementType, name, dimensions, std::move(initializers), access, isConst, nameToken.line);
        }
    }
    auto decl = std::make_unique<ArrayDeclNode>(elementType, name, dimensions, std::move(initializers), access, isConst, nameToken.line);
    decl->valueExpr = std::move(valueExpr);
    return decl;
}

void Parser::parseArrayInitializer(std::vector<std::unique_ptr<ASTNode>>& initializers) {
    consume(TokenType::TOKEN_LBRACE, "Expected '{'");
    if (!check(TokenType::TOKEN_RBRACE)) {
        do {
            if (check(TokenType::TOKEN_LBRACE)) {
                parseArrayInitializer(initializers);
            } else {
                initializers.push_back(parseExpression());
            }
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    consume(TokenType::TOKEN_RBRACE, "Expected '}'");
}

// @enum data declaration
std::unique_ptr<ASTNode> Parser::parseEnumDecl() {
    advance();
    int line = previous().line;
    Token nameTok = consume(TokenType::TOKEN_IDENT, "Expected enum name");
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after enum name");
    consume(TokenType::TOKEN_LBRACE, "Expected '{' to start enum values");

    std::vector<std::string> values;
    if (!check(TokenType::TOKEN_RBRACE)) {
        do {
            Token valTok = consume(TokenType::TOKEN_IDENT, "Expected enum value");
            values.push_back(valTok.lexeme);
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    
    consume(TokenType::TOKEN_RBRACE, "Expected '}' after enum values");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after enum declaration");

    return std::make_unique<EnumDeclNode>(nameTok.lexeme, std::move(values), line);
}

// @import statements, with path resolution and (.bry) extension
std::unique_ptr<ASTNode> Parser::parseImportDecl() {
    advance();
    int line = previous().line;

    Token startTok = consume(TokenType::TOKEN_IDENT, "Expected module name after 'import'");
    std::string fullName = startTok.lexeme;

    while (check(TokenType::TOKEN_DOT)) {
        advance(); 
        Token nextIdent = consume(TokenType::TOKEN_IDENT, "Expected identifier after '.'");
        fullName += "." + nextIdent.lexeme;
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after import statement");
    std::string filePath = fullName;
    for (char& c : filePath) {
        if (c == '.') c = '/';
    }
    filePath += ".bry";

    return std::make_unique<ImportNode>(fullName, filePath, line);
}

// @FFI declarations using extern keyword
std::unique_ptr<ASTNode> Parser::parseExternDecl() {
    int ln = peek().line;
    advance();
    consume(TokenType::TOKEN_FUNC, "Expected 'func' after 'extern'.");
    Token nameToken = consume(TokenType::TOKEN_IDENT, "Expected function name after 'func'.");
    consume(TokenType::TOKEN_LPARAN, "Expected '(' after function name.");

    std::vector<std::pair<std::string, std::string>> params;

    if(!check(TokenType::TOKEN_RPARAN)){
        do {
            Token typeTok = advance();
            Token nameTok = consume(TokenType::TOKEN_IDENT, "Expected parameter name");
            params.push_back({typeTok.lexeme, nameTok.lexeme});
        } while (!isAtEnd() && check(TokenType::TOKEN_COMMA) && (advance(), true));
    }
    consume(TokenType::TOKEN_RPARAN, "Expected ')' after extern parameters");

    std::string returnType = "void";
    if (check(TokenType::TOKEN_ARROW)) {
        advance();
        returnType = advance().lexeme;
    }
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after extern declaration");
    return std::make_unique<ExternDeclNode>(nameToken.lexeme, returnType, std::move(params), ln);
}
