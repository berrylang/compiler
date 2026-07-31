#include "../codegen.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/literals.h"
#include "../../sema/symboltable.h"
#include "../../parser/ast/functions.h"
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

std::string CodeGen::genLiteral(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    std::string lt = llvmType(expectedType);
    bool isFloat = (expectedType == "float" || expectedType == "double");

    if (node->type == NodeType::INT_LIT) {
        auto* lit = static_cast<IntLitNode*>(node);
        if (isFloat) {
            return llvm.__emitConvert("sitofp", "i32", std::to_string(lit->value), lt, out);
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
        return llvm.__emitCall("i8*", "bery_string_from_literal", {{"i8*", constExpr}}, out);
    }

    return "0";
}
std::string CodeGen::genIdentExpr(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    auto* ident = static_cast<IdentNode*>(node);

    size_t dot = ident->name.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(ident->name);

        if (parts.back() == "len") {
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string headType;
            std::string headPtr = genFieldChainAddressing(headParts, out, headType);
            if (headType == "string") {
                llvm.__declareExternFn("i64", "bery_string_length", {"i8*"});
                std::string strReg = llvm.__emitLoad("i8*", headPtr, out);
                std::string lenReg = llvm.__emitCall("i64", "bery_string_length", {{"i8*", strReg}}, out);
                return llvm.__emitSext("i64", lenReg, "i32", out);
            } if (headType.size() > 6 && headType.substr(0, 6) == "array<") {
                llvm.__declareExternFn("i64", "bery_array_length", {"i8*"});
                std::string arrReg = llvm.__emitLoad("i8*", headPtr, out);
                std::string lenReg = llvm.__emitCall("i64", "bery_array_length", {{"i8*", arrReg}}, out);
                return llvm.__emitConvert("trunc", "i64", lenReg, "i32", out);
            }
        }

        std::string chainType;
        std::string chainPtr = genFieldChainAddressing(parts, out, chainType);
        std::string flt = llvmType(chainType);
        std::string valReg = llvm.__emitLoad(flt, chainPtr, out);

        bool isParentFloat = (expectedType == "float" || expectedType == "double");
        if (isParentFloat && (chainType == "int" || chainType == "bigint")) {
            return llvm.__emitConvert("sitofp", flt, valReg, llvmType(expectedType), out);
        }
        return valReg;
    }
    Symbol& sym = symTable.get(ident->name);
    std::string realType = sym.type;
    if (realType.back() == ']')
        realType = realType.substr(0, realType.find('['));

    std::string realLT = llvmType(realType);

    if (expectedType == "ptr") {
        return sym.llvmRegister;
    }
    std::string reg = llvm.__emitLoad(realLT, sym.llvmRegister, out);

    bool isParentFloat = (expectedType == "float" || expectedType == "double");
    if (isParentFloat && (realType == "int" || realType == "bigint")) {
        return llvm.__emitConvert("sitofp", realLT, reg, llvmType(expectedType), out);
    }

    return reg;
}

std::string CodeGen::genUnaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    auto* unary = static_cast<UnaryExprNode*>(node);
    std::string lt = llvmType(expectedType);
    bool isFloat = (expectedType == "float" || expectedType == "double");

    if (unary->optr == "-" || unary->optr == "!" || unary->optr == "~") {
        std::string opReg = genExpression(unary->operand.get(), expectedType, out);
        if (unary->optr == "-")
            return llvm.__emitBinaryOp("-", lt, isFloat, isFloat ? "0.0" : "0", opReg, out);
        if (unary->optr == "!")
            return llvm.__emitBinaryOp("^", "i1", false, opReg, "1", out);
        return llvm.__emitBinaryOp("^", lt, false, opReg, "-1", out);
    }

    if (unary->operand->type != NodeType::IDENT) return "0";
    auto* ident = static_cast<IdentNode*>(unary->operand.get());
    std::string memPtr = symTable.get(ident->name).llvmRegister;
    std::string oldReg = genExpression(unary->operand.get(), expectedType, out);
    std::string one = isFloat ? "1.0" : "1";
    bool isIncrement = (unary->optr == "++" || unary->optr == "post++");
    std::string newRegVal = llvm.__emitBinaryOp(isIncrement ? "+" : "-", lt, isFloat, oldReg, one, out);
    llvm.__emitStore(lt, newRegVal, memPtr, out);
    return (unary->optr == "post++" || unary->optr == "post--") ? oldReg : newRegVal;
}

