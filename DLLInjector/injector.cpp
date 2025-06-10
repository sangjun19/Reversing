#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

BOOL InjectDLL(DWORD pid, const char* dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) return FALSE;

    LPVOID alloc = VirtualAllocEx(hProc, NULL, strlen(dllPath)+1, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProc, alloc, dllPath, strlen(dllPath)+1, NULL);

    HMODULE hKernel = GetModuleHandleA("kernel32.dll");
    FARPROC loadLib = GetProcAddress(hKernel, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)loadLib, alloc, 0, NULL);
    WaitForSingleObject(hThread, INFINITE);

    VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return TRUE;
}

int main() {
    DWORD pid;
    //경로 수정 필요
    char dllPath[MAX_PATH] = "hook.dll";

    printf("Enter PID: ");
    scanf("%lu", &pid);

    if (InjectDLL(pid, dllPath)) {
        printf("[+] DLL injected!\n");
    } else {
        printf("[-] Injection failed.\n");
    }
    return 0;
}
