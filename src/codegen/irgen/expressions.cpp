#include "../codegen.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/literals.h"
#include "../../sema/symboltable.h"
#include "../../parser/ast/functions.h"
#include "../../parser/ast/arraydeclare.h"
#include "../../parser/ast/vardecl.h"
#include <iomanip>
#include <sstream>
#include <iostream>
#include <memory>
#include <cstdint>
#include <cstring>

static std::vector<std::string> splitDots(const std::string& s) {
    std::vector<std::string> parts;
    size_t start =0, pos;
    while ((pos = s.find('.', start)) !=std::string::npos) {
        parts.push_back(s.substr(start, pos - start));
        start = pos+1;  
    }
    parts.push_back(s.substr(start));
    return parts;
}

std::string CodeGen::genLiteral(ASTNode* node, const std::string& expectedType, std::ostream& outputStream) {
    std::string lt = llvmType(expectedType);
    bool isFloat = (expectedType == "float" || expectedType == "double");

    if (node->type == NodeType::INT_LIT) {
        auto* lit = static_cast<IntLitNode*>(node);
        if (isFloat) {
            return llvm.__emitConvert("sitofp", "i32", std::to_string(lit->value), lt, outputStream);
        }
        return std::to_string(lit->value);
    }

    if (node->type == NodeType::DECIMAL_LIT) {
        auto* lit = static_cast<DecimalLitNode*>(node);
        return llvm.__formatFloatHexConstant(lit->value);
    }

    if (node->type == NodeType::BOOL_LIT) {
        return static_cast<BoolLitNode*>(node)->value ? "1" : "0";
    }

    if (node->type == NodeType::CHAR_LIT) {
        return std::to_string((int)static_cast<CharLitNode*>(node)->value);
    }

    if (node->type == NodeType::STRING_LIT) {
        auto* lit = static_cast<StringLitNode*>(node);
        std::string constExpr = llvm.__emitGlobalStringConstant(lit->value);
        llvm.__declareExternFn("i8*", "bery_string_from_literal", {"i8*"});
        return llvm.__emitCall("i8*", "bery_string_from_literal", {{"i8*", constExpr}}, outputStream);
    }

    return "0";
}
std::string CodeGen::genIdentExpr(ASTNode* node, const std::string& expectedType, std::ostream& outputStream) {
    auto* ident = static_cast<IdentNode*>(node);

    size_t dot = ident->name.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(ident->name);

        if (parts.back() == "len") {
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string headType;
            std::string headPtr = genFieldChainAddressing(headParts, outputStream, headType);
            if (headType == "string") {
                llvm.__declareExternFn("i64", "bery_string_length", {"i8*"});
                std::string strReg = llvm.__emitLoad("i8*", headPtr, outputStream);
                std::string lenReg = llvm.__emitCall("i64", "bery_string_length", {{"i8*", strReg}}, outputStream);
                return llvm.__emitConvert("trunc", "i64", lenReg, "i32", outputStream);
            } 
            if (headType.size() > 6 && headType.substr(0, 6) == "array<") {
                std::string allocType = "i8*";
                int fixedSize = -1;
                Symbol& base = symbolTable.get(headParts[0]);
                if (headParts.size() == 1) {
                    allocType = base.llvmAllocType;
                    fixedSize = base.arraySize;
                } else {
                    std::string currentType = base.type;
                    for (size_t i = 1; i < headParts.size(); ++i) {
                        ClassLayout& layout = classLayouts.at(currentType);
                        int fIdx = layout.fieldIndex.at(headParts[i]);
                        ASTNode* fieldDecl = layout.fieldInitializers[fIdx];
                        if (i == headParts.size() - 1 && fieldDecl->type == NodeType::ARRAY_DECL) {
                            auto* arrDecl = static_cast<ArrayDeclNode*>(fieldDecl);
                            if (!(arrDecl->dimensions.size() == 1 && arrDecl->dimensions[0] == -1)) {
                                allocType = llvm.__nestedArrayType(llvmType(arrDecl->elementType), arrDecl->dimensions);
                                fixedSize = 1;
                                for (int d : arrDecl->dimensions) fixedSize *= d;
                            }
                        }
                        currentType = layout.fields[fIdx].first;
                    }
                }

                if (allocType != "i8*" && fixedSize > 0) {
                    return std::to_string(fixedSize);
                } else {
                    llvm.__declareExternFn("i64", "bery_array_length", {"i8*"});
                    std::string arrReg = llvm.__emitLoad("i8*", headPtr, outputStream);
                    std::string lenReg = llvm.__emitCall("i64", "bery_array_length", {{"i8*", arrReg}}, outputStream);
                    return llvm.__emitConvert("trunc", "i64", lenReg, "i32", outputStream);
                }
            }
        }

        std::string chainType;
        std::string chainPtr = genFieldChainAddressing(parts, outputStream, chainType);
        std::string flt = llvmType(chainType);
        std::string valReg = llvm.__emitLoad(flt, chainPtr, outputStream);

        bool isParentFloat = (expectedType == "float" || expectedType == "double");
        if (isParentFloat && (chainType == "int" || chainType == "bigint")) {
            return llvm.__emitConvert("sitofp", flt, valReg, llvmType(expectedType), outputStream);
        }
        return valReg;
    }
    Symbol& sym = symbolTable.get(ident->name);
    std::string realType = sym.type;
    if (realType.back() == ']')
        realType = realType.substr(0, realType.find('['));

    std::string realLT = llvmType(realType);

    if (expectedType == "ptr") {
        return sym.llvmRegister;
    }
    std::string reg = llvm.__emitLoad(realLT, sym.llvmRegister, outputStream);

    bool isParentFloat = (expectedType == "float" || expectedType == "double");
    if (isParentFloat && (realType == "int" || realType == "bigint")) {
        return llvm.__emitConvert("sitofp", realLT, reg, llvmType(expectedType), outputStream);
    }

    return reg;
}

