

#include "../LLVMHelper.h"
#include <iomanip>
#include <cstring>
#include <cstdint>


std::string LLVMHelper::__escapeString(const std::string&  raw) {
    std::ostringstream escape;

    for (char c:raw) {
        if (c =='\n') { escape <<"\\0A";}
        else if (c=='\t') { escape <<"\\09";}
        else if (c == '\r') { escape <<"\\0D";}
        else if (c=='\\') { escape <<"\\5C";}
        else if (c== '"') { escape <<"\\22";}
        else if (c < 32 || c > 126) {
            escape <<"\\" << std::hex << std::uppercase
                <<std::setw(2) << std::setfill('0') <<
                (int)(unsigned char) c;
        }
        else {
            escape << c;
        }
    }
    escape <<"\\00";
    return escape.str();
}

std::string LLVMHelper::__formatDoubleConstant(double value) {
    std::ostringstream stringStream;
    stringStream << std::scientific << std::setprecision(17) << value;
    return stringStream.str();
}

std::string LLVMHelper::__emitGlobalStringConstant(const std::string& raw) {
    std::string escape = __escapeString(raw);

    int len = (int) raw.length()+1;

    std::string globalName = "@.str." + std::to_string(__str_counter++);
    __GLOBAL_STRINGS << globalName <<" = private unnamed_addr constant [" << len << 
        " x i8] c\"" << escape <<"\"\n";
    
    return "getelementptr ([" + std::to_string(len)+ " x i8], [" +std::to_string(len) 
        + " x i8]* " + globalName + ", i32 0, i32 0)";
}


std::string LLVMHelper::__formatFloatHexConstant(double value) {
    std::ostringstream stringStream;

    float f_val = static_cast<float>(value);
    double llvm_float = static_cast<double>(f_val);

    uint64_t hex_value;
    std::memcpy(&hex_value, &llvm_float, sizeof(double));

    stringStream <<"0x" << std::setfill('0') << std::setw(16) <<
        std::hex << std::uppercase << hex_value;
    
    return stringStream.str();
}

void LLVMHelper::__emitGlobalVar(const std::string& name, 
    const std::string& llvmType, const std::string& initVal, 
    std::ostream& outputStream) {

        outputStream <<"@"<<name<<" = global "<<llvmType<<" "<< initVal <<"\n";

    }

std::string LLVMHelper::__globalRef(const std::string& name) 

    {return "@" + name;}

std::string LLVMHelper::__globalRef(const std::string& name, const std::string& suffix) 
    {return "@" + name + suffix;}