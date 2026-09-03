#include "diagnostic_engine.h"
#include "diagnostic_codes.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

static const std::string COL_RED    = "\033[31m";
static const std::string COL_YELLOW = "\033[33m";
static const std::string COL_ORANGE = "\033[38;5;208m";
static const std::string COL_WHITE  = "\033[37m";
static const std::string COL_RESET  = "\033[0m";
static const std::string COL_GREEN  = "\033[32m";

DiagnosticEngine::DiagnosticEngine(const std::string& source, const std::string& filename)
    : filename(filename) {
    splitSource(source);
}
void DiagnosticEngine::splitSource(const std::string& source) {
    std::stringstream ss(source);
    std::string line;
    while (std::getline(ss, line)) sourceLines.push_back(line);
}

static std::string formatTemplate(const std::string& tmpl, const std::string& ctx) {
    size_t pos = tmpl.find("{}");
    if (pos == std::string::npos || ctx.empty()) return tmpl;
    std::string out = tmpl;
    out.replace(pos, 2, ctx);
    return out;
}

void DiagnosticEngine::report(const std::string& code, int line, int column, const std::string& lexeme, const std::string& context) {
    auto it = DiagnosticRegistry.find(code);
    if (it == DiagnosticRegistry.end()) {
        std::cerr << "[internal] Unknown diagnostic code: " << code << "\n";
        return;
    }

    Diagnostic d;
    d.code = code;
    d.severity = it->second.severity;
    d.line = line;
    d.column = column;
    d.lexeme = lexeme;
    d.context = context;

    diagnostics.push_back(d);
    if (d.severity == Severity::ERROR) errorCount++;
    else warningCount++;
}

bool DiagnosticEngine::hasErrors() const { return errorCount > 0; }
bool DiagnosticEngine::hasWarnings() const { return warningCount > 0;}

void DiagnosticEngine::printOne(const Diagnostic& d) {
    const auto& info = DiagnosticRegistry.at(d.code);
    std::string color = (d.severity == Severity::ERROR) ? COL_RED : COL_YELLOW;

    std::string message = formatTemplate(info.messageTemplate, d.context);
    std::cout << color << filename << " [" << d.code << "] " << d.line << ":" << d.column << ": " << message << COL_RESET << "\n\n";

    int idx = d.line - 1;
    int startIdx = std::max(0, idx - 1);
    int endIdx   = std::min((int)sourceLines.size() - 1, idx + 1);

    for (int i = startIdx; i <= endIdx; ++i) {
        std::cout << COL_WHITE << std::setw(2) << std::setfill('0') << (i + 1) << " | " << sourceLines[i] << COL_RESET << "\n";

        if (i == idx) {
            int caretLen = d.lexeme.empty() ? 1 : (int)d.lexeme.length();
            std::cout << "   | " << std::string(std::max(0, d.column - 1), ' ') << color << std::string(caretLen, '^') << COL_RESET << "\n";
        }
    }

    std::cout << "\n";
    if (!info.hint.empty()) {
        std::string hint = formatTemplate(info.hint, d.context);
        std::cout << COL_GREEN << " Hint : " << hint << COL_RESET << "\n";
    }
    std::cout << "\n";
}

void DiagnosticEngine::printStatsBox() {
    int total = errorCount + warningCount;
    std::ostringstream l1, l2, l3;
    l1 << "Total Errors   : "   << std::setw(2) << std::setfill('0') << errorCount;
    l2 << "Total Warnings : " << std::setw(2) << std::setfill('0') << warningCount;
    l3 << "Total Messages : " << std::setw(2) << std::setfill('0') << total;

    size_t width = std::max({l1.str().size(), l2.str().size(), l3.str().size()}) + 2;

    auto padRow = [&](const std::string& text) {
        std::cout << COL_ORANGE << "| " << text << std::string(width - text.size() - 1, ' ') << " |" << COL_RESET << "\n";
    };

    std::cout << COL_ORANGE << "+" << std::string(width + 1, '-') << "+" << COL_RESET << "\n";
    padRow(l1.str());
    padRow(l2.str());
    padRow(l3.str());
    std::cout << COL_ORANGE << "+" << std::string(width + 1, '-') << "+" << COL_RESET << "\n";
}

void DiagnosticEngine::printAll() {
    if (diagnostics.empty()) return;
    for (const auto& d : diagnostics) printOne(d);
    printStatsBox();
}