std::string CodeGen::genUnaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& outputStream) {
    auto* unary = static_cast<UnaryExprNode*>(node);
    std::string lt = llvmType(expectedType);
    bool isFloat = (expectedType == "float" || expectedType == "double");

    if (unary->optr == "-" || unary->optr == "!" || unary->optr == "~") {
        std::string opReg = genExpression(unary->operand.get(), expectedType, outputStream);
        if (unary->optr == "-")
            return llvm.__emitBinaryOp("-", lt, isFloat, isFloat ? "0.0" : "0", opReg, outputStream);
        if (unary->optr == "!")
            return llvm.__emitBinaryOp("^", "i1", false, opReg, "1", outputStream);
        return llvm.__emitBinaryOp("^", lt, false, opReg, "-1", outputStream);
    }

    bool isIncrement = (unary->optr == "++" || unary->optr == "post++");
    bool isPost = (unary->optr == "post++" || unary->optr == "post--");
    auto stepAt = [&](std::string memPtr, std::string valueType) -> std::string {
        std::string vlt = llvmType(valueType);
        bool vf = (valueType == "float" || valueType == "double");
        std::string oldReg = llvm.__emitLoad(vlt, memPtr, outputStream);
        std::string oneV = vf ? "1.0" : "1";
        std::string newRegVal = llvm.__emitBinaryOp(isIncrement ? "+" : "-", vlt, vf, oldReg, oneV, outputStream);
        llvm.__emitStore(vlt, newRegVal, memPtr, outputStream);
        return isPost ? oldReg : newRegVal;
    };

    if (unary->operand->type == NodeType::IDENT) {
        auto* ident = static_cast<IdentNode*>(unary->operand.get());
        size_t dot = ident->name.find('.');
        if (dot != std::string::npos) {
            std::vector<std::string> parts = splitDots(ident->name);
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string chainType;
            std::string chainPtr = genFieldChainAddressing(headParts, outputStream, chainType);
            ClassLayout& layout = classLayouts.at(chainType);
            int fieldIdx = layout.fieldIndex.at(parts.back());
            std::string objReg = llvm.__emitLoad(llvm.__pointerType(layout.llvmStructType), chainPtr, outputStream);
            std::string memPtr = llvm.__emitFieldGEP(layout.llvmStructType, objReg, fieldIdx, outputStream);
            return stepAt(memPtr, layout.fields[fieldIdx].first);
        }
        Symbol& sym = symbolTable.get(ident->name);
        return stepAt(sym.llvmRegister, sym.type);
    }

    if (unary->operand->type == NodeType::INDEX_EXPR) {
        auto* idxNode = static_cast<IndexExprNode*>(unary->operand.get());
        
        std::string arrBaseType, arrBaseReg, arrAllocType;
        size_t dot = idxNode->name.find('.');
        if (dot != std::string::npos) {
            std::vector<std::string> parts = splitDots(idxNode->name);
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string chainType;
            std::string chainPtr = genFieldChainAddressing(headParts, outputStream, chainType);
            ClassLayout& layout = classLayouts.at(chainType);
            int fieldIdx = layout.fieldIndex.at(parts.back());
            
            std::string objReg = llvm.__emitLoad(llvm.__pointerType(layout.llvmStructType), chainPtr, outputStream);
            arrBaseReg = llvm.__emitFieldGEP(layout.llvmStructType, objReg, fieldIdx, outputStream);
            arrBaseType = layout.fields[fieldIdx].first;
            
            ASTNode* fieldDecl = layout.fieldInitializers[fieldIdx];
            if (fieldDecl->type == NodeType::ARRAY_DECL) {
                auto* arrDecl = static_cast<ArrayDeclNode*>(fieldDecl);
                bool isDynamic = (arrDecl->dimensions.size() == 1 && arrDecl->dimensions[0] == -1);
                arrAllocType = isDynamic ? "i8*" : llvm.__nestedArrayType(llvmType(arrDecl->elementType), arrDecl->dimensions);
            } else {
                arrAllocType = llvmType(arrBaseType);
            }
        } else {
            Symbol& sym = symbolTable.get(idxNode->name);
            arrBaseType = sym.type;
            arrBaseReg = sym.llvmRegister;
            arrAllocType = sym.llvmAllocType;
        }

        if (arrBaseType.size() > 6 && arrBaseType.substr(0, 6) == "array<") {
            std::string elemType = arrBaseType.substr(6, arrBaseType.size() - 7);
            std::string elt = llvmType(elemType);
            bool isDynamic = (arrAllocType == "i8*");

            if (isDynamic) {
                llvm.__declareExternFn("i8*", "bery_array_get", {"i8*", "i64"});
                std::string arrReg = llvm.__emitLoad("i8*", arrBaseReg, outputStream);
                std::string idxReg = genExpression(idxNode->indices[0].get(), "int", outputStream);
                std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", outputStream);
                std::string rawReg = llvm.__emitCall("i8*", "bery_array_get", {{"i8*", arrReg}, {"i64", idxExt}}, outputStream);
                std::string castReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(elt), outputStream);

                if (!idxNode->memberChain.empty()) {
                    std::string finalType;
                    std::string fieldPtr = genFieldChainFromAddress(castReg, elemType, idxNode->memberChain, outputStream, finalType);
                    return stepAt(fieldPtr, finalType);
                }

                bool ef = (elemType == "float" || elemType == "double");
                std::string oldReg = llvm.__emitLoad(elt, castReg, outputStream);
                std::string oneV = ef ? "1.0" : "1";
                std::string newRegVal = llvm.__emitBinaryOp(isIncrement ? "+" : "-", elt, ef, oldReg, oneV, outputStream);
                llvm.__declareExternFn("void", "bery_array_set", {"i8*", "i64", "i8*"});
                std::string boxedReg = llvm.__emitBoxValue(elt, newRegVal, outputStream);
                llvm.__emitCall("void", "bery_array_set", {{"i8*", arrReg}, {"i64", idxExt}, {"i8*", boxedReg}}, outputStream);
                return isPost ? oldReg : newRegVal;
            } else {
                std::vector<std::pair<std::string, std::string>> indices;
                indices.push_back({"i32", "0"});
                for (auto& i : idxNode->indices)
                    indices.push_back({"i32", genExpression(i.get(), "int", outputStream)});
                std::string ptrReg = llvm.__emitTypedGEP(arrAllocType, arrBaseReg, indices, false, outputStream);

                if (!idxNode->memberChain.empty()) {
                    std::string finalType;
                    std::string fieldPtr = genFieldChainFromAddress(ptrReg, elemType, idxNode->memberChain, outputStream, finalType);
                    return stepAt(fieldPtr, finalType);
                }
                return stepAt(ptrReg, elemType);
            }
        }

        std::string baseType = arrBaseType.substr(0, arrBaseType.find('['));
        std::vector<std::pair<std::string, std::string>> indices;
        indices.push_back({"i32", "0"});
        for (auto& i : idxNode->indices)
            indices.push_back({"i32", genExpression(i.get(), "int", outputStream)});
        std::string ptrReg = llvm.__emitTypedGEP(arrAllocType, arrBaseReg, indices, false, outputStream);

        if (!idxNode->memberChain.empty()) {
            std::string finalType;
            std::string fieldPtr = genFieldChainFromAddress(ptrReg, baseType, idxNode->memberChain, outputStream, finalType);
            return stepAt(fieldPtr, finalType);
        }
        return stepAt(ptrReg, baseType);
    }
    return "0";
}

std::string CodeGen::genBetweenExpr(ASTNode* node, std::ostream& outputStream) {
    auto* bet = static_cast<BetweenExprNode*>(node);
    std::string opLT = llvmType(bet->resolvedType);
    bool isFloat = (bet->resolvedType == "float" || bet->resolvedType == "double");

    std::string tReg = genExpression(bet->value.get(), bet->resolvedType, outputStream);
    std::string lReg = genExpression(bet->lower.get(), bet->resolvedType, outputStream);
    std::string uReg = genExpression(bet->upper.get(), bet->resolvedType, outputStream);

    std::string cmp1 = llvm.__emitBinaryOp(">=", opLT, isFloat, tReg, lReg, outputStream);
    std::string cmp2 = llvm.__emitBinaryOp("<=", opLT, isFloat, tReg, uReg, outputStream);
    std::string andReg = llvm.__emitBinaryOp("&", "i1", false, cmp1, cmp2, outputStream);

    if (bet->isNegated) {
        return llvm.__emitBinaryOp("^", "i1", false, andReg, "1", outputStream);
    }
    return andReg;
}

