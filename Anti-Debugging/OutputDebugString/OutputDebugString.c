#include <windows.h>
#include <stdio.h>

int main() {
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);

    QueryPerformanceCounter(&start);
    OutputDebugStringA("디버거 감지 테스트 문자열");
    QueryPerformanceCounter(&end);

    double delta = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;

    printf("Time = %.6f ms\n", delta);

    if (delta > 0.05) {
        MessageBoxA(NULL, "디버거 탐지됨", "안티디버깅", MB_OK);
    } else {
        MessageBoxA(NULL, "디버거 없음", "안티디버깅", MB_OK);
    }

    return 0;
}