std::string CodeGen::genBetweenExpr(ASTNode* node, std::ostream& out) {
    auto* bet = static_cast<BetweenExprNode*>(node);
    std::string opLT = llvmType(bet->resolvedType);
    bool isFloat = (bet->resolvedType == "float" || bet->resolvedType == "double");

    std::string tReg = genExpression(bet->value.get(), bet->resolvedType, out);
    std::string lReg = genExpression(bet->lower.get(), bet->resolvedType, out);
    std::string uReg = genExpression(bet->upper.get(), bet->resolvedType, out);

    std::string cmp1 = llvm.__emitBinaryOp(">=", opLT, isFloat, tReg, lReg, out);
    std::string cmp2 = llvm.__emitBinaryOp("<=", opLT, isFloat, tReg, uReg, out);
    std::string andReg = llvm.__emitBinaryOp("&", "i1", false, cmp1, cmp2, out);

    if (bet->isNegated) {
        return llvm.__emitBinaryOp("^", "i1", false, andReg, "1", out);
    }
    return andReg;
}

std::string CodeGen::genBinaryExpr(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    auto* binary = static_cast<BinaryExprNode*>(node);

    std::string opType = binary->left->resolvedType;
    if (opType.empty() || opType == "unknown") {
        opType = binary->resolvedType;
    }

    if (binary->optr == "&&" || binary->optr == "||") {
        std::string resAlloc = llvm.__emitAlloca("i1", out);
        std::string lReg = genExpression(binary->left.get(), "bool", out);
        llvm.__emitStore("i1", lReg, resAlloc, out);
        int id = llvm.__uniqueId();
        std::string rightBlk = llvm.__labelWithId("logic_right", id);
        std::string endBlk = llvm.__labelWithId("logic_end", id);
        if (binary->optr == "&&")
            llvm.__emitCondBr(lReg, rightBlk, endBlk, out);
        else
            llvm.__emitCondBr(lReg, endBlk, rightBlk, out);
        llvm.__emitLabel(rightBlk, out);
        std::string rReg = genExpression(binary->right.get(), "bool", out);
        llvm.__emitStore("i1", rReg, resAlloc, out);
        llvm.__emitBr(endBlk, out);
        llvm.__emitLabel(endBlk, out);
        return llvm.__emitLoad("i1", resAlloc, out);
    }
    if (binary->resolvedType == "string") {
        std::string lReg = genExpression(binary->left.get(), "string", out);
        std::string rReg = genExpression(binary->right.get(), "string", out);
        if (binary->optr == "+") {
            llvm.__declareExternFn("i8*", "bery_string_concat", {"i8*", "i8*"});
            return llvm.__emitCall("i8*", "bery_string_concat", {{"i8*", lReg}, {"i8*", rReg}}, out);
        }
        llvm.__declareExternFn("i1", "bery_string_equals", {"i8*", "i8*"});
        std::string eqReg = llvm.__emitCall("i1", "bery_string_equals", {{"i8*", lReg}, {"i8*", rReg}}, out);
        if (binary->optr == "!=") {
            return llvm.__emitBinaryOp("^", "i1", false, eqReg, "1", out);
        }
        return eqReg;
    }

    std::string opLT = llvmType(opType);
    bool isOpFloat = (opType == "float" || opType == "double");
    if (binary->optr == "**") {
        std::string lReg = genExpression(binary->left.get(), binary->resolvedType, out);
        std::string rReg = genExpression(binary->right.get(), binary->resolvedType, out);
        return llvm.__emitPow(opLT, isOpFloat, lReg, rReg, out);
    }
    std::string lReg = genExpression(binary->left.get(), opType, out);
    std::string rReg = genExpression(binary->right.get(), opType, out);
    std::string resReg = llvm.__emitBinaryOp(binary->optr, opLT, isOpFloat, lReg, rReg, out);
    if (resReg == "0") return "0";
    bool expectFloat = (expectedType == "float" || expectedType == "double");
    bool gotInt = (binary->resolvedType == "int" || binary->resolvedType == "bigint");
    if (expectFloat && gotInt) {
        return llvm.__emitConvert("sitofp", opLT, resReg, llvmType(expectedType), out);
    }
    return resReg;
}