std::string CodeGen::genBinaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& outputStream) {
    auto* binary = static_cast<BinaryExprNode*>(node);

    std::string opType = binary->left->resolvedType;
    if (opType.empty() || opType == "unknown") {
        opType = binary->resolvedType;
    }

    if (binary->optr == "&&" || binary->optr == "||") {
        std::string resAlloc = llvm.__emitAlloca("i1", outputStream);
        std::string lReg = genExpression(binary->left.get(), "bool", outputStream);
        llvm.__emitStore("i1", lReg, resAlloc, outputStream);
        int id = llvm.__uniqueId();
        std::string rightBlk = llvm.__labelWithId("logic_right", id);
        std::string endBlk = llvm.__labelWithId("logic_end", id);
        if (binary->optr == "&&")
            llvm.__emitCondBr(lReg, rightBlk, endBlk, outputStream);
        else
            llvm.__emitCondBr(lReg, endBlk, rightBlk, outputStream);
        llvm.__emitLabel(rightBlk, outputStream);
        std::string rReg = genExpression(binary->right.get(), "bool", outputStream);
        llvm.__emitStore("i1", rReg, resAlloc, outputStream);
        llvm.__emitBr(endBlk, outputStream);
        llvm.__emitLabel(endBlk, outputStream);
        return llvm.__emitLoad("i1", resAlloc, outputStream);
    }
    if (binary->resolvedType == "string") {
        std::string lReg = genExpression(binary->left.get(), "string", outputStream);
        std::string rReg = genExpression(binary->right.get(), "string", outputStream);
        if (binary->optr == "+") {
            llvm.__declareExternFn("i8*", "bery_string_concat", {"i8*", "i8*"});
            return llvm.__emitCall("i8*", "bery_string_concat", {{"i8*", lReg}, {"i8*", rReg}}, outputStream);
        }
        llvm.__declareExternFn("i1", "bery_string_equals", {"i8*", "i8*"});
        std::string eqReg = llvm.__emitCall("i1", "bery_string_equals", {{"i8*", lReg}, {"i8*", rReg}}, outputStream);
        if (binary->optr == "!=") {
            return llvm.__emitBinaryOp("^", "i1", false, eqReg, "1", outputStream);
        }
        return eqReg;
    }

    std::string opLT = llvmType(opType);
    bool isOpFloat = (opType == "float" || opType == "double");
    if (binary->optr == "**") {
        std::string lReg = genExpression(binary->left.get(), binary->resolvedType, outputStream);
        std::string rReg = genExpression(binary->right.get(), binary->resolvedType, outputStream);
        return llvm.__emitPow(opLT, isOpFloat, lReg, rReg, outputStream);
    }
    std::string lReg = genExpression(binary->left.get(), opType, outputStream);
    std::string rReg = genExpression(binary->right.get(), opType, outputStream);
    std::string resReg = llvm.__emitBinaryOp(binary->optr, opLT, isOpFloat, lReg, rReg, outputStream);
    if (resReg == "0") return "0";
    bool expectFloat = (expectedType == "float" || expectedType == "double");
    bool gotInt = (binary->resolvedType == "int" || binary->resolvedType == "bigint");
    if (expectFloat && gotInt) {
        return llvm.__emitConvert("sitofp", opLT, resReg, llvmType(expectedType), outputStream);
    }
    return resReg;
}

std::string CodeGen::genTernaryExpr(ASTNode* node, std::ostream& outputStream) {
    auto* tern = static_cast<TernaryExprNode*>(node);
    std::string llvmRT = llvmType(tern->resolvedType);
    std::string resAlloc = llvm.__emitAlloca(llvmRT, outputStream);
    std::string condReg = genExpression(tern->condition.get(), "bool", outputStream);

    int id = llvm.__uniqueId();
    std::string trueBlk = llvm.__labelWithId("tern_true", id);
    std::string falseBlk = llvm.__labelWithId("tern_false", id);
    std::string endBlk = llvm.__labelWithId("tern_end", id);

    llvm.__emitCondBr(condReg, trueBlk, falseBlk, outputStream);

    llvm.__emitLabel(trueBlk, outputStream);
    std::string tReg = genExpression(tern->trueExpr.get(), tern->resolvedType, outputStream);
    llvm.__emitStore(llvmRT, tReg, resAlloc, outputStream);
    llvm.__emitBr(endBlk, outputStream);

    llvm.__emitLabel(falseBlk, outputStream);
    std::string fReg = genExpression(tern->falseExpr.get(), tern->resolvedType, outputStream);
    llvm.__emitStore(llvmRT, fReg, resAlloc, outputStream);
    llvm.__emitBr(endBlk, outputStream);

    llvm.__emitLabel(endBlk, outputStream);
    return llvm.__emitLoad(llvmRT, resAlloc, outputStream);
}


