#include "../codegen.h"
#include <stack>

void CodeGen::emitGCPush(const std::string& allocaReg, const std::string& lt, std::ostream& outputStream) {
    llvm.__emitGCPushCall(allocaReg, lt, outputStream);
    gcRootCounter++;
    gcRootScopeStack.top()++;
}

void CodeGen::emitGCPops(int count, std::ostream& outputStream) {
    llvm.__emitGCPopCalls(count, outputStream);
}

void CodeGen::pushGCScope() {
    gcRootScopeStack.push(0);
}

int CodeGen::popGCScope() {
    if (gcRootScopeStack.empty()) return 0;
    int count = gcRootScopeStack.top();
    gcRootScopeStack.pop();
    return count;
}