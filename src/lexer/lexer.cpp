/*

    Lexer Class

    actual implementation of the Lexer class.
    it includes - 
        1. keyword hashmap, which is checked by scanIdentifierOrKeyword() -> checkKeyword()
        2. Scanner, implemented by maximal munch method.

*/

#include "lexer.h"
#include <unordered_map>
#include <stdexcept>
#include <iostream>


static std::unordered_map<std::string, TokenType> keywords = {

    // @types
    {"int", TokenType::TOKEN_INT},
    {"float", TokenType::TOKEN_FLOAT},
    {"bigint", TokenType::TOKEN_BIGINT},
    {"double", TokenType::TOKEN_DOUBLE},
    {"bool", TokenType::TOKEN_BOOL},
    {"char", TokenType::TOKEN_CHAR},
    {"string", TokenType::TOKEN_STRING},

    // @blocks
    {"run", TokenType::TOKEN_RUN},

    // @literals
    {"true", TokenType::TOKEN_TRUE},
    {"false", TokenType::TOKEN_FALSE},
    {"null", TokenType::TOKEN_NULL},

    // @conditionals
    {"if", TokenType::TOKEN_IF},
    {"else", TokenType::TOKEN_ELSE},
    {"switch",TokenType::TOKEN_SWITCH},
    {"case",TokenType::TOKEN_CASE},
    {"default",TokenType::TOKEN_DEFAULT},
    
    // @controlflow
    {"break",TokenType::TOKEN_BREAK},
    {"continue", TokenType::TOKEN_CONTINUE},
    {"pass", TokenType::TOKEN_PASS},

    // @loops
    {"while", TokenType::TOKEN_WHILE},
    {"do", TokenType::TOKEN_DOWHILE},
    {"for", TokenType::TOKEN_FOR},
    {"in", TokenType::TOKEN_IN},

    // @functions
    {"func", TokenType::TOKEN_FUNC},
    {"return", TokenType::TOKEN_RETURN},

    // @special keywords
    {"enum", TokenType::TOKEN_ENUM},
    {"import", TokenType::TOKEN_IMPORT}, 
    {"extern", TokenType::TOKEN_EXTERN},
    {"const", TokenType::TOKEN_CONST},
    
    // @object oriented programming
    {"class", TokenType::TOKEN_CLASS},
    {"attributes", TokenType::TOKEN_ATTRIBUTES},
    {"methods", TokenType::TOKEN_METHODS},
    {"new", TokenType::TOKEN_NEW},
    {"public", TokenType::TOKEN_PUBLIC},
    {"private", TokenType::TOKEN_PRIVATE},
    {"protected", TokenType::TOKEN_PROTECTED},
};

Lexer::Lexer(const std::string& source) : source(source), current(0), line(1), errors(false) {}

std::vector<Token> Lexer::tokanize() {
    while (!isAtEnd()) {
        skipWhitespaces();
        if (!isAtEnd()) scanToken();
    }
    tokens.push_back({TokenType::TOKEN_EOF, "", line});

    // it goes to parser next. vector of tokens.
    return tokens;
}

