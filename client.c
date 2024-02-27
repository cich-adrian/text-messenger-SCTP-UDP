
KLIENT:


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

#define MAX_BUFFER_SIZE 1024

// Funkcja w wątku do odbierania wiadomości
void* Messagereceiver(void* arg) {
    int bytesRead, flags;
    struct sctp_sndrcvinfo sndrcvinfo;
    int sockFD = *(int*)arg;
    char messageBuff[MAX_BUFFER_SIZE + 1];
    

    while (1) {
   	 bytesRead = sctp_recvmsg(sockFD, messageBuff, sizeof(messageBuff), (struct sockaddr*)NULL, 0, &sndrcvinfo, &flags);
   	 if (bytesRead <= 0) break;
   	 messageBuff[bytesRead] = '\0';
   	 printf("%s\n", messageBuff);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int clientSock;
    struct sockaddr_in6 serverAddress;
    pthread_t recvpThreadt;
    if (argc < 2) {
   	 printf("Używam adres: %s <server address>\n", argv[0]);
   	 exit(1);
    }

    // Utworzenie gniazda SCTP
    clientSock = socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP);
    if (clientSock < 0) {
   	 perror("Nie utworzono gniazda");
   	 exit(1);
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin6_family = AF_INET6;
    serverAddress.sin6_port = htons(5555);
    if (inet_pton(AF_INET6, argv[1], &serverAddress.sin6_addr) <= 0) {
   	 perror("Nie udalo sie zmienic adresu");
   	 exit(1);
    }

    // Połączenie z serwerem
    if (connect(clientSock, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
   	 perror("Nie utworzono polaczenia");
   	 exit(1);
    }

    // Autentykacja użytkownika
    char username[32];
    char password[32];
    printf("Podaj nazwe uzytkownika: ");
    scanf("%s", username);
    printf("Podaj haslo: ");
    scanf("%s", password);
    char authMessage[64];
    sprintf(authMessage, "%s %s", username, password);
    sctp_sendmsg(clientSock, authMessage, strlen(authMessage) + 1, NULL, 0, 0, 0, 0, 0, 0);

    char authResult[MAX_BUFFER_SIZE + 1];
    memset(authResult, 0, sizeof(authResult));
    int bytesRead, flags;
    struct sctp_sndrcvinfo sndrcvinfo;
    bytesRead = sctp_recvmsg(clientSock, authResult, MAX_BUFFER_SIZE, (struct sockaddr*)NULL, 0, &sndrcvinfo, &flags);
    if (bytesRead <= 0) {
   	 perror("Blad w procesie autoryzacji");
   	 close(clientSock);
   	 exit(1);
    }

    printf("Wynik autoryzacji: %s\n", authResult);

    if (strcmp(authResult, "Autoryzacja nieudana") == 0) {
   	 printf("Autoryzacja nieudana. Wyjscie z programu.\n");
   	 close(clientSock);
   	 exit(1);
    }

    pthread_create(&recvpThreadt, NULL, Messagereceiver, &clientSock);

    // Wysyłanie wiadomości
    char messageBuff[MAX_BUFFER_SIZE + 1];
    printf("Wpisz wiadomosc do wyslania (wpisz 'exit' jesli chcesz wyjsc): ");
    while (1) {
   	 fgets(messageBuff, MAX_BUFFER_SIZE, stdin);
   	 if (strcmp(messageBuff, "exit\n") == 0) {
   		 break;
   	 }
   	 sctp_sendmsg(clientSock, messageBuff, strlen(messageBuff) + 1, NULL, 0, 0, 0, 0, 0, 0);
    }

    pthread_cancel(recvpThreadt);
    pthread_join(recvpThreadt, NULL);
    close(clientSock);

    return 0;
}
