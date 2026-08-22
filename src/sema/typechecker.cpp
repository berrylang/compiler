#include "typechecker.h"

/*

    Bery Type Checker,
    
    it divides type checking into smaller parts, by spliting them based on their
    node type.

*/

#include "../parser/ast/expressions.h"
#include "../parser/ast/literals.h"
#include "../parser/ast/arraydeclare.h"
#include "../parser/ast/functions.h"
#include "../parser/ast/vardecl.h"
#include "../parser/ast/classes.h"
#include <iostream>
#include <unordered_set>


static std::vector<std::string> splitDots(const std::string& s) {
    std::vector<std::string> parts;
    size_t start = 0, pos;
    while ((pos = s.find('.', start)) != std::string::npos) {
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    parts.push_back(s.substr(start));
    return parts;
}


TypeChecker::TypeChecker(SymbolTable& symbolTable, std::unordered_map<std::string, std::vector<FunctionSignature>>& funcs, bool& errorsFlag, std::unordered_map<std::string, ClassDefNode*>& classesMap, std::string& currentClassRef) 
    : symbolTable(symbolTable), functions(funcs), classes(classesMap), currentClass(currentClassRef), errors(errorsFlag) {}

bool TypeChecker::typeMatchesLiteral(const std::string& type, NodeType litType) {
   if (type == "int"    && litType == NodeType::INT_LIT)     return true;
   if (type == "bigint" && litType == NodeType::INT_LIT)     return true;
   if (type == "float"  && litType == NodeType::DECIMAL_LIT) return true;
   if (type == "bool"   && litType == NodeType::BOOL_LIT)    return true;
   if (type == "double" && litType == NodeType::DECIMAL_LIT) return true;
   if (type == "char"   && litType == NodeType::CHAR_LIT)    return true;
   if (type == "string" && litType == NodeType::STRING_LIT)  return true;
   if (type == "string" && litType == NodeType::NULL_LIT)    return true;
   return false;
}

std::string TypeChecker::analyzeExpression(ASTNode* node) {
    switch (node->type) {
        case NodeType::BINARY_EXPR:     return checkBinaryExpr(node);
        case NodeType::UNARY_EXPR:      return checkUnaryExpr(node);
        case NodeType::TERNARY_EXPR:    return checkTernaryExpr(node);
        case NodeType::BETWEEN_EXPR:    return checkBetweenExpr(node);
        case NodeType::CALL_EXPR:       return checkCallExpr(node);
        case NodeType::INDEX_EXPR:      return checkIndexExpr(node);
        case NodeType::ASSIGNMENT_EXPR: return checkAssignmentExpr(node);
        case NodeType::CAST_EXPR:       return checkCastExpr(node);
        case NodeType::IDENT:           return checkIdentifier(node);
        case NodeType::NEW_EXPR:        return checkNewExpr(node);
        case NodeType::REF_EXPR:        return checkRefExpr(node);
        default:                        return checkLiteral(node);
    }
}

std::string TypeChecker::resolveNumericPromotion(const std::string& lType, const std::string& rType) {
    if (lType == rType) return lType;
    if ((lType == "bigint" && rType == "int")    || (lType == "int"    && rType == "bigint")) return "bigint";
    if ((lType == "float"  && rType == "int")    || (lType == "int"    && rType == "float"))  return "float";
    if ((lType == "bigint" && rType == "float")  || (lType == "float"  && rType == "bigint")) return "float";
    if ((lType == "bigint" && rType == "double") || (lType == "double" && rType == "bigint")) return "double";
    if ((lType == "int"    && rType == "double") || (lType == "double" && rType == "int"))    return "double";
    if ((lType == "float"  && rType == "double") || (lType == "double" && rType == "float"))  return "double";
    return "";
}

std::string TypeChecker::checkBinaryExpr(ASTNode* node) {
    auto* binary = static_cast<BinaryExprNode*>(node);
    std::string lType = analyzeExpression(binary->left.get());
    std::string rType = analyzeExpression(binary->right.get());

    if (binary->optr == "+") {
        if (lType == "string" || rType == "string") {
            if((lType != "string" && lType != "int" && lType != "bigint" && lType != "float" && lType != "double" && lType != "char" && lType != "bool") 
                ||  (rType != "string" && rType != "int" && rType != "bigint" && rType != "float" && rType != "double" && rType != "char" && rType != "bool")){
               std::cerr <<"Bery:Error [Line " << binary->line <<"]: Invalid operand for string concatenation\n";
                errors = true;
                binary->resolvedType = "unknown";
                return binary->resolvedType; 
            }
            if(lType != "string"){
                auto cast = std::make_unique<CastExprNode>("string",std::move(binary->left), binary->line);
                cast->srcType = lType;
                binary->left = std::move(cast);

            }
            if(rType != "string"){
                auto cast = std::make_unique<CastExprNode>("string",std::move(binary->right), binary->line);
                cast->srcType = rType;
                binary->right = std::move(cast);

            }
            binary->resolvedType = "string";
            return binary->resolvedType;
        }
        
    }
    if (lType == "string" && rType == "string") {
        if (binary->optr == "==" || binary->optr == "!=") {
            binary->resolvedType = "bool";
            return binary->resolvedType;
        }
    }

    if (binary->optr == "&&" || binary->optr == "||") {
        if (lType != "bool" || rType != "bool") {
            std::cerr <<"Bery:Error [Line " << binary->line <<"]: Logical operator '" << binary->optr <<"' cannot be used on type '" << lType <<"' and '" << rType <<"'\n";
            errors = true;
            binary->resolvedType = "unknown";
            return binary->resolvedType;
        }
        binary->resolvedType = "bool";
        return binary->resolvedType;
    }

    std::string resolved = resolveNumericPromotion(lType, rType);
    if (resolved.empty()) {
        std::cerr <<"Bery:Error [Line " << binary->line <<"]: Type mismatch in binary expression '" << lType <<"' and '" << rType <<"'\n";
        errors = true;
        binary->resolvedType = "unknown";
        return binary->resolvedType;
    }

    if (binary->optr == "==" || binary->optr == "!=" ||
        binary->optr == ">"  || binary->optr == ">=" ||
        binary->optr == "<"  || binary->optr == "<=") {
        if (binary->optr != "==" && binary->optr != "!=") {
            if (lType == "string" || lType == "bool" || rType == "string" || rType == "bool") {
                std::cerr <<"Bery:Error [Line " << binary->line <<"]: Relational operator '" << binary->optr <<"' cannot be used on type '" << lType <<"' and '" << rType <<"'\n";
                errors = true;
                binary->resolvedType = "unknown";
                return binary->resolvedType;
            }
        }
        binary->resolvedType = "bool";
        return binary->resolvedType;
    }

    if (binary->optr == "<<" || binary->optr == ">>") {
        if (rType != "int" && rType != "bigint") {
            std::cerr <<"Bery:Error [Line " << binary->line <<"]: Right operand of shift must be an integer type\n";
            errors = true;
            binary->resolvedType = "unknown";
            return binary->resolvedType;
        }
        if (resolved != "int" && resolved != "bigint") {
            std::cerr <<"Bery:Error [Line " << binary->line <<"]: Left operand of shift must be an integer type\n";
            errors = true;
            binary->resolvedType = "unknown";
            return binary->resolvedType;
        }
        binary->resolvedType = resolved;
        return binary->resolvedType;
    }

    if (binary->optr == "&" || binary->optr == "^" || binary->optr == "|") {
        if ((lType != "int" && lType != "bigint") || (rType != "int" && rType != "bigint")) {
            std::cerr <<"Bery:Error [Line " << binary->line <<"]: Bitwise operators require integer operands\n";
            errors = true;
            binary->resolvedType = "unknown";
            return binary->resolvedType;
        }
        binary->resolvedType = resolved;
        return binary->resolvedType;
    }
    binary->resolvedType = resolved;
    return binary->resolvedType;
}

std::string TypeChecker::checkTernaryExpr(ASTNode* node) {
    auto* tern = static_cast<TernaryExprNode*>(node);
    std::string condType = analyzeExpression(tern->condition.get());
    if (condType != "bool") {
        std::cerr <<"Bery:Error [Line " << tern->line <<"]: Ternary condition must be 'bool', got '" << condType <<"'\n";
        errors = true;
        tern->resolvedType = "unknown";
        return tern->resolvedType;
    }

    std::string tType = analyzeExpression(tern->trueExpr.get());
    std::string fType = analyzeExpression(tern->falseExpr.get());

    std::string resolved = resolveNumericPromotion(tType, fType);
    if (resolved.empty()) {
        std::cerr <<"Bery:Error [Line " << tern->line <<"]: Ternary branch type mismatch ('" << tType <<"' vs '" << fType <<"')\n";
        errors = true;
        tern->resolvedType = "unknown";
        return tern->resolvedType;
    }

    tern->resolvedType = resolved;
    return tern->resolvedType;
}

std::string TypeChecker::checkUnaryExpr(ASTNode* node) {
    auto* unary = static_cast<UnaryExprNode*>(node);
    std::string optype = analyzeExpression(unary->operand.get());
    if(unary->optr=="++"||unary->optr=="--"||unary->optr=="post++"||unary->optr=="post--"){
        if(unary->operand->type != NodeType::IDENT && unary->operand->type != NodeType::INDEX_EXPR){
            std::cerr<<"Bery:Error [Line "<< unary->line <<"]: Identifier requried as operand of increment or decrement operator\n";
            errors = true;
            unary->resolvedType = "unknown";
            return unary->resolvedType;
        }
    }
    if(unary->optr == "!"){
        unary->resolvedType = "bool";
        return unary->resolvedType;
    }
    unary->resolvedType = optype;
    return unary->resolvedType;
}

std::string TypeChecker::checkBetweenExpr(ASTNode* node) {
    auto* between = static_cast<BetweenExprNode*>(node);
    std::string valueType = analyzeExpression(between->value.get());
    std::string lowerType = analyzeExpression(between->lower.get());
    std::string upperType = analyzeExpression(between->upper.get());

    auto validType = [](const std::string& type){
        return type == "int" || type == "bigint" || type == "float" || type == "double" || type == "char";
    };

    if(!validType(valueType) || !validType(lowerType) || !validType(upperType)){
        std::cerr<<"Bery:Error [Line "<< between->line <<"]: Between operator supports only int, bigint, float, double and char\n";
        errors = true;
        between->resolvedType = "unknown";
        return between->resolvedType;
    }

    std::string dominentType = "int";
    if (valueType == "double" || lowerType=="double" || upperType=="double") dominentType= "double";
    else if (valueType == "float" || lowerType=="float" || upperType=="float") dominentType = "float";
    else if (valueType == "bigint" || lowerType=="bigint" || upperType=="bigint") dominentType = "bigint";

    between->resolvedType = dominentType;
    return between->resolvedType;
}

std::string TypeChecker::checkCallExpr(ASTNode* node) {
    auto* call = static_cast<CallExprNode*>(node);

    static const std::unordered_set<std::string> builtinIO = {"print", "println","inputInt", "inputBigInt", "inputFloat","inputDouble", "inputBool", "inputChar", "inputString"  };

    static const std::unordered_map<std::string, std::string> inputTypes = {
        {"inputInt", "int"}, 
        {"inputBigInt", "bigint"}, {"inputFloat", "float"},
        {"inputDouble", "double"}, {"inputBool", "bool"},   
        {"inputChar", "char"}, {"inputString", "string"}
    };

    if (builtinIO.count(call->callee)) {
        if (call->callee == "print" && call->arguments.size() != 1) {
            std::cerr <<"Bery:Error [Line " << call->line <<"]: print() expects exactly 1 argument\n";
            errors = true;
        }
        if (call->callee == "println" && call->arguments.size() > 1) {
            std::cerr <<"Bery:Error [Line " << call->line <<"]: println() expects 0 or 1 argument\n";
            errors = true;
        }
        if (call->callee != "print" && call->callee != "println" && call->arguments.size() != 1) {
            std::cerr <<"Bery:Error [Line " << call->line <<"]: " << call->callee <<"() expects exactly 1 argument\n";
            errors = true;
        }
        for (auto& arg : call->arguments) {
            analyzeExpression(arg.get());
        }

        auto it = inputTypes.find(call->callee);
        call->resolvedType = (it != inputTypes.end()) ? it->second : "void";
        return call->resolvedType;
    }
    
    size_t dot = call->callee.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(call->callee);
        std::string method = parts.back();
        std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
        std::string objType = resolveChainType(headParts, call->line);
        if (objType == "unknown") { call->resolvedType = "unknown"; return call->resolvedType; }

        if (objType == "string") {
            if (method == "substr" || method == "copy") { 
                call->resolvedType = "string"; 
                return call->resolvedType; 
            }
            if (method == "len"){ 
                call->resolvedType = "int";    
                return call->resolvedType; 
            }
        }

        if (objType.size() > 6 && objType.substr(0, 6) == "array<") {
            std::string elemType = objType.substr(6, objType.size() - 7);
            if (method == "push" || method == "pop" || method == "insert" || method == "remove") {
                call->resolvedType = "void"; 
                return call->resolvedType;
            }
            if (method == "len") { 
                call->resolvedType = "int";
                return call->resolvedType; 
                }
            if (method == "get") { 
                call->resolvedType = elemType; 
                return call->resolvedType; 
            }
        }

        auto classIt = classes.find(objType);
        if (classIt != classes.end()) {
            ClassDefNode* cls = classIt->second;
            std::vector<FunctionDefNode*> candidateMethod = findMethod(cls, method);
            if(candidateMethod.empty()){
                std::cerr <<"Bery:Error [Line " << call->line <<"]: Class '" << objType <<"' has no method '" << method <<"'\n";
                errors = true;
                call->resolvedType = "unknown";
                return call->resolvedType;
            }

            std::vector<std::string> argTypes;
            for(auto& arg : call->arguments){argTypes.push_back(analyzeExpression(arg.get()));}
            FunctionDefNode* methodDef = resolveMethodOverload(candidateMethod, argTypes, method, call->line);
            if (!methodDef) {
                call->resolvedType = "unknown";
                return call->resolvedType;
            }
            if (!checkMemberAccess(methodDef->access, objType, method, "method", call->line)) {
                call->resolvedType = "unknown";
                return call->resolvedType;
            }
            call->resolvedParamTypes.clear();
            for(auto& p : methodDef->parameters){call->resolvedParamTypes.push_back(p.first);}
            
            
            call->resolvedType = methodDef->returnType.empty() ? "void" : methodDef->returnType;
            return call->resolvedType;
        }

        std::cerr <<"Bery:Error [Line " << call->line <<"]: Unknown method '" << method <<"' on type '" << objType <<"'\n";
        errors = true;
        call->resolvedType = "unknown";
        return call->resolvedType;
    }

    if (!currentClass.empty()) {
        auto selfClassIt =classes.find(currentClass);
        if (selfClassIt != classes.end()) {
            std::vector<FunctionDefNode*> candidateFunction = findMethod(selfClassIt->second, call->callee);
            if(!candidateFunction.empty()){
                std::vector<std::string> argumentTypesName;
                for(auto& argsss : call->arguments){
                    argumentTypesName.push_back(analyzeExpression(argsss.get()));
                }
                FunctionDefNode* f = resolveMethodOverload(candidateFunction, argumentTypesName, call->callee, call->line);
                if(!f){
                    call->resolvedType = "unknown";
                    return call->resolvedType;
                }
                 call->resolvedParamTypes.clear();
                for(auto& p : f->parameters){call->resolvedParamTypes.push_back(p.first);}
                call->resolvedType = f->returnType.empty() ? "void" : f->returnType;
                return call->resolvedType;
            }

            
        }

    }
    auto functionIT = functions.find(call->callee);
    if (functionIT == functions.end()) {
        std::cerr <<"Bery:Error [Line " << call->line <<"]: Undefined function '" << call->callee <<"'\n";
        errors = true;
        call->resolvedType = "unknown";
        return call->resolvedType;
    }
    std::vector<std::string> argTypes;
    for(auto& hello : call->arguments){
        argTypes.push_back(analyzeExpression(hello.get()));
    }
    const FunctionSignature* signature = resolveFunctionOverload(functionIT->second, argTypes, call->callee, call->line);
    if(!signature){
        call->resolvedType = "unknown";
        return call->resolvedType;
    }
    call->resolvedParamTypes = signature->parameterTypes;
    call->resolvedType = signature->returnType;
    return call->resolvedType;
}

