#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define SIZE_BUF 512

#define IP_ADDR "127.0.0.2"
#define PORT 8080

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

// Отправка сообщения серверу
void sendToSock(SOCKET s, char *msg, int len) {
    if (len > SIZE_BUF) {
        printf("Message not sent! Exceeding the max message size...\n");
        return;
    }
    if (send(s, msg, len, 0) == SOCKET_ERROR) {
        printf("Sending error %d.\n", WSAGetLastError());
        closesocket(s);
        exit(0);
    }
}

// Получение списка файлов в директории пользователя
void showList(SOCKET s, char* msgbuff) {
    printf("Server: ");
    while (1) {
        // Получение сообщения
        int len = 0;
        if ((len = recv(s, msgbuff, SIZE_BUF, 0)) == SOCKET_ERROR) {
            break;
        }
        msgbuff[len] = '\0';
        if (!strncmp(msgbuff, "-END", 4)) {
            break;
        }
        for (int i = 0; i < len; i++) {
            printf("%c", msgbuff[i]);
        }
    }
}

// Получение подтверждений и отладочных сообщений от сервера
void recvMsg(SOCKET s) {
    int len = 0;
    char msgbuff[SIZE_BUF];
    if ((len = recv(s, (char*)&msgbuff, SIZE_BUF, 0)) == SOCKET_ERROR) {
        exit(0);
    }
    printf("Server: ");
    for (int i = 0; i < len; i++) {
        printf ("%c", msgbuff[i]);
    }
}

// Передача файла сервер -> клиент
void getFileCom(SOCKET s, char *msgbuff, int len) {
    int j = 0;
    char filename[len - sizeof("GET") + 1];
    for (int i = sizeof("GET"); i < len; i++) {
        filename[j++] = msgbuff[i]; 
    }
    filename[j] = '\0';
    printf("Receiving a file \'%s\' %d from the server.\n", filename, sizeof(filename));

    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Creating file error.\n");
        fclose(file);
        closesocket(s);
        exit(0);
    }

    while (1) {
        if ((len = recv(s, msgbuff, SIZE_BUF, 0)) == SOCKET_ERROR) {
            printf("File retrieval error.\n");
            fclose(file);
            closesocket(s);
            exit(0);
        }
        if (!strncmp(msgbuff, "-END", 4)) {
            break;
        }
        if (len > 0) {
            fwrite(msgbuff, 1, len, file);
        } else { break; }
    }

    fclose(file);
}

// Передача файла клиент -> сервер
void sendFileCom(SOCKET s, char *msgbuff, int len) {
    int j = 0;
    char filename[len - sizeof("SEND") + 1];
    for (int i = sizeof("SEND"); i < len; i++) {
        filename[j++] = msgbuff[i]; 
    }
    filename[j] = '\0';

    printf("Sending a file \'%s\' %d to the server.\n", filename, sizeof(filename));

    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Failed to open file for reading.\n");
        closesocket(s);
        return;
    }

    while (1) {
        int bytesRead = fread(msgbuff, 1, SIZE_BUF, file);
        if (bytesRead > 0) {
            send(s, msgbuff, bytesRead, 0);
        } else {
            break;
        }
    }

    Sleep((DWORD)30);

    snprintf(msgbuff, SIZE_BUF, "-END");
    if (send(s, msgbuff, strlen(msgbuff), 0) == SOCKET_ERROR) {
        printf("Sending error %d.\n", WSAGetLastError());
        closesocket(s);
        exit(0);
    }

    fclose(file);
}

// Создание подключения к серверу
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

    // Создаём соединение с сервером
    SOCKET serverSocket = getConn(IP_ADDR, PORT);

    while (1) {
        char msgbuff[SIZE_BUF];
        int len; 
        do {
            // Получение сообщения
            int len = 0;
            if ((len = recv(serverSocket, (char*)&msgbuff, SIZE_BUF, 0)) == SOCKET_ERROR) {
                exit(0);
            }
            if (!strncmp(msgbuff, "GET", 3)) {
                getFileCom(serverSocket, (char*)&msgbuff, len);
                continue;
            } else if (!strncmp(msgbuff, "SEND", 4)) {
                sendFileCom(serverSocket, (char*)&msgbuff, len);
                continue;
            } else if (!strncmp(msgbuff, "LIST", 4)) {
                showList(serverSocket, (char*)&msgbuff);
                continue;
            } else if (!strncmp(msgbuff, "STOP", 4)) {
                printf("Server stopped.\n");
                return 1;
            } else if (!strncmp(msgbuff, "EXIT", 4)) {
                printf("Disconnection from the server.\n");
                return 1;
            }
            printf("Server: ");
            for (int i = 0; i < len; i++) {
                printf ("%c", msgbuff[i]);
            }
            break;
        } while (len != 1);

        // Отправка данных
        char *resp;
        resp = (char*)malloc(33*sizeof(char));
        scanMsg("Enter msg: ", resp, 33); // (char*)&
        
        // if (send(serverSocket, resp, sizeof(resp), 0) == SOCKET_ERROR) {
        //     printf("Sending error %d.\n", WSAGetLastError());
        //     closesocket(serverSocket);
        //     exit(0);
        // }
        sendToSock(serverSocket, resp, strlen(resp));
        free(resp);
    }

    printf("Connection lost.\n");
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}