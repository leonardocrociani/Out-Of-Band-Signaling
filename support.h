// ------------------- Funzioni per il server -------------------

struct nodo_client {
    long ID;
    long prec;
    long best;
    struct nodo_client* next;
};

struct nodo_client* new_estimates_list();
//ritorna una nuova lista per la valutazione delle stime.

void add_client(struct nodo_client* current, long ID, long current_time);
//aggiunge un client alla lista

void update_estimate(struct nodo_client* lista, long ID, long current_time, int sID);
//cerca ID nella lista, eventualmente aggiorna la migliore stima

int list_contains(struct nodo_client*lista, long ID);
//controlla se la lista contiene client con identificatore == ID

long estimate_secret(struct nodo_client*lista, long ID);
//ritorna la stima del client con identificatore == ID

void free_estimates_list(struct nodo_client** lista);
//dealloca la lista dallo heap

// ------------------- Funzioni per il supervisor -------------------

struct supervisor_n {
    long ID;
    int based_on;
    long stima;
    struct supervisor_n* next;
    struct server_n* servers;
};

struct server_n {
    int serverID;
    struct server_n* next;
};

struct supervisor_n* new_supervisor_list();
//crea una nuova lista per gestire le stime

void add_new_client(struct supervisor_n* list, long cID, long val, int sID);
//aggiunge un cliente alla lista dei supervisor

void update_client(struct supervisor_n* list, long cID, long val, int sID);
//cerca di affinare la stima del client con identificatore == cID. 
//se il server che ha fornito la stima non l'aveva mai fornita per questo client, incrementa il based_on

int client_exist(struct supervisor_n* list, long ID);
//cerca nella lista, se esiste il client con clientID == ID

int server_registered(struct server_n* list, int sID);
//controlla se il server esiste tra quelli che hanno fornito stime per un certo client

void print_supervisor_table(struct supervisor_n* list, int stream);
//stampa tutta la tabella delle stime del supervisor sullo stream passato come parametro.

void free_supervisor_list(struct supervisor_n** list);
//dealloca la lista e le sottoliste dei serverID dallo heap

// ------------------- Altre funzioni di supporto -------------------

void EC(int val, char* name);
//controllo errore

long get_ms(void);
//ottenimento di timestamp in milliseconds.

char* decToHex(long int decNum);
//ritorna il numero esadecimale corrispondente a decNum