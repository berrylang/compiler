#pragma once
#include <string>

enum class Severity { ERROR, WARNING };

struct Diagnostic {
    std::string code;
    Severity severity;
    int line;
    int column;
    std::string lexeme;
    std::string context;
    std::string hint;
};