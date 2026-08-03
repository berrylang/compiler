#include "../LLVMHelper.h"

void LLVMHelper::__emitLabel(const std::string& name, std::ostream& outputStream) {outputStream <<"\n"<< name <<":\n";}

void LLVMHelper::__emitBr(const std::string& label, std::ostream& outputStream) {
    outputStream <<"    br label %" << label <<"\n";
}

void LLVMHelper::__emitCondBr(const std::string& cond, const std::string& trueLabel, 
    const std::string& falseLabel,std::ostream& outputStream) {
    outputStream <<"    br i1 " << cond <<", label %" << trueLabel <<", label %"<< falseLabel <<"\n";
}