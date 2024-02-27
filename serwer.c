SERWER:



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdbool.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_CLIENTS_COUNT 10

// Struktura przechowująca dane klienta
struct client {
    int socketFD;
    bool isActive;
    char name[32];
    
};

struct client clients[MAX_CLIENTS_COUNT];
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

// Szuka wolnego miejsca dla nowego klienta
int addclientifpossible() {
    for (int i = 0; i < MAX_CLIENTS_COUNT; i++) {
   	 if (!clients[i].isActive) return i;
    }
    return -1;
}

// Usuwa klienta z listy aktywnych
void removeClientfromactivelist(int socketFD) {
    pthread_mutex_lock(&clientsMutex);
    for (int i = 0; i < MAX_CLIENTS_COUNT; i++) {
   	 if (clients[i].socketFD == socketFD) {
   		 clients[i].isActive = false;
   		 break;
   	 }
    }
    pthread_mutex_unlock(&clientsMutex);
}

// Autentykacja użytkownika
int authenticateUser(char* username, char* password) {
    if ((strcmp(username, "user1") == 0 && strcmp(password, "pass1") == 0) ||
   	 (strcmp(username, "user2") == 0 && strcmp(password, "pass2") == 0)) {
   	 return 1; // Użytkownik uwierzytelniony
    }
    return 0; // Uwierzytelnianie nie powiodło się
}

// Wysyłanie wiadomości do wszystkich klientów, z wyjątkiem nadawcy
void sendbroadcastMessage(char* buffer, int senderSocket) {
    pthread_mutex_lock(&clientsMutex);
    int i;
    char senderName[32];
    for (i = 0; i < MAX_CLIENTS_COUNT; i++) {
   	 if (clients[i].isActive && clients[i].socketFD == senderSocket) {
   		 strcpy(senderName, clients[i].name);
   		 break;
   	 }
    }
    char newBuffer[MAX_BUFFER_SIZE + 128];
    sprintf(newBuffer, "%s: %s", senderName, buffer);

    for (i = 0; i < MAX_CLIENTS_COUNT; i++) {
   	 if (clients[i].isActive && clients[i].socketFD != senderSocket) {
   		 struct sctp_sndrcvinfo sndrcvinfo;
   		 int flags = 0;
   		 sctp_sendmsg(clients[i].socketFD, newBuffer, strlen(newBuffer), (struct sockaddr*)NULL, 0, 0, 0, sndrcvinfo.sinfo_stream, 0, 0);
   	 }
    }
    pthread_mutex_unlock(&clientsMutex);
}

// Obsługa połączenia klienta
void* clientConnectionHandler(void* arg) {
    int clientSocket = (*(int*)arg);
    pthread_detach(pthread_self());

    char messageBuffer[MAX_BUFFER_SIZE + 1];
    int bytesRead, flags;
    struct sctp_sndrcvinfo sndrcvinfo;

    bytesRead = sctp_recvmsg(clientSocket, messageBuffer, sizeof(messageBuffer), (struct sockaddr*)NULL, 0, &sndrcvinfo, &flags);
    if (bytesRead < 0) {
   	 perror("Blad w sctp_recvmsg()");
   	 close(clientSocket);
   	 removeClientfromactivelist(clientSocket);
   	 return NULL;
    }
    messageBuffer[bytesRead] = '\0';

    // Autentykacja użytkownika
    char username[32];
    char password[32];
    sscanf(messageBuffer, "%s %s", username, password);

    if (authenticateUser(username, password)) {
   	 strcpy(messageBuffer, "Autoryzacja przebiegla pomyslnie");
   	 sctp_sendmsg(clientSocket, messageBuffer, strlen(messageBuffer) + 1, NULL, 0, 0, 0, 0, 0, 0);
    }
    else {
   	 strcpy(messageBuffer, "Blad autoryzacji");
   	 sctp_sendmsg(clientSocket, messageBuffer, strlen(messageBuffer) + 1, NULL, 0, 0, 0, 0, 0, 0);
   	 close(clientSocket);
   	 removeClientfromactivelist(clientSocket);
   	 return NULL;
    }

    pthread_mutex_lock(&clientsMutex);
    int clientSlot = addclientifpossible();
    if (clientSlot != -1) {
   	 strcpy(clients[clientSlot].name, username);
   	 clients[clientSlot].socketFD = clientSocket;
   	 clients[clientSlot].isActive = true;
    }
    pthread_mutex_unlock(&clientsMutex);

    // Odbieranie i przekazywanie wiadomości
    while (1) {
   	 bytesRead = sctp_recvmsg(clientSocket, messageBuffer, sizeof(messageBuffer), (struct sockaddr*)NULL, 0, &sndrcvinfo, &flags);
   	 if (bytesRead < 0) {
   		 perror("Blad w sctp_recvmsg()");
   		 break;
   	 }
   	 if (bytesRead == 0) {
   		 printf("Klient sie rozlaczyl.\n");
   		 break;
   	 }

   	 messageBuffer[bytesRead] = '\0';
   	 sendbroadcastMessage(messageBuffer, clientSocket);
    }

    close(clientSocket);
    removeClientfromactivelist(clientSocket);
    return NULL;
}

int main(int argc, char* argv[]) {
    int listenSocket, newConnectionSocket;
    struct sockaddr_in6 serverAddress;
    pthread_t clientThread;

    // Utworzenie gniazda nasłuchującego
    listenSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);
    if (listenSocket < 0) {
   	 perror("Blad w procesie utworzania 'SCTP socket'");
   	 exit(1);
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin6_family = AF_INET6;
    serverAddress.sin6_addr = in6addr_any;
    serverAddress.sin6_port = htons(5555);

    // Bindowanie gniazda do adresu i portu
    if (bind(listenSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
   	 perror("bind() zwrocil blad");
   	 exit(1);
    }
    if (listen(listenSocket, 5) < 0) {
   	 perror("listen() zwrocil blad");
   	 exit(1);
    }

    // Akceptowanie nowych połączeń i tworzenie wątków do ich obsługi
    while (1) {
   	 newConnectionSocket = accept(listenSocket, (struct sockaddr*)NULL, NULL);
   	 if (newConnectionSocket < 0) {
   		 perror("accept() zwrocil blad");
   		 continue;
   	 }

   	 pthread_create(&clientThread, NULL, clientConnectionHandler, &newConnectionSocket);
    }

    close(listenSocket);
    return 0;
}

