#include "../LLVMHelper.h"


std::string LLVMHelper::__emitAlloca(const std::string& llvmType, std::ostream&  outputStream) {
    std::string reg = __uniqueReg();
    outputStream<<"    "<<reg <<" = alloca "<< llvmType<<", align " << __alignOf(llvmType) <<"\n";
    return reg;
}


std::string LLVMHelper::__emitLoad(const std::string& llvmType,  
    const std::string& pointer, std::ostream& outputStream) {

        std::string reg = __uniqueReg();
        outputStream <<"   " <<reg <<" = load " << llvmType<<", "<< llvmType <<"* " << pointer <<", align " << __alignOf(llvmType) <<"\n";
        return reg;
    }


std::string LLVMHelper::__emitBitcast(const std::string& fromType, const std::string& value, 
    const std::string& toType, std::ostream& outputStream) {
    std::string reg = __uniqueReg();
    outputStream <<"   " << reg <<" = bitcast " << fromType <<" " << value <<" to "<<toType<<"\n";
    return reg;
}

std::string LLVMHelper::__emitSext(const std::string& fromType, const std::string& value, 
    const std::string& toType, std::ostream& outputStream) {
    std::string reg = __uniqueReg();
    outputStream <<"   "<< reg <<" = sext " << fromType <<" "<< value <<" to " << toType <<"\n";
    return reg;
}
std::string LLVMHelper::__emitGEP(const std::string& baseType, const std::string& pointer, const std::vector<std::string>& indices,
                         bool inbounds, std::ostream& outputStream) {

    std::string reg = __uniqueReg();
    outputStream <<"   "<< reg <<"  = getelementptr " << (inbounds ? "inbounds ": "") << baseType <<", " << baseType <<"* " << pointer;
    for (auto& index : indices) outputStream <<", " << index;
    outputStream <<"\n";
    return reg;
}
std::string LLVMHelper::__emitBoxValue(const std::string& llvmType,
     const std::string& valueRegister,   std::ostream& outputStream) {
    __declareExtern("declare i8* @bery_alloc(i64, i32)", "bery_alloc");
    std::string rawRegister = __emitCall("i8*", "bery_alloc", {{"i64", "8"}, {"i32", "0"}}, outputStream);
    std::string castRegister = __emitBitcast("i8*", rawRegister, llvmType + "*", outputStream);
    __emitStore(llvmType, valueRegister, castRegister, outputStream);
    return rawRegister; 
}

std::string LLVMHelper::__emitConvert(const std::string& instr, const std::string& fromType, const std::string& value, const std::string& toType, std::ostream& outputStream) {
    std::string reg = __uniqueReg();
    outputStream <<"   " << reg <<" = "<< instr <<" " << fromType <<" " << value <<" to "<< toType <<"\n";
    return reg;
}
void LLVMHelper::__emitStore(const std::string& llvmType, const std::string& value, 
    const std::string& pointer, std::ostream& outputStream) {
    
    outputStream <<"   store " << llvmType <<" " << value<<", "<<llvmType<<"* "<<pointer<<", align "<< __alignOf(llvmType) <<"\n";
}

std::string LLVMHelper::__emitTypedGEP(const std::string& baseType, const std::string& pointer,const std::vector<std::pair<std::string, std::string>>& typedIndices,
    bool inbounds,std::ostream& outputStream){
    std::vector<std::string> indices;
    for (auto& idx:typedIndices) indices.push_back(idx.first + " " +idx.second);
    return __emitGEP(baseType, pointer, indices,inbounds,outputStream);
}

std::string LLVMHelper::__emitFieldGEP(const std::string& structType, const std::string& pointer, int fieldIndex, std::ostream& outputStream) {
    return __emitTypedGEP(structType, pointer, {{"i32", "0"}, {"i32", std::to_string(fieldIndex)}},true, outputStream);
}

std::string LLVMHelper::__emitNamedAlloca(const std::string& prefix, const std::string& llvmType,std::ostream& outputStream) {
    std::string reg =__namedReg(prefix);
    outputStream <<"    " << reg <<" = alloca " << llvmType <<"\n";
    return reg;
}