void Lexer::scanToken() {
    char c = advance();

    if (isAlpha(c)) {
        scanIdentifierOrKeyword();
        return;
    }

    if (isDigit(c)) {
        scanNumber();
        return;
    }
    switch(c) {
        case '=' : 
            if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_EQUAL_EQUAL,"==",line});
                return;
            }
            tokens.push_back({TokenType::TOKEN_EQUAL, "=", line});
            return;
        case ';': 
            tokens.push_back({TokenType::TOKEN_SEMICOLON, ";", line});
            return;
        case '{':
            tokens.push_back({TokenType::TOKEN_LBRACE, "{", line});
            return;
        case '}':
            tokens.push_back({TokenType::TOKEN_RBRACE, "}", line});
            return;
        case ',':
            tokens.push_back({TokenType::TOKEN_COMMA, ",", line});
            return;
        case '[':
            tokens.push_back({TokenType::TOKEN_LBRACKET, "[", line});
            return;
        case ']':
            tokens.push_back({TokenType::TOKEN_RBRACKET, "]", line});
            return;
        case '\'':
            scanCharLit();
            return;
        case '"':
            scanStringLit();
            return;
        case '*':
            if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_MUL_ASSIGN, "*=", line});
            }
            else if(peek() == '*'){
                if(peekNext()== '='){
                    advance();
                    advance();
                    tokens.push_back({TokenType::TOKEN_DSTAR_ASSIGN, "**=", line});
                    return; 
                }else{
                    advance();
                    tokens.push_back({TokenType::TOKEN_POWER, "**", line});
                return;
                }
            }
            else{
                tokens.push_back({TokenType::TOKEN_STAR, "*", line});
            }
            return;
        case '/':
            if(peek()=='='){
                    advance();
                    tokens.push_back({TokenType::TOKEN_DIV_ASSIGN, "/=", line});
            }else{
                tokens.push_back({TokenType::TOKEN_FSLASH, "/", line});
                }
                return;
        case '%':
            if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_MODULE_ASSIGN,"%=",line});
            }else{
            tokens.push_back({TokenType::TOKEN_PERCENT, "%", line});
            }
            return;
        
        case '-':
            if(peek()=='-'){
                if(peekNext()=='-'){ 
                    advance();
                    advance();
                    skipComments(false);
                    return;
                }
                else if(peekNext()=='!'){ 
                    advance();
                    advance();
                    skipComments(true);
                    return;
                }
                else{
                    advance();
                    tokens.push_back({TokenType::TOKEN_DEC, "--", line});
                    return;
                }
            }
            else if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_SUB_ASSIGN,"-=",line});
                return;
            }
            else if(peek()=='>'){
                advance();
                tokens.push_back({TokenType::TOKEN_ARROW, "->", line});
                return;
            }
            else{
                tokens.push_back({TokenType::TOKEN_MINUS, "-", line});
                return;
            }
            return;
        case '+':
           if(peek()=='+'){
                advance();
                tokens.push_back({TokenType::TOKEN_INC, "++", line});
                return;
            }
            else if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_ADD_ASSIGN,"+=",line});
                return;
            }
            else{
                tokens.push_back({TokenType::TOKEN_PLUS, "+", line});
                return;
            }
            return;
        case '~':
            tokens.push_back({TokenType::TOKEN_TILDE, "~", line});
            return;
        case '!':
            if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_NOT_EQUAL, "!=", line});
                return;
            }
            else if(peek()=='>' && peekNext()=='<'){
                advance();
                advance();
                tokens.push_back({TokenType::TOKEN_NOT_BETWEEN, "!><", line});
                return;
            }
            else{
                tokens.push_back({TokenType::TOKEN_BANG, "!", line});
                return;
            }
            return;
        case '(':
            tokens.push_back({TokenType::TOKEN_LPARAN, "(", line});
            return;
        case ')':
            tokens.push_back({TokenType::TOKEN_RPARAN, ")", line});
            return;
        case '<':
            if(peek()=='<'){
                if(peekNext()=='='){
                    advance();
                    advance();
                tokens.push_back({TokenType::TOKEN_LSHIFT_ASSIGN, "<<=", line});
                return;
                }else{
                advance();
                tokens.push_back({TokenType::TOKEN_LSHIFT, "<<", line});
                return;
                }
            }
            else if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_LTEQUAL, "<=", line});
                return;
            }
            else{
               tokens.push_back({TokenType::TOKEN_LTHAN, "<", line});
                return;
            }
            return;
        case '>':
            if(peek()=='>'){
                if(peekNext()=='='){
                    advance();
                    advance();
                    tokens.push_back({TokenType::TOKEN_RSHIFT_ASSIGN, ">>=", line});
                    return;
                }else{
                advance();
                tokens.push_back({TokenType::TOKEN_RSHIFT, ">>", line});
                }
                return;
            }
            else if(peek()=='<'){
                advance();
                tokens.push_back({TokenType::TOKEN_BETWEEN, "><", line});
                return;
            }
            else if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_GTEQUAL, ">=", line});
                return;
            }
            else{
                tokens.push_back({TokenType::TOKEN_GTHAN, ">", line});
                return;
            }
            return;
        case '^':
            if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_XOR_ASSIGN, "^=", line});
                return;
            }else{
            tokens.push_back({TokenType::TOKEN_CARET, "^", line});
            }
            return;
        case '&':
            if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_AND_ASSIGN, "&=", line});
                return;
            }
            else if(peek()=='&'){
                advance();
                tokens.push_back({TokenType::TOKEN_AND, "&&", line});
                return;
            }else{
            tokens.push_back({TokenType::TOKEN_AMPERSAND, "&", line});
            }
            return;
        case '|':
            if(peek()=='='){
                advance();
                tokens.push_back({TokenType::TOKEN_OR_ASSIGN, "|=", line});
                return;
            }
            else if(peek()=='|'){
                advance();
                tokens.push_back({TokenType::TOKEN_OR, "||", line});
                return;
            }else{
            tokens.push_back({TokenType::TOKEN_PIPE, "|", line});
            }
            return;
        case ':':
            if(peek()==':'){
                advance();
                tokens.push_back({TokenType::TOKEN_DCOLON, "::", line});
                return;
            }else{
                tokens.push_back({TokenType::TOKEN_COLON, ":", line});
                return;
            }
        case '?':
            tokens.push_back({TokenType::TOKEN_QUESTION, "?", line});
            return;
        case '.':
            if (peek() == '.') {
                advance();
                tokens.push_back({TokenType::TOKEN_RANGE, "..", line});
                return;
            }
            tokens.push_back({TokenType::TOKEN_DOT, ".", line});
            return;
        }
        
}
/*

scanNumber() scans both integers and floating point numbers.

10 is valid integer
10.2 is valid floating point

10. is invalid
.10 is invalid

*/
void Lexer::scanNumber() {

    /*
    
    Needed support for -

    1. Decimal numbers 10, 10.2, etc. (currently supported )
    2. Binary numbers 0b0100202
    3. Hexadecimals   0x40FAA
    4. Octet numbers  0o242

    5. exponents      1e10, 3.5e-4

    
    */

    int start = current - 1;
    while (!isAtEnd() && isDigit(peek())) advance(); 

    if (!isAtEnd() && peek() == '.' && isDigit(peekNext())){ 
        advance();
        while (!isAtEnd() && isDigit(peek())) advance();

        tokens.push_back({TokenType::TOKEN_DECIMAL_LIT, source.substr(start, current - start), line});
        
    }
    else {
        tokens.push_back({TokenType::TOKEN_INT_LIT, source.substr(start, current - start), line});
    }
}

