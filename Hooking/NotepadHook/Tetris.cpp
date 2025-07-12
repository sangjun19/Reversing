// 빌드: g++ -m64 -shared -o Tetris.dll Tetris.cpp

#include <windows.h>
#include <cstdlib>
#include <ctime>
#include <stdio.h>

static WNDPROC g_OrigEditProc = nullptr; // 서브클래스 이전 원본 프로시저

constexpr int W = 10; // 가로 10칸
constexpr int H = 20; // 세로 20칸

static int board[H][W]; // 0 = 빈칸, 1 = 블록, 2 = 고정된 블록
static POINT minoPos = {3, 0}; // 블록 기준 좌표 (왼쪽 위)
static int minoDir = 0; // 블록 회전 방향 (0: 기본, 1: 90도, 2: 180도, 3: 270도)
static HWND hEdit = nullptr; // Notepad Edit 컨트롤
static bool suppress_selection = false; // 마우스/키보드 입력 차단 여부
static bool isMinoGenerated = false; // 현재 미노가 생성되었는지 여부
static int currentMinoIndex = 0; // 현재 미노 인덱스 (0~6, 7가지 모양 중 하나)
static bool needRedraw = true; // 화면 갱신 필요 여부
static CRITICAL_SECTION cs; // 동기화용 크리티컬 섹션
static bool isGameOver = false; // 게임 오버 상태

// 7가지 미노 정의
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
        {0,1,1,0},
        {0,1,1,0},
        {0,0,0,0}
    },
    // T
    {
        {0,0,0,0},
        {0,0,1,0},
        {0,1,1,1},
        {0,0,0,0}
    },
    // S
    {
        {0,0,0,0},
        {0,0,1,1},
        {0,1,1,0},
        {0,0,0,0}
    },
    // Z
    {
        {0,0,0,0},
        {0,1,1,0},
        {0,0,1,1},
        {0,0,0,0}
    },
    // J
    {
        {0,0,1,0},
        {0,0,1,0},
        {0,1,1,0},
        {0,0,0,0}
    },
    // L
    {
        {0,1,0,0},
        {0,1,0,0},
        {0,1,1,0},
        {0,0,0,0}
    }
};

// 회전된 블록 좌표 계산 (원본 좌표를 수정하지 않음)
void GetRotatedCoords(int originalI, int originalJ, int direction, int& newI, int& newJ)
{
    switch(direction) {
        case 0: // 기본 방향
            newI = originalI;
            newJ = originalJ;
            break;
        case 1: // 90도 시계방향 회전
            newI = originalJ;
            newJ = 3 - originalI;
            break;
        case 2: // 180도 회전
            newI = 3 - originalI;
            newJ = 3 - originalJ;
            break;
        case 3: // 270도 시계방향 회전
            newI = 3 - originalJ;
            newJ = originalI;
            break;
    }
}

// 블럭 고정 함수 + 줄 삭제
void FixMino() {
    EnterCriticalSection(&cs);
    
    // 현재 미노를 고정된 블록으로 변환
    for(int i = 0; i < 4; ++i)
    {
        for(int j = 0; j < 4; ++j)
        {
            if(mino[currentMinoIndex][i][j])
            {
                int rotatedI, rotatedJ;
                GetRotatedCoords(i, j, minoDir, rotatedI, rotatedJ);
                
                int x = minoPos.x + rotatedJ;
                int y = minoPos.y + rotatedI - 3;
                
                if (x >= 0 && x < W && y >= 0 && y < H)
                    board[y][x] = 2; // 고정된 블록으로 설정
            }
        }
    }
    
    // 줄 삭제 체크
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
                    board[k][j] = board[k - 1][j];
                }
            }
            // 맨 위 줄 초기화
            for(int j = 0; j < W; j++) {
                board[0][j] = 0;
            }
            i++;
        }
    }
    
    LeaveCriticalSection(&cs);
}

