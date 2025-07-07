// notepad_tetris_hook.cpp – inject into Notepad and play a simple falling‑block demo
// 빌드: cl /LD notepad_tetris_hook.cpp user32.lib gdi32.lib

#include <windows.h>
#include <cstdlib>
#include <ctime>
#include <stdio.h>

// Todo
// 내려가다 장애물 만나도 멈추지 않음
// 방향키 입력이 될때가 있고 안 될때가 있음

//----------------------------------------------
// 전역 상태
//----------------------------------------------
static WNDPROC g_OrigEditProc = nullptr;   // 서브클래스 이전 원본 프로시저

constexpr int W = 10;        // 가로 10칸
constexpr int H = 20;        // 세로 20칸

static int   board[H][W];    // 0 = 빈칸, 1 = 블록
static POINT minoPos = {3, -1}; // 블록 기준 좌표 (왼쪽 위)
static HWND  hEdit = nullptr; // Notepad Edit 컨트롤
static bool  suppress_selection = false;
static bool  isMinoGenerated = false;
static int   currentMinoIndex = 0;

//----------------------------------------------
// 7가지 미노 정의 (4×4)
//----------------------------------------------
static const int mino[7][4][4] = {
    // I
    {
        {0,0,1,0},
        {0,0,1,0},
        {0,0,1,0},
        {0,0,1,0}
    },
    // O
    {
        {0,0,0,0},
        {0,0,0,0},
        {0,1,1,0},
        {0,1,1,0}
    },
    // Z
    {
        {0,0,0,0},
        {0,0,0,0},
        {0,1,1,0},
        {0,0,1,1}
    },
    // S
    {
        {0,0,0,0},
        {0,0,0,0},
        {0,0,1,1},
        {0,1,1,0}
    },
    // J
    {
        {0,0,0,0},
        {0,0,1,0},
        {0,0,1,0},
        {0,1,1,0}
    },
    // L
    {
        {0,0,0,0},
        {0,0,1,0},
        {0,0,1,0},
        {0,0,1,1}
    },
    // T
    {
        {0,0,0,0},
        {0,0,0,0},
        {0,1,1,1},
        {0,0,1,0}
    }
};

int MoveMino(int dy, int dx)
{
    int y = minoPos.y + dy;
    int x = minoPos.x + dx;
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(mino[currentMinoIndex][i][j]) // 현재 미노의 블록이 있는 위치
            {
                int newY = y + i - 3; // 미노 기준 좌표로 변환
                int newX = x + j;
                if(newY < 0 || newY >= H || newX < 0 || newX >= W || board[newY][newX]) {
                    return 0; // 벽이나 다른 블록에 닿으면 이동 불가
                }
            }
        }
    }    
    minoPos.y = y;
    minoPos.x = x;
    return 1; // 이동 성공
}

//----------------------------------------------
// Edit 서브클래스 – 마우스/키보드 입력 차단
//----------------------------------------------
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_LBUTTONDOWN:
            suppress_selection = true;
            SetCapture(hWnd);
            SetFocus(hWnd);
            return 0;
        case WM_LBUTTONUP:
            suppress_selection = false;
            ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE:
            if (suppress_selection) return 0;
            break;
        case WM_KEYDOWN: // 키 입력 무력화
            switch (wParam) {
                case VK_UP:
                case VK_DOWN:
                    MoveMino(1, 0);
                    return 0;
                case VK_LEFT:
                    MoveMino(0, -1);
                    return 0;
                case VK_RIGHT:
                    MoveMino(0, 1);
                    return 0;
                case VK_SPACE:
                    return 0;
            }
            break;
    }
    return CallWindowProcW(g_OrigEditProc, hWnd, uMsg, wParam, lParam);
}

void ResetBoard()
{
    for(int i = 0; i < H; ++i)
        for(int j = 0; j < W; ++j)
            board[i][j] = board[i][j] * (board[i][j] - 1);
}

