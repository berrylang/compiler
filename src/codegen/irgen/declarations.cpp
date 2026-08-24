#include "../codegen.h"
#include "../../parser/ast/vardecl.h"
#include "../../parser/ast/arraydeclare.h"
#include "../../parser/ast/classes.h"
#include "../../parser/ast/functions.h"
#include "../../parser/ast/classes.h"
#include "../../parser/ast/expressions.h"
#include "../../sema/symboltable.h"

void CodeGen::genClassDecl(ASTNode* node) {
    auto* cls = static_cast<ClassDefNode*>(node);

    ClassLayout layout;
    layout.name = cls->name;
    layout.parentName = cls->parentName;
    layout.llvmStructType = llvm.__structTypeName(cls->name);

    std::vector<std::string> fieldTypes;
    if (!cls->parentName.empty() && classLayouts.count(cls->parentName)) {
        ClassLayout& parentLayout = classLayouts.at(cls->parentName);
        layout.fields = parentLayout.fields;
        layout.fieldIndex = parentLayout.fieldIndex;
        layout.fieldInitializers = parentLayout.fieldInitializers;
        layout.hasConstructor = parentLayout.hasConstructor;
        layout.constructorOwner = parentLayout.constructorOwner;
        layout.hasDestructor = parentLayout.hasDestructor;
        layout.destructorOwner = parentLayout.destructorOwner;
        for (auto* declNode : parentLayout.fieldInitializers) {
            if (declNode->type == NodeType::VAR_DECL) {
                fieldTypes.push_back(llvmType(static_cast<VarDeclNode*>(declNode)->varType));
            } else {
                auto* arrDecl = static_cast<ArrayDeclNode*>(declNode);
                std::string lt = (arrDecl->dimensions.size() == 1 && arrDecl->dimensions[0] == -1)
                    ? "i8*" : llvm.__nestedArrayType(llvmType(arrDecl->elementType), arrDecl->dimensions);
                fieldTypes.push_back(lt);
            }
        }
    }

    if (cls->attributes) {
        for (size_t i = 0; i < cls->attributes->attributes.size(); ++i) {
            auto* attr = cls->attributes->attributes[i].get();
            if (attr->type == NodeType::VAR_DECL) {
                auto* field = static_cast<VarDeclNode*>(attr);
                std::string lt = llvmType(field->varType);
                layout.fieldIndex[field->name] = (int)layout.fields.size();
                layout.fields.push_back({field->varType, field->name});
                layout.fieldInitializers.push_back(field);
                fieldTypes.push_back(lt);
            } else if (attr->type == NodeType::ARRAY_DECL) {
                auto* field = static_cast<ArrayDeclNode*>(attr);
                std::string beryType = "array<" + field->elementType + ">";
                std::string lt = (field->dimensions.size() == 1 && field->dimensions[0] == -1)
                    ? "i8*" : llvm.__nestedArrayType(llvmType(field->elementType), field->dimensions);
                layout.fieldIndex[field->name] = (int)layout.fields.size();
                layout.fields.push_back({beryType, field->name});
                layout.fieldInitializers.push_back(field);
                fieldTypes.push_back(lt);
            }
        }
    }
    llvm.__emitStructType(layout.llvmStructType, fieldTypes);

    layout.instanceSize = 0;
    for (auto& field : layout.fields) {
        layout.instanceSize += (size_t)llvm.__alignOf(llvmType(field.first));
    }
    if (layout.instanceSize == 0) {
        layout.instanceSize = 1;
    }

    llvm.__emitClassNameGlobal(cls->name);
    if (cls->methods) {
        for (auto& m : cls->methods->methods) {
            auto* f = static_cast<FunctionDefNode*>(m.get());
            if (f->isConstructor) { layout.hasConstructor = true; layout.constructorOwner = cls->name; }
            if (f->isDestructor)  { layout.hasDestructor  = true; layout.destructorOwner  = cls->name; }
        }
    }
    classLayouts[cls->name] = layout;
    if (cls->methods) {
        for (auto& m : cls->methods->methods) {
            auto* func = static_cast<FunctionDefNode*>(m.get());
            std::vector<std::string> beryParamTypes;
            for (auto& p : func->parameters) beryParamTypes.push_back(p.first);
            std::string mangledName = func->isConstructor ? 
                llvm.__mangleOverload(llvm.__mangleConstructor(cls->name), beryParamTypes) : func->isDestructor ? 
                llvm.__mangleDestructor(cls->name) : llvm.__mangleOverload(llvm.__mangleMethod(cls->name, func->name), beryParamTypes);

            CodeGenFunctionSignature signature;
            signature.returnType = func->returnType;
            signature.parameterTypes.push_back(llvm.__pointerType(cls->name));
            for (auto& p : func->parameters) signature.parameterTypes.push_back(p.first);
            functions[mangledName] = signature;

            std::ostringstream methodOut;
            std::string retLT = (func->returnType == "void" || func->returnType.empty())? "void" : llvmType(func->returnType);
            currentFuncReturn = func->returnType;

            std::vector<std::pair<std::string, std::string>> params;
            std::string selfPtrType = llvm.__pointerType(layout.llvmStructType);
            params.push_back({selfPtrType, llvm.__arguementRegName("self")});
            for (auto& p : func->parameters) params.push_back({llvmType(p.first), llvm.__arguementRegName(p.second)});
            llvm.__emitFunctionHeader(retLT, mangledName, params, methodOut);

            symbolTable.pushScope();
            pushGCScope();
            currentClassName = cls->name;
            currentSelfRef = cls->attributes->selfRef;

            std::string selfReg = llvm.__emitNamedAlloca("self", selfPtrType, methodOut);
            llvm.__emitStore(selfPtrType, llvm.__arguementRegName("self"), selfReg, methodOut);

            Symbol selfSym;
            selfSym.type = cls->name;
            selfSym.isConst = false;
            selfSym.isInitialized = true;
            selfSym.llvmRegister = selfReg;
            selfSym.llvmAllocType = layout.llvmStructType + "*";
            selfSym.line = cls->line;
            symbolTable.add(cls->attributes->selfRef, selfSym);

            std::string loadedSelf = llvm.__emitLoad(layout.llvmStructType + "*", selfReg, methodOut);

            for (auto& field : layout.fields) {
                auto& beryT = field.first;
                auto& fieldName = field.second;
                ASTNode* declNode = layout.fieldInitializers[layout.fieldIndex[fieldName]];
                
                std::string lt;
                if (declNode->type == NodeType::VAR_DECL) {
                    lt = llvmType(static_cast<VarDeclNode*>(declNode)->varType);
                } else {
                    auto* arrDecl = static_cast<ArrayDeclNode*>(declNode);
                    bool isDynamic = (arrDecl->dimensions.size() == 1 && arrDecl->dimensions[0] == -1);
                    lt = isDynamic ? "i8*" : llvm.__nestedArrayType(llvmType(arrDecl->elementType), arrDecl->dimensions);
                }
                
                int idx = layout.fieldIndex[fieldName];
                std::string gepReg = llvm.__emitFieldGEP(layout.llvmStructType, loadedSelf, idx, methodOut);

                Symbol fieldSym;
                fieldSym.type = beryT;
                fieldSym.isConst = false;
                fieldSym.isInitialized = true;
                fieldSym.llvmRegister = gepReg;
                fieldSym.llvmAllocType = lt;
                fieldSym.line = cls->line;
                
                if (declNode->type == NodeType::ARRAY_DECL) {
                    auto* arrDecl = static_cast<ArrayDeclNode*>(declNode);
                    fieldSym.arrayDimensions = arrDecl->dimensions;
                    fieldSym.arraySize = 1;
                    for (int d : arrDecl->dimensions) fieldSym.arraySize *= d;
                }
                
                symbolTable.add(fieldName, fieldSym);
            }

            for (auto& p : func->parameters) {
                std::string pLT = llvmType(p.first);
                std::string pReg = llvm.__emitNamedAlloca(p.second, pLT, methodOut);

                Symbol paramSym;
                paramSym.type = p.first;
                paramSym.isConst = false;
                paramSym.isInitialized = true;
                paramSym.llvmRegister = pReg;
                paramSym.llvmAllocType = pLT;
                paramSym.line = func->line;
                symbolTable.add(p.second, paramSym);

                llvm.__emitStore(pLT, llvm.__arguementRegName(p.second), pReg, methodOut);
            }

            for (auto& statement : func->body->statements)
                genStatement(statement.get(), methodOut);

            int roots = popGCScope();
            emitGCPops(roots, methodOut);
            symbolTable.popScope();
            currentClassName = "";
            currentSelfRef = "";

            llvm.__emitDefaultReturn(retLT, methodOut);
            llvm.__emitFunctionFooter(methodOut);
            llvm.__GLOBAL_STRINGS << methodOut.str();
            currentFuncReturn = "";
        }
    }

    classLayouts[cls->name] = std::move(layout);
}