// 충돌 검사 함수
bool CheckCollision(int y, int x, int minoIndex, int direction)
{
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < 4; j++) {
            if(mino[minoIndex][i][j]) // 현재 미노의 블록이 있는 위치
            {
                int rotatedI, rotatedJ;
                GetRotatedCoords(i, j, direction, rotatedI, rotatedJ);
                
                int newY = y + rotatedI - 3; // -3 오프셋
                int newX = x + rotatedJ;
                
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

// 블록 회전 함수
int RotateMino()
{
    // 크리티컬 섹션으로 동기화
    EnterCriticalSection(&cs);
    
    int newDir = (minoDir + 1) % 4; // 시계 방향 회전
    
    // 회전 후 충돌 검사
    if(!CheckCollision(minoPos.y, minoPos.x, currentMinoIndex, newDir)) {
        minoDir = newDir;
        needRedraw = true;
        LeaveCriticalSection(&cs); // 크리티컬 섹션 해제
        return 1;
    }
    
    // 벽 킥 시도 (좌우로 1칸씩 이동해서 회전 가능한지 확인)
    for(int kick = 1; kick <= 2; kick++) {
        // 왼쪽으로 킥
        if(!CheckCollision(minoPos.y, minoPos.x - kick, currentMinoIndex, newDir)) {
            minoPos.x -= kick;
            minoDir = newDir;
            needRedraw = true;
            LeaveCriticalSection(&cs);
            return 1;
        }
        
        // 오른쪽으로 킥
        if(!CheckCollision(minoPos.y, minoPos.x + kick, currentMinoIndex, newDir)) {
            minoPos.x += kick;
            minoDir = newDir;
            needRedraw = true;
            LeaveCriticalSection(&cs);
            return 1;
        }
    }
    
    LeaveCriticalSection(&cs);
    return 0; // 회전 실패
}

// 키 입력시 mino 이동 함수
int MoveMino(int dy, int dx)
{
    EnterCriticalSection(&cs);
    
    int newY = minoPos.y + dy;
    int newX = minoPos.x + dx;
    
    if(!CheckCollision(newY, newX, currentMinoIndex, minoDir)) {
        minoPos.y = newY;
        minoPos.x = newX;
        needRedraw = true;
        LeaveCriticalSection(&cs);
        return 1; // 이동 성공
    }
    
    LeaveCriticalSection(&cs);
    return 0; // 이동 실패
}

// 하드 드롭 구현
void HardDrop()
{
    EnterCriticalSection(&cs);
    
    // 더 이상 떨어질 수 없을 때까지 반복
    while(!CheckCollision(minoPos.y + 1, minoPos.x, currentMinoIndex, minoDir)) {
        minoPos.y++;
    }
    
    FixMino(); // 블록 고정
    needRedraw = true; // 화면 갱신 필요
    isMinoGenerated = false; // 새 미노 생성 대기
    LeaveCriticalSection(&cs);
}

// 조작에 필요한 것 외에 마우스/키보드 입력 차단
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
                    RotateMino();
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
                    HardDrop();
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
        case EM_SETSEL: // 입력 커서 무력화
            if (suppress_selection) return 0; // 선택 영역 설정 무력화
            break;
    }
    return CallWindowProcW(g_OrigEditProc, hWnd, uMsg, wParam, lParam);
}

// 보드 초기화
void ResetBoard()
{
    // 고정된 블록(2)은 유지하고 움직이는 블록(1)만 제거
    for(int i = 0; i < H; ++i)
        for(int j = 0; j < W; ++j)
            if(board[i][j] == 1)
                board[i][j] = 0;
}