std::string TypeChecker::checkIndexExpr(ASTNode* node) {
    auto* idxNode = static_cast<IndexExprNode*>(node);
    std::string arrType;
    int dimCount = 0;

    size_t dot = idxNode->name.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(idxNode->name);
        arrType = resolveChainType(parts, idxNode->line);
        if (arrType == "unknown") {
            idxNode->resolvedType = "unknown";
            return idxNode->resolvedType;
        }
        std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
        std::string classType = resolveChainType(headParts, idxNode->line);
        auto classIt = classes.find(classType);
        if (classIt != classes.end()) {
            ASTNode* field = findField(classIt->second, parts.back());
            if (field && field->type == NodeType::ARRAY_DECL) {
                dimCount = (int)static_cast<ArrayDeclNode*>(field)->dimensions.size();
            } else {
                dimCount = 1;
            }
        } else {
            dimCount = 1;
        }
    } else {
        if (!symbolTable.exists(idxNode->name)) {
            std::cerr <<"Bery:Error [Line " << idxNode->line <<"]: Undefined array '" << idxNode->name <<"'\n";
            errors = true;
            idxNode->resolvedType = "unknown";
            return idxNode->resolvedType;
        }
        Symbol& sym = symbolTable.get(idxNode->name);
        arrType = sym.type;
        dimCount = (int)sym.arrayDimensions.size();
    }

    if (arrType == "string") {
        for (auto& index : idxNode->indices) {
            std::string indexType = analyzeExpression(index.get());
            if (indexType != "int" && indexType != "bigint") {
                std::cerr <<"Bery:Error [Line " << idxNode->line <<"]: String index must be an integer\n";
                errors = true;
                idxNode->resolvedType = "unknown";
                return idxNode->resolvedType;
            }
        }
        idxNode->resolvedType = "char";
        return idxNode->resolvedType;
    }

    if (!(arrType.size() > 6 && arrType.substr(0, 6) == "array<")) {
        std::cerr <<"Bery:Error [Line " << idxNode->line <<"]: Variable '" << idxNode->name <<"' is not subscriptable\n";
        errors = true;
        idxNode->resolvedType = "unknown";
        return idxNode->resolvedType;
    }

    if (idxNode->indices.size() > (size_t)dimCount && dimCount > 0) {
        std::cerr <<"Bery:Error [Line " << idxNode->line <<"]: Too many indices for array '" << idxNode->name <<"'\n";
        errors = true;
        idxNode->resolvedType = "unknown";
        return idxNode->resolvedType;
    }

    for (auto& index : idxNode->indices) analyzeExpression(index.get());
    std::string elemType = arrType.substr(6, arrType.size() - 7);

    if (!idxNode->memberChain.empty()) {
        idxNode->resolvedType = resolveFieldChainFrom(elemType, idxNode->memberChain, idxNode->line);
        return idxNode->resolvedType;
    }

    idxNode->resolvedType = elemType;
    return idxNode->resolvedType;
}
std::string TypeChecker::checkAssignmentExpr(ASTNode* node) {
    auto* assign = static_cast<AssignmentExprNode*>(node);
    std::string targetName = "";
    std::string targetType = analyzeExpression(assign->target.get());
    std::string valueType = analyzeExpression(assign->value.get());

    if (assign->op == "+=") {
        if (targetType != "int" && targetType != "float" && targetType != "double" && targetType != "bigint" && targetType != "string") {
            std::cerr <<"Bery:Error [Line " << assign->line <<"]: Cannot use compound assignment '" << assign->op <<"' on type '" << targetType <<"'\n";
            errors = true;
        }
    }
    else if (assign->op != "=") {
        if (targetType != "int" && targetType != "float" && targetType != "double" && targetType != "bigint") {
            std::cerr <<"Bery:Error [Line " << assign->line <<"]: Cannot use compound assignment '" << assign->op <<"' on type '" << targetType <<"'\n";
            errors = true;
        }
    }
    
    if (assign->target->type == NodeType::IDENT) {
        auto* ident = static_cast<IdentNode*>(assign->target.get());
        targetName = ident->name;
        size_t dot = ident->name.find('.');
        if (dot != std::string::npos) {
            std::vector<std::string> parts = splitDots(ident->name);
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string fieldName = parts.back();
            std::string containerType = resolveChainType(headParts, assign->line);
            if (containerType == "unknown") { assign->resolvedType = "unknown"; return assign->resolvedType; }
            auto classIt = classes.find(containerType);
            if (classIt == classes.end()) {
                std::cerr <<"Bery:Error [Line " << assign->line <<"]: '" << headParts.back() <<"' is not an object\n";
                errors = true;
                assign->resolvedType = "unknown";
                return assign->resolvedType;
            }
            std::string fieldType = resolveFieldType(classIt->second, fieldName);
            if (fieldType.empty()) {
                std::cerr <<"Bery:Error [Line " << assign->line <<"]: Class '" << containerType<<"' has no member '" << fieldName <<"'\n";
                errors = true;
                assign->resolvedType = "unknown";
                return assign->resolvedType;
            }
            ASTNode* field = findField(classIt->second, fieldName);
            if (!field) {
                std::cerr <<"Bery:Error [Line " << assign->line <<"]: Class '" << containerType<<"' has no member '" << fieldName <<"'\n";
                errors = true;
                assign->resolvedType = "unknown";
                return assign->resolvedType;
            }

            AccessSpecifier acc = (field->type == NodeType::VAR_DECL) ? static_cast<VarDeclNode*>(field)->access: static_cast<ArrayDeclNode*>(field)->access;

            if (!checkMemberAccess(acc, containerType, fieldName, "field", assign->line)) {
                assign->resolvedType = "unknown";
                return assign->resolvedType;
            }
            targetType = fieldType; 
        } else {



            if (!symbolTable.exists(ident->name)) {
                std::cerr <<"Bery:Error [Line " << assign->line <<"]: Undefined variable '" << ident->name <<"'\n";
                errors = true;
                ident->resolvedType = "unknown";
                return ident->resolvedType;
            }
            Symbol& s = symbolTable.get(ident->name);
            if (s.isConst) {
                std::cerr <<"Bery:Error [Line " << assign->line <<"]: cannot reassign constant variable '" << ident->name <<"'\n";
                errors = true;
                ident->resolvedType = "unknown";
                return ident->resolvedType;
            }
            s.isInitialized = true;
            targetType = s.type;
        }
    } else if (assign->target->type == NodeType::INDEX_EXPR) {
        auto* idxNode = static_cast<IndexExprNode*>(assign->target.get());
        size_t dot = idxNode->name.find('.');
        if (dot != std::string::npos) {
            std::vector<std::string> parts = splitDots(idxNode->name);
            std::string arrType = resolveChainType(parts, idxNode->line);
            if (arrType == "unknown") {
                assign->resolvedType = "unknown";
                return assign->resolvedType;
            }
            targetName = idxNode->name;
        } else {
            if (!symbolTable.exists(idxNode->name)) {
                std::cerr <<"Bery:Error [Line " << idxNode->line <<"]: Undefined array '" << idxNode->name <<"'\n";
                errors = true;
                assign->resolvedType = "unknown";
                return assign->resolvedType;
            }
            targetName = idxNode->name;
        }

        targetType = analyzeExpression(assign->target.get());
        if (targetType == "unknown") {
            assign->resolvedType = "unknown";
            return assign->resolvedType;
        }
    } else {
        std::cerr <<"Bery:Error [Line " << assign->line <<"] : Invalid assignment target\n";
        errors = true;
        assign->resolvedType = "unknown";
        return assign->resolvedType;
    }

    std::string exptype = analyzeExpression(assign->value.get());
    
    if (exptype != "unknown" && exptype != targetType) {
        if (!(targetType == "float" && exptype == "int") &&  !(targetType == "double" && exptype == "int") &&
            !(targetType == "bigint" && exptype == "int") &&  !(targetType == "double" && exptype == "float")) {
            
            std::cerr <<"Bery:Error [Line " << assign->line <<"] : Type missmatch for assignment to  '" << targetName <<"'. Expected '" << targetType <<"', got '" << exptype <<"'\n";
            errors = true;
            assign->resolvedType = "unknown";
            return assign->resolvedType;
        }
    }
    assign->resolvedType = targetType;
    return assign->resolvedType;
}

