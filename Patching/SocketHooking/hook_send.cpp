#include <windows.h>
#include <winsock2.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

typedef int (WINAPI *SEND)(SOCKET, const char *, int, int);
SEND original_send = NULL;
BYTE original_bytes[5];
BYTE patch_bytes[5];

int WINAPI hooked_send(SOCKET s, const char *buf, int len, int flags) {
    char new_buf[1024] = {0};
    strcpy(new_buf, "hacked");

    // 원본 함수 복원
    DWORD oldProtect;
    VirtualProtect((LPVOID)original_send, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)original_send, original_bytes, 5);
    VirtualProtect((LPVOID)original_send, 5, oldProtect, &oldProtect);

    // 원본 함수 호출
    int ret = original_send(s, new_buf, strlen(new_buf), flags);

    // 다시 후킹 적용
    VirtualProtect((LPVOID)original_send, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)original_send, patch_bytes, 5);
    VirtualProtect((LPVOID)original_send, 5, oldProtect, &oldProtect);

    char dbg[128];
    sprintf(dbg, "[hooked_send] ret = %d, sent: %s", ret, new_buf);
    OutputDebugStringA(dbg);

    return ret;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason, LPVOID lpReserved) {
    if (ul_reason == DLL_PROCESS_ATTACH) {
        MessageBoxA(NULL, "DLL injected", "Status", MB_OK);

        HMODULE hMod = GetModuleHandleA("ws2_32.dll");
        FARPROC target = GetProcAddress(hMod, "send");
        original_send = (SEND)target;

        // 원본 5바이트 백업
        memcpy(original_bytes, (void*)target, 5);

        // 패치 바이트 생성
        DWORD offset = (DWORD)hooked_send - (DWORD)target - 5;
        patch_bytes[0] = 0xE9;
        memcpy(patch_bytes + 1, &offset, 4);

        // 후킹 적용
        DWORD oldProtect;
        VirtualProtect((LPVOID)target, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)target, patch_bytes, 5);
        VirtualProtect((LPVOID)target, 5, oldProtect, &oldProtect);
    }
    return TRUE;
}