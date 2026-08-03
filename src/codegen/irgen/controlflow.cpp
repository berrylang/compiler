#include "../codegen.h"
#include "../../parser/ast/controlflow.h"
#include "../../parser/ast/blocknode.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/literals.h"
#include "../../sema/symboltable.h"
#include <iostream>

void CodeGen::genBlock(ASTNode* node, std::ostream& outputStream) {
    auto* block = static_cast<BlockNode*>(node);
    symbolTable.pushScope(); 
    pushGCScope();
    for (auto& statement : block->statements) {
        genStatement(statement.get(), outputStream);
    }
    int roots = popGCScope();
    emitGCPops(roots, outputStream);
    symbolTable.popScope();
}

void CodeGen::genIfStmt(ASTNode* node, std::ostream& outputStream) {
    auto* ifStmt = static_cast<IfStmtNode*>(node);
    std::string conditionReg = genExpression(ifStmt->conditions.get(), "bool", outputStream);
    int blockId = llvm.__uniqueId();
    std::string thenLabel = llvm.__labelWithId("if_then", blockId);
    std::string elseLabel = llvm.__labelWithId("if_else", blockId);
    std::string endLabel = llvm.__labelWithId("if_end", blockId);

    if (ifStmt->elseBranch) llvm.__emitCondBr(conditionReg, thenLabel, elseLabel, outputStream);
    else llvm.__emitCondBr(conditionReg, thenLabel, endLabel, outputStream);
    llvm.__emitLabel(thenLabel, outputStream);
    genBlock(ifStmt->ifBranch.get(), outputStream);
    llvm.__emitBr(endLabel, outputStream);

    if (ifStmt->elseBranch) {
        llvm.__emitLabel(elseLabel, outputStream);
        if (ifStmt->elseBranch->type == NodeType::IF_STMT) genIfStmt(ifStmt->elseBranch.get(), outputStream);
        else genBlock(ifStmt->elseBranch.get(), outputStream);
        llvm.__emitBr(endLabel, outputStream);
    }
    llvm.__emitLabel(endLabel, outputStream);
}

void CodeGen::genWhileStmt(ASTNode* node, std::ostream& outputStream) {
    auto* whilestmt = static_cast<WhileStmtNode*>(node);
    int blockid = llvm.__uniqueId();
    std::string conditionLabel = llvm.__labelWithId("while_cond", blockid);
    std::string bodyLabel = llvm.__labelWithId("while_body", blockid);
    std::string endLabel = llvm.__labelWithId("while_end", blockid);

    breakTracker.push_back(endLabel);
    continueTracker.push_back(conditionLabel);
    llvm.__emitBr(conditionLabel, outputStream);
    llvm.__emitLabel(conditionLabel, outputStream);
    std::string conditionReg = genExpression(whilestmt->condition.get(), "bool", outputStream);
    llvm.__emitCondBr(conditionReg, bodyLabel, endLabel, outputStream);
    llvm.__emitLabel(bodyLabel, outputStream);
    genBlock(whilestmt->body.get(), outputStream);
    llvm.__emitBr(conditionLabel, outputStream);
    llvm.__emitLabel(endLabel, outputStream);
    continueTracker.pop_back();
    breakTracker.pop_back();
}

void CodeGen::genDoWhileStmt(ASTNode* node, std::ostream& outputStream) {
    auto* dowhile = static_cast<DoWhileStmtNode*>(node);
    int blockId = llvm.__uniqueId();
    std::string bodyLabel = llvm.__labelWithId("dowhile_body", blockId);
    std::string conditionLabel = llvm.__labelWithId("dowhile_cond", blockId);
    std::string endLabel = llvm.__labelWithId("dowhile_end", blockId);
    breakTracker.push_back(endLabel);
    continueTracker.push_back(conditionLabel);
    llvm.__emitBr(bodyLabel, outputStream);
    llvm.__emitLabel(bodyLabel, outputStream);
    genBlock(dowhile->body.get(), outputStream);
    continueTracker.pop_back();
    llvm.__emitBr(conditionLabel, outputStream);
    llvm.__emitLabel(conditionLabel, outputStream);
    std::string conditionReg = genExpression(dowhile->condition.get(), "bool", outputStream);

    llvm.__emitCondBr(conditionReg, bodyLabel, endLabel, outputStream);
    llvm.__emitLabel(endLabel, outputStream);

    breakTracker.pop_back();
}


