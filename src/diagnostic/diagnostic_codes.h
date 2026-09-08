#pragma once
#include <string>
#include <unordered_map>
#include "diagnostic.h"

struct DiagnosticInfo {
    Severity severity;
    std::string messageTemplate;
    std::string hint;
};

static const std::unordered_map<std::string, DiagnosticInfo> DiagnosticRegistry = {

    // adding instructions - 
    // we have to add like this -
    //
    //
    //      {errorcode, {severity, msg, hint} }
    // 
    // hint should be basic, and targeted only for that specific error/warning.
    //
    //


    // @LEXER ERRORS and WARNINGS
    { "ERROR009", { Severity::ERROR, "Empty char literal", "put a character between the quotes" } },
    { "ERROR012", { Severity::ERROR, "Newline in char literal", "char literals cannot span multiple lines" } },
    { "ERROR010", { Severity::ERROR, "Incomplete escape sequence", "finish the escape sequence, e.g. '\\\\n'" } },
    { "ERROR011", { Severity::ERROR, "Invalid escape sequence '\\{}'", "use one of: \\n \\t \\r \\\\ \\0 \\\" \\'" } },
    { "ERROR013", { Severity::ERROR, "Multi-character char literal", "char literals must hold exactly one character" } },
    { "ERROR014", { Severity::ERROR, "Unclosed char literal", "add a closing '" } },
    { "ERROR015", { Severity::ERROR, "Invalid escape sequence '\\{}' in string", "use one of: \\n \\t \\r \\\\ \\0 \\\" \\'" } },
    { "ERROR016", { Severity::ERROR, "Unclosed string literal", "add a closing \"" } },
    { "ERROR017", { Severity::ERROR, "Unclosed comment", "add '!--' to close the comment block" } },


    // @PARSER ERRORS and WARNINGS
    { "ERROR001", { Severity::ERROR, "Expected ';' after {}", "put ';' symbol right after {}" } },
    { "ERROR002", { Severity::ERROR, "Expected '}' after {}", "add '}' to close {}" } },
    

    // @SEMA ERRORS and WARNINGS
    { "ERROR003", { Severity::ERROR, "Undefined variable '{}'", "declare '{}' before using it" } },
    { "ERROR004", { Severity::ERROR, "'{}' already declared in this scope", "rename this or remove the earlier declaration" } },
    { "ERROR005", { Severity::ERROR, "'{}' used outside of a loop or switch", "use {} only inside a loop/switch body" } },
};