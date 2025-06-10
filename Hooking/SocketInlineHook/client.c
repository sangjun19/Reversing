// client.c
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in server;
    char buffer[1024] = "hello";

    DWORD pid = GetCurrentProcessId();
    printf("[*] My PID: %lu\n", pid);
    system("pause");

    WSAStartup(MAKEWORD(2,2), &wsa);

    s = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(8888);

    connect(s, (struct sockaddr *)&server, sizeof(server));
    send(s, buffer, strlen(buffer), 0);

    closesocket(s);
    WSACleanup();
    return 0;
}