std::string CodeGen::genAssignmentExpr(ASTNode* node, std::ostream& outputStream) {
    auto* assign = static_cast<AssignmentExprNode*>(node);
    std::string targetLT, memPtr, targetberyType;

    if (assign->target->type == NodeType::IDENT) {
        auto* ident = static_cast<IdentNode*>(assign->target.get());
        size_t dot = ident->name.find('.');

        if (dot != std::string::npos) {
            std::vector<std::string> parts = splitDots(ident->name);
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string chainType;
            std::string chainPtr = genFieldChainAddressing(headParts, outputStream, chainType);
            ClassLayout& layout = classLayouts.at(chainType);
            int fieldIdx = layout.fieldIndex.at(parts.back());
            targetLT = llvmType(layout.fields[fieldIdx].first);
            targetberyType = layout.fields[fieldIdx].first;

            std::string objReg = llvm.__emitLoad(llvm.__pointerType(layout.llvmStructType), chainPtr, outputStream);
            memPtr = llvm.__emitFieldGEP(layout.llvmStructType, objReg, fieldIdx, outputStream);
        } else {
            Symbol& sym = symbolTable.get(ident->name);
            targetLT = llvmType(sym.type);
            memPtr = sym.llvmRegister;
            targetberyType = sym.type;

            if (sym.type == "string" && assign->op == "=") {
                llvm.__declareExtern("declare i8* @bery_string_copy(i8*)", "bery_string_copy");
                std::string srcReg = genExpression(assign->value.get(), "string", outputStream);
                std::string copyReg = llvm.__emitCall("i8*", "bery_string_copy", {{"i8*", srcReg}}, outputStream);
                llvm.__emitStore("i8*", copyReg, memPtr, outputStream);
                return copyReg;
            }
        }

    } else if (assign->target->type == NodeType::INDEX_EXPR) {
        auto* idxNode = static_cast<IndexExprNode*>(assign->target.get());
        
        std::string arrBaseType;
        std::string arrBaseReg;
        std::string arrAllocType;
        
        size_t dot = idxNode->name.find('.');
        if (dot != std::string::npos) {
            std::vector<std::string> parts = splitDots(idxNode->name);
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string chainType;
            std::string chainPtr = genFieldChainAddressing(headParts, outputStream, chainType);
            ClassLayout& layout = classLayouts.at(chainType);
            int fieldIdx = layout.fieldIndex.at(parts.back());
            
            std::string objReg = llvm.__emitLoad(llvm.__pointerType(layout.llvmStructType), chainPtr, outputStream);
            arrBaseReg = llvm.__emitFieldGEP(layout.llvmStructType, objReg, fieldIdx, outputStream);
            arrBaseType = layout.fields[fieldIdx].first;
            
            ASTNode* fieldDecl = layout.fieldInitializers[fieldIdx];
            if (fieldDecl->type == NodeType::ARRAY_DECL) {
                auto* arrDecl = static_cast<ArrayDeclNode*>(fieldDecl);
                bool isDynamic = (arrDecl->dimensions.size() == 1 && arrDecl->dimensions[0] == -1);
                arrAllocType = isDynamic ? "i8*" : llvm.__nestedArrayType(llvmType(arrDecl->elementType), arrDecl->dimensions);
            } else {
                arrAllocType = llvmType(arrBaseType);
            }
        } else {
            Symbol& sym = symbolTable.get(idxNode->name);
            arrBaseType = sym.type;
            arrBaseReg = sym.llvmRegister;
            arrAllocType = sym.llvmAllocType;
        }

        if (arrBaseType.size() > 6 && arrBaseType.substr(0, 6) == "array<") {
            std::string elemType = arrBaseType.substr(6, arrBaseType.size() - 7);
            std::string lt = llvmType(elemType);
            bool isDynamic = (arrAllocType == "i8*");

            if (isDynamic) {
                std::string arrReg = llvm.__emitLoad("i8*", arrBaseReg, outputStream);
                std::string idxReg = genExpression(idxNode->indices[0].get(), "int", outputStream);
                std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", outputStream);

                if (!idxNode->memberChain.empty()) {
                    llvm.__declareExternFn("i8*", "bery_array_get", {"i8*", "i64"});
                    std::string rawReg = llvm.__emitCall("i8*", "bery_array_get", {{"i8*", arrReg}, {"i64", idxExt}}, outputStream);
                    std::string castReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(lt), outputStream);
                    std::string finalType;
                    memPtr = genFieldChainFromAddress(castReg, elemType, idxNode->memberChain, outputStream, finalType);
                    targetLT = llvmType(finalType);
                    targetberyType = finalType;
                } else {
                    llvm.__declareExternFn("void", "bery_array_set", {"i8*", "i64", "i8*"});
                    std::string valReg = classLayouts.count(elemType)? genClassCopyValue(assign->value.get(), elemType, outputStream): genExpression(assign->value.get(), elemType, outputStream);
                    std::string boxedReg = llvm.__emitBoxValue(lt, valReg, outputStream);
                    llvm.__emitCall("void", "bery_array_set", {{"i8*", arrReg}, {"i64", idxExt}, {"i8*", boxedReg}}, outputStream);
                    return valReg;
                }
            } else {
                std::vector<std::pair<std::string, std::string>> indices;
                indices.push_back({"i32", "0"});
                for (auto& idx : idxNode->indices)
                    indices.push_back({"i32", genExpression(idx.get(), "int", outputStream)});
                std::string ptrReg = llvm.__emitTypedGEP(arrAllocType, arrBaseReg, indices, false, outputStream);

                if (!idxNode->memberChain.empty()) {
                    std::string finalType;
                    memPtr = genFieldChainFromAddress(ptrReg, elemType, idxNode->memberChain, outputStream, finalType);
                    targetLT = llvmType(finalType);
                    targetberyType = finalType;
                } else {
                    targetLT = lt;
                    targetberyType = elemType;
                    memPtr = ptrReg;
                }
            }
        } else {
            std::string baseType = arrBaseType.substr(0, arrBaseType.find('['));
            std::vector<std::pair<std::string, std::string>> indices;
            indices.push_back({"i32", "0"});
            for (auto& idx : idxNode->indices)
                indices.push_back({"i32", genExpression(idx.get(), "int", outputStream)});
            std::string ptrReg = llvm.__emitTypedGEP(arrAllocType, arrBaseReg, indices, false, outputStream);

            if (!idxNode->memberChain.empty()) {
                std::string finalType;
                memPtr = genFieldChainFromAddress(ptrReg, baseType, idxNode->memberChain, outputStream, finalType);
                targetLT = llvmType(finalType);
                targetberyType = finalType;
            } else {
                targetLT = llvmType(baseType);
                targetberyType = baseType;
                memPtr = ptrReg;
            }
        }
    } 

    std::string valReg = classLayouts.count(targetberyType) ?genClassCopyValue(assign->value.get(), targetberyType, outputStream)
        : genExpression(assign->value.get(), targetberyType, outputStream);

    if (assign->op == "=") {
        llvm.__emitStore(targetLT, valReg, memPtr, outputStream);
        return valReg;
    }
    else if (assign->op == "+=" && targetberyType == "string") {
        llvm.__declareExternFn("i8*", "bery_string_copy", {"i8*"});
        std::string currVal = llvm.__emitLoad("i8*", memPtr, outputStream);
        std::string concatReg = llvm.__emitCall("i8*", "bery_string_concat", {{"i8*", currVal}, {"i8*", valReg}}, outputStream);
        llvm.__emitStore("i8*", concatReg, memPtr, outputStream);
        return concatReg;
    }

    bool isFloat = (targetberyType == "float" || targetberyType == "double");
    std::string curVal = llvm.__emitLoad(targetLT, memPtr, outputStream);
    std::string resReg;

    if (assign->op == "**=") {
        resReg = llvm.__emitPow(targetLT, isFloat, curVal, valReg, outputStream);
    } else {
        std::string baseOp = assign->op.substr(0, assign->op.size() - 1);
        resReg = llvm.__emitBinaryOp(baseOp, targetLT, isFloat, curVal, valReg, outputStream);
    }

    llvm.__emitStore(targetLT, resReg, memPtr, outputStream);
    return resReg;
}