void CodeGen::genSwitchStmt(ASTNode* node, std::ostream& outputStream) {
    auto* sw = static_cast<SwitchStmtNode*>(node);

    std::string conditionReg = genExpression(sw->condition.get(), "any", outputStream);
    std::string condType = sw->condition->resolvedType; 
    if (condType.empty() || condType == "unknown") { condType = "int"; } 
    std::string llvmCondType = llvmType(condType);

    int blockId = llvm.__uniqueId();
    std::string endLabel = llvm.__labelWithId("switch_end", blockId);

    breakTracker.push_back(endLabel);
    std::string nextComparisonLabel = llvm.__indexedLabel("switch_cmp", 0, blockId);
    llvm.__emitBr(nextComparisonLabel, outputStream);

    for (size_t i = 0; i < sw->cases.size(); i++) {
        auto& c = sw->cases[i];

        std::string bodyLabel = llvm.__indexedLabel("switch_body", i, blockId);
        std::string nextComparison = (i + 1 < sw->cases.size()) ? llvm.__indexedLabel("switch_cmp", i + 1, blockId) : (sw->hasDefault ? llvm.__labelWithId("switch_default", blockId) : endLabel);
        llvm.__emitLabel(nextComparisonLabel, outputStream);
        std::string caseReg = genExpression(c.value.get(), "any", outputStream);
        std::string isMatch = llvm.__emitBinaryOp("==", llvmCondType, false, conditionReg, caseReg, outputStream);
        llvm.__emitCondBr(isMatch, bodyLabel, nextComparison, outputStream);

        nextComparisonLabel = nextComparison;

        llvm.__emitLabel(bodyLabel, outputStream);
        symbolTable.pushScope();
        pushGCScope();
        for (auto& statement : c.statements) {
            genStatement(statement.get(), outputStream);
        }
        int roots = popGCScope();
        emitGCPops(roots, outputStream);
        symbolTable.popScope();

        bool endsWithBreak = !c.statements.empty() && c.statements.back()->type == NodeType::BREAK_STMT;
        if (!endsWithBreak) {
            std::string nextBody = (i+1<sw->cases.size())?llvm.__indexedLabel("switch_body",i+1,blockId):(sw->hasDefault? llvm.__labelWithId("switch_default_body",blockId) : endLabel);
            llvm.__emitBr(nextBody, outputStream);
        }
    }

    if (sw->hasDefault) {
        llvm.__emitLabel(llvm.__labelWithId("switch_default", blockId), outputStream);
        llvm.__emitBr(llvm.__labelWithId("switch_default_body", blockId), outputStream);
        llvm.__emitLabel(llvm.__labelWithId("switch_default_body", blockId), outputStream);
        symbolTable.pushScope();
        pushGCScope();
        for (auto& statement : sw->defaultBlock) {
            genStatement(statement.get(), outputStream);
        }
        int roots = popGCScope();
        emitGCPops(roots, outputStream);
        symbolTable.popScope();

        bool endsWithBreak = !sw->defaultBlock.empty() && sw->defaultBlock.back()->type == NodeType::BREAK_STMT;
        if (!endsWithBreak) {
            llvm.__emitBr(endLabel, outputStream);
        }
    } else {
        llvm.__emitLabel(nextComparisonLabel, outputStream);
        llvm.__emitBr(endLabel, outputStream);
    }

    llvm.__emitLabel(endLabel, outputStream);
    breakTracker.pop_back();
}

void CodeGen::genBreakStmt(ASTNode* node, std::ostream& outputStream) {
    if (breakTracker.empty()) return;
    llvm.__emitBr(breakTracker.back(), outputStream);
}

void CodeGen::genContinueStmt(ASTNode* node, std::ostream& outputStream) {
    if (continueTracker.empty()) return;
    llvm.__emitBr(continueTracker.back(), outputStream);
}