std::string CodeGen::genTernaryExpr(ASTNode* node, std::ostream& out) {
    auto* tern = static_cast<TernaryExprNode*>(node);
    std::string llvmRT = llvmType(tern->resolvedType);
    std::string resAlloc = llvm.__emitAlloca(llvmRT, out);
    std::string condReg = genExpression(tern->condition.get(), "bool", out);

    int id = llvm.__uniqueId();
    std::string trueBlk = llvm.__labelWithId("tern_true", id);
    std::string falseBlk = llvm.__labelWithId("tern_false", id);
    std::string endBlk = llvm.__labelWithId("tern_end", id);

    llvm.__emitCondBr(condReg, trueBlk, falseBlk, out);

    llvm.__emitLabel(trueBlk, out);
    std::string tReg = genExpression(tern->trueExpr.get(), tern->resolvedType, out);
    llvm.__emitStore(llvmRT, tReg, resAlloc, out);
    llvm.__emitBr(endBlk, out);

    llvm.__emitLabel(falseBlk, out);
    std::string fReg = genExpression(tern->falseExpr.get(), tern->resolvedType, out);
    llvm.__emitStore(llvmRT, fReg, resAlloc, out);
    llvm.__emitBr(endBlk, out);

    llvm.__emitLabel(endBlk, out);
    return llvm.__emitLoad(llvmRT, resAlloc, out);
}


