#include <windows.h>
#include <stdio.h>

void DumpDebugRegisters() {
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (GetThreadContext(GetCurrentThread(), &ctx)) {
        printf("DR0: 0x%llX\n", ctx.Dr0);
        printf("DR1: 0x%llX\n", ctx.Dr1);
        printf("DR2: 0x%llX\n", ctx.Dr2);
        printf("DR3: 0x%llX\n", ctx.Dr3);
        printf("DR6: 0x%llX\n", ctx.Dr6);
        printf("DR7: 0x%llX\n", ctx.Dr7);
    } else {
        printf("Error: (%lu)\n", GetLastError());
    }
}

int main() {    
    __asm__("int3");
    printf("Main Start\n");
    int n = 1234;

    n = 100;    
    DumpDebugRegisters();
    
    return 0;
}
