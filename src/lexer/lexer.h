#pragma once 

/*

LEXER DEFINITION

This is header file served for Lexer's definition.
it contains every helper functions which eventually helps the 'tokanize()' method.

*/

#include <string>
#include <vector>
#include "token.h"
#include "../diagnostic/diagnostic_engine.h"

class Lexer {

    /*
    
    Remaining things:

    1. Source location inside the token, which will eventually used by diagnostic to print the errors.
    2. Diagnostic Intregration, it shouldn't print errors (std::cerr) itself.

    3. Unicode support - Lexer only supports only ASCII for now, changed needed for UTF-8 Support.
    
    4. Support for Decimal, Octate, Hexadecimal and Binary Numbers
    5. Numerical overflow detection (such as 999999999999999)

    6. Lexer test suite

    7. Benchmark testing

    
    
    */
public:
    Lexer(const std::string& source, DiagnosticEngine& diag);
    std::vector<Token> tokanize();


    // removed it. but let it be here for now -
    // bool hasErrors();

private:
    // @tokens data
    std::string source;
    int current;
    int line;
    int col;
    int startColumn;
    int startLine;
    // bool errors;
    std::vector<Token> tokens;
    DiagnosticEngine& diag;

    void emit(TokenType type, const std::string& lexeme);
    void bumpLine();
    // @pointers inside the source
    char advance();
    char peek();
    char peekNext();
    bool isAtEnd();

    // @skip unnessesory
    void skipWhitespaces();
    void skipComments(bool isMLC);

    // @actual scanning
    void scanToken();
    void scanIdentifierOrKeyword();
    void scanNumber(); 
    void scanCharLit(); 
    void scanStringLit();

    // @identification of alpha-numeric data
    bool isDigit(char c);
    bool isAlpha(char c);
    bool isAlphaNumeric(char c);

    // @distinguish between tokens
    TokenType checkKeyword(const std::string& lexeme);
};