// @todo : Enhance it later for Errors
void Lexer::scanCharLit() {

    /*
    
        Single character datatype currently only finds
        ASCII values.

        @todo : Changed it to the UTF-8 encoding.
    
    */
    if (errors) return; 

    if (peek() == '\'') { 
        errors = true;
        std::cerr << "Bery:Error [Line "<< line << "]: Empty Char Literal\n";
        advance(); 
        return;
    }

    char value = 0;

    if (peek() == '\\') { 
        advance(); 
        if (isAtEnd() || peek() == '\'') { 
            errors = true;
            std::cerr << "Bery:Error [Line " << line <<"]: Incomplete Escape Sequence\n";
            return;
        }

        char es = advance();
        switch (es) {

            // supported escape sequences - \n, \t, \r, \\, \0, \" and \'
            // change it for UTF-8 encoding.
            case 'n':  value = '\n'; return;
            case 't':  value = '\t'; return;
            case 'r':  value = '\r'; return;
            case '\\': value = '\\'; return;
            case '0':  value = '\0'; return;
            case '"':  value = '\"'; return;
            case '\'': value = '\''; return;
            default:
                errors = true;
                std::cerr << "Bery:Error [Line " << line <<"]: Invalid Escape Sequence\n";
                return;
        }
    } 
    
    else {
        if (peek() == '\n' || peek() == '\r') {
            errors = true;
            std::cerr << "Bery:Error [Line " << line <<"]: Newline in char literal\n";
            return;
        }
        value = advance();
    }

    if (!isAtEnd() && peek() == '\'') {        
        advance(); 
        tokens.push_back({TokenType::TOKEN_CHAR_LIT, std::string(1,value), line});
        return;
    }

    bool foundClosingQuote = false;
    while (!isAtEnd() && peek() != '\'' && peek() != '\n' && peek() != '\r') {
        advance();
    }
    if (!isAtEnd() && peek() == '\'') {
        advance(); 
        foundClosingQuote = true;
    }
    errors = true;

    if (foundClosingQuote) {
        std::cerr << "Bery:Error [Line " << line <<"]: Multi-character Char Literal\n";
    } else {
        std::cerr << "Bery:Error [Line " << line <<"]: Unclosed Char Literal\n";
    }
}

