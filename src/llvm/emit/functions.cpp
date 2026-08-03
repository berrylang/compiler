#include "../LLVMHelper.h"

void LLVMHelper::__emitFunctionHeader(const std::string& returnType, const std::string& name, 
    const std::vector<std::pair<std::string, std::string>>& params, std::ostream& outputStream) {
    outputStream <<"\ndefine " << returnType <<" @" << name <<"(";
    for(size_t i = 0; i < params.size(); ++i) {
        outputStream << params[i].first <<" " << params[i].second;
        if (i+1 < params.size()) 
            outputStream <<", ";

    }
    outputStream <<") {\nentry:\n";
}
void LLVMHelper::__emitFunctionFooter(std::ostream& outputStream) {
    outputStream <<"}\n";
}
void LLVMHelper::__emitReturn(const std::string& llvmType, const std::string& value, std::ostream& outputStream) {
    if(llvmType == "void" || value.empty()) {
        outputStream <<"   ret void\n";
        return;
    }
    outputStream <<"   ret "<< llvmType <<" " << value <<"\n";
}
void LLVMHelper::__emitDefaultReturn(const std::string& llvmType, std::ostream& outputStream) {
    if (llvmType == "void" || llvmType.empty()) 
        outputStream<<"    ret void\n";
    else if (llvmType =="float"|| llvmType=="double") outputStream <<  "    ret "<< llvmType <<" 0.0\n";
    else if (llvmType.back() == '*')
        outputStream<<"    ret " << llvmType <<" null\n";
    else outputStream <<"   ret " << llvmType <<" 0\n";
}

std::string LLVMHelper::__arguementRegName(const std::string& paramName) {
    return "%" + paramName + "_arg";
}