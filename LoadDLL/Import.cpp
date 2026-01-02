#include <windows.h>
#include <iostream>

using namespace std;

typedef int (*Add)(int, int);

int main() {
    HMODULE hDLL = LoadLibraryW(L"add.dll");
    if (hDLL == NULL) {
        cerr << "DLL 로드 실패" << endl;
        return 1;
    }

    Add AddNum = (Add)GetProcAddress(hDLL, "Add");
    if (AddNum == NULL) {
        cerr << "Add 함수 로드 실패" << endl;
        FreeLibrary(hDLL);
        return 1;
    }

    int result = AddNum(5, 10);
    cout << result << endl;

    FreeLibrary(hDLL);
    return 0;

}