// notepad_tetris_hook.cpp – inject into Notepad and play a simple falling‑block demo
// 빌드: cl /LD notepad_tetris_hook.cpp user32.lib gdi32.lib

#include <windows.h>
#include <cstdlib>
#include <ctime>
#include <stdio.h>

//----------------------------------------------
// 전역 상태
//----------------------------------------------
static WNDPROC g_OrigEditProc = nullptr;   // 서브클래스 이전 원본 프로시저

constexpr int W = 10;        // 가로 10칸
constexpr int H = 20;        // 세로 20칸

static int   board[H][W];    // 0 = 빈칸, 1 = 블록, 2 = 고정된 블록
static POINT minoPos = {3, -1}; // 블록 기준 좌표 (왼쪽 위)
static HWND  hEdit = nullptr; // Notepad Edit 컨트롤
static bool  suppress_selection = false;
static bool  isMinoGenerated = false;
static int   currentMinoIndex = 0;
static bool  needRedraw = true; // 화면 갱신 필요 여부
static CRITICAL_SECTION cs; // 동기화용 크리티컬 섹션

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

// 충돌 검사 함수
bool CheckCollision(int y, int x, int minoIndex)
{
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(mino[minoIndex][i][j]) // 현재 미노의 블록이 있는 위치
            {
                int newY = y + i - 3; // 미노 기준 좌표로 변환
                int newX = x + j;
                
                // 경계 체크
                if(newX < 0 || newX >= W) return true;
                if(newY >= H) return true;
                
                // 기존 블록과 충돌 체크 (고정된 블록만)
                if(newY >= 0 && board[newY][newX] == 2) return true;
            }
        }
    }
    return false;
}

int MoveMino(int dy, int dx)
{
    EnterCriticalSection(&cs);
    
    int newY = minoPos.y + dy;
    int newX = minoPos.x + dx;
    
    if(!CheckCollision(newY, newX, currentMinoIndex)) {
        minoPos.y = newY;
        minoPos.x = newX;
        needRedraw = true;
        LeaveCriticalSection(&cs);
        return 1; // 이동 성공
    }
    
    LeaveCriticalSection(&cs);
    return 0; // 이동 실패
}

//----------------------------------------------
// Edit 서브클래스 – 마우스/키보드 입력 차단
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
        case WM_KEYDOWN: // 키 입력 처리
            switch (wParam) {
                case VK_UP:
                    // 회전 기능 (향후 구현)
                    return 0;
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
                    // 하드 드랍 (향후 구현)
                    return 0;
            }
            return 0; // 모든 키 입력 무력화
        case WM_CHAR:
            return 0; // 문자 입력 무력화
        case WM_PASTE:
            return 0; // 붙여넣기 무력화
        case WM_CUT:
            return 0; // 잘라내기 무력화
        case WM_COPY:
            return 0; // 복사 무력화
    }
    return CallWindowProcW(g_OrigEditProc, hWnd, uMsg, wParam, lParam);
}

void ResetBoard()
{
    // 고정된 블록(2)은 유지하고 움직이는 블록(1)만 제거
    for(int i = 0; i < H; ++i)
        for(int j = 0; j < W; ++j)
            if(board[i][j] == 1)
                board[i][j] = 0;
}

//----------------------------------------------
// 보드 + 문자열 생성 헬퍼
//----------------------------------------------
static void BuildBoardString(wchar_t* out, size_t outSz)
{
    EnterCriticalSection(&cs);
    
    out[0] = L'\0';

    // 보드 배열에서 움직이는 블록 클리어
    ResetBoard();

    // 현재 미노를 board[][]에 반영
    if(isMinoGenerated) {
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
    }

    // 위/아래 벽 + 내용 문자열 생성
    for(int i = 0; i < H; ++i)
    {
        wcscat_s(out, outSz, L"■");
        for (int j = 0; j < W; ++j) {
            if(board[i][j] == 2)
                wcscat_s(out, outSz, L"▣"); // 고정된 블록
            else if(board[i][j] == 1)
                wcscat_s(out, outSz, L"▣"); // 움직이는 블록
            else
                wcscat_s(out, outSz, L"□"); // 빈칸
        }
        wcscat_s(out, outSz, L"■\r\n");
    }
    for(int j = 0; j < W + 2; ++j) wcscat_s(out, outSz, L"■");

    // 디버그: y 좌표 출력
    wchar_t tmp[32];
    swprintf_s(tmp, L"\r\nY = %d, X = %d", minoPos.y, minoPos.x);
    wcscat_s(out, outSz, tmp);
    
    LeaveCriticalSection(&cs);
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
        if(needRedraw) {
            BuildBoardString(mapStr, BUF_SZ);
            SendMessageW(hEdit, WM_SETTEXT, 0, (LPARAM)mapStr);
            UpdateWindow(hEdit);
            needRedraw = false;
        }
        Sleep(16); // 약 60FPS
    }
    return 0;
}

