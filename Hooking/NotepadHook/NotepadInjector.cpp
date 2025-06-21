#include <windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <stdio.h>

// 메모장 프로세스 찾기
DWORD FindNotepadPID() {
    DWORD pid = 0;
    PROCESSENTRY32 pe32 = { 0 };
    pe32.dwSize = sizeof(PROCESSENTRY32);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE)
        return 0;

    if (Process32First(hSnap, &pe32)) {
        do {
            if (_tcsicmp(pe32.szExeFile, _T("notepad.exe")) == 0) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hSnap, &pe32));
    }

    CloseHandle(hSnap);
    return pid;
}

// 파일 존재 여부 확인
BOOL FileExists(const char* filePath) {
    DWORD dwAttrib = GetFileAttributesA(filePath);
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// DLL 인젝션 함수
BOOL InjectDLL(DWORD pid, const char* dllPath) {
    printf("[*] Starting DLL injection process...\n");
    printf("[*] Target PID: %lu\n", pid);
    printf("[*] DLL Path: %s\n", dllPath);
    
    // 1. DLL 파일 존재 여부 확인
    printf("[*] Checking if DLL file exists...\n");
    if (!FileExists(dllPath)) {
        printf("[-] ERROR: DLL file not found at: %s\n", dllPath);
        printf("[*] Current working directory info:\n");
        char currentDir[MAX_PATH];
        if (GetCurrentDirectoryA(MAX_PATH, currentDir)) {
            printf("    Current directory: %s\n", currentDir);
        }
        return FALSE;
    }
    printf("[+] DLL file found!\n");

    // 2. 프로세스 열기
    printf("[*] Opening target process...\n");
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        printf("[-] OpenProcess failed: %lu\n", GetLastError());
        printf("[-] Possible reasons:\n");
        printf("    - Insufficient privileges (try running as Administrator)\n");
        printf("    - Process no longer exists\n");
        printf("    - Anti-virus blocking access\n");
        return FALSE;
    }
    printf("[+] Process opened successfully!\n");

    // 3. 메모리 할당
    printf("[*] Allocating memory in target process...\n");
    size_t pathLen = strlen(dllPath) + 1;
    printf("[*] Allocating %zu bytes for DLL path\n", pathLen);
    
    LPVOID alloc = VirtualAllocEx(hProc, NULL, pathLen, MEM_COMMIT, PAGE_READWRITE);
    if (!alloc) {
        printf("[-] VirtualAllocEx failed: %lu\n", GetLastError());
        CloseHandle(hProc);
        return FALSE;
    }
    printf("[+] Memory allocated at: 0x%p\n", alloc);

    // 4. DLL 경로 쓰기
    printf("[*] Writing DLL path to target process memory...\n");
    if (!WriteProcessMemory(hProc, alloc, dllPath, pathLen, NULL)) {
        printf("[-] WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }
    printf("[+] DLL path written successfully!\n");

    // 5. LoadLibraryA 주소 가져오기
    printf("[*] Getting LoadLibraryA address...\n");
    FARPROC loadLib = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!loadLib) {
        printf("[-] GetProcAddress failed.\n");
        VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }
    printf("[+] LoadLibraryA address: 0x%p\n", loadLib);

    // 6. 원격 스레드 생성
    printf("[*] Creating remote thread...\n");
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)loadLib, alloc, 0, NULL);
    if (!hThread) {
        printf("[-] CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }
    printf("[+] Remote thread created successfully!\n");

    // 7. 스레드 완료 대기
    printf("[*] Waiting for remote thread to complete...\n");
    DWORD waitResult = WaitForSingleObject(hThread, 5000); // 5초 타임아웃
    
    if (waitResult == WAIT_TIMEOUT) {
        printf("[-] Thread execution timed out!\n");
        CloseHandle(hThread);
        VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    } else if (waitResult != WAIT_OBJECT_0) {
        printf("[-] WaitForSingleObject failed: %lu\n", GetLastError());
        CloseHandle(hThread);
        VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return FALSE;
    }

    // 8. 실행 결과 확인
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    printf("[*] LoadLibraryA returned: 0x%lx\n", exitCode);

    if (exitCode == 0) {
        printf("[-] LoadLibraryA failed! Possible reasons:\n");
        printf("    - DLL not found in target process context\n");
        printf("    - DLL architecture mismatch (32bit vs 64bit)\n");
        printf("    - DLL dependencies missing\n");
        printf("    - DLL initialization failed\n");
        printf("    - Access denied to DLL file\n");
        
        // 추가 디버그 정보
        printf("[*] Debug info:\n");
        printf("    - Your exe architecture: %s\n", sizeof(void*) == 8 ? "64-bit" : "32-bit");
        printf("    - Try using absolute path instead of relative path\n");
        printf("    - Make sure DLL and target process have same architecture\n");
    } else {
        printf("[+] DLL loaded successfully! Module handle: 0x%lx\n", exitCode);
    }

    // 9. 리소스 정리
    CloseHandle(hThread);
    VirtualFreeEx(hProc, alloc, 0, MEM_RELEASE);
    CloseHandle(hProc);

    return exitCode != 0;
}

int main() {
    // 절대 경로로 변경 (여기를 실제 DLL 경로로 수정하세요)
    char dllPath[MAX_PATH];
    
    // 현재 디렉터리 기준으로 절대 경로 생성
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    snprintf(dllPath, MAX_PATH, "%s\\test.dll", currentDir);
    
    // 또는 직접 절대 경로 지정
    // strcpy_s(dllPath, MAX_PATH, "C:\\full\\path\\to\\your\\test.dll");

    printf("=== DLL Injection Tool ===\n");
    printf("[*] Searching for notepad.exe...\n");
    
    DWORD pid = FindNotepadPID();
    // DWORD pid = 15296; // 하드코딩된 PID 사용시

    if (pid == 0) {
        printf("[-] Could not find notepad.exe\n");
        printf("[*] Please start notepad.exe and try again\n");
    } else {
        printf("[+] Found notepad.exe (PID: %lu)\n", pid);
        printf("[*] Attempting to inject DLL...\n");
        printf("=====================================\n");

        if (InjectDLL(pid, dllPath)) {
            printf("=====================================\n");
            printf("[+] DLL injection successful!\n");
        } else {
            printf("=====================================\n");
            printf("[-] DLL injection failed.\n");
        }
    }
    
    printf("\nPress any key to exit...\n");
    system("pause");
    return 0;
}