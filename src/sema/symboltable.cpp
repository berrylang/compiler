/*

    SymbolTable actual implementation, 
    this file defines logic of each process performed on symbol table.

    1. push scope
    2. pop scope
    3. declare the sybmol
    4. check if it is present or not
    5. get the symbol (fetching)

    
*/

#include "symboltable.h"
#include <stdexcept>
#include <iostream>

SymbolTable::SymbolTable() {
    scopes.push_back(std::unordered_map<std::string, Symbol>());
}

void SymbolTable::pushScope() {
    scopes.push_back(std::unordered_map<std::string, Symbol>());
}

void SymbolTable::popScope() {
    if (scopes.size() > 1) { 
        scopes.pop_back();
    }
}

void SymbolTable::add(const std::string& name, Symbol sym) {
    sym.name = name;
    scopes.back()[name] = std::move(sym);
}

void SymbolTable::addVariable(const std::string& name, const std::string& type, bool isConst, bool isInitialized, int line, std::vector<int> dims) {
    Symbol sym;
    sym.symbolType =SymbolType::VARIABLE;
    sym.type = type;
    sym.isConst = isConst;
    sym.isInitialized = isInitialized;
    sym.line = line;
    sym.arrayDimensions = std::move(dims);
    sym.arraySize = sym.arrayDimensions.empty() ? 0:1;
    for (int d : sym.arrayDimensions) sym.arraySize *= d;
    add(name, std::move(sym));
}

void SymbolTable::addFunction(const std::string& name, const std::string& returnType, std::vector<std::string> parameterTypes, int line) {
    Symbol sym;
    sym.symbolType = SymbolType::FUNCTION;
    sym.type = returnType;
    sym.parameterTypes = std::move(parameterTypes);
    sym.line = line;
    add(name, std::move(sym));
}

void SymbolTable::addClass(const std::string& name, int line) {
    Symbol sym;
    sym.symbolType = SymbolType::CLASS;
    sym.type = name;
    sym.line = line;
    add(name, std::move(sym));
}
bool SymbolTable::existsInCurrentScope(const std::string& name) {
    return scopes.back().find(name) != scopes.back().end();
}
bool SymbolTable::exists(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        if (it->find(name) != it->end()) {
            return true;
        }
    }
    return false;
}
bool SymbolTable::existsAsType(const std::string& name, SymbolType symbolType) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second.symbolType == symbolType;
    }
    return false;
}

Symbol& SymbolTable::get(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    std::cerr <<"Bery:'internal compiler error': symbol '" << name <<"' not found in symbol table\n";
    std::abort();
}