std::string CodeGen::genAssignmentExpr(ASTNode* node, std::ostream& out) {
    auto* assign = static_cast<AssignmentExprNode*>(node);
    std::string targetLT, memPtr, targetBerryType;

    if (assign->target->type == NodeType::IDENT) {
        auto* ident = static_cast<IdentNode*>(assign->target.get());
        size_t dot = ident->name.find('.');

        if (dot != std::string::npos) {
            std::vector<std::string> parts = splitDots(ident->name);
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string chainType;
            std::string chainPtr = genFieldChainAddressing(headParts, out, chainType);
            ClassLayout& layout = classLayouts.at(chainType);
            int fieldIdx = layout.fieldIndex.at(parts.back());
            targetLT = llvmType(layout.fields[fieldIdx].first);
            targetBerryType = layout.fields[fieldIdx].first;

            std::string objReg = llvm.__emitLoad(llvm.__pointerType(layout.llvmStructType), chainPtr, out);
            memPtr = llvm.__emitFieldGEP(layout.llvmStructType, objReg, fieldIdx, out);
        } else {
            Symbol& sym = symTable.get(ident->name);
            targetLT = llvmType(sym.type);
            memPtr = sym.llvmRegister;
            targetBerryType = sym.type;

            if (sym.type == "string" && assign->op == "=") {
                llvm.__declareExtern("declare i8* @bery_string_copy(i8*)", "bery_string_copy");
                std::string srcReg = genExpression(assign->value.get(), "string", out);
                std::string copyReg = llvm.__emitCall("i8*", "bery_string_copy", {{"i8*", srcReg}}, out);
                llvm.__emitStore("i8*", copyReg, memPtr, out);
                return copyReg;
            }
        }

    } else if (assign->target->type == NodeType::INDEX_EXPR) {
        auto* idxNode = static_cast<IndexExprNode*>(assign->target.get());
        Symbol& sym = symTable.get(idxNode->name);

        if (sym.type.size() > 6 && sym.type.substr(0, 6) == "array<") {
            std::string elemType = sym.type.substr(6, sym.type.size() - 7);
            std::string lt = llvmType(elemType);
            std::string arrReg = llvm.__emitLoad("i8*", sym.llvmRegister, out);
            std::string idxReg = genExpression(idxNode->indices[0].get(), "int", out);
            std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", out);

            if (!idxNode->memberChain.empty()) {
                // objArr[i].x = value; - fetch the boxed element (holds the object
                // pointer), walk the field chain from there, then fall through to
                // the normal store logic below like any other assignment target.
                llvm.__declareExternFn("i8*", "bery_array_get", {"i8*", "i64"});
                std::string rawReg = llvm.__emitCall("i8*", "bery_array_get", {{"i8*", arrReg}, {"i64", idxExt}}, out);
                std::string castReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(lt), out);
                std::string finalType;
                memPtr = genFieldChainFromAddress(castReg, elemType, idxNode->memberChain, out, finalType);
                targetLT = llvmType(finalType);
                targetBerryType = finalType;
            } else {
                llvm.__declareExternFn("void", "bery_array_set", {"i8*", "i64", "i8*"});
                std::string valReg = genExpression(assign->value.get(), elemType, out);
                std::string boxedReg = llvm.__emitBoxValue(lt, valReg, out);
                llvm.__emitCall("void", "bery_array_set", {{"i8*", arrReg}, {"i64", idxExt}, {"i8*", boxedReg}}, out);
                return valReg;
            }
        } else {
            std::string baseType = sym.type.substr(0, sym.type.find('['));
            std::string arrType = sym.llvmAllocType;
            std::vector<std::pair<std::string, std::string>> indices;
            indices.push_back({"i32", "0"});
            for (auto& idx : idxNode->indices)
                indices.push_back({"i32", genExpression(idx.get(), "int", out)});
            std::string ptrReg = llvm.__emitTypedGEP(arrType, sym.llvmRegister, indices, false, out);

            if (!idxNode->memberChain.empty()) {
                std::string finalType;
                memPtr = genFieldChainFromAddress(ptrReg, baseType, idxNode->memberChain, out, finalType);
                targetLT = llvmType(finalType);
                targetBerryType = finalType;
            } else {
                targetLT = llvmType(baseType);
                targetBerryType = baseType;
                memPtr = ptrReg;
            }
        }
    }

    std::string valReg = genExpression(assign->value.get(), targetBerryType, out);

    if (assign->op == "=") {
        llvm.__emitStore(targetLT, valReg, memPtr, out);
        return valReg;
    }
    else if (assign->op == "+=" && targetBerryType == "string") {
        llvm.__declareExternFn("i8*", "bery_string_copy", {"i8*"});
        std::string currVal = llvm.__emitLoad("i8*", memPtr, out);
        std::string concatReg = llvm.__emitCall("i8*", "bery_string_concat", {{"i8*", currVal}, {"i8*", valReg}}, out);
        llvm.__emitStore("i8*", concatReg, memPtr, out);
        return concatReg;
    }

    bool isFloat = (targetBerryType == "float" || targetBerryType == "double");
    std::string curVal = llvm.__emitLoad(targetLT, memPtr, out);
    std::string resReg;

    if (assign->op == "**=") {
        resReg = llvm.__emitPow(targetLT, isFloat, curVal, valReg, out);
    } else {
        std::string baseOp = assign->op.substr(0, assign->op.size() - 1);
        resReg = llvm.__emitBinaryOp(baseOp, targetLT, isFloat, curVal, valReg, out);
    }

    llvm.__emitStore(targetLT, resReg, memPtr, out);
    return resReg;
}

