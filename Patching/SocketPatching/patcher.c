// patcher.c
#include <stdio.h>
#include <windows.h>
#include <tlhelp32.h>
#include <string.h>

#define TARGET_STRING "hello"
#define REPLACE_STRING "hacked"

BOOL patch_buffer(DWORD pid) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        printf("[-] Failed to open process.\n");
        return FALSE;
    }

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    MEMORY_BASIC_INFORMATION mbi;
    BYTE *addr = 0;

    while (addr < si.lpMaximumApplicationAddress) {
        if (VirtualQueryEx(hProc, addr, &mbi, sizeof(mbi)) == 0)
            break;

        if ((mbi.State == MEM_COMMIT) && (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE))) {
            BYTE *buffer = malloc(mbi.RegionSize);
            SIZE_T bytesRead;

            if (ReadProcessMemory(hProc, addr, buffer, mbi.RegionSize, &bytesRead)) {
                for (SIZE_T i = 0; i < bytesRead - strlen(TARGET_STRING); i++) {
                    if (memcmp(buffer + i, TARGET_STRING, strlen(TARGET_STRING)) == 0) {
                        // found, patch it
                        SIZE_T written;
                        WriteProcessMemory(hProc, addr + i, REPLACE_STRING, strlen(REPLACE_STRING), &written);
                        printf("[+] Patched at address: %p\n", addr + i);
                        free(buffer);
                        CloseHandle(hProc);
                        return TRUE;
                    }
                }
            }

            free(buffer);
        }

        addr += mbi.RegionSize;
    }

    CloseHandle(hProc);
    return FALSE;
}

int main() {
    DWORD pid;
    printf("Enter target PID: ");
    scanf("%lu", &pid);

    if (patch_buffer(pid)) {
        printf("[+] Successfully patched.\n");
    } else {
        printf("[-] Patch failed.\n");
    }

    return 0;
}
