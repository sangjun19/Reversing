// dllmain.cpp
#include <windows.h>

static WNDPROC g_OrigEditProc = nullptr;   // 원래 Edit 창 프로시저 백업

constexpr int W = 10;     // 가로 10
constexpr int H = 20;     // 세로 20

static wchar_t board[H][W + 1];   // +1 은 널 종단
static POINT   piecePos = { W/2, 0 };   // 내려오는 블록(■) 좌표
static HWND hEdit = nullptr;  // Edit 컨트롤 핸들


// 1) 새 Edit 창 프로시저 ------------------------------------------------
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT  uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_CHAR || uMsg == WM_KEYDOWN)
    {
        switch (wParam) {
            case VK_UP: {
                const wchar_t* text = L"TEST\r\nInjected from DLL!\r\n";
                SendMessageW(hEdit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);      // move caret to end
                SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)text);      // paste
                break;
            }
            case VK_DOWN:
                break;
            case VK_LEFT:
                break;
            case VK_RIGHT:
                break;
            case VK_SPACE:
                break;
            default:
                return 0;
        }
    }

    // 원래 프로시저로 전달
    return CallWindowProcW(g_OrigEditProc, hWnd, uMsg, wParam, lParam);
}

DWORD WINAPI DrawMap(LPVOID)
{
    if (!hEdit) return 0;

    // 초기 맵 설정
    for (int i = 0; i < H; ++i) {
        for (int j = 0; j < W; ++j) {
            board[i][j] = L'.';
        }
        board[i][W] = L'\0';  // 널 종단
    }

    // 맵을 Edit 컨트롤에 출력
    SendMessageW(hEdit, EM_SETSEL, 0, -1);  // 전체 선택
    SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)L"");  // 초기화

    while (true) {
        // 맵을 문자열로 변환
        wchar_t mapStr[H * (W + 1) + 1] = { 0 };
        for (int i = 0; i < H; ++i) {
            wcscat_s(mapStr, board[i]);
            wcscat_s(mapStr, L"\r\n");
        }

        // Edit 컨트롤에 출력
        SendMessageW(hEdit, EM_SETSEL, 0, -1);  // 전체 선택
        SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)mapStr);  // 출력

        Sleep(500);  // 잠시 대기
    }

    return 0;
}

// 2) Edit 컨트롤 찾아서 서브클래싱 -------------------------------------
DWORD WINAPI HookThread(LPVOID)
{
    // 메모장 메인 윈도우 & Edit 컨트롤 핸들 검색 (최대 5초 대기)
    HWND hNotepad = nullptr;
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

// 3) 진입점 -------------------------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD   ul_reason_for_call, LPVOID  /*lpReserved*/)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule); // 성능 + 데드락 예방
        CreateThread(nullptr, 0, HookThread, nullptr, 0, nullptr);
        // CreateThread(nullptr, 0, InjectThread, nullptr, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        // (선택) DLL 언로드 시 서브클래스 원복
        //  hEdit 핸들을 전역에 저장해 두었다면:
        SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)g_OrigEditProc);
        break;
    }
    return TRUE;
}
