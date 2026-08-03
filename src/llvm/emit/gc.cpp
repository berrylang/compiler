#include "../LLVMHelper.h"

void LLVMHelper::__emitGCPushCall(const std::string& reg, const std::string& llvmType, std::ostream& outputStream) {
    __declareExtern("declare void @bery_gc_push_root(i8**)", "bery_gc_push_root");
    std::string castReg = __emitBitcast(llvmType + "*", reg, "i8**", outputStream);
    __emitCall("void", "bery_gc_push_root", {{"i8**", castReg}}, outputStream);
}

void LLVMHelper::__emitGCPopCalls(int count, std::ostream& outputStream) {
    __declareExtern("declare void @bery_gc_pop_root()", "bery_gc_pop_root");
    for (int i = 0; i < count; ++i) {
        __emitCall("void", "bery_gc_pop_root", {}, outputStream);
    }
}