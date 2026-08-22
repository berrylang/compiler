

#include "../LLVMHelper.h"



std::string LLVMHelper::__structTypeName(const std::string& className) {
    return "%class." + className;
}

void LLVMHelper::__emitStructType(const std::string& structTypeName, const std::vector<std::string>&  fieldLLVMTypes) {

    __STRUCTURE_declares << structTypeName <<" = type { ";
    if (fieldLLVMTypes.empty()) {
        __STRUCTURE_declares <<"i8";
    }   else {
        for (size_t i = 0; i < fieldLLVMTypes.size(); i++) {
            __STRUCTURE_declares << fieldLLVMTypes[i];
            if (i + 1 < fieldLLVMTypes.size())  __STRUCTURE_declares <<", ";
        }
    }
    __STRUCTURE_declares <<" }\n";
}


void LLVMHelper::__emitClassNameGlobal(const std::string& className) {
    std::string escape = __escapeString(className);

    int len = (int) className.size() + 1;

    __GLOBAL_STRINGS <<"@.classname." << className <<" = private unnamed_addr constant [" << len <<" x i8] c\"" << escape <<"\"\n";
    __GLOBAL_STRINGS <<"@" << className <<"_typeid = global i32 0\n";
}


std::string LLVMHelper::__emitClassNameGEP(const std::string& className, int nameLen) {
    return "getelementptr (["+std::to_string(nameLen) + " x i8], [" +std::to_string(nameLen)+
        " x i8]* @.classname." +className +", i32 0, i32 0)";
}

std::string LLVMHelper::__emitDestructorBitcast(const std::string& structType, const std::string& className, std::ostream& outputStream) {
    std::string reg = __uniqueReg();
    outputStream <<"   "<< reg <<" = bitcast void (" << structType <<"*)* @" << className<<"$destructor to i8*\n";
    return reg;
}


std::string LLVMHelper::__emitTypeRegisterCall(const std::string& className, int nameLen, long long instanceSize, 
                                        const std::string& destructorArg, std::ostream& outputStream) {
    __declareExtern("declare i32 @bery_type_register(i8*, i64, i8*, i64, i8*)", "bery_type_register");
    std::string idRegister = __uniqueReg();
    outputStream<<"    " << idRegister <<" = call i32 @bery_type_register(i8* "<< __emitClassNameGEP(className, nameLen)
    <<", i64 " << instanceSize <<", i8* null, i64 0, " << destructorArg <<")\n";
    outputStream <<"   store i32 " << idRegister <<", i32* @" << className <<"_typeid\n";
    return idRegister;
}

std::string LLVMHelper::__mangleMethod(const std::string& className, const std::string& methodName) {
    return className + "_" + methodName;
}

std::string LLVMHelper::__mangleConstructor(const std::string& className) {
    return className + "$constructor";
}

std::string LLVMHelper::__mangleDestructor(const std::string& className) {
    return className + "$destructor";
}

std::string LLVMHelper::__mangleOverload(const std::string& base, const std::vector<std::string>& paramTypes) {
    std::string result = base;
    for (auto& beryType : paramTypes) {
        std::string lt = __llvmType(beryType);
        if (lt.empty()) {
            result += "." + beryType;
        } else if (beryType == "string") {
            result += ".str";
        } else if (beryType.size() > 6 && beryType.substr(0, 6) == "array<") {
            result += ".arr";
        } else {
            result += "." + lt;
        }
    }
    return result;
}