std::string CodeGen::genCastExpr(ASTNode* node, std::ostream& outputStream) {
    auto* castNode = static_cast<CastExprNode*>(node);
    std::string srcReg = genExpression(castNode->expr.get(), castNode->srcType, outputStream);
    std::string sType = castNode->srcType;
    std::string tType = castNode->targetType;
    if (sType == tType) return srcReg;

    std::string sLLVM = llvmType(sType);
    std::string tLLVM = llvmType(tType);

    if (tType == "string") {
        llvm.__declareExternFn("i8*", "bery_to_string_int", {"i32"});
        llvm.__declareExternFn("i8*", "bery_to_string_bigint", {"i64"});
        llvm.__declareExternFn("i8*", "bery_to_string_float", {"float"});
        llvm.__declareExternFn("i8*", "bery_to_string_double", {"double"});
        llvm.__declareExternFn("i8*", "bery_to_string_char", {"i8"});
        llvm.__declareExternFn("i8*", "bery_to_string_bool", {"i1"});

        if (sType == "int")    return llvm.__emitCall("i8*", "bery_to_string_int", {{"i32", srcReg}}, outputStream);
        if (sType == "bigint") return llvm.__emitCall("i8*", "bery_to_string_bigint", {{"i64", srcReg}}, outputStream);
        if (sType == "float")  return llvm.__emitCall("i8*", "bery_to_string_float", {{"float", srcReg}}, outputStream);
        if (sType == "double") return llvm.__emitCall("i8*", "bery_to_string_double", {{"double", srcReg}}, outputStream);
        if (sType == "char")   return llvm.__emitCall("i8*", "bery_to_string_char", {{"i8", srcReg}}, outputStream);
        if (sType == "bool")   return llvm.__emitCall("i8*", "bery_to_string_bool", {{"i1", srcReg}}, outputStream);
    }

    bool srcFloat = (sType == "float" || sType == "double");
    bool tgtFloat = (tType == "float" || tType == "double");

    if (srcFloat && !tgtFloat)
        return llvm.__emitConvert("fptosi", sLLVM, srcReg, tLLVM, outputStream);
    if (!srcFloat && tgtFloat)
        return llvm.__emitConvert("sitofp", sLLVM, srcReg, tLLVM, outputStream);
    if (srcFloat && tgtFloat) {
        if (sType == "float")
            return llvm.__emitConvert("fpext", "float", srcReg, "double", outputStream);
        return llvm.__emitConvert("fptrunc", "double", srcReg, "float", outputStream);
    }
    int sW = (sType == "bigint") ? 64 : (sType == "int" ? 32 : (sType == "char" ? 8 : 1));
    int tW = (tType == "bigint") ? 64 : (tType == "int" ? 32 : (tType == "char" ? 8 : 1));
    if (sW > tW)
        return llvm.__emitConvert("trunc", sLLVM, srcReg, tLLVM, outputStream);
    if (sW < tW)
        return llvm.__emitConvert(sType == "bool" ? "zext" : "sext", sLLVM, srcReg, tLLVM, outputStream);
    return srcReg;
}
std::string CodeGen::genIndexExpr(ASTNode* node, std::ostream& outputStream) {
    auto* idx = static_cast<IndexExprNode*>(node);
    
    std::string arrBaseType;
    std::string arrBaseReg;
    std::string arrAllocType;
    
    size_t dot = idx->name.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(idx->name);
        std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
        std::string chainType;
        std::string chainPtr = genFieldChainAddressing(headParts, outputStream, chainType);
        ClassLayout& layout = classLayouts.at(chainType);
        int fieldIdx = layout.fieldIndex.at(parts.back());
        
        std::string objReg = llvm.__emitLoad(llvm.__pointerType(layout.llvmStructType), chainPtr, outputStream);
        arrBaseReg = llvm.__emitFieldGEP(layout.llvmStructType, objReg, fieldIdx, outputStream);
        arrBaseType = layout.fields[fieldIdx].first;
        
        ASTNode* fieldDecl = layout.fieldInitializers[fieldIdx];
        if (fieldDecl->type == NodeType::ARRAY_DECL) {
            auto* arrDecl = static_cast<ArrayDeclNode*>(fieldDecl);
            bool isDynamic = (arrDecl->dimensions.size() == 1 && arrDecl->dimensions[0] == -1);
            arrAllocType = isDynamic ? "i8*" : llvm.__nestedArrayType(llvmType(arrDecl->elementType), arrDecl->dimensions);
        } else {
            arrAllocType = llvmType(arrBaseType);
        }
    } else {
        Symbol& sym = symbolTable.get(idx->name);
        arrBaseType = sym.type;
        arrBaseReg = sym.llvmRegister;
        arrAllocType = sym.llvmAllocType;
    }

    if (arrBaseType == "string") {
        llvm.__declareExternFn("i8", "bery_string_char_at", {"i8*", "i64"});
        std::string str = llvm.__emitLoad("i8*", arrBaseReg, outputStream);
        std::string index = genExpression(idx->indices[0].get(), "int", outputStream);
        std::string index64 = llvm.__emitSext("i32", index, "i64", outputStream);
        return llvm.__emitCall("i8", "bery_string_char_at", {{"i8*", str}, {"i64", index64}}, outputStream);
    }
    
    if (arrBaseType.size() > 6 && arrBaseType.substr(0, 6) == "array<") {
        std::string elemType = arrBaseType.substr(6, arrBaseType.size() - 7);
        std::string lt = llvmType(elemType);
        bool isDynamic = (arrAllocType == "i8*");

        if (isDynamic) {
            llvm.__declareExternFn("i8*", "bery_array_get", {"i8*", "i64"});
            std::string arrReg = llvm.__emitLoad("i8*", arrBaseReg, outputStream);
            std::string idxReg = genExpression(idx->indices[0].get(), "int", outputStream);
            std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", outputStream);
            std::string rawReg = llvm.__emitCall("i8*", "bery_array_get", {{"i8*", arrReg}, {"i64", idxExt}}, outputStream);
            std::string castReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(lt), outputStream);
            
            if (!idx->memberChain.empty()) {
                std::string finalType;
                std::string fieldPtr = genFieldChainFromAddress(castReg, elemType, idx->memberChain, outputStream, finalType);
                return llvm.__emitLoad(llvmType(finalType), fieldPtr, outputStream);
            }
            return llvm.__emitLoad(lt, castReg, outputStream);
        } else {
            std::vector<std::pair<std::string, std::string>> indices;
            indices.push_back({"i32", "0"});
            for (auto& i : idx->indices) {
                indices.push_back({"i32", genExpression(i.get(), "int", outputStream)});
            }
            std::string ptrReg = llvm.__emitTypedGEP(arrAllocType, arrBaseReg, indices, false, outputStream);
            
            if (!idx->memberChain.empty()) {
                std::string finalType;
                std::string fieldPtr = genFieldChainFromAddress(ptrReg, elemType, idx->memberChain, outputStream, finalType);
                return llvm.__emitLoad(llvmType(finalType), fieldPtr, outputStream);
            }
            return llvm.__emitLoad(lt, ptrReg, outputStream);
        }
    }

    std::string baseType = arrBaseType.substr(0, arrBaseType.find('['));
    std::string lt = llvmType(baseType);
    std::vector<std::pair<std::string, std::string>> indices;
    indices.push_back({"i32", "0"});
    for (auto& i : idx->indices)
        indices.push_back({"i32", genExpression(i.get(), "int", outputStream)});
        
    std::string ptrReg = llvm.__emitTypedGEP(arrAllocType, arrBaseReg, indices, false, outputStream);
    if (!idx->memberChain.empty()) {
        std::string finalType;
        std::string fieldPtr = genFieldChainFromAddress(ptrReg, baseType, idx->memberChain, outputStream, finalType);
        return llvm.__emitLoad(llvmType(finalType), fieldPtr, outputStream);
    }
    return llvm.__emitLoad(lt, ptrReg, outputStream);
}
std::string CodeGen::genCallExpr(ASTNode* node, std::ostream& outputStream) {
    auto* call = static_cast<CallExprNode*>(node);

    static const std::unordered_set<std::string> breFns = {
        "print", "println", "inputInt", "inputBigInt", "inputFloat",
        "inputDouble", "inputBool", "inputChar", "inputString"
    };
    if (breFns.count(call->callee))
        return genBREPrintCall(node, outputStream);

    if (call->callee == "super" || call->callee.rfind("super.", 0) == 0) {
        std::string parentName = classLayouts.at(currentClassName).parentName;
        Symbol& selfSym = symbolTable.get(currentSelfRef);
        std::string selfPtrType = llvm.__pointerType(classLayouts.at(currentClassName).llvmStructType);
        std::string selfPtr = llvm.__emitLoad(selfPtrType, selfSym.llvmRegister, outputStream);

        if (call->callee == "super") {
            if (call->resolvedParamTypes.empty() && call->arguments.empty()) {
                bool anyAncestorCtor = false;
                std::string cur = parentName;
                while (!cur.empty() && classLayouts.count(cur)) {
                    if (classLayouts.at(cur).hasConstructor) { anyAncestorCtor = true; break; }
                    cur = classLayouts.at(cur).parentName;
                }
                if (!anyAncestorCtor) return "0";
            }
            std::string ctorOwner = parentName;
            std::string mangled;
            while (!ctorOwner.empty() && classLayouts.count(ctorOwner)) {
                std::string candidate = llvm.__mangleOverload(llvm.__mangleConstructor(ctorOwner), call->resolvedParamTypes);
                if (functions.count(candidate)) { mangled = candidate; break; }
                ctorOwner = classLayouts.at(ctorOwner).parentName;
            }
            if (mangled.empty()) return "0";
            CodeGenFunctionSignature& sig = functions[mangled];
            std::string ownerPtrType = llvm.__pointerType(classLayouts.at(ctorOwner).llvmStructType);
            std::string castReg = (ctorOwner == currentClassName) ? selfPtr
                : llvm.__emitBitcast(selfPtrType, selfPtr, ownerPtrType, outputStream);

            std::vector<std::pair<std::string, std::string>> args;
            args.push_back({ownerPtrType, castReg});
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                std::string paramType = sig.parameterTypes[i + 1];
                std::string argReg = classLayouts.count(paramType) ? genClassCopyValue(call->arguments[i].get(), paramType, outputStream)
                    : genExpression(call->arguments[i].get(), paramType, outputStream);
                args.push_back({llvmType(paramType), argReg});
            }
            llvm.__emitCall("void", mangled, args, outputStream);
            return "0";
        }

        std::string method = call->callee.substr(6);
        std::string owner = findMethodOwner(parentName, method, call->resolvedParamTypes);
        if (owner.empty()) return "0";
        std::string mangled = llvm.__mangleOverload(llvm.__mangleMethod(owner, method), call->resolvedParamTypes);
        CodeGenFunctionSignature& sig = functions[mangled];
        std::string ownerPtrType = llvm.__pointerType(classLayouts.at(owner).llvmStructType);
        std::string castReg = (owner == currentClassName) ? selfPtr
            : llvm.__emitBitcast(selfPtrType, selfPtr, ownerPtrType, outputStream);

        std::vector<std::pair<std::string, std::string>> args;
        args.push_back({ownerPtrType, castReg});
        for (size_t i = 0; i < call->arguments.size(); ++i) {
            std::string paramType = sig.parameterTypes[i + 1];
            std::string argReg = classLayouts.count(paramType) ? genClassCopyValue(call->arguments[i].get(), paramType, outputStream)
                : genExpression(call->arguments[i].get(), paramType, outputStream);
            args.push_back({llvmType(paramType), argReg});
        }
        if (sig.returnType.empty() || sig.returnType == "void") {
            llvm.__emitCall("void", mangled, args, outputStream);
            return "0";
        }
        return llvm.__emitCall(llvmType(sig.returnType), mangled, args, outputStream);
    }

    size_t dot = call->callee.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(call->callee);
        std::string method = parts.back();
        std::vector<std::string> headParts(parts.begin(), parts.end() - 1);

        std::string objType;
        std::string objPtr = genFieldChainAddressing(headParts, outputStream, objType);

        if (objType == "string") {
            std::string strReg = llvm.__emitLoad("i8*", objPtr, outputStream);
            if (method == "len") {
                llvm.__declareExternFn("i64", "bery_string_length", {"i8*"});
                std::string lenReg = llvm.__emitCall("i64", "bery_string_length", {{"i8*", strReg}}, outputStream);
                return llvm.__emitConvert("trunc", "i64", lenReg, "i32", outputStream);
            }
            if (method == "copy") {
                llvm.__declareExternFn("i8*", "bery_string_copy", {"i8*"});
                return llvm.__emitCall("i8*", "bery_string_copy", {{"i8*", strReg}}, outputStream);
            }
            if (method == "substr") {
                llvm.__declareExternFn("i8*", "bery_string_substring", {"i8*", "i64", "i64"});
                std::string s0 = genExpression(call->arguments[0].get(), "int", outputStream);
                std::string s1 = genExpression(call->arguments[1].get(), "int", outputStream);
                std::string e0 = llvm.__emitSext("i32", s0, "i64", outputStream);
                std::string e1 = llvm.__emitSext("i32", s1, "i64", outputStream);
                return llvm.__emitCall("i8*", "bery_string_substring", {{"i8*", strReg}, {"i64", e0}, {"i64", e1}}, outputStream);
            }
        }

        if (objType.size() > 6 && objType.substr(0, 6) == "array<") {
            std::string elemType = objType.substr(6, objType.size() - 7);
            std::string lt = llvmType(elemType);

            if (method == "len") {
                std::string allocType = "i8*";
                int fixedSize = -1;
                Symbol& base = symbolTable.get(headParts[0]);
                
                if (headParts.size() == 1) {
                    allocType = base.llvmAllocType;
                    fixedSize = base.arraySize;
                } else {
                    std::string currentType = base.type;
                    for (size_t i = 1; i < headParts.size(); ++i) {
                        ClassLayout& layout = classLayouts.at(currentType);
                        int fIdx = layout.fieldIndex.at(headParts[i]);
                        ASTNode* fieldDecl = layout.fieldInitializers[fIdx];
                        if (i == headParts.size() - 1 && fieldDecl->type == NodeType::ARRAY_DECL) {
                            auto* arrDecl = static_cast<ArrayDeclNode*>(fieldDecl);
                            if (!(arrDecl->dimensions.size() == 1 && arrDecl->dimensions[0] == -1)) {
                                allocType = llvm.__nestedArrayType(llvmType(arrDecl->elementType), arrDecl->dimensions);
                                fixedSize = 1;
                                for (int d : arrDecl->dimensions) fixedSize *= d;
                            }
                        }
                        currentType = layout.fields[fIdx].first;
                    }
                }

                if (allocType != "i8*" && fixedSize > 0) {
                    return std::to_string(fixedSize);
                }

                llvm.__declareExternFn("i64", "bery_array_length", {"i8*"});
                std::string arrReg = llvm.__emitLoad("i8*", objPtr, outputStream);
                std::string lenReg = llvm.__emitCall("i64", "bery_array_length", {{"i8*", arrReg}}, outputStream);
                return llvm.__emitConvert("trunc", "i64", lenReg, "i32", outputStream);
            }

            std::string arrReg = llvm.__emitLoad("i8*", objPtr, outputStream);
            
            if (method == "push") {
                llvm.__declareExternFn("void", "bery_array_push", {"i8*", "i8*"});
                std::string valReg = classLayouts.count(elemType)? genClassCopyValue(call->arguments[0].get(), elemType, outputStream): genExpression(call->arguments[0].get(), elemType, outputStream);
                std::string castReg = llvm.__emitBoxValue(lt, valReg, outputStream);
                llvm.__emitCall("void", "bery_array_push", {{"i8*", arrReg}, {"i8*", castReg}}, outputStream);
                return "0";
            }
            if (method == "pop") {
                llvm.__declareExternFn("i8*", "bery_array_pop", {"i8*"});
                std::string rawReg = llvm.__emitCall("i8*", "bery_array_pop", {{"i8*", arrReg}}, outputStream);
                std::string castReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(lt), outputStream);
                return llvm.__emitLoad(lt, castReg, outputStream);
            }
            if (method == "insert") {
                llvm.__declareExternFn("void", "bery_array_insert", {"i8*", "i64", "i8*"});
                std::string idxReg = genExpression(call->arguments[0].get(), "int", outputStream);
                std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", outputStream);
                std::string valReg = classLayouts.count(elemType) ? genClassCopyValue(call->arguments[1].get(), elemType, outputStream): genExpression(call->arguments[1].get(), elemType, outputStream);
                std::string castReg = llvm.__emitBoxValue(lt, valReg, outputStream);
                llvm.__emitCall("void", "bery_array_insert", {{"i8*", arrReg}, {"i64", idxExt}, {"i8*", castReg}}, outputStream);
                return "0";
            }
            if (method == "remove") {
                llvm.__declareExternFn("void", "bery_array_remove", {"i8*", "i64"});
                std::string idxReg = genExpression(call->arguments[0].get(), "int", outputStream);
                std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", outputStream);
                llvm.__emitCall("void", "bery_array_remove", {{"i8*", arrReg}, {"i64", idxExt}}, outputStream);
                return "0";
            }
        }
        if (classLayouts.count(objType)) {
            std::string owner = findMethodOwner(objType, method, call->resolvedParamTypes);
            if (!owner.empty()) {
                std::string mangled = llvm.__mangleOverload(llvm.__mangleMethod(owner, method), call->resolvedParamTypes);
                CodeGenFunctionSignature& sig = functions[mangled];

                std::string objPtrType = llvm.__pointerType(classLayouts.at(objType).llvmStructType);
                std::string receiverReg = llvm.__emitLoad(objPtrType, objPtr, outputStream);

                std::string ownerPtrType = llvm.__pointerType(classLayouts.at(owner).llvmStructType);
                std::string castReg = (owner == objType) ? receiverReg : llvm.__emitBitcast(objPtrType, receiverReg, ownerPtrType, outputStream);

                std::vector<std::pair<std::string, std::string>> args;
                args.push_back({ownerPtrType, castReg});
                for (size_t i = 0; i < call->arguments.size(); ++i) {
                    std::string paramType = sig.parameterTypes[i + 1];
                    std::string argReg = classLayouts.count(paramType) ? genClassCopyValue(call->arguments[i].get(), paramType, outputStream) : genExpression(call->arguments[i].get(), paramType, outputStream);
                    args.push_back({llvmType(paramType), argReg});
                }

                if (sig.returnType.empty() || sig.returnType == "void") {
                    llvm.__emitCall("void", mangled, args, outputStream);
                    return "0";
                }
                return llvm.__emitCall(llvmType(sig.returnType), mangled, args, outputStream);
            }
        }
        return "0";
    }
    if (!currentClassName.empty() && classLayouts.count(currentClassName)) {
        std::string owner = findMethodOwner(currentClassName, call->callee, call->resolvedParamTypes);
        if (!owner.empty()) {
            std::string mangled = llvm.__mangleOverload(llvm.__mangleMethod(owner, call->callee), call->resolvedParamTypes);
            CodeGenFunctionSignature& sig = functions[mangled];
            Symbol& selfSym = symbolTable.get(currentSelfRef);

            std::string selfPtrType = llvm.__pointerType(classLayouts.at(currentClassName).llvmStructType);
            std::string receiverReg = llvm.__emitLoad(selfPtrType, selfSym.llvmRegister, outputStream);

            std::string ownerPtrType = llvm.__pointerType(classLayouts.at(owner).llvmStructType);
            std::string castReg = (owner == currentClassName)
                ? receiverReg
                : llvm.__emitBitcast(selfPtrType, receiverReg, ownerPtrType, outputStream);

            std::vector<std::pair<std::string, std::string>> args;
            args.push_back({ownerPtrType, castReg});
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                std::string paramType = sig.parameterTypes[i + 1];
                std::string argReg = classLayouts.count(paramType) ? genClassCopyValue(call->arguments[i].get(), paramType, outputStream) : genExpression(call->arguments[i].get(), paramType, outputStream);
                args.push_back({llvmType(paramType), argReg});
            }

            if (sig.returnType.empty() || sig.returnType == "void") {
                llvm.__emitCall("void", mangled, args, outputStream);
                return "0";
            }
            return llvm.__emitCall(llvmType(sig.returnType), mangled, args, outputStream);
        }
    }
    std::string calleeKey = call->callee;
    if (functions.find(calleeKey) == functions.end()) {
        calleeKey = llvm.__mangleOverload(call->callee, call->resolvedParamTypes);
    }
    if (functions.find(calleeKey) == functions.end()) return "0";
    CodeGenFunctionSignature& sig = functions[calleeKey];

    std::vector<std::pair<std::string, std::string>> args;
    for (size_t i = 0; i < call->arguments.size(); ++i) {
         std::string argReg = classLayouts.count(sig.parameterTypes[i])? genClassCopyValue(call->arguments[i].get(), sig.parameterTypes[i], outputStream): genExpression(call->arguments[i].get(), sig.parameterTypes[i], outputStream);
        args.push_back({llvmType(sig.parameterTypes[i]), argReg});
    }

    if (sig.returnType == "void") {
        llvm.__emitCall("void", calleeKey, args, outputStream);
        return "0";
    }
    return llvm.__emitCall(llvmType(sig.returnType), calleeKey, args, outputStream);
}