std::string TypeChecker::checkCastExpr(ASTNode* node) {
    auto* castNode = static_cast<CastExprNode*>(node);
    std::string srcType = analyzeExpression(castNode->expr.get());
    castNode->srcType = srcType; 

    auto isPrimitive = [](const std::string& t) {
        return t == "int" || t == "bigint" || t == "float" || t == "double" || t == "char" || t == "bool";
    };

    if (!isPrimitive(srcType) || !isPrimitive(castNode->targetType)) {
        std::cerr <<"Bery:Error [Line " << castNode->line <<"]: Invalid cast from '" << srcType <<"' to '"  << castNode->targetType   <<"'.\n";
        errors = true;
        castNode->resolvedType = "unknown";
        return castNode->resolvedType;
    }
    castNode->resolvedType = castNode->targetType;
    return castNode->resolvedType;
}

std::string TypeChecker::checkIdentifier(ASTNode* node) {
    auto* ident = static_cast<IdentNode*>(node);
    size_t dot = ident->name.find('.');
    if (dot != std::string::npos) {
        std::vector<std::string> parts = splitDots(ident->name);
        if (parts.back() == "len") {
            std::vector<std::string> headParts(parts.begin(), parts.end() - 1);
            std::string headType = resolveChainType(headParts, ident->line);
            if (headType == "unknown") { 
                node->resolvedType = "unknown"; 
                return node->resolvedType; 
            
            }
            if (headType == "string" ||(headType.size() > 6 && headType.substr(0, 6) == "array<")) {
                ident->resolvedType = "int";
                return ident->resolvedType;
            }
        }
        ident->resolvedType = resolveChainType(parts, ident->line);
        return ident->resolvedType;
    }
    if(!symbolTable.exists(ident->name)){
        std::cerr<<"Bery:Error [Line "<< ident->line <<"]: Undefined Variable '"<<ident->name<<"'\n";
        errors=true;
        node->resolvedType = "unknown";
        return node->resolvedType;
    }
    ident->resolvedType = symbolTable.get(ident->name).type;
    return ident->resolvedType;
}