std::string CodeGen::genCastExpr(ASTNode* node, std::ostream& out) {
    auto* castNode = static_cast<CastExprNode*>(node);
    std::string srcReg = genExpression(castNode->expr.get(), castNode->srcType, out);
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

        if (sType == "int")    return llvm.__emitCall("i8*", "bery_to_string_int", {{"i32", srcReg}}, out);
        if (sType == "bigint") return llvm.__emitCall("i8*", "bery_to_string_bigint", {{"i64", srcReg}}, out);
        if (sType == "float")  return llvm.__emitCall("i8*", "bery_to_string_float", {{"float", srcReg}}, out);
        if (sType == "double") return llvm.__emitCall("i8*", "bery_to_string_double", {{"double", srcReg}}, out);
        if (sType == "char")   return llvm.__emitCall("i8*", "bery_to_string_char", {{"i8", srcReg}}, out);
        if (sType == "bool")   return llvm.__emitCall("i8*", "bery_to_string_bool", {{"i1", srcReg}}, out);
    }

    bool srcFloat = (sType == "float" || sType == "double");
    bool tgtFloat = (tType == "float" || tType == "double");

    if (srcFloat && !tgtFloat)
        return llvm.__emitConvert("fptosi", sLLVM, srcReg, tLLVM, out);
    if (!srcFloat && tgtFloat)
        return llvm.__emitConvert("sitofp", sLLVM, srcReg, tLLVM, out);
    if (srcFloat && tgtFloat) {
        if (sType == "float")
            return llvm.__emitConvert("fpext", "float", srcReg, "double", out);
        return llvm.__emitConvert("fptrunc", "double", srcReg, "float", out);
    }
    int sW = (sType == "bigint") ? 64 : (sType == "int" ? 32 : (sType == "char" ? 8 : 1));
    int tW = (tType == "bigint") ? 64 : (tType == "int" ? 32 : (tType == "char" ? 8 : 1));
    if (sW > tW)
        return llvm.__emitConvert("trunc", sLLVM, srcReg, tLLVM, out);
    if (sW < tW)
        return llvm.__emitConvert(sType == "bool" ? "zext" : "sext", sLLVM, srcReg, tLLVM, out);
    return srcReg;
}
std::string CodeGen::genIndexExpr(ASTNode* node, std::ostream& out) {
    auto* idx = static_cast<IndexExprNode*>(node);
    Symbol& sym = symTable.get(idx->name);
    if (sym.type == "string") {
        llvm.__declareExternFn("i8", "bery_string_char_at", {"i8*", "i64"});
        std::string str = llvm.__emitLoad("i8*", sym.llvmRegister, out);
        std::string index = genExpression(idx->indices[0].get(), "int", out);
        std::string index64 = llvm.__emitSext("i32", index, "i64", out);
        return llvm.__emitCall("i8", "bery_string_char_at", {{"i8*", str}, {"i64", index64}}, out);
    }
    if (sym.type.size() > 6 && sym.type.substr(0, 6) == "array<") {
        std::string elemType = sym.type.substr(6, sym.type.size() - 7);
        std::string lt = llvmType(elemType);
        llvm.__declareExternFn("i8*", "bery_array_get", {"i8*", "i64"});
        std::string arrReg = llvm.__emitLoad("i8*", sym.llvmRegister, out);
        std::string idxReg = genExpression(idx->indices[0].get(), "int", out);
        std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", out);
        std::string rawReg = llvm.__emitCall("i8*", "bery_array_get", {{"i8*", arrReg}, {"i64", idxExt}}, out);
        std::string castReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(lt), out);
        if (!idx->memberChain.empty()) {
            std::string finalType;
            std::string fieldPtr = genFieldChainFromAddress(castReg, elemType, idx->memberChain, out, finalType);
            return llvm.__emitLoad(llvmType(finalType), fieldPtr, out);
        }
        return llvm.__emitLoad(lt, castReg, out);
    }

    std::string baseType = sym.type.substr(0, sym.type.find('['));
    std::string lt = llvmType(baseType);
    std::string arrType = sym.llvmAllocType;
    std::vector<std::pair<std::string, std::string>> indices;
    indices.push_back({"i32", "0"});
    for (auto& i : idx->indices)
        indices.push_back({"i32", genExpression(i.get(), "int", out)});
    std::string ptrReg = llvm.__emitTypedGEP(arrType, sym.llvmRegister, indices, false, out);
    if (!idx->memberChain.empty()) {
        std::string finalType;
        std::string fieldPtr = genFieldChainFromAddress(ptrReg, baseType, idx->memberChain, out, finalType);
        return llvm.__emitLoad(llvmType(finalType), fieldPtr, out);
    }
    return llvm.__emitLoad(lt, ptrReg, out);
}

