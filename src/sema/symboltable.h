#pragma once

/*

   Bery Symbol Table,

   It is a scoped key-value store, which stores every declared variable, it's datatype, whether it is constant and 
   whether it's initialized or not.

   Internally it's a stack of hashmaps. 
      Each '{' pushes a new scope
      lookup tranverse stack from top to bottom.
      and each '}' pops it.
   
   Functions and classes are declared at global scope and Never popped,
   since Bery allows forward references to them from anywhere in file

   @todo : add resolvedType for IR generator

*/
#include <string>
#include <unordered_map>
#include <vector>

enum class SymbolType {
   VARIABLE, FUNCTION, CLASS
};

struct Symbol {

   SymbolType symbolType = SymbolType::VARIABLE; // default set to variable
   std::string name;

   // declared Bery type for variables,
   // return type for functions,
   // class name itself for classes, because ofc classes are user defined datatypes.
   std::string type;
   std::vector<std::string> parameterTypes; // for functions only.


   bool isConst = false;
   bool isInitialized = false;
   
   std::vector<int> arrayDimensions;
   int arraySize = 0;
   std::string llvmRegister;
   std::string llvmAllocType;
   int line = 0;
};


class SymbolTable {
public:
   SymbolTable();
   void pushScope();
   void popScope();

   void add(const std::string& name, Symbol sym);

   void addVariable(const std::string& name, const std::string& type, bool isConst, bool isInitialized, int line, std::vector<int> dims = {});
   void addFunction(const std::string& name, const std::string& returnType, std::vector<std::string> parameterTypes, int line);
   void addClass(const std::string& name, int line);

   bool exists(const std::string& name);
   bool existsInCurrentScope(const std::string& name);
   bool existsAsType(const std::string& name, SymbolType symbolType);
   Symbol& get(const std::string& name);

private:
   std::vector<std::unordered_map<std::string, Symbol>> scopes;
};