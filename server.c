#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include "support.h"
#define IP "127.0.0.1"
#define W 3

volatile sig_atomic_t end_flag = 0;

void*master(void*arg);

void*worker(void* arg);

struct data {
    sem_t empty;
    sem_t endwrite;
    int serverID;
    long secret;
    long clientID;
};

struct workerArg {
    int serverID;
    int serverSock;
    fd_set* clients_to_handle;
    int* max_fd;
    pthread_mutex_t* clients_to_handle_mtx;
    pthread_mutex_t* estimates_list_mtx;
    struct nodo_client* estimates_list;
    struct data* shmptr;
};

struct masterArg {
    int serverID;
    int serverSock;
    fd_set* arrClients;
    int* arrMaxWorker;
    pthread_mutex_t* arrWorkerMutex;
};

void handlerSIGTERM(int s) {
    end_flag = 1;
}
int main (int argc, char* argv[]) {
    // -- Organizzazione di base --
    int serverID = atoi(argv[1]);
    char *shm_name = argv[2];
    int PORT = 9000 + serverID;
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(IP);
    int sSk = socket(AF_INET, SOCK_STREAM, 0);
    if (sSk == -1) { perror("socket"); exit(EXIT_FAILURE); }
    EC(bind(sSk, (struct sockaddr *)&server, sizeof(server)), "bind");
    EC(listen(sSk, SOMAXCONN), "listen");
    fprintf(stdout, "SERVER %d ACTIVE\n", serverID);
    fflush(stdout);

    // -- Registrazione del signal handler --
    struct sigaction s;
    sigset_t set;
    sigfillset(&set);
    pthread_sigmask(SIG_SETMASK,&set,NULL);
    EC(sigaction(SIGTERM, NULL, &s), "sigaction");
    s.sa_handler = handlerSIGTERM;
    EC(sigaction(SIGTERM, &s, NULL), "sigaction");
    sigdelset(&set, SIGTERM);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    // -- Allocazione memoria per gestione worker --
    fd_set * workerSockets = (fd_set *) malloc(W * sizeof(fd_set));
    if (workerSockets == NULL) { fprintf(stderr, "malloc error\n"); exit(EXIT_FAILURE); }

    int * workerMax = (int *) malloc(W * sizeof(int));
    if (workerMax == NULL) { fprintf(stderr, "malloc error\n"); exit(EXIT_FAILURE); }

    pthread_mutex_t * workerMutex = malloc((W * sizeof(pthread_mutex_t)));
    if (workerMutex == NULL) { fprintf(stderr, "malloc error\n"); exit(EXIT_FAILURE); }

    // -- Struttura dati per la stima dei secret --
    struct nodo_client * estimates_list = new_estimates_list();
    pthread_mutex_t estimates_list_mtx;
    pthread_mutex_init(&estimates_list_mtx, NULL);  //tutti i thread worker lavorano sulla stessa DS

    // -- Creazione segmento memoria condivisa --
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) { fprintf(stderr, "shm_open error\n"); exit(EXIT_FAILURE); }
    struct data *shmptr = mmap(0, sizeof(struct data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shmptr == MAP_FAILED) { perror("mmap"); exit(EXIT_FAILURE); }
    EC(close(fd), "close");

    // -- Creazione dei thread worker --
    for (int i=0; i<W; i++) {
        FD_ZERO(&workerSockets[i]);
        workerMax[i]=sSk;
        pthread_mutex_init(&workerMutex[i],NULL);
        pthread_t tid;

        struct workerArg* arg = malloc(sizeof(struct workerArg));
        if (arg == NULL) { fprintf(stderr, "malloc error\n"); exit(EXIT_FAILURE); }

        arg->serverID = serverID;
        arg->serverSock = sSk;
        arg->clients_to_handle = &workerSockets[i];
        arg->max_fd = &workerMax[i];
        arg->clients_to_handle_mtx = &workerMutex[i];
        arg->estimates_list = estimates_list;
        arg->estimates_list_mtx = &estimates_list_mtx;
        arg->shmptr = shmptr;

        pthread_create(&tid,NULL, &worker,arg);
        pthread_detach(tid);
    }

    // -- Creazione del thread master --
    pthread_t tid;
    struct masterArg *arg = malloc(sizeof(struct masterArg));
    if (arg == NULL) { fprintf(stderr, "malloc error\n"); exit(EXIT_FAILURE); }

    arg->serverID = serverID;
    arg->serverSock = sSk;
    arg->arrClients = workerSockets;   //puntatore a fd_set
    arg->arrMaxWorker = workerMax;           //puntatore a int
    arg->arrWorkerMutex = workerMutex;    //puntatore a pthread_mutex

    pthread_create(&tid, NULL, &master, arg);
    pthread_detach(tid);

    while (end_flag == 0) { pause(); }

    // -- Ricevuto SIGTERM --
    char buff[64];
    sprintf(buff, "SERVER %d EXITING\n", serverID);
    EC(write(1, buff, strlen(buff)), "write");
    free(workerSockets);
    free(workerMax);
    free(workerMutex);
    pthread_mutex_lock(&estimates_list_mtx);
    free_estimates_list(&estimates_list);
    pthread_mutex_unlock(&estimates_list_mtx);
    exit(EXIT_SUCCESS);
}