std::string CodeGen::genCallExpr(ASTNode* node, std::ostream& out) {
    auto* call = static_cast<CallExprNode*>(node);

    static const std::unordered_set<std::string> breFns = {
        "print", "println", "inputInt", "inputBigInt", "inputFloat",
        "inputDouble", "inputBool", "inputChar", "inputString"
    };
    if (breFns.count(call->callee))
        return genBREPrintCall(node, out);

    size_t dot = call->callee.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(call->callee);
        std::string method = parts.back();
        std::vector<std::string> headParts(parts.begin(), parts.end() - 1);

        std::string objType;
        std::string objPtr = genFieldChainAddressing(headParts, out, objType);

        if (objType == "string") {
            std::string strReg = llvm.__emitLoad("i8*", objPtr, out);
            if (method == "len") {
                llvm.__declareExternFn("i64", "bery_string_length", {"i8*"});
                std::string lenReg = llvm.__emitCall("i64", "bery_string_length", {{"i8*", strReg}}, out);
                return llvm.__emitConvert("trunc", "i64", lenReg, "i32", out);
            }
            if (method == "copy") {
                llvm.__declareExternFn("i8*", "bery_string_copy", {"i8*"});
                return llvm.__emitCall("i8*", "bery_string_copy", {{"i8*", strReg}}, out);
            }
            if (method == "substr") {
                llvm.__declareExternFn("i8*", "bery_string_substring", {"i8*", "i64", "i64"});
                std::string s0 = genExpression(call->arguments[0].get(), "int", out);
                std::string s1 = genExpression(call->arguments[1].get(), "int", out);
                std::string e0 = llvm.__emitSext("i32", s0, "i64", out);
                std::string e1 = llvm.__emitSext("i32", s1, "i64", out);
                return llvm.__emitCall("i8*", "bery_string_substring", {{"i8*", strReg}, {"i64", e0}, {"i64", e1}}, out);
            }
        }

        if (objType.size() > 6 && objType.substr(0, 6) == "array<") {
            std::string elemType = objType.substr(6, objType.size() - 7);
            std::string lt = llvmType(elemType);
            std::string arrReg = llvm.__emitLoad("i8*", objPtr, out);

            if (method == "len") {
                llvm.__declareExternFn("i64", "bery_array_length", {"i8*"});
                std::string lenReg = llvm.__emitCall("i64", "bery_array_length", {{"i8*", arrReg}}, out);
                return llvm.__emitConvert("trunc", "i64", lenReg, "i32", out);
            }
            if (method == "push") {
                llvm.__declareExternFn("void", "bery_array_push", {"i8*", "i8*"});
                std::string valReg = genExpression(call->arguments[0].get(), elemType, out);
                std::string castReg = llvm.__emitBoxValue(lt, valReg, out);
                llvm.__emitCall("void", "bery_array_push", {{"i8*", arrReg}, {"i8*", castReg}}, out);
                return "0";
            }
            if (method == "pop") {
                llvm.__declareExternFn("i8*", "bery_array_pop", {"i8*"});
                std::string rawReg = llvm.__emitCall("i8*", "bery_array_pop", {{"i8*", arrReg}}, out);
                std::string castReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(lt), out);
                return llvm.__emitLoad(lt, castReg, out);
            }
            if (method == "insert") {
                llvm.__declareExternFn("void", "bery_array_insert", {"i8*", "i64", "i8*"});
                std::string idxReg = genExpression(call->arguments[0].get(), "int", out);
                std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", out);
                std::string valReg = genExpression(call->arguments[1].get(), elemType, out);
                std::string castReg = llvm.__emitBoxValue(lt, valReg, out);
                llvm.__emitCall("void", "bery_array_insert", {{"i8*", arrReg}, {"i64", idxExt}, {"i8*", castReg}}, out);
                return "0";
            }
            if (method == "remove") {
                llvm.__declareExternFn("void", "bery_array_remove", {"i8*", "i64"});
                std::string idxReg = genExpression(call->arguments[0].get(), "int", out);
                std::string idxExt = llvm.__emitSext("i32", idxReg, "i64", out);
                llvm.__emitCall("void", "bery_array_remove", {{"i8*", arrReg}, {"i64", idxExt}}, out);
                return "0";
            }
        }

        if (classLayouts.count(objType)) {
            std::string mangled = llvm.__mangleMethod(objType, method);
            if (functions.count(mangled)) {
                CodeGenFunctionSignature& sig = functions[mangled];
                std::string classPtrType = llvm.__pointerType(classLayouts.at(objType).llvmStructType);
                std::string receiverReg = llvm.__emitLoad(classPtrType, objPtr, out);

                std::vector<std::pair<std::string, std::string>> args;
                args.push_back({classPtrType, receiverReg});
            

                if (sig.returnType.empty() || sig.returnType == "void") {
                    llvm.__emitCall("void", mangled, args, out);
                    return "0";
                }
                return llvm.__emitCall(llvmType(sig.returnType), mangled, args, out);
            }
        }
        return "0";
    }
    if (!currentClassName.empty() && classLayouts.count(currentClassName)) {
        std::string mangled = llvm.__mangleMethod(currentClassName, call->callee);
        if (functions.count(mangled)) {
            CodeGenFunctionSignature& sig = functions[mangled];
            Symbol& selfSym = symTable.get(currentSelfRef);
            std::string classPtrType = llvm.__pointerType(classLayouts.at(currentClassName).llvmStructType);
            std::string receiverReg = llvm.__emitLoad(classPtrType, selfSym.llvmRegister, out);

            std::vector<std::pair<std::string, std::string>> args;
            args.push_back({classPtrType, receiverReg});

            if (sig.returnType.empty() || sig.returnType == "void") {
                llvm.__emitCall("void", mangled, args, out);
                return "0";
            }
            return llvm.__emitCall(llvmType(sig.returnType), mangled, args, out);
        }
    }
    if (functions.find(call->callee) == functions.end()) return "0";
    CodeGenFunctionSignature& sig = functions[call->callee];

    std::vector<std::pair<std::string, std::string>> args;
    for (size_t i = 0; i < call->arguments.size(); ++i) {
        std::string argReg = genExpression(call->arguments[i].get(), sig.paramTypes[i], out);
        args.push_back({llvmType(sig.paramTypes[i]), argReg});
    }

    if (sig.returnType == "void") {
        llvm.__emitCall("void", call->callee, args, out);
        return "0";
    }
    return llvm.__emitCall(llvmType(sig.returnType), call->callee, args, out);
}