std::string TypeChecker::checkLiteral(ASTNode* node) {
    switch (node->type) {
        case NodeType::INT_LIT:     
            node->resolvedType = "int";
            return node->resolvedType;
        case NodeType::DECIMAL_LIT:     
            node->resolvedType = "float";
            return node->resolvedType;
        case NodeType::CHAR_LIT:     
            node->resolvedType = "char";
            return node->resolvedType;
        case NodeType::BOOL_LIT:     
            node->resolvedType = "bool";
            return node->resolvedType;
        case NodeType::STRING_LIT:     
            node->resolvedType = "string";
            return node->resolvedType;  
        case NodeType::NULL_LIT:     
            node->resolvedType = "null";
            return node->resolvedType;
        default:
            std::cerr <<"Bery:Error [Line " << node->line <<"]: Unknown literal type\n";
            errors = true;
            node->resolvedType = "unknown";
            return node->resolvedType;
    }
}

std::string TypeChecker::checkNewExpr(ASTNode* node) {
    auto* newExpr = static_cast<NewExprNode*>(node);
    auto classIt = classes.find(newExpr->className);
    if (classIt == classes.end()) {
        std::cerr <<"Bery:Error [Line " << newExpr->line <<"]: Unknown class '" << newExpr->className <<"'\n";
        errors = true;
        newExpr->resolvedType = "unknown";
        return newExpr->resolvedType;
    }

    FunctionDefNode* ctor = nullptr;
    if (classIt->second->methods) {
        for (auto& m : classIt->second->methods->methods) {
            auto* f = static_cast<FunctionDefNode*>(m.get());
            if (f->isConstructor) { ctor = f; break; }
        }
    }

    if (!ctor) {
        if (!newExpr->arguments.empty()) {
            std::cerr <<"Bery:Error [Line " << newExpr->line <<"]: Class '" << newExpr->className <<"' has no constructor accepting " << newExpr->arguments.size() <<" argument(s)\n";
            errors = true;
        }
        for (auto& arg : newExpr->arguments) analyzeExpression(arg.get());
        newExpr->resolvedType = newExpr->className;
        return newExpr->resolvedType;
    }

    if (ctor->parameters.size() != newExpr->arguments.size()) {
        std::cerr <<"Bery:Error [Line " << newExpr->line <<"]: Constructor for '" << newExpr->className <<"' expects "<< ctor->parameters.size() <<" arguments, got " << newExpr->arguments.size() <<"\n";
        errors = true;
        newExpr->resolvedType = "unknown";
        return newExpr->resolvedType;
    }
    for (size_t i = 0; i < newExpr->arguments.size(); ++i) {
        std::string argType   = analyzeExpression(newExpr->arguments[i].get());
        std::string paramType = ctor->parameters[i].first;
        if (argType != "unknown" && argType != paramType) {
            if (!(paramType == "float"  && argType == "int") && !(paramType == "double" && argType == "float") &&
                !(paramType == "double" && argType == "int") && !(paramType == "bigint" && argType == "int")) {
                std::cerr <<"Bery:Error [Line " << newExpr->line <<"]: Type mismatch in constructor argument " << i+1<<" of '" << newExpr->className <<"'. Expected '" << paramType <<"', got '" << argType <<"'\n";
                errors = true;
            }
        }
    }

    newExpr->resolvedType = newExpr->className;
    return newExpr->resolvedType;
}

