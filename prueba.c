#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>


struct Asteroides {
    
    int posX, posY;
    sem_t sem_mutex;
   // int flag;
   int Mutexio;
   int semaforita;
    int kernelio;
    int   Deuterio;
};



void asignar_valores(struct Asteroides *asteroide);

int main() {
    struct Asteroides mapa_asteroides[5];

    // 2. La Línea de Ensamblaje
    for (int i = 0; i < 5; i++) {
        // Delegamos la asignación enviando la dirección de memoria
        asignar_valores(&mapa_asteroides[i] ); 
    }

    return 0;
}


void asignar_valores(struct Asteroides *asteroide) {
    // Validación de espacio vacío
   
    //no sobreponer
        asteroide->posX = 3;
        asteroide->posY = 3;

        //random
    asteroide->Deuterio = 100;
    asteroide->Mutexio = 1;
    asteroide->semaforita = 1;
    asteroide->kernelio = 1;

    //%d es el formato para imprimir enteros
    printf("Asignando valores al asteroide...\n");
    printf("posX: %d\n", asteroide->posX);
    printf("posY: %d\n", asteroide->posY);
    printf("Deuterio: %d\n", asteroide->Deuterio);
    printf("Mutexio: %d\n", asteroide->Mutexio);
    printf("semaforita: %d\n", asteroide->semaforita);
    printf("kernelio: %d\n", asteroide->kernelio);

    //asteroide->flag = 1;
    sem_init(&asteroide->sem_mutex, 1, 1);

    
}




