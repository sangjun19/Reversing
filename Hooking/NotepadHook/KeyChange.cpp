// dllmain.cpp
#include <windows.h>

static WNDPROC g_OrigEditProc = nullptr;   // 원래 Edit 창 프로시저 백업

// 1) 새 Edit 창 프로시저 ------------------------------------------------
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT  uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_CHAR)
    {
        // 모든 키 입력을 'A' 유니코드로 대체
        wParam = L'A';          // 대문자 A
        // wParam = L'a';       // 소문자 a - 원하면 이쪽 사용
    }

    // 원래 프로시저로 전달
    return CallWindowProcW(g_OrigEditProc, hWnd, uMsg, wParam, lParam);
}

// 2) Edit 컨트롤 찾아서 서브클래싱 -------------------------------------
DWORD WINAPI HookThread(LPVOID)
{
    // 메모장 메인 윈도우 & Edit 컨트롤 핸들 검색 (최대 5초 대기)
    HWND hNotepad = nullptr, hEdit = nullptr;
    for (int i = 0; i < 100 && !hEdit; ++i)
    {
        hNotepad = FindWindowW(L"Notepad", nullptr);
        if (hNotepad)
            hEdit = FindWindowExW(hNotepad, nullptr, L"Edit", nullptr);

        Sleep(50);
    }
    if (!hEdit) return 0;

    // 서브클래스 적용: 원래 프로시저 주소를 저장해 두어야 함
#ifdef _WIN64
    g_OrigEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
#else
    g_OrigEditProc = (WNDPROC)SetWindowLongW(hEdit, GWL_WNDPROC, (LONG)EditSubclassProc);
#endif

    return 0;
}

// 3) 진입점 -------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD   ul_reason_for_call, LPVOID  /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);          // 성능 + 데드락 예방
        CreateThread(nullptr, 0, HookThread, nullptr, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        // (선택) DLL 언로드 시 서브클래스 원복
        //  hEdit 핸들을 전역에 저장해 두었다면:
        //  SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)g_OrigEditProc);
        break;
    }
    return TRUE;
}