std::string TypeChecker::checkRefExpr(ASTNode* node) {
    auto* refNode = static_cast<RefExprNode*>(node);
    if (refNode->target->type != NodeType::IDENT && refNode->target->type != NodeType::INDEX_EXPR) {
        std::cerr <<"Bery:Error [Line " << refNode->line <<"]: 'ref' can only be used on a variable, field, or array element\n";
        errors = true;
        refNode->resolvedType = "unknown";
        return refNode->resolvedType;
    }
    std::string targetType = analyzeExpression(refNode->target.get());
    refNode->resolvedType = targetType;
    return refNode->resolvedType;
}

std::string TypeChecker::resolveFieldType(ClassDefNode* cls, const std::string& fieldName) {
    ASTNode* field = findField(cls, fieldName);
    if (!field) return "";
    if (field->type == NodeType::VAR_DECL) return static_cast<VarDeclNode*>(field)->varType;
    if (field->type == NodeType::ARRAY_DECL) return "array<" + static_cast<ArrayDeclNode*>(field)->elementType + ">";
    return "";
}

ASTNode* TypeChecker::findField(ClassDefNode* cls, const std::string& fieldName) {
    if (cls->attributes) {
        for (auto& attrNode : cls->attributes->attributes) {
            if (attrNode->type == NodeType::VAR_DECL) {
                auto* field = static_cast<VarDeclNode*>(attrNode.get());
                if (field->name == fieldName) return field;
            } else if (attrNode->type == NodeType::ARRAY_DECL) {
                auto* field = static_cast<ArrayDeclNode*>(attrNode.get());
                if (field->name == fieldName) return field;
            }
        }
    }
    if (!cls->parentName.empty()) {
        auto it = classes.find(cls->parentName);
        if (it != classes.end()) return findField(it->second, fieldName);
    }
    return nullptr;
}

