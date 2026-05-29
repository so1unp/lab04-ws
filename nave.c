#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct Nave{
    int oxigeno;
    int combustible;
    int posX, posY;
    int bodegaMinerales[4];
    pthread_mutex_t mutex;
} ;

int main(int argc, char *argv[])
{
    /*hilos del cliente nave*/
    void* hilo_soporte_vital(void* arg);
    void* hilo_propulsion(void* arg);
    void* hilo_extraccion(void* arg);
    void* hilo_radar(void* arg);

    // Termina la ejecución del programa.
    exit(EXIT_SUCCESS);
}
