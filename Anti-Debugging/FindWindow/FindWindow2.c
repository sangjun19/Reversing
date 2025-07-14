#include <windows.h>
#include <stdio.h>
#include <string.h>

BOOL ContainsOllyDbg(HWND hwnd) {
    char title[256];
    GetWindowTextA(hwnd, title, sizeof(title));

    if (strstr(title, "OllyDbg") != NULL) {
        return TRUE;
    }
    return FALSE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (IsWindowVisible(hwnd)) {
        if (ContainsOllyDbg(hwnd)) {
            MessageBoxA(NULL, "OllyDbg 창 감지됨!", "안티디버깅", MB_OK);
            ExitProcess(1);
        }
    }
    return TRUE;
}

int main() {
    EnumWindows(EnumWindowsProc, 0);
    MessageBoxA(NULL, "디버거 없음", "안티디버깅", MB_OK);
    return 0;
}
