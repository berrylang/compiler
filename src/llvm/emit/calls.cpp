

#include "../LLVMHelper.h"

std::string LLVMHelper::__emitCall(const std::string& returnType, const std::string& functionName, 
                            const std::vector<std::pair<std::string, std::string>>& arguments, std::ostream& outputStream) {
    std::string reg;

    if (returnType != "void") {
        reg = __uniqueReg();
        outputStream <<"   " << reg<<" = call "<< returnType <<" @"<< functionName <<"("; 
    }
    else {
        outputStream <<"   call " << returnType <<" @" << functionName<<"(";

    }
    for (size_t i = 0; i < arguments.size(); ++i) {
        outputStream << arguments[i].first <<" " <<arguments[i].second;
        if (i + 1 < arguments.size()) outputStream <<", ";
    }

    outputStream <<")\n";
    return reg;
}