//----------------------------------------------
// 보드 + 문자열 생성 헬퍼
//----------------------------------------------
static void BuildBoardString(wchar_t* out, size_t outSz)
{
    out[0] = L'\0';

    // 보드 배열 클리어
    // ZeroMemory(board, sizeof(board)); // 수정 필요!
    ResetBoard();

    // 현재 미노를 board[][]에 반영
    for(int i = 0; i < 4; ++i)
    {
        for(int j = 0; j < 4; ++j)
        {
            if(mino[currentMinoIndex][i][j])
            {
                int x = minoPos.x + j;
                int y = minoPos.y + i - 3;
                if (x >= 0 && x < W && y >= 0 && y < H)
                    board[y][x] = 1;
            }
        }
    }

    // 위/아래 벽 + 내용 문자열 생성
    for(int i = 0; i < H; ++i)
    {
        wcscat_s(out, outSz, L"■");
        for (int j = 0; j < W; ++j)
            wcscat_s(out, outSz, board[i][j] ? L"▣" : L"□");
        wcscat_s(out, outSz, L"■\r\n");
    }
    for(int j = 0; j < W + 2; ++j) wcscat_s(out, outSz, L"■");

    // 디버그: y 좌표 출력
    wchar_t tmp[32];
    swprintf_s(tmp, L"\r\nY = %d, X = %d", minoPos.y, minoPos.x);
    wcscat_s(out, outSz, tmp);
}

//----------------------------------------------
// 게임 화면 그리기 스레드
//----------------------------------------------
DWORD WINAPI DrawMap(LPVOID)
{
    while(!hEdit) Sleep(100);

    constexpr size_t BUF_SZ = 4096;
    wchar_t mapStr[BUF_SZ];

    while(true)
    {
        BuildBoardString(mapStr, BUF_SZ);
        SendMessageW(hEdit, WM_SETTEXT, 0, (LPARAM)mapStr);
        UpdateWindow(hEdit);
        Sleep(50); // 화면 주사 간격
    }
    return 0;
}

void FixMino() {
    for(int i = 0; i < H; i++)
        for(int j = 0; j < W; j++)
            board[i][j] *= 2;
}

//----------------------------------------------
// 블록을 한 칸씩 떨어뜨리는 스레드
//----------------------------------------------
DWORD WINAPI DropMino(LPVOID)
{
    while(true) {
        while(!isMinoGenerated) Sleep(10);

        while(true)
        {
            if(minoPos.y + 1 == H) // Mino가 바닥에 닿았을 때
            {
                FixMino();
                isMinoGenerated = false;
                break;
            }
            minoPos.y++;
            Sleep(1000);
        }
    }
}

//----------------------------------------------
// 새 미노 생성 스레드 (한 번 실행)
//----------------------------------------------
DWORD WINAPI GenerateMino(LPVOID)
{
    while(true)
    {
        if (!isMinoGenerated)
        {
            srand(static_cast<unsigned int>(time(nullptr)));
            currentMinoIndex = rand() % 7;
            minoPos.x = 3;
            minoPos.y = -1;
            isMinoGenerated = true;
        }
    }
    return 0;
}

//----------------------------------------------
// Notepad Edit 컨트롤 찾고 서브클래싱
//----------------------------------------------
DWORD WINAPI HookThread(LPVOID)
{
    HWND hNotepad = nullptr;
    for(int i = 0; i < 100 && !hEdit; ++i)
    {
        hNotepad = FindWindowW(L"Notepad", nullptr);
        if(hNotepad)
            hEdit = FindWindowExW(hNotepad, nullptr, L"Edit", nullptr);
        Sleep(50);
    }
    if(!hEdit) return 0;

#ifdef _WIN64
    g_OrigEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
#else
    g_OrigEditProc = (WNDPROC)SetWindowLongW(hEdit, GWL_WNDPROC, (LONG)EditSubclassProc);
#endif
    return 0;
}

//----------------------------------------------
// DllMain – 스레드 기동
//----------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(nullptr, 0, HookThread, nullptr, 0, nullptr);
            CreateThread(nullptr, 0, GenerateMino, nullptr, 0, nullptr);
            CreateThread(nullptr, 0, DropMino, nullptr, 0, nullptr);
            CreateThread(nullptr, 0, DrawMap, nullptr, 0, nullptr);
            break;
        case DLL_PROCESS_DETACH:
            if (hEdit && g_OrigEditProc)
                SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)g_OrigEditProc);
            break;
    }
    return TRUE;
}
