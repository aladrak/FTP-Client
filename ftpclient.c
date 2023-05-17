#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define PACKET_MAX_SIZE 512

// Ф-ия ввода команд
void scanMsg(char *question, char *scanBuffer, int bufferLength) { 
    printf("%s  (Max %d characters)\n", question, bufferLength - 1); 
    fgets(scanBuffer, bufferLength, stdin); 
    if (scanBuffer[strlen(scanBuffer) -1] != '\n'){ 
        int dropped = 0; 
        while (fgetc(stdin) != '\n') { dropped++; } 
        if (dropped > 0) { 
            printf("Your input has exceeded the limit and the buffer has been extended by %d characters, try again!\n", dropped);
            free(scanBuffer);
            scanBuffer = (char*)malloc((bufferLength + dropped)*sizeof(char)); 
            scanMsg(question, scanBuffer, bufferLength + dropped); 
        } 
    } else { scanBuffer[strlen(scanBuffer) -1] = '\0'; }
} 

void sendToSock(SOCKET s, char *msg, int len) {
    if (len > PACKET_MAX_SIZE) {
        printf("Message not sent! Exceeding the max message size...\n");
        return;
    }
    if (send(s, msg, len, 0) == SOCKET_ERROR) {
        printf("Sending error %d.\n", WSAGetLastError());
        closesocket(s);
        exit(0);
    }
}

SOCKET getConn(char *ipAddr, int port) {
    SOCKET s;
    // Создание сокета
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Failed to create listening socket. Error %d\n", WSAGetLastError());
        exit(0);
    }
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET; // тип адреса (TCP/IP)
    //адрес сервера. Т.к. TCP/IP представляет адреса в числовом виде, то для перевода
    // адреса используем функцию inet_addr.
    serverAddress.sin_addr.s_addr = inet_addr(ipAddr);
    // Порт. Используем функцию htons для перевода номера порта из обычного в //TCP/IP представление.
    serverAddress.sin_port = htons(port);

   // Выполняем соединение с сервером:
    if (connect(s, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
        printf("Connection error %d.\n", WSAGetLastError());
        closesocket(s);
        exit(0);
    }
    printf("Connection successfully!\n");
    return s;
}

int main() {
    WSADATA wsaData;
    
    // Инициализация Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Error %d during initialization Winsock.\n", WSAGetLastError());
        exit(1);
    }

    // 
    SOCKET serverSocket = getConn("127.0.0.2", 8080);

    char msgbuff[PACKET_MAX_SIZE];

    while (1) {
        // Получение сообщения
        int len; 
        do {
            if ((len = recv(serverSocket, (char*)&msgbuff, PACKET_MAX_SIZE, 0)) == SOCKET_ERROR) {
                return -1;
            }
            printf("Server: ");
            for (int i = 0; i < len; i++) {
                printf ("%c", msgbuff[i]);
            }
            printf("\n");
            break;
        } while (len != 1);

        // Отправка данных
        
        char *resp;
        resp = (char*)malloc(5*sizeof(char));
        scanMsg("Enter msg: ", resp, 5); // (char*)&
        
        // if (send(serverSocket, resp, sizeof(resp), 0) == SOCKET_ERROR) {
        //     printf("Sending error %d.\n", WSAGetLastError());
        //     closesocket(serverSocket);
        //     exit(0);
        // }
        sendToSock(serverSocket, resp, strlen(resp));
    }

    printf("Connection lost.\n");
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}