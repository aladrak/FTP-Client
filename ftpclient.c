#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#define SIZE_BUF 512

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

// Получение подтверждений и отладочных сообщений от сервера
void recvMsg(SOCKET s) {
    int len = 0;
    char buff[SIZE_BUF];
    if ((len = recv(s, (char*)&buff, SIZE_BUF, 0)) == SOCKET_ERROR) {
        exit(0);
    }
    printf("Server: ");
    for (int i = 0; i < len; i++) {
        printf ("%c", buff[i]);
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

    // Создаём соединение с сервером
    SOCKET serverSocket = getConn("127.0.0.2", 8080);

    while (1) {
        char msgbuff[SIZE_BUF];
        int len; 
        do {
            // Получение сообщения
            recvMsg(serverSocket);
            // printf("\n");
            break;
        } while (len != 1);

        // Отправка данных
        char *resp;
        resp = (char*)malloc(16*sizeof(char));
        scanMsg("Enter msg: ", resp, 16); // (char*)&
        
        // if (send(serverSocket, resp, sizeof(resp), 0) == SOCKET_ERROR) {
        //     printf("Sending error %d.\n", WSAGetLastError());
        //     closesocket(serverSocket);
        //     exit(0);
        // }
        sendToSock(serverSocket, resp, strlen(resp));
// Отправка файла от клиента
        if (!strncmp(resp, "-sendfile", 9)) {
            recvMsg(serverSocket);

            char *filename;
            filename = (char*)malloc(128*sizeof(char));
            scanMsg("Enter filename: ", filename, 128);
            filename[strcspn(filename, "\n")] = '\0';

            sendToSock(serverSocket, filename, strlen(filename));
            
            recvMsg(serverSocket);

            FILE *file = fopen(filename, "rb");
            if (file == NULL) {
                printf("Failed to open file for reading.\n");
                closesocket(serverSocket);
                return 1;
            }

            while (1) {
                int bytesRead = fread(msgbuff, 1, SIZE_BUF, file);
                if (bytesRead > 0) {
                    send(serverSocket, (char*)&msgbuff, bytesRead, 0);
                    // sendToSock(serverSocket, msgbuff, bytesRead);
                } else {
                    break;
                }
            }
            sendToSock(serverSocket, "-end", 5);

            fclose(file);
            free(filename);
// Отправка файла клиенту
        } else if (!strncmp(resp, "-getfile", 8)) {
            // Получение сообщения
            if ((len = recv(serverSocket, (char*)&msgbuff, SIZE_BUF, 0)) == SOCKET_ERROR)
                break;
            msgbuff[len] = '\0';
            if (!strncmp(msgbuff, "-end", 4))
                break;
            for (int i = 0; i < len; i++) {
                printf("%c", msgbuff[i]);
            }
            char filename[128];
            snprintf(filename, 128, "");
            // filename = (char*)malloc(128*sizeof(char));
            scanMsg("Enter filename: ", (char*)&filename, 128);
            filename[strcspn(filename, "\n")] = '\0';

            sendToSock(serverSocket, (char*)&filename, strlen(filename));

            if ((len = recv(serverSocket, (char*)&msgbuff, SIZE_BUF, 0)) == SOCKET_ERROR)
                break;
            msgbuff[len] = '\0';
            if (!strncmp(msgbuff, "-end", 4))
                break;
            for (int i = 0; i < len; i++)
                printf("%c", msgbuff[i]);

            printf("Receiving a file from the server.");
            FILE *file = fopen(filename, "wb");
            if (file == NULL) {
                printf("Creating file error.\n");
                fclose(file);
                closesocket(serverSocket);
                exit(0);
            }
            while (1) {
                if ((len = recv(serverSocket, msgbuff, SIZE_BUF, 0)) == SOCKET_ERROR) {
                    printf("File retrieval error.\n");
                    fclose(file);
                    closesocket(serverSocket);
                    exit(0);
                }
                if (!strncmp(msgbuff, "-end", 4)) {
                    break;
                }
                if (len > 0) {
                    fwrite(msgbuff, 1, len, file);
                } 
            }

            fclose(file);

        } else if (!strncmp(resp, "-list", 8)) {
            printf("Server: ");
            while (1) {
                // Получение сообщения
                int len = 0;
                char buff[SIZE_BUF];
                if ((len = recv(serverSocket, (char*)&buff, SIZE_BUF, 0)) == SOCKET_ERROR) {
                    break;
                }
                buff[len] = '\0';
                if (!strncmp(buff, "-end", 4)) {
                    break;
                }
                for (int i = 0; i < len; i++) {
                    printf("%c", buff[i]);
                }
                // printf("\n");
            }
        }
    }

    printf("Connection lost.\n");
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}