void CodeGen::genVarDecl(ASTNode* node, std::ostream& outputStream) {
    auto* decl = static_cast<VarDeclNode*>(node);
    std::string lt = llvmType(decl->varType);
    std::string memoryReg = llvm.__emitNamedAlloca(decl->name, lt, outputStream);
    Symbol sym;
    sym.symbolType = SymbolType::VARIABLE;
    sym.type = decl->varType;
    sym.isConst = decl->isConst;
    sym.isInitialized = decl->value != nullptr;
    sym.line = decl->line;
    sym.llvmRegister = memoryReg;
    sym.llvmAllocType = lt;
    symbolTable.add(decl->name, sym);
    if (decl->varType == "string" || classLayouts.count(decl->varType)) {
        emitGCPush(memoryReg, lt, outputStream);
    }
    if (!decl->value) return;
    std::string valueReg = classLayouts.count(decl->varType)? genClassCopyValue(decl->value.get(), decl->varType, outputStream): genExpression(decl->value.get(), decl->varType, outputStream);
    llvm.__emitStore(lt, valueReg, memoryReg, outputStream);
}

void CodeGen::genArrayDecl(ASTNode* node, std::ostream& outputStream) {
    auto* decl = static_cast<ArrayDeclNode*>(node);
    if (decl->dimensions.size() == 1 && decl->dimensions[0] == -1) {
        std::string memoryReg = llvm.__emitNamedAlloca(decl->name, "i8*", outputStream);
        Symbol sym;
        sym.symbolType = SymbolType::VARIABLE;
        sym.type = llvm.__arrayBeryType(decl->elementType);
        sym.isInitialized = true;
        sym.line = decl->line;
        sym.llvmRegister = memoryReg;
        sym.llvmAllocType = "i8*";
        symbolTable.add(decl->name, sym);
        if (decl->valueExpr) {
            std::string valueReg = genExpression(decl->valueExpr.get(), sym.type, outputStream);
            llvm.__emitStore("i8*", valueReg, memoryReg, outputStream);
        } else {
            llvm.__declareExternFn("i8*", "bery_array_new", {"i64"});
            std::string arrReg = llvm.__emitCall("i8*", "bery_array_new", {{"i64", "4"}}, outputStream);
            llvm.__emitStore("i8*", arrReg, memoryReg, outputStream);

            if (!decl->initializers.empty()) {
                std::string lt = llvmType(decl->elementType);
                llvm.__declareExternFn("void", "bery_array_push", {"i8*", "i8*"});
                for (auto& initVal : decl->initializers) {
                    std::string valReg = classLayouts.count(decl->elementType) ? genClassCopyValue(initVal.get(), decl->elementType, outputStream): genExpression(initVal.get(), decl->elementType, outputStream);
                    std::string boxedReg = llvm.__emitBoxValue(lt, valReg, outputStream);
                    llvm.__emitCall("void", "bery_array_push", {{"i8*", arrReg}, {"i8*", boxedReg}}, outputStream);
                }
            }
        }
        emitGCPush(memoryReg, "i8*", outputStream);
        return;
    }
    std::string lt = llvmType(decl->elementType);
    std::string arrType = llvm.__nestedArrayType(lt, decl->dimensions);

    std::string memReg = llvm.__namedReg(decl->name);
    std::string typeSig = llvm.__arrayTypeSignature(decl->elementType, (int)decl->dimensions.size());

    Symbol sym;
    sym.symbolType = SymbolType::VARIABLE;
    sym.type = typeSig;
    sym.isInitialized = !decl->initializers.empty();
    sym.line = decl->line;
    sym.llvmRegister = memReg;
    sym.llvmAllocType = arrType;
    sym.arrayDimensions = decl->dimensions;
    sym.arraySize = 1;
    for (int d : decl->dimensions) sym.arraySize *= d;
    symbolTable.add(decl->name, sym);

    outputStream <<"    " << memReg <<" = alloca " << arrType <<"\n";
    if (decl->initializers.empty()) return;

    std::string flatPtr = llvm.__emitBitcast(llvm.__pointerType(arrType), memReg, llvm.__pointerType(lt), outputStream);
    for (size_t i = 0; i < decl->initializers.size(); ++i) {
       std::string valReg = classLayouts.count(decl->elementType) ? genClassCopyValue(decl->initializers[i].get(), decl->elementType, outputStream) : genExpression(decl->initializers[i].get(), decl->elementType, outputStream);
        std::string ptrReg = llvm.__emitTypedGEP(lt, flatPtr, {{"i32", std::to_string(i)}}, false, outputStream);
        llvm.__emitStore(lt, valReg, ptrReg, outputStream);
    }
}

