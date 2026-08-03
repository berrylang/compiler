#include "../codegen.h"
#include <string>
#include <iomanip>
#include "../../parser/ast/literals.h"
#include "../../parser/ast/expressions.h"
#include "../../parser/ast/vardecl.h"


std::string CodeGen::llvmType(const std::string& t) {
    std::string base = llvm.__llvmType(t);
    if (!base.empty()) return base;
    if (classLayouts.count(t)) return classLayouts.at(t).llvmStructType + "*";
    return "i32";
}


std::string CodeGen::extractConstant(ASTNode* node) {
    if (!node) return "0";
    if (node->type == NodeType::INT_LIT) {
        return std::to_string(static_cast<IntLitNode*>(node)->value);
    } else if (node->type == NodeType::DECIMAL_LIT) {
        return llvm.__formatDoubleConstant(static_cast<DecimalLitNode*>(node)->value);
    }
    else if (node->type == NodeType::BOOL_LIT) {
        return static_cast<BoolLitNode*>(node)->value ? "1" : "0";
    } 
    else if (node->type == NodeType::CHAR_LIT) {
        return std::to_string(static_cast<CharLitNode*>(node)->value);
    }else if (node->type == NodeType::STRING_LIT) {
        return llvm.__emitGlobalStringConstant(static_cast<StringLitNode*>(node)->value);
    }
    else if (node->type == NodeType::NULL_LIT) {
        return "null";
    }
    else if (node->type == NodeType::UNARY_EXPR) {
        auto* unary = static_cast<UnaryExprNode*>(node);
        if (unary->optr == "-") {
            return "-" + extractConstant(unary->operand.get());
        }
    }
    return "0";
}


void CodeGen::genStatement(ASTNode* statement, std::ostream& outputStream) {
    if (!statement) return;
    
    if (statement->type == NodeType::VAR_DECL) genVarDecl(statement, outputStream);
    else if (statement->type == NodeType::ARRAY_DECL) genArrayDecl(statement, outputStream);
    else if (statement->type == NodeType::ASSIGNMENT_EXPR || statement->type == NodeType::UNARY_EXPR ||statement->type == NodeType::CALL_EXPR) genExpression(statement, "any", outputStream);
    else if (statement->type == NodeType::IF_STMT) genIfStmt(statement, outputStream);
    else if (statement->type == NodeType::WHILE_STMT) genWhileStmt(statement, outputStream);
    else if (statement->type == NodeType::DOWHILE_STMT) genDoWhileStmt(statement, outputStream);
    else if (statement->type == NodeType::SWITCH_STMT) genSwitchStmt(statement, outputStream);
    else if (statement->type == NodeType::BREAK_STMT) genBreakStmt(statement, outputStream);
    else if (statement->type == NodeType::BLOCK) genBlock(statement, outputStream);
    else if (statement->type == NodeType::PASS_STMT) {}
    else if (statement->type == NodeType::CONTINUE_STMT) genContinueStmt(statement, outputStream);
    else if (statement->type == NodeType::RETURN_STMT) genReturnStmt(statement, outputStream);
    else if (statement->type == NodeType::ENUM_DECL) {
        auto* enumDecl = static_cast<EnumDeclNode*>(statement);
        int currentValue = 0;
        
        for (const auto& val : enumDecl->values) {
            std::string mangledName = enumDecl->name + "." + val;
            std::string lt = "i32";
            std::string safeRegName = enumDecl->name + "_" + val; 
            std::string memReg = "%" + safeRegName + "_" + std::to_string(llvm.__uniqueId());
            
            Symbol sym;
            sym.symbolType = SymbolType::VARIABLE;
            sym.type = "int";
            sym.isConst = true;
            sym.isInitialized = true;
            sym.line = enumDecl->line;
            sym.llvmRegister = memReg;
            sym.llvmAllocType = lt;
            symbolTable.add(mangledName, sym);
            outputStream <<"    " << memReg <<" = alloca " << lt <<"\n";
            llvm.__emitStore(lt, std::to_string(currentValue++), memReg, outputStream);
        }
    }
    else if (statement->type == NodeType::FOR_STMT) genForStmt(statement, outputStream);
    else if (statement->type == NodeType::FOR_IN_STMT) genForInStmt(statement, outputStream);
}