void CodeGen::genForStmt(ASTNode* node, std::ostream& outputStream) {
    auto* forStmt = static_cast<ForStmtNode*>(node);

    symbolTable.pushScope();
    pushGCScope();
    for (auto& initStmt : forStmt->init) {
        genStatement(initStmt.get(), outputStream);
    }

    int blockid = llvm.__uniqueId();
    std::string conditionLabel = llvm.__labelWithId("for_cond", blockid);
    std::string bodyLabel = llvm.__labelWithId("for_body", blockid);
    std::string updateLabel = llvm.__labelWithId("for_update", blockid);
    std::string endLabel = llvm.__labelWithId("for_end", blockid);

    llvm.__emitBr(conditionLabel, outputStream);
    llvm.__emitLabel(conditionLabel, outputStream);
    if (forStmt->condition) {
        std::string conditionReg = genExpression(forStmt->condition.get(), "bool", outputStream);
        llvm.__emitCondBr(conditionReg, bodyLabel, endLabel, outputStream);
    } else {
        llvm.__emitBr(bodyLabel, outputStream);
    }
    llvm.__emitLabel(bodyLabel, outputStream);

    breakTracker.push_back(endLabel);
    continueTracker.push_back(updateLabel);

    genBlock(forStmt->body.get(), outputStream);

    continueTracker.pop_back();
    breakTracker.pop_back();

    llvm.__emitBr(updateLabel, outputStream);
    llvm.__emitLabel(updateLabel, outputStream);
    for (auto& updateExpr : forStmt->update) {
        genExpression(updateExpr.get(), "any", outputStream);
    }
    llvm.__emitBr(conditionLabel, outputStream);
    llvm.__emitLabel(endLabel, outputStream);
    symbolTable.popScope();
    int roots = popGCScope();
    emitGCPops(roots, outputStream);
}


