#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <mqueue.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <semaphore.h>
#include <ncurses.h>
#include "comun.h"

struct Nave{
    int oxigeno;
    int combustible;
    int posX, posY;
    int bodegaMinerales[4];
    sem_t *sem_mutex;
};

/*hilos del cliente nave*/
void* hilo_soporte_vital(void* arg);
void* hilo_propulsion(void* arg);
void* hilo_extraccion(void* arg);
void* hilo_radar(void* arg);

/*funciones*/
int trueque_estacion(struct Nave* nave);

static int calcular_total_minerales(struct Nave* nave) {
    int total = 0;
    for (int i = 0; i < 4; i++) {
        total += nave->bodegaMinerales[i];
    }
    return total;
}

int main(int argc, char *argv[])
{
    struct Nave nave;
    /*Inicializa la nave*/
    nave.oxigeno = 100;
    nave.combustible = 5;
    nave.posX = 0;
    nave.posY = 0;
    for (int i = 0; i < 4; i++) {
        nave.bodegaMinerales[i] = 0;
    }
    /*inicializa los hilos*/
    pthread_t hiloExtraccion;
    
    /*inicializa el mutex*/
    if ((nave.sem_mutex = sem_open( "mutex_nave", O_CREAT, 0666, 1)) == (sem_t *) -1) {
        perror("No se pudo crear el semáforo");
        exit(EXIT_FAILURE);
    }

    /*Crea los hilos para cada función de la nave*/
    int rc = pthread_create(&hiloExtraccion, NULL, hilo_extraccion, &nave);
    if (rc != 0) { perror("pthread_create"); exit(EXIT_FAILURE); }
    printf("hilo_extraccion creado, tid=%lu\n", (unsigned long)hiloExtraccion);
    pthread_join(hiloExtraccion, NULL); // Espera a que el hilo de extracción termine (en este caso, no terminará).
    // Termina la ejecución del programa.
    exit(EXIT_SUCCESS);
}

/**
 * Función para el hilo de extracción de minerales.
 * Esta función simula la extracción de minerales de asteroides adyacentes, el consumo de combustible y la actualización de la bodega de minerales de la nave.
 */
void* hilo_extraccion(void* arg){
    struct Nave* nave = (struct Nave*) arg;
    // Simula la extracción de minerales y el consumo de combustible
    printf("hilo_extraccion iniciado, pthread_self=%lu\n", (unsigned long)pthread_self());
    srand((unsigned int)time(NULL)); // Inicializa la semilla para la generación de números aleatorios
    int asteroideAdyacente = 1;

    while(1){
        sleep(2); // Simula el tiempo de extracción

        /* verifica si hay un asteroide adyacente */
        if (!asteroideAdyacente) {
            printf("Extracción: no hay asteroides adyacentes.\n");
            continue; // Intenta de nuevo en la siguiente iteración
        }

        sem_wait(nave->sem_mutex); // --- BLOQUEO ---

        if (nave->combustible <= 0) {
            printf("Extracción: no hay combustible.\n");
            sem_post(nave->sem_mutex);
            break; // Sale del bucle si no hay combustible
        }

        /*gasta combustible*/
        nave->combustible -= 1;

        /*extrae minerales */
        int indiceMIneral;
        for (int i = 0; i < 4; i++) {
            indiceMIneral = i;
            nave->bodegaMinerales[indiceMIneral] += 1;
        }

        printf("Extracción: -1 combustible, +%d minerales (repartidos). Combustible restante: %d\n", indiceMIneral, nave->combustible + 1);
        sem_post(nave->sem_mutex); // --- DESBLOQUEO ---
    }
    return NULL;
}

/** 
 * Función para realizar el trueque en la estación espacial.
 * Esta función se encarga de enviar los recursos de la nave a la estación espacial a través de una cola de mensajes y recibir los recursos necesarios a cambio.
 *
 */
int trueque_estacion(struct Nave* nave){
    mqd_t cola_respuesta;
    mqd_t cola_estacion;
    struct mq_attr attr;
    char buffer[TAMANIO_MAX_MSG];
    char respuesta_buffer[TAMANIO_MAX_MSG];

    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = TAMANIO_MAX_MSG;
    attr.mq_curmsgs = 0;

    /*crea cola de respuesta (lectura)*/
    cola_respuesta = mq_open(NOMBRE_COLA_ESTACION, PERMISOS_COLA, &attr);
    if (cola_respuesta == (mqd_t) -1) {
        perror("Error al abrir la cola de respuesta");
        return -1;
    }
    printf("Cola de respuesta creada");

    /*obtener total minerales*/
    sem_wait(nave->sem_mutex); // --- BLOQUEO ---
    int totalMinerales = calcular_total_minerales(nave);
    sem_post(nave->sem_mutex); // --- DESBLOQUEO ---

    snprintf(buffer, TAMANIO_MAX_MSG, "MINERALES:%d", totalMinerales); // Prepara el mensaje con la cantidad de minerales para enviar a la estación

    /*abrir cola de estación para enviar solicitud*/
    cola_estacion = mq_open(NOMBRE_COLA_ESTACION, PERMISOS_COLA);
    if (cola_estacion == (mqd_t) -1) {
        perror("Error al abrir la cola de estación");
        mq_close(cola_respuesta);
        return -1;
    }
    /*enviar solicitud*/
    if (mq_send(cola_estacion, buffer, strlen(buffer), 0) == -1) {
        perror("Error al enviar mensaje a la estación");
        mq_close(cola_estacion);
        mq_close(cola_respuesta);
        return -1;
    }

    mq_close(cola_estacion);

     /*esperar respuesta de la estación*/
    ssize_t bytes_leidos = mq_receive(cola_respuesta, buffer, 256, NULL);
    if (bytes_leidos == -1) {
        perror("Error al recibir mensaje de la estación");
        mq_close(cola_respuesta);
        return -1;
    }

    respuesta_buffer[bytes_leidos] = '\0'; // Asegura que el buffer esté null-terminated

    printf("Respuesta de la estación: %s\n", respuesta_buffer);

    /* parsear respuesta del tipo: FUEL:<x>;OXY:<y> */
    int combustible_recibido = 0, oxigeno_recibido = 0;
    sscanf(respuesta_buffer, "FUEL:%d;OXY:%d", &combustible_recibido, &oxigeno_recibido); // scanf para extraer los valores de combustible y oxígeno

     sem_wait(nave->sem_mutex); // --- BLOQUEO ---
    // Aquí se actualizarían los recursos de la nave según la respuesta recibida de la estación
    nave->combustible += combustible_recibido;
    nave->oxigeno += oxigeno_recibido;
    for (int i = 0; i < 4; i++) {
        nave->bodegaMinerales[i] = 0; // Se asume que se entregan todos los minerales a la estación
    }
    sem_post(nave->sem_mutex); // --- DESBLOQUEO --
    mq_close(cola_respuesta);

    printf("Trueque realizado: +%d combustible, +%d oxígeno. Minerales entregados a la estación.\n", combustible_recibido, oxigeno_recibido);
    return 0;
}