std::vector<FunctionDefNode*> TypeChecker::findMethod(ClassDefNode* cls, const std::string& methodName) {
    std::vector<FunctionDefNode*> foundMethod;
    if (cls->methods) {
        for (auto& m : cls->methods->methods) {
            auto* f = static_cast<FunctionDefNode*>(m.get());
            if (f->isConstructor || f->isDestructor) continue;
            if (f->name == methodName) foundMethod.push_back(f);
        }
    }
    if(!foundMethod.empty()){
        return foundMethod;
    }
    if (!cls->parentName.empty()) {
        auto it = classes.find(cls->parentName);
        if (it != classes.end()) return findMethod(it->second, methodName);
    }
    return foundMethod;
}

FunctionDefNode* TypeChecker::resolveMethodOverload(const std::vector<FunctionDefNode*>& candidate, const std::vector<std::string>& argTypes, const std::string& label, int line){
    for(auto* a : candidate){
        std::vector<std::string> parameterTypeList;
        for(auto& p : a->parameters){
            parameterTypeList.push_back(p.first);
        }
        if(isParameterTypeExactlyMatching(parameterTypeList, argTypes)){
            return a;
        }
    }
    FunctionDefNode* c = nullptr;
    int matchCount = 0;
    for(auto* f : candidate){
        if(f->parameters.size()!=argTypes.size()){continue;}
        bool thirtyfour = true;
        for(size_t i = 0; i<argTypes.size();i++){
            if(argTypes[i] == "unknown"){continue;}
            if(!isParameterTypePromotable(argTypes[i],f->parameters[i].first)){thirtyfour = false; break;}
        }
        if(thirtyfour){
            matchCount++;
            c = f;
        }
    }
    if(matchCount==1){return c;}
    if(matchCount>1){
         std::cerr <<"Bery:Error [Line " << line <<"]: Ambiguous call to method '"<< label <<"' with "<< argTypes.size() <<" arguments  '\n";
         errors = true;
         return nullptr;
    }
    bool isAnyMethodMatchingToThisWhatToThisToThisCallCall = false;
    for(auto* f : candidate){
        if(f->parameters.size() == argTypes.size()){
            isAnyMethodMatchingToThisWhatToThisToThisCallCall = true;
        }
    }
    if(!isAnyMethodMatchingToThisWhatToThisToThisCallCall){
        std::cerr << "Bery:Error [Line "<< line << "]: No overloaded method '"<< label <<"' accepts the "<< argTypes.size() <<" arguments. \n";
    }
    else{std::cerr << "Bery:Error [Line "<< line << "]: No matching overload of method '"<< label <<"' for this given arugment types\n";}
    errors = true;
    return nullptr;
}
const FunctionSignature* TypeChecker::resolveFunctionOverload(const std::vector<FunctionSignature>& candidate, const std::vector<std::string>& argTypes, const std::string& label, int line){
    for(auto& a : candidate){
        if(isParameterTypeExactlyMatching(a.parameterTypes, argTypes)){
            return &a;
        }
    }
    const FunctionSignature* c = nullptr;
    int matchCount = 0;
    for(auto& f : candidate){
        if(f.parameterTypes.size()!=argTypes.size()){continue;}
        bool thirtyfour = true;
        for(size_t i = 0; i<argTypes.size();i++){
            if(argTypes[i] == "unknown"){continue;}
            if(!isParameterTypePromotable(argTypes[i],f.parameterTypes[i])){thirtyfour = false; break;}
        }
        if(thirtyfour){
            matchCount++;
            c = &f;
        }
    }
    if(matchCount==1){return c;}
    if(matchCount>1){
         std::cerr <<"Bery:Error [Line " << line <<"]: Ambiguous call to function '"<< label <<"' with "<< argTypes.size() <<" arguments  '\n";
         errors = true;
         return nullptr;
    }
    bool isAnyMethodMatchingToThisWhatToThisToThisCallCall = false;
    for(auto& f : candidate){
        if(f.parameterTypes.size() == argTypes.size()){
            isAnyMethodMatchingToThisWhatToThisToThisCallCall = true;
        }
    }
    if(!isAnyMethodMatchingToThisWhatToThisToThisCallCall){
        std::cerr << "Bery:Error [Line "<< line << "]: No overloaded function '"<< label <<"' accepts the "<< argTypes.size() <<" arguments. \n";
    }
    else{std::cerr << "Bery:Error [Line "<< line << "]: No matching overload of function '"<< label <<"' for this given arugment types\n";}
    errors = true;
    return nullptr;
}
bool TypeChecker::isParameterTypePromotable(const std::string& from, const std::string& to){
    if(from==to){return true;}
    if(to=="float" && from=="int"){return true;}
    if(to=="double" && from=="int"){return true;}
    if(to=="double" && from=="float"){return true;}
    if(to=="bigint" && from=="int"){return true;}
    return false;
    

}
bool TypeChecker::isParameterTypeExactlyMatching(const std::vector<std::string>& a, const std::vector<std::string>& b){
    if(a.size()!=b.size()){
        return false;
    }
    for(size_t i = 0;i < a.size(); i++){
        if(a[i]!=b[i]){
            return false;
        }
    }
    return true;
}