void CodeGen::genFuncDef(ASTNode* node, const std::string& irName, std::ostream& outputStream) {
    auto* func = static_cast<FunctionDefNode*>(node);
    std::string retLT = (func->returnType == "void") ? "void" : llvmType(func->returnType);
    currentFuncReturn = func->returnType;

    std::vector<std::pair<std::string, std::string>> params;
    for (auto& p : func->parameters) {
        params.push_back({llvmType(p.first), llvm.__arguementRegName(p.second)});
    }
    llvm.__emitFunctionHeader(retLT, irName, params, outputStream);

    symbolTable.pushScope();
    pushGCScope();
    for (auto& param : func->parameters) {
        std::string parameterType = llvmType(param.first);
        std::string parameterName = param.second;
        std::string memoryReg = llvm.__emitNamedAlloca(parameterName, parameterType, outputStream);
        Symbol sym;
        sym.symbolType = SymbolType::VARIABLE;
        sym.type = param.first;
        sym.isInitialized = true;
        sym.line = func->line;
        sym.llvmRegister = memoryReg;
        sym.llvmAllocType = parameterType;
        symbolTable.add(parameterName, sym);

        llvm.__emitStore(parameterType, llvm.__arguementRegName(parameterName), memoryReg, outputStream);
        bool isHeapTracked = param.first == "string"||classLayouts.count(param.first) || (param.first.size() > 6 && param.first.substr(0, 6) == "array<");
        if (isHeapTracked) { emitGCPush(memoryReg, parameterType, outputStream); }
    }

    for (auto& statement : func->body->statements) genStatement(statement.get(), outputStream);
    int roots = popGCScope();
    emitGCPops(roots, outputStream);
    symbolTable.popScope();
    bool endsWithReturn = false;
    if (!func->body->statements.empty() &&
        func->body->statements.back()->type == NodeType::RETURN_STMT) {
        endsWithReturn = true;
    }

    if (!endsWithReturn) {
        llvm.__emitReturn(retLT, retLT == "void" ? "" : "0", outputStream);
    }
    llvm.__emitDefaultReturn(retLT, outputStream);
    llvm.__emitFunctionFooter(outputStream);
    currentFuncReturn = "";
}


void CodeGen::genReturnStmt(ASTNode* node, std::ostream& outputStream) {
    auto* retNode = static_cast<ReturnStmtNode*>(node);
    
    std::string valueReg;
    if (retNode->value) {
        valueReg = classLayouts.count(currentFuncReturn)? genClassCopyValue(retNode->value.get(), currentFuncReturn, outputStream)
            : genExpression(retNode->value.get(), currentFuncReturn, outputStream); }
    int totalRoots = 0;
    std::stack<int> tempStack = gcRootScopeStack;
    while (!tempStack.empty()) {
        totalRoots += tempStack.top();
        tempStack.pop();
    }
    emitGCPops(totalRoots, outputStream);
    llvm.__emitReturn(llvmType(currentFuncReturn), retNode->value ? valueReg : "", outputStream);
    llvm.__emitLabel(llvm.__uniqueLabel("dead_code"), outputStream);
}