// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;
    SOCKET s, new_socket;
    struct sockaddr_in server, client;
    char buffer[1024] = {0};
    int c;

    WSAStartup(MAKEWORD(2,2), &wsa);

    s = socket(AF_INET, SOCK_STREAM, 0);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);

    bind(s, (struct sockaddr *)&server, sizeof(server));
    listen(s, 3);
    printf("[*] Waiting for connection...\n");

    c = sizeof(struct sockaddr_in);
    new_socket = accept(s, (struct sockaddr *)&client, &c);
    printf("[*] Client connected.\n");

    recv(new_socket, buffer, sizeof(buffer), 0);
    printf("[*] Received: %s\n", buffer);

    closesocket(new_socket);
    closesocket(s);
    WSACleanup();
    system("pause");
    return 0;
}
