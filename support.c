#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "support.h"

// ------------------- Funzioni per il server -------------------

struct nodo_client* new_estimates_list() {
    struct nodo_client *lista = (struct nodo_client *)malloc(sizeof(struct nodo_client));
    lista->ID = -1;
    lista->prec = -1;
    lista->best = -1;
    lista->next = NULL;
    return lista;
}

void add_client(struct nodo_client* current, long ID, long current_time) {
    if (current->ID == -1) {    //è il primo nodo.
        current->ID = ID;
        current->prec = current_time;
        current->best = current_time;
        current->next = NULL;
    } else if (current->next == NULL) {
        struct nodo_client *nodo = (struct nodo_client *)malloc(sizeof(struct nodo_client));
        nodo->ID = ID;
        nodo->prec = current_time;
        nodo->best = current_time;
        nodo->next = NULL;
        current->next = nodo;
    } else {
        add_client(current->next, ID, current_time);
    }
}

void update_estimate(struct nodo_client* lista, long ID, long current_time, int sID) {
    struct nodo_client* temp = lista;
    while (temp!=NULL) {
        if (temp->ID == ID) {
            if ( (current_time - temp->prec) < temp->best ) {
                temp->best = current_time - temp->prec;
            } 
            temp->prec = current_time;
            return;
        } else {
            temp = temp->next;
        }
    }
}

int list_contains(struct nodo_client*lista, long ID) {
    struct nodo_client* temp = lista;
    while (temp != NULL) {
        if (ID == temp->ID) { return 1; }
        temp = temp->next;
    }
    return 0;
}

long estimate_secret(struct nodo_client*lista, long ID) {
    struct nodo_client* temp = lista;
    while (temp != NULL) {
        if (ID == temp->ID) { return temp->best; }
        temp = temp->next;
    }
    return -1;
}

void free_estimates_list(struct nodo_client** lista) {
    struct nodo_client* temp = *lista;
    struct nodo_client* prec = NULL;
    while (temp!=NULL) {
        prec = temp;
        temp = temp->next;
        free(prec);
    }
}

// ------------------- Funzioni per il supervisor -------------------

struct supervisor_n* new_supervisor_list() {
    struct supervisor_n* list = (struct supervisor_n*)malloc(sizeof (struct supervisor_n));
    list->ID = -1;
    list->based_on = 0;
    list->stima = -1;
    list->next = NULL;

    struct server_n* server_ = (struct server_n*)malloc(sizeof(struct server_n));
    list->servers = server_;
    list->servers->serverID = -1;
    list->servers->next = NULL;

    return list;
}

void add_new_client(struct supervisor_n* list, long cID, long val, int sID) {
    if (list->ID == -1) {   //primo e unico nodo.
        list->ID = cID;
        list->based_on = 1;
        list->stima = val;
        list->servers->serverID = sID;
    } else if (list->next == NULL) {
        struct supervisor_n* nodo = (struct supervisor_n*)malloc(sizeof(struct supervisor_n));
        nodo->ID = cID;
        nodo->stima = val;
        nodo->based_on = 1;
        nodo->next = NULL;

        struct server_n* servers = (struct server_n*)malloc(sizeof(struct server_n));
        nodo->servers = servers;
        nodo->servers->next = NULL;
        nodo->servers->serverID = sID;

        list->next = nodo;
    } else {
        add_new_client(list->next, cID, val, sID);
    }
}

void update_client(struct supervisor_n* list, long cID, long val, int sID) {
    struct supervisor_n* temp = list;
    while (temp!=NULL) {
        if (temp->ID == cID) {
            //trovato il client nella lista.
            if ( temp->stima == -1 ) {  //prima e unica stima.
                temp->stima = val;
            }
            else if ( val < temp->stima && val!=-1 && val!=0) {
                temp->stima = val;
            }
            //aggiorno il based_on se il server non è presente nella lista.
            if (!server_registered(temp->servers, sID)) {
                //appendi in coda il server e basedon++
                temp->based_on++;
                struct server_n* temp1 = temp->servers;
                while (temp1->next!=NULL) {
                    temp1 = temp1->next;
                }   
                //qui, temp1->next è null
                struct server_n* node = (struct server_n*)malloc(sizeof(struct server_n));
                node->next = NULL;
                node->serverID = sID;
                temp1->next = node;
            }
            return;
        } else {
            temp = temp->next;
        }
    }
}

int client_exist(struct supervisor_n* list, long ID) {
    struct supervisor_n* temp = list;
    while (temp!=NULL) {
        if (temp->ID == ID) {return 1;} 
        temp = temp->next;
    }
    return 0;
}

int server_registered(struct server_n* list, int sID) {
    struct server_n * temp = list;
    while (temp != NULL) {
        if (temp->serverID == sID) { return 1; }
        temp = temp->next;
    }
    return 0;
}

void print_supervisor_table(struct supervisor_n* list, int stream) {    //stream = 0 -> stderr ; = 1 -> stdout
    FILE* str;
    if (stream == 0) { str = stderr; } else { str = stdout; }
    struct supervisor_n* temp = list;
    if (temp->stima == -1) { fprintf(str, "\n[!] THERE'S NO ESTIMATES\n"); fflush(str); return; }
    fprintf(str, "\n\n* * * * * * * * [ ESTIMATES TABLE ] * * * * * * *\n");
    while (temp!=NULL) {
        fprintf(str, "SUPERVISOR ESTIMATE %ld FOR %s BASED ON %d\n", temp->stima, decToHex(temp->ID), temp->based_on);
        fprintf(str, "* * * * * * * * * * * * * * * * * * * * * * * * *\n");
        temp = temp->next;
    }
    fprintf(str, "\n");
    fflush(str);
}

void free_supervisor_list(struct supervisor_n** list) {
    struct supervisor_n * temp = *list;
    struct supervisor_n * prec = NULL;
    while (temp!=NULL) {
        struct server_n * temp1 = temp->servers;
        struct server_n * prec1;
        while (temp1!=NULL) {
            prec1 = temp1;
            temp1 = temp1->next;
            free(prec1);
        }
        prec = temp;
        temp = temp->next;
        free(prec);
    }
}

// ------------------- Altre funzioni di supporto -------------------

void EC(int val, char* name) {
    if (val == -1) { perror(name); exit(EXIT_FAILURE); }
}

long get_ms() {
    long ms;  // millisecondi
    time_t s; // secondi
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    s = spec.tv_sec;
    ms = spec.tv_nsec / 1.0e6;
    if (ms > 999) {
        s++;
        ms = 0;
    }
    return (s * 1000) + ms;
}

char* decToHex(long int decNum) {
    char * r = (char*)malloc(sizeof(char)*100);
    char hex[50]; int i = 0, k = 0;
    while (decNum != 0) {int temp = 0; temp = decNum % 16; if (temp < 10) { hex[i] = temp + 48; i++; } else { hex[i] = temp + 55; i++; }decNum=decNum/16; }
    for (int j = i - 1; j >= 0; j--) {/*printf("%c", hex[j]);*/ r[k]=hex[j]; k++;}
    return r;
}