// 보드 + 문자열 생성
static void BuildBoardString(wchar_t* out, size_t outSz)
{
    EnterCriticalSection(&cs);
    
    out[0] = L'\0';

    // 보드 배열에서 움직이는 블록 클리어
    ResetBoard();

    // 현재 미노를 board에 반영
    if(isMinoGenerated) {
        for(int i = 0; i < 4; ++i)
        {
            for(int j = 0; j < 4; ++j)
            {
                if(mino[currentMinoIndex][i][j])
                {
                    int rotatedI, rotatedJ;
                    GetRotatedCoords(i, j, minoDir, rotatedI, rotatedJ);
                    
                    int x = minoPos.x + rotatedJ;
                    int y = minoPos.y + rotatedI - 3;
                    
                    if (x >= 0 && x < W && y >= 0 && y < H)
                        board[y][x] = 1;
                }
            }
        }
    }

    for(int i = 0; i < H; ++i)
    {
        wcscat_s(out, outSz, L"■");
        for (int j = 0; j < W; ++j) {
            if(board[i][j] == 2) // 고정 블록
                wcscat_s(out, outSz, L"▣");
            else if(board[i][j] == 1) // 움직이는 블록
                wcscat_s(out, outSz, L"▣");
            else // 빈칸
                wcscat_s(out, outSz, L"　");
        }
        wcscat_s(out, outSz, L"■\r\n");
    }
    for(int j = 0; j < W + 2; ++j) wcscat_s(out, outSz, L"■");

    // 좌표 및 방향 출력
    // wchar_t tmp[64];
    // swprintf_s(tmp, L"\r\nY = %d, X = %d, Dir = %d", minoPos.y, minoPos.x, minoDir);
    // wcscat_s(out, outSz, tmp);

    if(isGameOver) {
        wcscat_s(out, outSz, L"\r\nGame Over Wait 3 seconds to reset...");
        LeaveCriticalSection(&cs);                    
    }
    
    LeaveCriticalSection(&cs);
}

// 게임 화면 그리기 스레드
DWORD WINAPI DrawMap(LPVOID)
{
    while(!hEdit) Sleep(100);

    constexpr size_t BUF_SZ = 4096;
    wchar_t mapStr[BUF_SZ];

    while(true)
    {
        if(needRedraw) { // 화면 갱신 필요 여부 확인
            BuildBoardString(mapStr, BUF_SZ); // 보드 생성
            SendMessageW(hEdit, WM_SETTEXT, 0, (LPARAM)mapStr); // Edit 컨트롤에 문자열 설정
            UpdateWindow(hEdit); // 즉시 갱신
            if (isGameOver) {
                Sleep(3000); // 게임 오버 시 2초 대기 후 보드 초기화
                EnterCriticalSection(&cs);
                memset(board, 0, sizeof(board)); // 보드 초기화
                isGameOver = false; // 게임 오버 상태 해제
                LeaveCriticalSection(&cs);
            }
            needRedraw = false;
        }
        Sleep(16); // 60FPS
    }
    return 0;
}

// 블록을 한 칸씩 떨어뜨리는 스레드
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
                isMinoGenerated = false; // 새 미노 생성 대기
                needRedraw = true; // 화면 갱신 필요
                break;
            }
            Sleep(800); // 0.8초마다 떨어짐 (약간 빠르게)
        }
    }
    return 0;
}

// 새 미노 생성 스레드
DWORD WINAPI GenerateMino(LPVOID)
{
    srand(static_cast<unsigned int>(time(nullptr)));
    
    while(true)
    {
        if (!isMinoGenerated)
        {
            currentMinoIndex = rand() % 7; // 랜덤 미노 생성
            minoPos.x = 3;
            minoPos.y = 0;
            minoDir = 0;
            
            // 게임 오버 체크
            if(CheckCollision(minoPos.y, minoPos.x, currentMinoIndex, minoDir)) {                
                isGameOver = true;
                // EnterCriticalSection(&cs);
                // memset(board, 0, sizeof(board));
                // LeaveCriticalSection(&cs);
            }
            
            isMinoGenerated = true;
            needRedraw = true;
            Sleep(100); // 생성 딜레이
        }
        Sleep(10);
    }
    return 0;
}

// Notepad Edit 컨트롤 찾고 서브클래싱
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

// DllMain – 스레드 기동
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            InitializeCriticalSection(&cs);
            memset(board, 0, sizeof(board)); // 보드 초기화
            CreateThread(nullptr, 0, HookThread, nullptr, 0, nullptr); // Edit 서브클래싱 스레드
            CreateThread(nullptr, 0, GenerateMino, nullptr, 0, nullptr); // 미노 생성 스레드
            CreateThread(nullptr, 0, DropMino, nullptr, 0, nullptr); // 블록 떨어뜨리는 스레드
            CreateThread(nullptr, 0, DrawMap, nullptr, 0, nullptr); // 화면 그리기 스레드
            break;
        case DLL_PROCESS_DETACH:
            // DLL 언로드 시 Edit 서브클래스 원복
            if (hEdit && g_OrigEditProc)
                SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)g_OrigEditProc);
            DeleteCriticalSection(&cs);
            break;
    }
    return TRUE;
}