std::string CodeGen::genNewExpr(ASTNode* node, std::ostream& outputStream) {
    auto* newExpr = static_cast<NewExprNode*>(node);
    auto it = classLayouts.find(newExpr->className);
    if (it == classLayouts.end()) return "null";
    ClassLayout& layout = it->second;

    llvm.__declareExternFn("i8*", "bery_alloc", {"i64", "i32"});
    std::string typeIdReg = llvm.__emitLoad("i32", llvm.__globalRef(newExpr->className, "_typeid"), outputStream);
    std::string rawReg = llvm.__emitCall("i8*", "bery_alloc", {{"i64", std::to_string(layout.instanceSize)}, {"i32", typeIdReg}}, outputStream);

    std::string objReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(layout.llvmStructType), outputStream);
    for (size_t i = 0; i < layout.fields.size(); ++i) {
        std::string gepReg = llvm.__emitFieldGEP(layout.llvmStructType, objReg, (int)i, outputStream);
        ASTNode* declNode = layout.fieldInitializers[i];
        
        if (declNode->type == NodeType::VAR_DECL) {
            auto* varDecl = static_cast<VarDeclNode*>(declNode);
            std::string flt = llvmType(varDecl->varType);
            if (varDecl->value) {
                std::string valReg = classLayouts.count(varDecl->varType)? genClassCopyValue(varDecl->value.get(), varDecl->varType, outputStream) : genExpression(varDecl->value.get(), varDecl->varType, outputStream);
                llvm.__emitStore(flt, valReg, gepReg, outputStream);
            } else {
                bool isPtr = !flt.empty() && flt.back() == '*';
                std::string zeroVal = (flt == "float" || flt == "double") ? "0.0" : (isPtr ? "null" : "0");
                llvm.__emitStore(flt, zeroVal, gepReg, outputStream);
            }
        } else if (declNode->type == NodeType::ARRAY_DECL) {
            auto* arrDecl = static_cast<ArrayDeclNode*>(declNode);
            bool isDynamic = (arrDecl->dimensions.size() == 1 && arrDecl->dimensions[0] == -1);
            std::string flt = isDynamic ? "i8*" : llvm.__nestedArrayType(llvmType(arrDecl->elementType), arrDecl->dimensions);
            
            if (isDynamic) {
                if (arrDecl->valueExpr) {
                    std::string valReg = genExpression(arrDecl->valueExpr.get(), layout.fields[i].first, outputStream);
                    llvm.__emitStore("i8*", valReg, gepReg, outputStream);
                } else {
                    llvm.__declareExternFn("i8*", "bery_array_new", {"i64"});
                    std::string arrReg = llvm.__emitCall("i8*", "bery_array_new", {{"i64", "4"}}, outputStream);
                    llvm.__emitStore("i8*", arrReg, gepReg, outputStream);
                    if (!arrDecl->initializers.empty()) {
                        std::string eltLT = llvmType(arrDecl->elementType);
                        llvm.__declareExternFn("void", "bery_array_push", {"i8*", "i8*"});
                        for (auto& initVal : arrDecl->initializers) {
                            std::string valReg = genExpression(initVal.get(), arrDecl->elementType, outputStream);
                            std::string boxedReg = llvm.__emitBoxValue(eltLT, valReg, outputStream);
                            llvm.__emitCall("void", "bery_array_push", {{"i8*", arrReg}, {"i8*", boxedReg}}, outputStream);
                        }
                    }
                }
            } else {
                if (!arrDecl->initializers.empty()) {
                    std::string eltLT = llvmType(arrDecl->elementType);
                    std::string flatPtr = llvm.__emitBitcast(llvm.__pointerType(flt), gepReg, llvm.__pointerType(eltLT), outputStream);
                    for (size_t j = 0; j < arrDecl->initializers.size(); ++j) {
                        std::string valReg = genExpression(arrDecl->initializers[j].get(), arrDecl->elementType, outputStream);
                        std::string ptrReg = llvm.__emitTypedGEP(eltLT, flatPtr, {{"i32", std::to_string(j)}}, false, outputStream);
                        llvm.__emitStore(eltLT, valReg, ptrReg, outputStream);
                    }
                }
            }
        }
    }

    if (layout.hasConstructor) {
        std::string ctorOwner = layout.constructorOwner;
        std::string mangled;
        while (!ctorOwner.empty() && classLayouts.count(ctorOwner)) {
            std::string candidate = llvm.__mangleOverload(llvm.__mangleConstructor(ctorOwner), newExpr->resolvedParamTypes);
            if (functions.count(candidate)) { mangled = candidate; break; }
            ctorOwner = classLayouts.at(ctorOwner).parentName;
        }
        if (!mangled.empty()) {
            CodeGenFunctionSignature& sig = functions[mangled];
            std::string ownerPtrType = llvm.__pointerType(classLayouts.at(ctorOwner).llvmStructType);
            std::string ctorSelf = (ctorOwner == newExpr->className) ? objReg
                : llvm.__emitBitcast(llvm.__pointerType(layout.llvmStructType), objReg, ownerPtrType, outputStream);

            std::vector<std::pair<std::string, std::string>> args;
            args.push_back({ownerPtrType, ctorSelf});
            for (size_t i = 0; i < newExpr->arguments.size(); ++i) {
                std::string paramType = sig.parameterTypes[i + 1];
                std::string argReg = classLayouts.count(paramType) ? genClassCopyValue(newExpr->arguments[i].get(), paramType, outputStream)
                    : genExpression(newExpr->arguments[i].get(), paramType, outputStream);
                args.push_back({llvmType(paramType), argReg});
            }
            llvm.__emitCall("void", mangled, args, outputStream);
        }
    }

    return objReg;
}


