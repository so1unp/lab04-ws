#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>

struct Asteroides {
    int posX;
    int posY;
    sem_t sem_mutex;
    int Mutexio;
    int semaforita;
    int kernelio;
    int Deuterio;
};

struct Nave {
    sem_t sem_mutex;
    int combustible;
    int posX;
    int posY;
    int bodegaMinerales[4];
};

struct Asteroides mapa_asteroides[100];

void asignar_valores(struct Asteroides *asteroide);
void* hilo_extraccion(void* arg);

int main() {
    for (int i = 0; i < 5; i++) {
        asignar_valores(&mapa_asteroides[i]);
    }

    struct Nave nave;
    nave.combustible = 3;
    nave.posX = 5;
    nave.posY = 5;
    for (int i = 0; i < 4; i++) {
        nave.bodegaMinerales[i] = 0;
    }
    sem_init(&nave.sem_mutex, 0, 1);

    pthread_t hiloExtraccion;
    int errorHilo_extraccion = pthread_create(&hiloExtraccion, NULL, hilo_extraccion, &nave);
    if (errorHilo_extraccion != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_join(hiloExtraccion, NULL);

    exit(EXIT_SUCCESS);
    return 0;
}

//fabrica
void asignar_valores(struct Asteroides *asteroide) {
    asteroide->posX = 3;
    asteroide->posY = 3;
    asteroide->Deuterio = 100;
    asteroide->Mutexio = 1;
    asteroide->semaforita = 1;
    asteroide->kernelio = 1;

    printf("Asignando valores al asteroide...\n");
    printf("posX: %d\n", asteroide->posX);
    printf("posY: %d\n", asteroide->posY);
    printf("Deuterio: %d\n", asteroide->Deuterio);
    printf("Mutexio: %d\n", asteroide->Mutexio);
    printf("semaforita: %d\n", asteroide->semaforita);
    printf("kernelio: %d\n", asteroide->kernelio);
    printf("Inicializando semáforo del asteroide...\n");
    
    sem_init(&asteroide->sem_mutex, 1, 1);
}





void* hilo_extraccion(void* arg) {
    struct Nave* nave = (struct Nave*) arg;
    //recibo
    struct Asteroides* asteroide = &mapa_asteroides[0];

    sem_wait(&asteroide->sem_mutex);
    asteroide->Deuterio = 40;
    sem_post(&asteroide->sem_mutex);

    int i = 0;
    int count = 0;
    printf("--------------------------------------------------------------\n");
    while (1) {

        i++;
        sleep(2);
        int asteroideAdyacente = rand() % 2;

        if (!asteroideAdyacente) {
            printf("Extracción: numero %d, no hay asteroides adyacentes.\n", i);
            continue;
        }

        sem_wait(&nave->sem_mutex);
        
        //nave a asteroide 
        if (nave->combustible <= 0) {
            printf("Extracción: numero %d, no hay combustible.\n", i);
            asignar_valores(&mapa_asteroides[6]);
            sem_post(&nave->sem_mutex);
            break;
        }

        
        nave->combustible -= 1;
        //extraer
        int indiceMIneral = count;
        nave->bodegaMinerales[indiceMIneral]++;
        

        sem_wait(&asteroide->sem_mutex);
        asteroide->Deuterio -= 1;
        count++;
        printf("Extracción: numero %d, cantidad total extraída: %d, Combustible restante: %d\n", i,count, nave->combustible);
        count--;
        printf("Minerales en bodega: %d, %d, %d, %d\n", nave->bodegaMinerales[0], nave->bodegaMinerales[1], nave->bodegaMinerales[2], nave->bodegaMinerales[3]);
        printf("Datos del asteroide después de la extracción:\n");
        printf("Deuterio: %d\n", asteroide->Deuterio);
        printf("Mutexio: %d\n", asteroide->Mutexio);
        printf("semaforita: %d\n", asteroide->semaforita);
        printf("kernelio: %d\n", asteroide->kernelio);

        printf("--------------------------------------------------------------\n");

        //matar 
        if (asteroide->Deuterio == 0) {
            printf("El asteroide se ha agotado.\n");
            break;
        }

        sem_post(&asteroide->sem_mutex);
        sem_post(&nave->sem_mutex);
        count++;
        
    }
    return NULL;
}