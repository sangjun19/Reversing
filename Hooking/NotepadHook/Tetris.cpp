// dllmain.cpp
#include <windows.h>
#include <cstdlib>
#include <ctime>
#include <stdio.h>

static WNDPROC g_OrigEditProc = nullptr;   // 원래 Edit 창 프로시저 백업

constexpr int W = 10;     // 가로 10
constexpr int H = 20;     // 세로 20

static int board[H][W + 1];
static POINT   piecePos = { 3, 0 };  // x = 1 ~ 10
static HWND hEdit = nullptr;  // Edit 컨트롤 핸들
static bool suppress_selection = false;
static bool isMinoGenerated = false;
static int currentMinoIndex = 0;

int mino[7][4][4] = 
{
    {
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0}
    },
    {
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {0, 1, 1, 0}
    },
    {
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 1, 1, 0},
        {0, 0, 1, 1}
    },
    {
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 0, 1, 1},
        {0, 1, 1, 0}
    },
    {
        {0, 0, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 1, 1, 0}
    },
    {
        {0, 0, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 0},
        {0, 0, 1, 1}
    },
    {
        {0, 0, 0, 0},
        {0, 0, 0, 0},
        {0, 1, 1, 1},
        {0, 0, 1, 0}
    }
};


// 1) 새 Edit 창 프로시저 ------------------------------------------------
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        // 마우스 드래그 차단
        case WM_LBUTTONDOWN:
            suppress_selection = true;
            SetCapture(hWnd);  // 마우스 캡처 (드래그 무효화 목적)
            SetFocus(hWnd);    // 클릭 시 포커스도 강제로 주기
            return 0;

        case WM_LBUTTONUP:
            suppress_selection = false;
            ReleaseCapture();
            return 0;

        case WM_MOUSEMOVE:
            if (suppress_selection)
                return 0;
            break;

        // 마우스 포인터는 숨기지 않음
        case WM_SETCURSOR:
            // 아무 것도 하지 않으면 기본 커서가 유지됨
            break;

        // 키보드 입력 차단 (예시)
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_UP:
                case VK_DOWN:
                case VK_LEFT:
                case VK_RIGHT:
                case VK_SPACE:
                    return 0;
            }
            break;
    }

    // 기본 처리
    return CallWindowProcW(g_OrigEditProc, hWnd, uMsg, wParam, lParam);
}

DWORD WINAPI DrawMap(LPVOID)
{
    if (!hEdit) return 0;
    wchar_t display[H][W];

    // 초기 맵 설정
    // for(int i = 0; i < H; i++) {
    //     for(int j = 0; j < W; j++) {
    //         if(board[i][j] == 0) 
    //             display[i][j] = L'▣';  // 블록 표시
    //         else
    //             display[i][j] = L'.';
    //     }
    //     board[i][W] = L'\0';  // 문자열 종단
    // }

    // 맵을 Edit 컨트롤에 출력
    SendMessageW(hEdit, EM_SETSEL, 0, -1);  // 전체 선택
    SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)L"");  // 초기화

    while (true) {
        // 맵을 문자열로 변환
        wchar_t mapStr[4095] = L"\0";
        for (int i = 0; i < H; i++) {
            wcscat_s(mapStr, L"■");
            for(int j = 0; j < W; j++) // Mino 그리기
            {
                if(board[i][j] == 1) 
                    wcscat_s(mapStr, L"▣");
                else
                    wcscat_s(mapStr, L"□");
            }
            // wcscat_s(mapStr, display[i]);
            wcscat_s(mapStr, L"■");
            wcscat_s(mapStr, L"\r\n");
        }
        for(int j = 0; j < W + 2; j++) 
            wcscat_s(mapStr, L"■");
        
        wcscat_s(mapStr, L"\r\n");

        // 디버깅용
        wchar_t buffer[16];
        swprintf_s(buffer, 16, L"%d", piecePos.y);
        wcscat_s(mapStr, buffer);
            

        // Edit 컨트롤에 출력
        SendMessageW(hEdit, EM_SETSEL, 0, -1);  // 전체 선택
        SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)mapStr);  // 출력

        Sleep(500);  // 잠시 대기
    }

    return 0;
}

DWORD WINAPI DropMino(LPVOID)
{
    while(!isMinoGenerated) {
        Sleep(100);
    }

    while(true) {
        piecePos.y++;
        Sleep(1000);
    }
}

DWORD WINAPI GenerateMino(LPVOID)
{
    if(isMinoGenerated) {
        srand(static_cast<unsigned int>(time(nullptr)));
        currentMinoIndex = rand() % 7;
        isMinoGenerated = true;
    }

    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(mino[currentMinoIndex][i][j]) {
                int x = piecePos.x + j;
                int y = piecePos.y + i - 3;

                if(x >= 1 && x <= W && y >= 0 && y < H) {
                    board[y][x] = 1;  // 블록 표시
                }
            }
        }
    }
    
    return 0;
}

DWORD WINAPI GamePlayThread(LPVOID)
{
    while(!hEdit)
        Sleep(10);

    // 게임 맵 그리기 스레드 시작
    CreateThread(nullptr, 0, DrawMap, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, GenerateMino, nullptr, 0, nullptr); // Test, 블럭 다운에 생성
    CreateThread(nullptr, 0, DropMino, nullptr, 0, nullptr);

    return 0;
}

// 2) Edit 컨트롤 찾아서 서브클래싱 -------------------------------------
DWORD WINAPI HookThread(LPVOID)
{
    // 메모장 메인 윈도우 & Edit 컨트롤 핸들 검색 (최대 5초 대기)
    HWND hNotepad = nullptr;
    for(int i = 0; i < 100 && !hEdit; ++i)
    {
        hNotepad = FindWindowW(L"Notepad", nullptr);
        if (hNotepad)
            hEdit = FindWindowExW(hNotepad, nullptr, L"Edit", nullptr);

        Sleep(50);
    }
    if(!hEdit) return 0;

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
        DisableThreadLibraryCalls(hModule); // 성능 + 데드락 예방
        CreateThread(nullptr, 0, HookThread, nullptr, 0, nullptr);
        CreateThread(nullptr, 0, GamePlayThread, nullptr, 0, nullptr);
    
        break;

    case DLL_PROCESS_DETACH:
        // (선택) DLL 언로드 시 서브클래스 원복
        //  hEdit 핸들을 전역에 저장해 두었다면:
        SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)g_OrigEditProc);
        break;
    }
    return TRUE;
}
