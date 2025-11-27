#include <stdio.h>

int main() {    
    printf("Hello ");
    __asm__("int3");
    printf("World\n");
    printf("Finished\n");
    HardwareBP_Detection();
}