std::string CodeGen::genNewExpr(ASTNode* node, std::ostream& out) {
    auto* newExpr = static_cast<NewExprNode*>(node);
    auto it = classLayouts.find(newExpr->className);
    if (it == classLayouts.end()) return "null";
    ClassLayout& layout = it->second;

    llvm.__declareExternFn("i8*", "bery_alloc", {"i64", "i32"});
    std::string typeIdReg = llvm.__emitLoad("i32", llvm.__globalRef(newExpr->className, "_typeid"), out);
    std::string rawReg = llvm.__emitCall("i8*", "bery_alloc", {{"i64", std::to_string(layout.instanceSize)}, {"i32", typeIdReg}}, out);

    std::string objReg = llvm.__emitBitcast("i8*", rawReg, llvm.__pointerType(layout.llvmStructType), out);
    for (size_t i = 0; i < layout.fields.size(); ++i) {
        std::string flt = llvmType(layout.fields[i].first);
        std::string gepReg = llvm.__emitFieldGEP(layout.llvmStructType, objReg, (int)i, out);
        if (layout.fieldInitializers[i]) {
            std::string valReg = genExpression(layout.fieldInitializers[i], layout.fields[i].first, out);
            llvm.__emitStore(flt, valReg, gepReg, out);
        } else {
            bool isPtr = !flt.empty() && flt.back() == '*';
            std::string zeroVal = (flt == "float" || flt == "double") ? "0.0" : (isPtr ? "null" : "0");
            llvm.__emitStore(flt, zeroVal, gepReg, out);
        }
    }

    if (layout.hasConstructor) {
        std::string mangled = llvm.__mangleConstructor(newExpr->className);
        CodeGenFunctionSignature& sig = functions[mangled];
        std::vector<std::pair<std::string, std::string>> args;
        args.push_back({llvm.__pointerType(layout.llvmStructType), objReg});
        for (size_t i = 0; i < newExpr->arguments.size(); ++i) {
            std::string argReg = genExpression(newExpr->arguments[i].get(), sig.paramTypes[i + 1], out);
            args.push_back({llvmType(sig.paramTypes[i + 1]), argReg});
        }
        llvm.__emitCall("void", mangled, args, out);
    }

    return objReg;
}