void CodeGen::genForInStmt(ASTNode* node, std::ostream& outputStream) {
    auto* forIn = static_cast<ForInNode*>(node);
    symbolTable.pushScope();
    pushGCScope();

    if (forIn->rangeEnd) {
        std::string lt = "i32";
        bool isFloat = false;

        if (forIn->varType == "float" || forIn->varType == "double") {
            lt = "double";
            isFloat = true;
        } else if (forIn->varType == "char") {
            lt = "i8";
        } else if (forIn->varType == "bigint") {
            lt = "i64";
        }
        std::string varReg = llvm.__emitNamedAlloca(forIn->varName, lt, outputStream);
        Symbol sym;
        sym.symbolType = SymbolType::VARIABLE;
        sym.type = forIn->varType;
        sym.isInitialized = true;
        sym.line = forIn->line;
        sym.llvmRegister = varReg;
        sym.llvmAllocType = lt;
        symbolTable.add(forIn->varName, sym);
        std::string startReg = genExpression(forIn->iterableOrStart.get(), lt, outputStream);
        llvm.__emitStore(lt, startReg, varReg, outputStream);

        std::string limitReg = genExpression(forIn->rangeEnd.get(), lt, outputStream);
        std::string stepReg = isFloat ? "1.0" : "1";
        if (forIn->step) {
            stepReg = genExpression(forIn->step.get(), lt, outputStream);
        }
        int blockid = llvm.__uniqueId();
        std::string conditionLabel = llvm.__labelWithId("forin_cond", blockid);
        std::string bodyLabel = llvm.__labelWithId("forin_body", blockid);
        std::string updateLabel = llvm.__labelWithId("forin_update", blockid);
        std::string endLabel = llvm.__labelWithId("forin_end", blockid);

        llvm.__emitBr(conditionLabel, outputStream);
        llvm.__emitLabel(conditionLabel, outputStream);
        std::string currValReg = llvm.__emitLoad(lt, varReg, outputStream);
        std::string conditionReg = llvm.__emitBinaryOp("<=", lt, isFloat, currValReg, limitReg, outputStream);

        llvm.__emitCondBr(conditionReg, bodyLabel, endLabel, outputStream);
        llvm.__emitLabel(bodyLabel, outputStream);
        breakTracker.push_back(endLabel);
        continueTracker.push_back(updateLabel);
        genBlock(forIn->body.get(), outputStream);
        continueTracker.pop_back();
        breakTracker.pop_back();
        llvm.__emitBr(updateLabel, outputStream);
        llvm.__emitLabel(updateLabel, outputStream);
        std::string upLoadReg = llvm.__emitLoad(lt, varReg, outputStream);
        std::string addReg = llvm.__emitBinaryOp("+", lt, isFloat, upLoadReg, stepReg, outputStream);
        llvm.__emitStore(lt, addReg, varReg, outputStream);
        llvm.__emitBr(conditionLabel, outputStream);
        llvm.__emitLabel(endLabel, outputStream);

    } else {
        std::string iterType = forIn->iterableOrStart->resolvedType;
        std::string lt = llvmType(forIn->varType);

        std::string idxType ="i32";
        std::string varReg =  llvm.__emitNamedAlloca(forIn->varName, lt,outputStream);
        Symbol sym;
        sym.symbolType = SymbolType::VARIABLE;
        sym.type = forIn->varType;
        sym.isInitialized = true;
        sym.line = forIn->line;
        sym.llvmRegister = varReg;
        sym.llvmAllocType = lt;
        symbolTable.add(forIn->varName, sym);

        std::string idxReg = llvm.__emitNamedAlloca("forin_idx", idxType, outputStream);
        llvm.__emitStore(idxType, "0", idxReg, outputStream);

        int blockid = llvm.__uniqueId();
        std::string conditionLabel = llvm.__labelWithId("forin_cond", blockid);
        std::string bodyLabel = llvm.__labelWithId("forin_body", blockid);
        std::string updateLabel = llvm.__labelWithId("forin_update", blockid);
        std::string endLabel = llvm.__labelWithId("forin_end", blockid);

        if (iterType.size() > 6 && iterType.substr(0, 6) == "array<") {
            std::string elementType = iterType.substr(6, iterType.size() - 7);
            std::string elementLlvmType = llvmType(elementType);
            llvm.__declareExternFn("i64", "bery_array_length", {"i8*"});
            llvm.__declareExternFn("i8*", "bery_array_get", {"i8*", "i64*"});
            std::string arrReg = genExpression(forIn->iterableOrStart.get(), iterType, outputStream);
            std::string arrHold = llvm.__emitNamedAlloca("forin_arr", "i8*", outputStream);
            llvm.__emitStore("i8*", arrReg, arrHold, outputStream);
            emitGCPush(arrHold, "i8*", outputStream);

            std::string lenReg64 = llvm.__emitCall("i64", "bery_array_length", {{"i8*", arrReg}}, outputStream);
            std::string limitReg = llvm.__emitConvert("trunc", "i64", lenReg64, "i32", outputStream);
            llvm.__emitBr(conditionLabel, outputStream);
            llvm.__emitLabel(conditionLabel, outputStream);

            std::string currIdxReg = llvm.__emitLoad(idxType, idxReg, outputStream);
            std::string conditionReg = llvm.__emitBinaryOp("<", idxType, false, currIdxReg, limitReg, outputStream);
            llvm.__emitCondBr(conditionReg, bodyLabel, endLabel, outputStream);
            llvm.__emitLabel(bodyLabel, outputStream);

            std::string arrLoadReg = llvm.__emitLoad("i8*", arrHold, outputStream);
            std::string indexExt = llvm.__emitSext("i32", currIdxReg, "i64", outputStream);
            std::string rawReg = llvm.__emitCall("i8*", "bery_array_get", {{"i8*", arrLoadReg}, {"i64", indexExt}}, outputStream);
            std::string castReg = llvm.__emitBitcast("i8*", rawReg, elementLlvmType + "*", outputStream);
            std::string valReg = llvm.__emitLoad(elementLlvmType, castReg, outputStream);
            std::string finalReg = valReg;
            if (lt != elementLlvmType) {
                if ((elementLlvmType == "i32" || elementLlvmType == "i64") && (lt == "float" || lt == "double")) {
                    finalReg = llvm.__emitConvert("sitofp", elementLlvmType, valReg, lt, outputStream);
                } else if (elementLlvmType == "i32" && lt == "i64") {
                    finalReg = llvm.__emitConvert("sext", "i32", valReg, "i64", outputStream);
                } else if (elementLlvmType == "float" && lt == "double") {
                    finalReg = llvm.__emitConvert("fpext", "float", valReg, "double", outputStream);
                }
            }
            llvm.__emitStore(lt, finalReg, varReg, outputStream);

            breakTracker.push_back(endLabel);
            continueTracker.push_back(updateLabel);
            genBlock(forIn->body.get(), outputStream);
            continueTracker.pop_back();
            breakTracker.pop_back();
            llvm.__emitBr(updateLabel, outputStream);

            llvm.__emitLabel(updateLabel, outputStream);
            std::string upLoadIdx = llvm.__emitLoad(idxType, idxReg, outputStream);
            std::string addIdx = llvm.__emitBinaryOp("+", idxType, false, upLoadIdx, "1", outputStream);
            llvm.__emitStore(idxType, addIdx, idxReg, outputStream);
            llvm.__emitBr(conditionLabel, outputStream);
            llvm.__emitLabel(endLabel, outputStream);

        } else if (iterType == "string") {
            llvm.__declareExternFn("i64", "bery_string_length", {"i8*"});
            llvm.__declareExternFn("i8*","bery_string_char_at", {"i8*", "i64*"});

            std::string strReg = genExpression(forIn->iterableOrStart.get(), "string", outputStream);
            std::string strHold = llvm.__emitNamedAlloca("forin_str", "i8*", outputStream);
            llvm.__emitStore("i8*", strReg, strHold, outputStream);
            emitGCPush(strHold, "i8*", outputStream);

            std::string lenReg64 = llvm.__emitCall("i64", "bery_string_length", {{"i8*", strReg}}, outputStream);
            std::string limitReg = llvm.__emitConvert("trunc", "i64", lenReg64, "i32", outputStream);
            llvm.__emitBr(conditionLabel, outputStream);
            llvm.__emitLabel(conditionLabel, outputStream);

            std::string currIdxReg = llvm.__emitLoad(idxType, idxReg, outputStream);
            std::string conditionReg = llvm.__emitBinaryOp("<", idxType, false, currIdxReg, limitReg, outputStream);
            llvm.__emitCondBr(conditionReg, bodyLabel, endLabel, outputStream);
            llvm.__emitLabel(bodyLabel, outputStream);

            std::string strLoadReg = llvm.__emitLoad("i8*", strHold, outputStream);
            std::string indexExt = llvm.__emitSext("i32", currIdxReg, "i64", outputStream);

            std::string charReg = llvm.__emitCall("i8", "bery_string_char_at", {{"i8*", strLoadReg}, {"i64", indexExt}}, outputStream);
            llvm.__emitStore("i8", charReg, varReg, outputStream);

            breakTracker.push_back(endLabel);
            continueTracker.push_back(updateLabel);
            genBlock(forIn->body.get(), outputStream);
            continueTracker.pop_back();
            breakTracker.pop_back();
            llvm.__emitBr(updateLabel, outputStream);

            llvm.__emitLabel(updateLabel, outputStream);
            std::string upLoadIdx = llvm.__emitLoad(idxType, idxReg, outputStream);
            std::string addIdx = llvm.__emitBinaryOp("+", idxType, false, upLoadIdx, "1", outputStream);
            llvm.__emitStore(idxType, addIdx, idxReg, outputStream);
            llvm.__emitBr(conditionLabel, outputStream);
            llvm.__emitLabel(endLabel, outputStream);

        } else {
            auto* identNode = static_cast<IdentNode*>(forIn->iterableOrStart.get());
            std::string arrName = identNode->name;
            std::string arrPtr = genExpression(forIn->iterableOrStart.get(), "ptr", outputStream);

            int arrSize = symbolTable.get(arrName).arraySize;
            std::string limitReg = std::to_string(arrSize);

            llvm.__emitBr(conditionLabel, outputStream);
            llvm.__emitLabel(conditionLabel, outputStream);
            std::string currIdxReg = llvm.__emitLoad(idxType, idxReg, outputStream);

            std::string conditionReg = llvm.__emitBinaryOp("<", idxType, false, currIdxReg, limitReg, outputStream);
            llvm.__emitCondBr(conditionReg, bodyLabel, endLabel, outputStream);

            llvm.__emitLabel(bodyLabel, outputStream);

            std::string gepReg = llvm.__emitTypedGEP(lt, arrPtr, {{idxType, currIdxReg}}, true, outputStream);
            std::string valReg = llvm.__emitLoad(lt, gepReg, outputStream);
            llvm.__emitStore(lt, valReg, varReg, outputStream);

            breakTracker.push_back(endLabel);
            continueTracker.push_back(updateLabel);

            genBlock(forIn->body.get(), outputStream);

            continueTracker.pop_back();
            breakTracker.pop_back();
            llvm.__emitBr(updateLabel, outputStream);

            llvm.__emitLabel(updateLabel, outputStream);
            std::string upLoadIdx = llvm.__emitLoad(idxType, idxReg, outputStream);

            std::string addIdx = llvm.__emitBinaryOp("+", idxType, false, upLoadIdx, "1", outputStream);
            llvm.__emitStore(idxType, addIdx, idxReg, outputStream);

            llvm.__emitBr(conditionLabel, outputStream);
            llvm.__emitLabel(endLabel, outputStream);
        }
    }

    symbolTable.popScope();
    int roots = popGCScope();
    emitGCPops(roots, outputStream);
}