std::string CodeGen::genRefExpr(ASTNode* node, const std::string& expectedType,std::ostream& outputStream) {
    auto* refNode = static_cast<RefExprNode*>(node);
    return genExpression(refNode->target.get(), expectedType, outputStream);
}


std::string CodeGen::genClassCopyValue(ASTNode* valueNode, const std::string& classType, std::ostream& outputStream) {
    std::string srcReg = genExpression(valueNode, classType, outputStream);

    if (valueNode->type == NodeType::REF_EXPR || valueNode->type == NodeType::NEW_EXPR) {
        return srcReg;
    }

    return cloneClassInstance(classType, srcReg, outputStream);
}

std::string CodeGen::cloneClassInstance(const std::string& classType, const std::string& srcRegister, std::ostream& outputStream){
    std::string lt = llvmType(classType); // "" classname
    ClassLayout& layout = classLayouts.at(classType);

    // i8 - character
    // i8* - string
    // i8** - identifier
    // i8* bery_alloc(i64, i32) {}
    llvm.__declareExternFn("i8*", "bery_alloc", {"i64", "i32"});
    // @Car._typeid
    std::string typeIdeReg = llvm.__emitLoad("i32", llvm.__globalRef(classType, "_typeid"), outputStream);
    std::string rawReg = llvm.__emitCall("i8*", "bery_alloc", {{"i64", std::to_string(layout.instanceSize)}, {"i32", typeIdeReg}}, outputStream);

    std::string srcBytes = llvm.__emitBitcast(lt, srcRegister, "i8*", outputStream);
    llvm.__declareExternFn("i8*", "memcpy", {"i8*", "i8*", "i64"});
    llvm.__emitCall("i8*", "memcpy", {{"i8*", rawReg}, {"i8*", srcBytes}, {"i64", std::to_string(layout.instanceSize)}}, outputStream);

    return llvm.__emitBitcast("i8*", rawReg, lt, outputStream);
}


