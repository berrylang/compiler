

#include "../LLVMHelper.h"

void LLVMHelper::__declareExtern(const std::string& declareText, const std::string& key) {
    if (declaredExterns.count(key)) return;
    declaredExterns.insert(key);
    __BRE_declares << declareText <<"\n";
}

std::string LLVMHelper::__formatDeclare(const std::string& returnType, const std::string& functionName, const std::vector<std::string>& parameterTypes) {
    std::string decl = "declare " + returnType + " @" + functionName + "(";
    for (size_t i = 0; i < parameterTypes.size(); ++i) {
        decl += parameterTypes[i];
        if (i + 1 < parameterTypes.size()) decl += ", ";
    }
    decl += ")";
    return decl;
}


void LLVMHelper::__declareExternFn(const std::string& returnType, const std::string& functionName, const std::vector<std::string>& parameterTypes) {
    __declareExtern(__formatDeclare(returnType, functionName, parameterTypes), functionName);
}