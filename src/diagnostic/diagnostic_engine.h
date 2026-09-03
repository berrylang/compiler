#pragma once
#include <vector>
#include <string>
#include "diagnostic.h"

class DiagnosticEngine {
public:
    DiagnosticEngine(const std::string& source, const std::string& filename);

    void report(const std::string& code, int line, int column, const std::string& lexeme, const std::string& context = "");

    bool hasErrors() const;
    bool hasWarnings() const;
    void printAll();

private:
    std::string filename;
    std::vector<Diagnostic> diagnostics;
    std::vector<std::string> sourceLines;
    int errorCount = 0;
    int warningCount = 0;

    void splitSource(const std::string& source);
    void printOne(const Diagnostic& d);
    void printStatsBox();
};