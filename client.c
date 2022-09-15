#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include "support.h"
#define IP "127.0.0.1"

void paramsCtrl(int argc, char* argv[]) {
    int legal = 1;
    if (argc < 4) { printf("Too few arguments\n Expected 4\n"); legal = 0; }
    if (argc > 4) { printf("Too much arguments\n Expected 4\n"); legal = 0;}
    if (atoi(argv[2]) < 1) { printf("Illegal number of <#servers_to_connect_to>\n"); legal = 0; }
    if (atoi(argv[2]) >= atoi(argv[1])) { printf("<#servers_to_connect_to> must be less then <#servers_available>\n"); legal = 0;}
    if (atoi(argv[3]) <= 3*atoi(argv[2])) { printf("<#messages_to_send> must be at least 3 times <#servers_to_connect_to>\n"); legal = 0;}
    if (!legal) {
        fprintf(stdout, "Correct usage: %s <#servers_available> <#servers_to_connect_to> <#messages_to_send>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
}

int isInArr(int* arr, int num, int dim) {
    for (int i=0; i<dim; i++) { if(arr[i]==num){return 1;} }
    return 0;
}

int main (int argc, char*argv[]) {
    paramsCtrl(argc, argv);
    srand(time(NULL)*getpid());

    int secret = rand() % 3001;
    int S = atoi(argv[1]);
    int k = atoi(argv[2]);
    int m = atoi(argv[3]);
    long ID = rand();
    struct timespec timer;
    timer.tv_sec = secret/1000;
    timer.tv_nsec = (secret%1000)*1000*1000;
    int serverIDs[k];
    int serverSockets[k];
    struct sockaddr_in servers[k];

    fprintf(stdout,"CLIENT %s SECRET %d\n", decToHex(ID), secret);

    for (int i=0; i<k; i++) {   //scelta casuale dei server e connessione

        serverIDs[i] = ( rand() % S ) + 1; // [1, S-1]
        while (isInArr(serverIDs, serverIDs[i], i))
            serverIDs[i] = ( rand() % S ) + 1;

        servers[i].sin_family = AF_INET;
        servers[i].sin_addr.s_addr = inet_addr(IP);
        servers[i].sin_port = htons(9000+serverIDs[i]);

        serverSockets[i] = socket(AF_INET,SOCK_STREAM,0);
        if (serverSockets[i] == -1) { perror("socket"); exit(EXIT_FAILURE); }

        int nval = connect(serverSockets[i],(struct sockaddr *)&servers[i], sizeof(servers[i]));
        if (nval == -1) { perror("connect"); exit(EXIT_FAILURE); }
    }
    for (int i=0; i<m; i++) {   //selezione casuale del socket e invio del messaggio
        int sock = serverSockets[rand() % k];
        long msg = htonl(ID);
        EC(write(sock, &msg, sizeof(long)), "write");
        EC(nanosleep(&timer, NULL), "nanosleep");
    }

    for (int i=0; i<k; i++) {   //chiusura dei socket
        EC(close(serverSockets[i]), "close");
    }

    fprintf(stdout, "CLIENT %s DONE\n", decToHex(ID));
    fflush(stdout);
}