std::string CodeGen::genFieldChainAddressing(const std::vector<std::string>& parts, std::ostream& out, std::string& outType) {
    Symbol& base = symTable.get(parts[0]);
    std::vector<std::string> rest(parts.begin() + 1, parts.end());
    return genFieldChainFromAddress(base.llvmRegister, base.type, rest, out, outType);
}

std::string CodeGen::genFieldChainFromAddress(std::string curPtr, std::string curType, const std::vector<std::string>& parts, std::ostream& out, std::string& outType) {
    for (size_t i = 0; i < parts.size(); ++i) {
        ClassLayout& layout = classLayouts.at(curType);
        std::string objReg = llvm.__emitLoad(llvm.__pointerType(layout.llvmStructType), curPtr, out);
        int fieldIdx = layout.fieldIndex.at(parts[i]);
        curPtr = llvm.__emitFieldGEP(layout.llvmStructType, objReg, fieldIdx, out);
        curType = layout.fields[fieldIdx].first;
    }
    outType = curType;
    return curPtr;
}

std::string CodeGen::genExpression(ASTNode* node, const std::string& expectedType, std::ostream& out) {
    if (!node) return "0";
    switch (node->type) {
        case NodeType::NULL_LIT:        return "null";
        case NodeType::INT_LIT:
        case NodeType::DECIMAL_LIT:
        case NodeType::BOOL_LIT:
        case NodeType::CHAR_LIT:
        case NodeType::STRING_LIT:      return genLiteral(node, expectedType, out);
        case NodeType::IDENT:           return genIdentExpr(node, expectedType, out);
        case NodeType::GROUPED_EXPR:    return genExpression(static_cast<GroupedExprNode*>(node)->expression.get(), expectedType, out);
        case NodeType::UNARY_EXPR:      return genUnaryExpr(node, expectedType, out);
        case NodeType::BETWEEN_EXPR:    return genBetweenExpr(node, out);
        case NodeType::BINARY_EXPR:     return genBinaryExpr(node, expectedType, out);
        case NodeType::TERNARY_EXPR:    return genTernaryExpr(node, out);
        case NodeType::ASSIGNMENT_EXPR: return genAssignmentExpr(node, out);
        case NodeType::CAST_EXPR:       return genCastExpr(node, out);
        case NodeType::INDEX_EXPR:      return genIndexExpr(node, out);
        case NodeType::CALL_EXPR:       return genCallExpr(node, out);
        case NodeType::NEW_EXPR:        return genNewExpr(node, out);
        default:                        return "0";
    }
}