void FixMino() {
    EnterCriticalSection(&cs);
    
    // 현재 미노를 고정된 블록으로 변환
    for(int i = 0; i < 4; ++i)
    {
        for(int j = 0; j < 4; ++j)
        {
            if(mino[currentMinoIndex][i][j])
            {
                int x = minoPos.x + j;
                int y = minoPos.y + i - 3;
                if (x >= 0 && x < W && y >= 0 && y < H)
                    board[y][x] = 2; // 고정된 블록으로 설정
            }
        }
    }
    
    // 줄 삭제 체크 (기본적인 구현)
    for(int i = H - 1; i >= 0; i--) {
        bool fullLine = true;
        for(int j = 0; j < W; j++) {
            if(board[i][j] != 2) {
                fullLine = false;
                break;
            }
        }
        if(fullLine) {
            // 줄 삭제 및 위쪽 줄들 아래로 이동
            for(int k = i; k > 0; k--) {
                for(int j = 0; j < W; j++) {
                    board[k][j] = board[k-1][j];
                }
            }
            // 맨 위 줄 초기화
            for(int j = 0; j < W; j++) {
                board[0][j] = 0;
            }
            i++; // 같은 줄 다시 체크
        }
    }
    
    LeaveCriticalSection(&cs);
}

//----------------------------------------------
// 블록을 한 칸씩 떨어뜨리는 스레드
//----------------------------------------------
DWORD WINAPI DropMino(LPVOID)
{
    while(true) {
        while(!isMinoGenerated) Sleep(10);

        // 블록이 더 이상 떨어질 수 없을 때까지 반복
        while(isMinoGenerated)
        {
            // 아래로 이동 시도
            if(!MoveMino(1, 0)) {
                // 이동 실패 시 블록 고정
                FixMino();
                isMinoGenerated = false;
                needRedraw = true;
                break;
            }
            Sleep(1000); // 1초마다 떨어짐
        }
    }
    return 0;
}

//----------------------------------------------
// 새 미노 생성 스레드
//----------------------------------------------
DWORD WINAPI GenerateMino(LPVOID)
{
    srand(static_cast<unsigned int>(time(nullptr)));
    
    while(true)
    {
        if (!isMinoGenerated)
        {
            currentMinoIndex = rand() % 7;
            minoPos.x = 3;
            minoPos.y = -1;
            
            // 게임 오버 체크
            if(CheckCollision(minoPos.y, minoPos.x, currentMinoIndex)) {
                // 게임 오버 처리 (현재는 보드만 초기화)
                EnterCriticalSection(&cs);
                memset(board, 0, sizeof(board));
                LeaveCriticalSection(&cs);
            }
            
            isMinoGenerated = true;
            needRedraw = true;
            Sleep(100); // 생성 딜레이
        }
        Sleep(10);
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

    SetFocus(hEdit);
    needRedraw = true;

    return 0;
}

//----------------------------------------------
// DllMain – 스레드 기동
//----------------------------------------------
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            InitializeCriticalSection(&cs);
            memset(board, 0, sizeof(board)); // 보드 초기화
            CreateThread(nullptr, 0, HookThread, nullptr, 0, nullptr);
            CreateThread(nullptr, 0, GenerateMino, nullptr, 0, nullptr);
            CreateThread(nullptr, 0, DropMino, nullptr, 0, nullptr);
            CreateThread(nullptr, 0, DrawMap, nullptr, 0, nullptr);
            break;
        case DLL_PROCESS_DETACH:
            if (hEdit && g_OrigEditProc)
                SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)g_OrigEditProc);
            DeleteCriticalSection(&cs);
            break;
    }
    return TRUE;
}