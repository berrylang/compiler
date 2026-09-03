#pragma once

/*

TOKENS LIST

this file serves only one perpose - lists of all TOKENS in the language.
if you are adding some functionalities, first check if it's token is present here;
if not then add it in respected block.


current tokens = 93

*/

#include <string>

enum class TokenType {
    // @type
    TOKEN_INT, TOKEN_BOOL, TOKEN_FLOAT, TOKEN_BIGINT, TOKEN_DOUBLE, TOKEN_CHAR, TOKEN_STRING,
    
    // @literals
    TOKEN_INT_LIT, TOKEN_DECIMAL_LIT, TOKEN_CHAR_LIT, TOKEN_STRING_LIT, TOKEN_TRUE, TOKEN_FALSE ,

    // @specials
    TOKEN_IDENT, TOKEN_EOF,
    
    // @assignments
    TOKEN_EQUAL, 
    TOKEN_ADD_ASSIGN, TOKEN_SUB_ASSIGN, 
    TOKEN_MUL_ASSIGN, TOKEN_DIV_ASSIGN,
    TOKEN_DSTAR_ASSIGN, TOKEN_MODULE_ASSIGN,
    TOKEN_AND_ASSIGN, TOKEN_OR_ASSIGN,TOKEN_XOR_ASSIGN,
    TOKEN_LSHIFT_ASSIGN,TOKEN_RSHIFT_ASSIGN,
    
    // @symbols
    TOKEN_SEMICOLON,
    TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET,  TOKEN_RBRACKET,
    TOKEN_LPARAN, TOKEN_RPARAN,
    TOKEN_COMMA, 
     
    // @operators
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_FSLASH,TOKEN_POWER, TOKEN_PERCENT,
    TOKEN_CARET, TOKEN_AMPERSAND, TOKEN_PIPE, TOKEN_BETWEEN, TOKEN_NOT_BETWEEN,
    TOKEN_INC, TOKEN_DEC, TOKEN_BANG, TOKEN_TILDE,
    TOKEN_LSHIFT, TOKEN_RSHIFT,
    TOKEN_EQUAL_EQUAL,TOKEN_NOT_EQUAL,TOKEN_LTHAN,TOKEN_GTHAN,TOKEN_GTEQUAL,TOKEN_LTEQUAL,
    TOKEN_AND, TOKEN_OR,
    TOKEN_QUESTION, TOKEN_COLON,
    TOKEN_NULL,
    
    // @conditionals @keywords
    TOKEN_IF, TOKEN_ELSE,
    TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT,
    
    // @special-controlflow @keywords
    TOKEN_BREAK, TOKEN_CONTINUE, TOKEN_PASS,  
    
    // @loops @keywords
    TOKEN_FOR,TOKEN_IN, TOKEN_WHILE, TOKEN_DOWHILE, 

    //@function
    TOKEN_FUNC,TOKEN_RETURN,TOKEN_ARROW, TOKEN_DOT, TOKEN_RANGE, 

    //@class
    TOKEN_CLASS, TOKEN_ATTRIBUTES, TOKEN_METHODS, TOKEN_DCOLON, TOKEN_NEW, TOKEN_PUBLIC, TOKEN_PRIVATE, TOKEN_PROTECTED,TOKEN_REF, TOKEN_SUPER,

    // @blocks @keywords
    TOKEN_ENUM, TOKEN_RUN, 
    
    // @spical-statementds @keywords
    TOKEN_IMPORT, TOKEN_EXTERN, TOKEN_CONST, TOKEN_DELETE

};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    
};
