#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define NB_BUS_X   5
#define NB_BUS_Y   4
#define NB_TRIPS  10

// Contrôle du tunnel
typedef struct {
    sem_t  mutex;
    sem_t  semX, semY;
    int    nbX, nbY;
    int    attX, attY;
    int    turn;
    int    total;
} Tunnel;

typedef struct {
    int     id;
    char    city;      // 'X' ou 'Y'
    Tunnel* control;
} BusArgs;

static void enter_tunnel(BusArgs *b) {
    Tunnel *c = b->control;
    if (b->city == 'X') {
        sem_wait(&c->mutex);
        if (c->nbY > 0 || (c->attY > 0 && c->turn == 1)) {
            c->attX++;
            sem_post(&c->mutex);
            sem_wait(&c->semX);
            sem_wait(&c->mutex);
            c->attX--;
        }
        c->nbX++;
        sem_post(&c->mutex);
    } else {
        sem_wait(&c->mutex);
        if (c->nbX > 0 || (c->attX > 0 && c->turn == 0)) {
            c->attY++;
            sem_post(&c->mutex);
            sem_wait(&c->semY);
            sem_wait(&c->mutex);
            c->attY--;
        }
        c->nbY++;
        sem_post(&c->mutex);
    }
}

static void exit_tunnel(BusArgs *b) {
    Tunnel *c = b->control;
    sem_wait(&c->mutex);
    if (b->city == 'X') {
        c->nbX--;
        if (c->nbX == 0) {
            c->turn = 1;
            if (c->attY > 0) sem_post(&c->semY);
            else if (c->attX > 0) sem_post(&c->semX);
        }
    } else {
        c->nbY--;
        if (c->nbY == 0) {
            c->turn = 0;
            if (c->attX > 0) sem_post(&c->semX);
            else if (c->attY > 0) sem_post(&c->semY);
        }
    }
    sem_post(&c->mutex);
}

static void* bus_thread(void *arg) {
    BusArgs *b = (BusArgs*)arg;
    Tunnel  *c = b->control;

    for (int i = 1; i <= NB_TRIPS; i++) {
        // Aller
        enter_tunnel(b);
        printf("Bus %d de %c : %c -> %c (Trajet %d aller)\n",
               b->id, b->city, b->city, (b->city == 'X') ? 'Y' : 'X', i);
        sem_wait(&c->mutex);
        c->total++;
        sem_post(&c->mutex);
        exit_tunnel(b);

        // Retour
        enter_tunnel(b);
        printf("Bus %d de %c : %c -> %c (Trajet %d retour)\n",
               b->id, b->city, (b->city == 'X') ? 'Y' : 'X', b->city, i);
        sem_wait(&c->mutex);
        c->total++;
        sem_post(&c->mutex);
        exit_tunnel(b);
    }
    return NULL;
}

int main() {
    Tunnel control = {
        .nbX   = 0,
        .nbY   = 0,
        .attX  = 0,
        .attY  = 0,
        .turn  = 0,
        .total = 0
    };

    sem_init(&control.mutex, 0, 1);
    sem_init(&control.semX,  0, 0);
    sem_init(&control.semY,  0, 0);

    pthread_t threads[NB_BUS_X + NB_BUS_Y];
    BusArgs   args[NB_BUS_X + NB_BUS_Y];

    for (int i = 0; i < NB_BUS_X; i++) {
        args[i] = (BusArgs){ .id = i+1, .city = 'X', .control = &control };
        pthread_create(&threads[i], NULL, bus_thread, &args[i]);
    }

    for (int i = 0; i < NB_BUS_Y; i++) {
        args[NB_BUS_X + i] = (BusArgs){ .id = i+1, .city = 'Y', .control = &control };
        pthread_create(&threads[NB_BUS_X + i], NULL, bus_thread, &args[NB_BUS_X + i]);
    }

    for (int i = 0; i < NB_BUS_X + NB_BUS_Y; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Nombre total de trajets effectués : %d (attendu 180)\n", control.total);

    sem_destroy(&control.mutex);
    sem_destroy(&control.semX);
    sem_destroy(&control.semY);
    return 0;
}
