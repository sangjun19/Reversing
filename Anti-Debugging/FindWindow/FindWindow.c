#include <windows.h>
#include <stdio.h>

int main() {

    HWND hwnd = FindWindow(NULL, "OllyDbg - [CPU]");

    if (hwnd != NULL) {
        MessageBox(NULL, "디버거 탐지됨! (OLLYDBG 창 존재)", "안티디버깅", MB_OK);
        return 1;
    } else {
        MessageBox(NULL, "디버거 없음", "안티디버깅", MB_OK);
    }

    return 0;
}