void* master(void* arg) {
    // -- Fetching degli argomenti --
    int serverID = ((struct masterArg*)arg)->serverID;
    int serverSock = ((struct masterArg*)arg)->serverSock;
    fd_set* arrClients= ((struct masterArg*)arg )->arrClients;  //array
    int* arrMaxWorker=((struct masterArg*)arg )->arrMaxWorker;    //array
    pthread_mutex_t* arrWorkerMutex= ((struct masterArg*)arg )->arrWorkerMutex; //array
    free(arg);

    // -- Accettazione connessione e distribuzione dei socket nei set dei thread worker --
    int w = 0;
    while (1) {

        int clientSock = accept(serverSock,NULL,0);
        if (clientSock == -1) { perror("accept"); exit(EXIT_FAILURE); }
        fprintf(stdout,"SERVER %d CONNECT FROM CLIENT\n", serverID);

        pthread_mutex_lock(&(arrWorkerMutex[w]));
        FD_SET(clientSock, &arrClients[w]);
        if (arrMaxWorker[w] < clientSock) {
            arrMaxWorker[w] = clientSock;
        }
        pthread_mutex_unlock(&(arrWorkerMutex[w]));

        w = (w+1) % W;
    }
}
void* worker(void* arg){
    // -- Fetching degli argomenti --
    int serverID = ((struct workerArg*)arg)->serverID;
    fd_set* clients_to_handle= ((struct workerArg*)arg)->clients_to_handle;
    int* max_fd=((struct workerArg*)arg)->max_fd;
    int serverSock = ((struct workerArg*)arg)->serverSock;
    pthread_mutex_t* clients_to_handle_mtx= ((struct workerArg*)arg)->clients_to_handle_mtx;
    struct nodo_client* estimates_list = ((struct workerArg*)arg)->estimates_list;
    pthread_mutex_t* estimates_list_mtx = ((struct workerArg*)arg)->estimates_list_mtx;
    struct data* shmptr = ((struct workerArg*)arg)->shmptr;
    free(arg);

    while (1) {

        // -- Aggiornamento dei client da servire --
        pthread_mutex_lock(clients_to_handle_mtx);
        fd_set readSock = *clients_to_handle;
        int max = *max_fd;
        pthread_mutex_unlock(clients_to_handle_mtx);

        struct timeval tv = {0, 1000};

        EC(select(max+1,&readSock,NULL,NULL, &tv), "select");
        long ms = get_ms();

        for (int sd=0; sd<max+1; sd++){

            if (FD_ISSET(sd,&readSock)) {
                long x;
                size_t n = read(sd,&x,8);
                if (n == -1) { perror("read"); continue; }
                long clientID = ntohl(x);
                if (n > 0) {
                    // -- Messaggio valido -> Aggiornamento stima --
                    pthread_mutex_lock(estimates_list_mtx);
                    if(!list_contains(estimates_list, clientID)) {
                        add_client(estimates_list, clientID, ms);
                    } else {
                        update_estimate(estimates_list, clientID, ms, serverID);
                    }
                    pthread_mutex_unlock(estimates_list_mtx);

                    fprintf(stdout, "SERVER %d INCOMING FROM %s @ %ld\n", serverID, decToHex(clientID), ms);
                }

                if (n == 0) {
                    // -- Fetching della stima dalla struttura dati --
                    long stima;
                    pthread_mutex_lock(estimates_list_mtx);
                    stima = estimate_secret(estimates_list, clientID);
                    pthread_mutex_unlock(estimates_list_mtx);

                    if (stima != -1) {  // -1 valore di errore della estimate_secret
                        fprintf(stdout, "SERVER %d CLOSING %s ESTIMATE %ld\n", serverID, decToHex(clientID), stima);
                        // -- Scrittura della stima nella memoria condivisa col supervisor --
                        sem_wait(&shmptr->empty);    //il parametro system call è 1 perchè sem_wait setta errno
                        shmptr->serverID = serverID;
                        shmptr->clientID = clientID;
                        shmptr->secret = stima;
                        sem_post(&shmptr->endwrite);
                    } else { fprintf(stderr, "Something went wrong while estimating secret for client %s\n", decToHex(clientID));}
                    // -- Rimozione del client dall'insieme di quelli da gestire --
                    pthread_mutex_lock(clients_to_handle_mtx);
                    FD_CLR(sd,clients_to_handle);
                    while (!FD_ISSET((*max_fd), clients_to_handle) && (*max_fd)>=serverSock) {   //sicuramente sSk è settato quindi non ho problemi.
                        (*max_fd)--;
                    }
                    EC(close(sd), "close");
                    pthread_mutex_unlock(clients_to_handle_mtx);
                }
            }
        }
    }
}
