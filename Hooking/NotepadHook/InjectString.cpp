// dllmain.cpp  (UNICODE build)
#include <windows.h>
#include <commdlg.h>

DWORD WINAPI InjectThread(LPVOID) {
    // 1) Wait for the Notepad top-level window.
    HWND hNotepad = nullptr;
    for (int i = 0; i < 100 && !hNotepad; ++i) {          // 5 s timeout
        hNotepad = FindWindowW(L"Notepad", nullptr);      // class "Notepad"
        Sleep(50);
    }
    if (!hNotepad) return 0;

    // 2) Grab the child Edit control.
    HWND hEdit = FindWindowExW(hNotepad, nullptr, L"Edit", nullptr);
    if (!hEdit) return 0;

    // 3) Append text at the caret (or at the end if nothing selected).
    const wchar_t* text = L"TEST\r\nInjected from DLL!\r\n";
    SendMessageW(hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);      // move caret to end
    SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)text);      // paste

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID /*lpReserved*/) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);               // avoid extra thread calls
        CreateThread(nullptr, 0, InjectThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