bool TypeChecker::checkMemberAccess(AccessSpecifier access, const std::string& className, const std::string& memberName, const std::string& type, int line) {
    if (access == AccessSpecifier::PUBLIC) return true;
    if (currentClass == className) return true;
    if (access == AccessSpecifier::PROTECTED && !currentClass.empty()) {
        std::string cur = currentClass;
        while (!cur.empty()) {
            if (cur == className) return true;
            auto it = classes.find(cur);
            cur = (it != classes.end()) ? it->second->parentName : "";
        }
    }
    std::string levelName = (access == AccessSpecifier::PRIVATE) ? "private" : "protected";
    std::cerr << "Bery:Error [Line " << line << "]: Cannot access " << levelName << " " << type << " '" << memberName << "' of class '" << className << "' from outside the class\n";
    errors = true;
    return false;
}

std::string TypeChecker::resolveChainType(const std::vector<std::string>& parts, int line) {
    if (!symbolTable.exists(parts[0])) {
        std::cerr <<"Bery:Error [Line " << line <<"]: Undefined variable '" << parts[0] <<"'\n";
        errors = true;
        return "unknown";
    }
    std::string currentType = symbolTable.get(parts[0]).type;
    std::vector<std::string> rest(parts.begin() + 1, parts.end());
    return resolveFieldChainFrom(currentType, rest, line);
}


std::string TypeChecker::resolveFieldChainFrom(std::string currentType, const std::vector<std::string>& parts, int line) {
    for (size_t i = 0; i < parts.size(); ++i) {
        auto classIt = classes.find(currentType);
        if (classIt == classes.end()) {
            std::cerr <<"Bery:Error [Line " << line <<"]: '" << currentType <<"' is not an object, cannot access '." << parts[i] <<"'\n";
            errors = true;
            return "unknown";
        }
        ASTNode* field = findField(classIt->second, parts[i]);
        if (!field){
            std::cerr <<"Bery:Error [Line " <<line <<"]: Class '" << currentType <<"' has no member '" << parts[i] <<"'\n";
            errors = true;
            return "unknown";
        }
        AccessSpecifier acc = (field->type == NodeType::VAR_DECL)? static_cast<VarDeclNode*>(field)->access: static_cast<ArrayDeclNode*>(field)->access;
            
        if (!checkMemberAccess(acc, currentType, parts[i], "field", line)) {return "unknown";}
        if (field->type == NodeType::VAR_DECL) currentType = static_cast<VarDeclNode*>(field)->varType;
        else currentType = "array<" + static_cast<ArrayDeclNode*>(field)->elementType + ">";
    }
    return currentType;
}