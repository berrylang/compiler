#pragma once

/*
   
   Bery IR Code Generator,

   it tranverse the AST and emits LLVM IR text directly into an output stream.
   it operates in three passes:
      @global pass - emtting global variables, arrays, functions signatures,extern declarations,
      @functions pass - emits each func body as an LLVM functions.
      @run{} pass - emits @main() which calls 'bery_runtime_startup()', runs the run block statement, and then calss 'bery_runtime_shutdown()'

*/
#include <fstream>
#include <sstream>
#include <string>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include "../parser/ast/node.h"
#include "../sema/symboltable.h"
#include "../parser/ast/classes.h"
#include "../llvm/LLVMHelper.h"


// @function signature data
struct CodeGenFunctionSignature {
   std::string returnType;
   std::vector<std::string> parameterTypes;
};

class CodeGen {
public:
   CodeGen(ASTNode* root, SymbolTable& symbolTable);
   // @main Codegen function, which traverse the AST after sema
   void generate(const std::string& outputPath);

private:
   // @data - AST, SymbolTable
   ASTNode* root;
   SymbolTable& symbolTable;
   LLVMHelper llvm;
   

   // @data, and literals
   std::string llvmType(const std::string& beryType);
   std::string extractConstant(ASTNode* node);


   // @controlflow tracking
   std::vector<std::string> breakTracker;
   std::vector<std::string> continueTracker;

   // @functions
   std::unordered_map<std::string, CodeGenFunctionSignature> functions;
   std::string currentFuncReturn;
   void genFuncDef(ASTNode* node, std::ostream& outputStream);
   void genReturnStmt(ASTNode* node, std::ostream& outputStream);
   
   // @garbage collector
   std::stack<int> gcRootScopeStack;
   int gcRootCounter = 0;
   void emitGCPush(const std::string& allocaReg, const std::string& llvmType, std::ostream& outputStream);
   void emitGCPops(int count, std::ostream& outputStream);
   void pushGCScope();
   int popGCScope();
   
   // @BRE
   std::string genBREPrintCall(ASTNode* node, std::ostream& outputStream);
   
   // @controlflow
   void genBlock(ASTNode* node, std::ostream& outputStream);
   void genStatement(ASTNode* statement, std::ostream& outputStream);
   void genBreakStmt(ASTNode* node, std::ostream& outputStream);
   void genContinueStmt(ASTNode* node, std::ostream& outputStream);
   
   // @declarations
   void genVarDecl(ASTNode* node, std::ostream& outputStream);
   void genArrayDecl(ASTNode* node, std::ostream& outputStream);
   
   // @conditionals
   void genIfStmt(ASTNode* node, std::ostream& outputStream);
   void genSwitchStmt(ASTNode* node, std::ostream& outputStream);

   // @loops
   void genDoWhileStmt(ASTNode* node, std::ostream& outputStream);
   void genWhileStmt(ASTNode* node, std::ostream& outputStream);
   void genForStmt(ASTNode* node, std::ostream& outputStream);
   void genForInStmt(ASTNode* node, std::ostream& outputStream);
   
   // @expression helpers
   std::string genExpression(ASTNode* node, const std::string& expectedType, std::ostream& outputStream);
   std::string genLiteral(ASTNode* node, const std::string& expectedType, std::ostream& outputStream);
   std::string genIdentExpr(ASTNode* node, const std::string& expectedType, std::ostream& outputStream);
   std::string genUnaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& outputStream);
   std::string genBetweenExpr(ASTNode* node, std::ostream& outputStream);
   std::string genBinaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& outputStream);
   std::string genTernaryExpr(ASTNode* node, std::ostream& outputStream);
   std::string genAssignmentExpr(ASTNode* node, std::ostream& outputStream);
   std::string genCastExpr(ASTNode* node, std::ostream& outputStream);
   std::string genIndexExpr(ASTNode* node, std::ostream& outputStream);
   std::string genCallExpr(ASTNode* node, std::ostream& outputStream);
   

   // @oop
   struct ClassLayout {
      std::string name;
      std::vector<std::pair<std::string, std::string>> fields;
      std::string llvmStructType; 
      std::unordered_map<std::string, int> fieldIndex;
      size_t instanceSize = 0;
      std::vector<ASTNode*> fieldInitializers;
      bool hasConstructor = false;
      bool hasDestructor = false;
   };
   std::unordered_map<std::string, ClassLayout> classLayouts;
   void genClassDecl(ASTNode* node);
   std::string genNewExpr(ASTNode* node, std::ostream& outputStream);
   std::string currentClassName = "";
   std::string currentSelfRef = "";
   std::string genFieldChainAddressing(const std::vector<std::string>& parts, std::ostream& outputStream, std::string& outputType);
   std::string genFieldChainFromAddress(std::string currentPointer, std::string currentType, const std::vector<std::string>& parts, std::ostream& outputStream, std::string& outputType);
};