void Lexer::scanStringLit() {
    std::string value = "";
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') line++;
        if (peek() == '\\') {
            advance();
            if (isAtEnd()) return;
            
            char es = advance();
            switch (es) {
                case 'n':  value += '\n'; return;
                case 't':  value += '\t'; return;
                case 'r':  value += '\r'; return;
                case '\\': value += '\\'; return;
                case '0':  value += '\0'; return;
                case '"':  value += '\"'; return;
                case '\'': value += '\''; return;
                default:
                    errors = true;
                    std::cerr << "Bery:Error [Line " << line <<"]: Invalid escape sequence in string\n";
                    value += es; 
                    return;
            }
        } else {
            value += advance();
        }
    }
    if (isAtEnd()) {
        errors = true;
        std::cerr << "Bery:Error [Line " << line <<"]: Unclosed string literal\n";
        return;
    }

    advance(); 
    tokens.push_back({TokenType::TOKEN_STRING_LIT, value, line});
}
void Lexer::scanIdentifierOrKeyword() {
    int start = current - 1;
    while (!isAtEnd() && isAlphaNumeric(peek())) advance();
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back({checkKeyword(lexeme), lexeme, line});
}


void Lexer::skipWhitespaces() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') advance();
        else if (c == '\n') {
            line++;
            advance();
        }
        else break;
    }
}

void Lexer::skipComments(bool isMLC){
    if(isMLC){
        while(!isAtEnd()){
            if(peek()=='!' && peekNext()=='-'){
                advance();
                advance();
                if(peek()=='-'){
                    advance();
                    return;
                }
            }
            if(peek()=='\n'){line++;}
            advance();
        }
        errors=true;
        std::cerr<<"Bery:Error [Line " << line <<"]: Unclosed Comments\n";
        
    }
    else{
        while(!isAtEnd() && peek()!='\n'){advance();}

    }
}

TokenType Lexer::checkKeyword(const std::string& lexeme) {

    // O(1) lookup in the hashmap to check keyword;
    auto it = keywords.find(lexeme); 
    if (it != keywords.end()) return it->second;
    return TokenType::TOKEN_IDENT;
}

// @helpers
bool Lexer::isAtEnd() { return current >= (int) source.size(); }
bool Lexer::isAlphaNumeric(char c) {return isAlpha(c) || isDigit(c);}
bool Lexer::isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    // it makes identifiers to follow -
    // [a-zA-Z_][a-zA-Z0-9_]

}
bool Lexer::isDigit(char c) {return c >= '0' && c <= '9';}
char Lexer::advance() {return source[current++];}
char Lexer::peek() {return source[current];}
char Lexer::peekNext() {

    // Bound check added if lexer try to perform maximal 
    // munch at the end of the file.
    if(source.size() > current + 1) 
        return source[current + 1];
    return '\0';
}

// @todo : change it later for better Errors;
bool Lexer::hasErrors() {return errors;}