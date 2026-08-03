#include "../codegen.h"
#include <string>
#include <iomanip>
#include "../../parser/ast/functions.h"


std::string CodeGen::genBREPrintCall(ASTNode* node, std::ostream& outputStream) {
    auto* call = static_cast<CallExprNode*>(node);
    const std::string& callee = call->callee;

    auto emitPrint = [&](ASTNode* arg) {
        std::string argType = arg->resolvedType;

        if (argType.empty() || argType == "unknown") {
        switch (arg->type) {
            case NodeType::INT_LIT:     
                argType = "int";    
                break;
            case NodeType::DECIMAL_LIT: 
                argType = "double";  
                break;
            case NodeType::STRING_LIT:  
                argType = "string"; 
                break;
            case NodeType::BOOL_LIT:    
                argType = "bool";   
                break;
            case NodeType::CHAR_LIT:    
                argType = "char";   
                break;
            default:                    
                argType = "int";    
                break;
        }
    }
        std::string sym, llvmT;
        if      (argType == "int")    { sym = "__bery_print_int";    llvmT = "i32"; }
        else if (argType == "bigint") { sym = "__bery_print_bigint"; llvmT = "i64"; }
        else if (argType == "float")  { sym = "__bery_print_float";  llvmT = "float"; }
        else if (argType == "double") { sym = "__bery_print_double"; llvmT = "double"; }
        else if (argType == "bool")   { sym = "__bery_print_bool";   llvmT = "i1"; }
        else if (argType == "char")   { sym = "__bery_print_char";   llvmT = "i8"; }
        else if (argType == "string") { 
            sym = "__bery_print_string"; 
            llvmT = "i8*"; 
        }
        else { sym = "__bery_print_int";    llvmT = "i32"; }

        llvm.__declareExtern("declare void @" + sym + "(" + llvmT + ")", sym);
        std::string argReg = genExpression(arg, argType, outputStream);
        llvm.__emitCall("void", sym, {{llvmT, argReg}}, outputStream);
    };

    if (callee == "print") {
        emitPrint(call->arguments[0].get());
        return "0";
    }

    if (callee == "println") {
        if (!call->arguments.empty()) {
            emitPrint(call->arguments[0].get());
        }
        llvm.__declareExtern("declare void @bery_println()", "bery_println");
        llvm.__emitCall("void", "bery_println", {}, outputStream);
        return "0";
    }
    struct InputMapping {
        const char* callee;
        const char* sym;
        const char* llvmRet;
    };
    static const InputMapping inputMap[] = {
        {"inputInt",    "__bery_input_int",    "i32"},
        {"inputBigInt", "__bery_input_bigint", "i64"},
        {"inputFloat",  "__bery_input_float",  "float"},
        {"inputDouble", "__bery_input_double", "double"},
        {"inputBool",   "__bery_input_bool",   "i1"},
        {"inputChar",   "__bery_input_char",   "i8"},
        {"inputString", "bery_input",          "i8*"},
    };

    for (auto& m : inputMap) {
        if (callee == m.callee) {
            llvm.__declareExtern(
                std::string("declare ") + m.llvmRet + " @" + m.sym + "(i8*)",
                m.sym
            );
            std::string promptReg = genExpression(call->arguments[0].get(), "string", outputStream);
            return llvm.__emitCall(m.llvmRet, m.sym, {{"i8*", promptReg}}, outputStream);
        }
    }

    return "0";
}