std::string CodeGen::genFieldChainAddressing(const std::vector<std::string>& parts, std::ostream& outputStream, std::string& outputType) {
    std::vector<std::string> resolvedParts = parts;
    if (resolvedParts[0] == "super") resolvedParts[0] = currentSelfRef;
    Symbol& base = symbolTable.get(resolvedParts[0]);
    std::vector<std::string> rest(resolvedParts.begin() + 1, resolvedParts.end());
    return genFieldChainFromAddress(base.llvmRegister, base.type, rest, outputStream, outputType);
}

std::string CodeGen::genFieldChainFromAddress(std::string currentPointer, std::string currentType, const std::vector<std::string>& parts, std::ostream& outputStream, std::string& outputType) {
    for (size_t i = 0; i < parts.size(); ++i) {
        ClassLayout& layout = classLayouts.at(currentType);
        std::string objReg = llvm.__emitLoad(llvm.__pointerType(layout.llvmStructType), currentPointer, outputStream);
        int fieldIdx = layout.fieldIndex.at(parts[i]);
        currentPointer = llvm.__emitFieldGEP(layout.llvmStructType, objReg, fieldIdx, outputStream);
        currentType = layout.fields[fieldIdx].first;
    }
    outputType = currentType;
    return currentPointer;
}

std::string CodeGen::genExpression(ASTNode* node, const std::string& expectedType, std::ostream& outputStream) {
    if (!node) return "0";
    switch (node->type) {
        case NodeType::NULL_LIT:        return "null";
        case NodeType::INT_LIT:
        case NodeType::DECIMAL_LIT:
        case NodeType::BOOL_LIT:
        case NodeType::CHAR_LIT:
        case NodeType::STRING_LIT:      return genLiteral(node, expectedType, outputStream);
        case NodeType::IDENT:           return genIdentExpr(node, expectedType, outputStream);
        case NodeType::GROUPED_EXPR:    return genExpression(static_cast<GroupedExprNode*>(node)->expression.get(), expectedType, outputStream);
        case NodeType::UNARY_EXPR:      return genUnaryExpr(node, expectedType, outputStream);
        case NodeType::BETWEEN_EXPR:    return genBetweenExpr(node, outputStream);
        case NodeType::BINARY_EXPR:     return genBinaryExpr(node, expectedType, outputStream);
        case NodeType::TERNARY_EXPR:    return genTernaryExpr(node, outputStream);
        case NodeType::ASSIGNMENT_EXPR: return genAssignmentExpr(node, outputStream);
        case NodeType::CAST_EXPR:       return genCastExpr(node, outputStream);
        case NodeType::INDEX_EXPR:      return genIndexExpr(node, outputStream);
        case NodeType::CALL_EXPR:       return genCallExpr(node, outputStream);
        case NodeType::NEW_EXPR:        return genNewExpr(node, outputStream);
        case NodeType::REF_EXPR:        return genRefExpr(node, expectedType, outputStream);
        default:                        return "0";
    }
}