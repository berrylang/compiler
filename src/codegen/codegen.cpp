#include "codegen.h"

/*
   
   Bery IR Code Generator,

   it tranverse the AST and emits LLVM IR text directly into an output stream.
   it operates in three passes:
      @global pass - emtting global variables, arrays, functions signaturenatures,extern declarations,
      @functions pass - emits each func body as an LLVM functions.
      @run{} pass - emits @main() which calls 'bery_runtime_startup()', runs the run block statement, and then calss 'bery_runtime_shutdown()'

*/

#include "../parser/ast/programnode.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/literals.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/expressions.h"
#include "../parser/ast/blocknode.h"
#include "../parser/ast/functions.h"
#include "../sema/symboltable.h"
#include <fstream>
#include <iomanip>
#include <functional>

CodeGen::CodeGen(ASTNode* root, SymbolTable& symbolTable)
   : root(root), symbolTable(symbolTable){}

void CodeGen::generate(const std::string& outputPath) {
    auto* program = static_cast<ProgramNode*>(root);
    
    std::ostringstream globalsOutputStream;
    globalsOutputStream <<"declare double @llvm.pow.f64(double, double)\n";
    globalsOutputStream <<"declare void @bery_runtime_startup()\n";
    globalsOutputStream <<"declare void @bery_runtime_shutdown()\n";

    std::vector<ASTNode*> classNodes;
    for (auto& node : program->globals)
        if (node->type == NodeType::CLASS_DEF) classNodes.push_back(node.get());

    std::vector<ASTNode*> orderedClasses;
    std::unordered_set<std::string> emittedClasses;
    std::function<void(ASTNode*)> emitClassInOrder = [&](ASTNode* n) {
        auto* cls = static_cast<ClassDefNode*>(n);
        if (emittedClasses.count(cls->name)) return;
        if (!cls->parentName.empty()) {
            for (auto* p : classNodes) {
                if (static_cast<ClassDefNode*>(p)->name == cls->parentName) {
                    emitClassInOrder(p);
                    break;
                }
            }
        }
        emittedClasses.insert(cls->name);
        orderedClasses.push_back(n);
    };
    for (auto* n : classNodes) emitClassInOrder(n);

    for (auto& node : program->globals) {
        if (node->type == NodeType::FUNC_DEF) {
            auto* func = static_cast<FunctionDefNode*>(node.get());
            CodeGenFunctionSignature signature;
            signature.returnType = func->returnType;
            for (auto& p : func->parameters) signature.parameterTypes.push_back(p.first);
            functions[func->name] = signature;
            
            genFuncDef(node.get(), globalsOutputStream);
        }
        else if (node->type == NodeType::EXTERN_DECL) {
            auto* extern_node = static_cast<ExternDeclNode*>(node.get());
            CodeGenFunctionSignature signature;
            signature.returnType = extern_node->returnType;

            std::vector<std::string> parameterLLVMTypes;
            for (auto& p : extern_node->parameters) {
                signature.parameterTypes.push_back(p.first);
                parameterLLVMTypes.push_back(llvmType(p.first));
            }
            functions[extern_node->name] = signature;
            globalsOutputStream << llvm.__formatDeclare(llvmType(extern_node->returnType), extern_node->name, parameterLLVMTypes) <<"\n";
        }
    }

    for (auto* n : orderedClasses) genClassDecl(n);

    for (auto& node : program->globals) {
        if (node->type == NodeType::VAR_DECL) {
            auto* decl = static_cast<VarDeclNode*>(node.get());
            std::string lt = llvmType(decl->varType);
            std::string initVal = extractConstant(decl->value.get());
            
            symbolTable.get(decl->name).llvmRegister = llvm.__globalRef(decl->name);
            symbolTable.get(decl->name).llvmAllocType = lt;             
            llvm.__emitGlobalVar(decl->name, lt, initVal, globalsOutputStream);
        }
        else if (node->type == NodeType::ENUM_DECL) {
            auto* enumDecl = static_cast<EnumDeclNode*>(node.get());
            int currentValue = 0;
            
            for (const auto& val : enumDecl->values) {
                std::string mangledName = enumDecl->name + "." + val;
                std::string lt = "i32";
                
                symbolTable.get(mangledName).llvmRegister = llvm.__globalRef(mangledName);
                symbolTable.get(mangledName).llvmAllocType = lt;
                llvm.__emitGlobalVar(mangledName, lt, std::to_string(currentValue++), globalsOutputStream);
            }
        }
        else if (node->type == NodeType::ARRAY_DECL) {
            auto* decl = static_cast<ArrayDeclNode*>(node.get());
            if (decl->dimensions.size() == 1 && decl->dimensions[0] == -1) {
                llvm.__declareExternFn("i8*", "bery_array_new", {"i64"});
                std::string memoryReg = llvm.__globalRef(decl->name, "_slot");
                symbolTable.get(decl->name).llvmRegister = memoryReg;
                symbolTable.get(decl->name).llvmAllocType = "i8*";
                llvm.__emitGlobalVar(decl->name + "_slot", "i8*", "null", globalsOutputStream);
                continue;
            }
            symbolTable.get(decl->name).llvmRegister = llvm.__globalRef(decl->name);
            std::string lt= llvmType(decl->elementType);
            std::string arrType =llvm.__nestedArrayType(lt, decl->dimensions);
            symbolTable.get(decl->name).llvmAllocType = arrType;
            std::string initVal;
            if (decl->initializers.empty()) {
                initVal = "zeroinitializer";
            } else {
                std::function<std::string(int, int)> buildNestedInit = 
                [&](int dimIndex, int flatOffset) -> std::string {
                    if (dimIndex == decl->dimensions.size() - 1) {
                        std::string res = "[";
                        int size = decl->dimensions[dimIndex];
                        for (int i = 0; i < size; ++i) {
                            int idx = flatOffset + i;
                            std::string val = (idx < (int)decl->initializers.size()) ? extractConstant(decl->initializers[idx].get()) : "0";
                            res += lt + " " + val;
                            if (i + 1 < size) res += ", ";
                        }
                        res += "]";
                        return res;
                    }

                    std::string res = "[";
                    int size = decl->dimensions[dimIndex];
                    int childFlatSize = 1;
                    for (size_t i = dimIndex + 1; i < decl->dimensions.size(); ++i) {
                        childFlatSize *= decl->dimensions[i];
                    }
                    std::string childType = lt;
                    for (int i = decl->dimensions.size() - 1; i > dimIndex; --i) {
                        childType = "[" + std::to_string(decl->dimensions[i]) + " x " + childType + "]";
                    }

                    for (int i = 0; i < size; ++i) {
                        res += childType + " " + buildNestedInit(dimIndex + 1, flatOffset + (i * childFlatSize));
                        if (i + 1 < size) res += ", ";
                    }
                    res += "]";
                    return res;
                };

                initVal = buildNestedInit(0, 0);
            }
            llvm.__emitGlobalVar(decl->name, arrType, initVal, globalsOutputStream);
        }
    }

    std::ostringstream body;
    llvm.__emitFunctionHeader("i32", "main", {}, body);
    body <<"    call void @bery_runtime_startup()\n";
    for (auto& clPair : classLayouts) {
        ClassLayout& cl = clPair.second;
        int nameLen = (int)cl.name.length() + 1;
        std::string destructorArg = "i8* null";
        if (cl.hasDestructor) {
            std::string ownerStructType = classLayouts.at(cl.destructorOwner).llvmStructType;
            std::string dtorReg = llvm.__emitDestructorBitcast(ownerStructType, cl.destructorOwner, body);
            destructorArg = "i8* " + dtorReg;
        }
        llvm.__emitTypeRegisterCall(cl.name, nameLen, (long long)cl.instanceSize, destructorArg, body);
    }
    pushGCScope();
    for (auto& node : program->globals) {
        if (node->type == NodeType::ARRAY_DECL) {
            auto* decl = static_cast<ArrayDeclNode*>(node.get());
            if (decl->dimensions.size() == 1 && decl->dimensions[0] == -1) {
                std::string arrReg = llvm.__emitCall("i8*", "bery_array_new", {{"i64", "4"}}, body);
                llvm.__emitStore("i8*", arrReg, llvm.__globalRef(decl->name, "_slot"),body);
            }
        }
    }

    if (program->runBlock) {
        symbolTable.pushScope();
        for (auto& node : program->runBlock->statements) {
            genStatement(node.get(), body);
        }
        symbolTable.popScope();
    }
    int rootsInMain = popGCScope();
    emitGCPops(rootsInMain, body);
    body <<"    call void @bery_runtime_shutdown()\n";
    llvm.__emitBr("main_end", body);
    llvm.__emitLabel("main_end", body);
    llvm.__emitReturn("i32", "0", body);
    llvm.__emitFunctionFooter(body);

    std::ofstream outputStream(outputPath);
    outputStream << llvm.__STRUCTURE_declares.str();
    outputStream << globalsOutputStream.str();
    outputStream << llvm.__BRE_declares.str();
    outputStream << llvm.__GLOBAL_STRINGS.str() <<"\n";
    outputStream << body.str();
}