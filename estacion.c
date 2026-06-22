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

#define NOMBRE_COLA_AUXILIO "/cola_auxilio_deuterio"

WINDOW *win;


/*struct Asteroides {
  int minerales;
  int posX, posY;
  int ancho, largo;
  sem_t *sem_mutex;
};
struct Estacion {
  int minerales;
  int posX, posY;
  int ancho, largo;
  sem_t *sem_mutex;
};
struct Mapa {
  struct Nave *nave;
  struct Asteroides asteroides[10];
  struct Estacion estacion;
  WINDOW *ventana;
};
*/

// Reutilizamos la lógica adaptada a la Estación
struct EstacionProceso {
    int oxigeno;
    int combustible;
    int posX, posY;
    int bodegaMinerales[4];
    int activa; // <-- 1 = Operativa, 0 = Inutilizable (Combustible desvanecido)
    sem_t *sem_mutex;
    sem_t *mutex_pantalla;
};
/*  
combustible
transaccion
mapa
hangar 
*/
/* Hilos de la estación */
void* hilo_consumo_interno(void* arg); // Equivalente a soporte vital
//este 
void* hilo_atender_trueques(void* arg); // El que escucha la cola mq_queue
void* hilo_grafico_estacion(void* arg); // Equivalente al radar (dibuja la estación fija)


int main(int argc, char *argv[])
{
    struct EstacionProceso estacion;
    pthread_t hiloConsumo, hiloTrueques, hiloGrafico;

    /* Inicializa los recursos de la estación al máximo */
    estacion.oxigeno = 100;
    estacion.combustible = 100;
    
    // --- OBTENCIÓN DE COORDENADAS DESDE EL SERVIDOR ---
    // Aquí puedes poner una lógica para leer de una cola del servidor.
    // Por ahora, para que empiece fija en pantalla, le ponemos un valor estático:
    estacion.posX = 20; 
    estacion.posY = 10;

    for (int i = 0; i < 4; i++) {
        estacion.bodegaMinerales[i] = 0;
    }
    
    // Limpieza e inicialización de semáforos
    sem_unlink("mutex_estacion");
    sem_unlink("mutex_pantalla");
    
    if ((estacion.sem_mutex = sem_open("mutex_estacion", O_CREAT, 0666, 1)) == (sem_t *) -1) {
        perror("No se pudo crear el semáforo de la estación");
        exit(EXIT_FAILURE);
    }
    if ((estacion.mutex_pantalla = sem_open("mutex_pantalla", O_CREAT, 0666, 1)) == (sem_t *) -1) {
        perror("No se pudo crear el semáforo para la pantalla");
        exit(EXIT_FAILURE);
    }

    // Inicialización de Ncurses
    initscr();
    cbreak();
    noecho();
    curs_set(FALSE);

    // Ventana superior de estado interno
    win = newwin(5, 40, 5, 5);
    if (win == NULL) {
        endwin();
        perror("No se pudo crear la ventana");
        exit(EXIT_FAILURE);
    }
    box(win, 0, 0);
    wrefresh(win);

    // 1. Hilo de Consumo Interno (Soporte Vital de la estación)
    if (pthread_create(&hiloConsumo, NULL, hilo_consumo_interno, &estacion) != 0) {
        endwin();
        perror("Error al crear hilo de consumo interno");
        exit(EXIT_FAILURE);
    }

    // 2. Hilo de Trueques (Escucha de manera pasiva las solicitudes de las naves)
    if (pthread_create(&hiloTrueques, NULL, hilo_atender_trueques, &estacion) != 0) {
        endwin();
        perror("Error al crear hilo de trueques");
        exit(EXIT_FAILURE);
    }

    // 3. Hilo Gráfico (Radar fijo de la estación)
    if (pthread_create(&hiloGrafico, NULL, hilo_grafico_estacion, &estacion) != 0) {
        endwin();
        perror("Error al crear hilo gráfico");
        exit(EXIT_FAILURE);
    }

    // El main espera que los componentes esenciales terminen
    pthread_join(hiloConsumo, NULL);
    pthread_join(hiloTrueques, NULL);
    pthread_join(hiloGrafico, NULL);

    endwin();
    return 0;


    // Termina la ejecución del programa.
    exit(EXIT_SUCCESS);
}

/**
 * Hilo de consumo periódico de la estación.
 * Consume recursos de forma constante. Si baja de 20%, simulará la recarga automática del servidor.
 */
void* hilo_consumo_interno(void* arg) {
    struct EstacionProceso* est = (struct EstacionProceso*) arg;
    mqd_t cola_auxilio;
    
    // Abrimos la cola de auxilio en modo ESCRITURA
    // Las naves la abrirán en modo LECTURA
    cola_auxilio = mq_open(NOMBRE_COLA_AUXILIO, O_WRONLY | O_CREAT, 0666, NULL);

    while (1) {
        sem_wait(est->sem_mutex);

        // Si ya está inactiva, no seguimos consumiendo ni enviando SOS
        if (!est->activa) {
            sem_post(est->sem_mutex);
            sleep(2);
            continue;
        }
        
        // pierde comustible periodicamente
        est->combustible -= 3; 
        if (est->combustible <= 0) {
          est->combustible = 0;
          est->activa = 0; //-- ¡LA ESTACIÖN SE PIERDE POR COMPLETO!
          printf("[Estación] ¡CRÍTICO! Combustible agotado. Estación permanentemente inactiva.\n");
            
            // Opcional: Enviar un mensaje especial al servidor "ESTACION_DESTRUIDA" 
            // para que sume al contador global de Game Over.
        }

        int f_actual = est->combustible;
        int ox_actual = est->oxigeno;
        int sigue_viva = est->activa;
        sem_post(est->sem_mutex);

        // --- AJUSTE: Alerta de Deuterio al bajar del 20% ---
        // Actualizar subventana ncurses de estado
        werase(win);
        box(win, 0, 0);
        if (!sigue_viva) {
            mvwprintw(win, 1, 1, "ESTACION: INUTILIZABLE");
            mvwprintw(win, 2, 1, "FUERA DE SERVICIO (0%% FUEL)");
        } else if (f_actual <= 20) {
            mvwprintw(win, 1, 1, "¡¡ALERTA: SIN DEUTERIO!!");
            mvwprintw(win, 2, 1, "Oxi: %3d%% | Fuel: %3d%%", ox_actual, f_actual);
            
            // Grito de auxilio a las naves
            char mensaje_sos[TAMANIO_MAX_MSG];
            snprintf(mensaje_sos, TAMANIO_MAX_MSG, "SOS_DEUTERIO:POS_X:%d;POS_Y:%d", est->posX, est->posY);
            if (cola_auxilio != (mqd_t)-1) {
                mq_send(cola_auxilio, mensaje_sos, strlen(mensaje_sos), 0);
            }
        } else {
            mvwprintw(win, 1, 1, "Estacion Operativa");
            mvwprintw(win, 2, 1, "Oxi: %3d%% | Fuel: %3d%%", ox_actual, f_actual);
        }
        wrefresh(win);

        sleep(2); 
    }
    
    if (cola_auxilio != (mqd_t)-1) mq_close(cola_auxilio);
    return NULL;
}

/**
 * Hilo que atiende los trueques. Abre la cola POSIX en modo LECTURA/ESCRITURA
 * y procesa lo que pide nave.c enviando la respuesta correspondiente.
 */
void* hilo_atender_trueques(void* arg) {
    //el pid es necesario para responder a la nave correcta, ya que todas las naves escuchan la misma cola de trueques.
    //antes , cuando no habia pid, la estación respondía a todas las naves que le escribían, lo que generaba caos en el sistema. Ahora, con el PID incluido en el mensaje, la estación puede enviar la respuesta solo a la nave que hizo la solicitud, evitando confusiones y asegurando que cada nave reciba la información correcta.
    //antes el chat no era individual, sino
    struct EstacionProceso* est = (struct EstacionProceso*) arg;
    mqd_t cola_estacion;
    struct mq_attr attr;
    char buffer[TAMANIO_MAX_MSG];

    attr.mq_flags = 0;
    attr.mq_maxmsg = 3;
    attr.mq_msgsize = TAMANIO_MAX_MSG;
    attr.mq_curmsgs = 0;

    mq_unlink(NOMBRE_COLA_ESTACION); 
    cola_estacion = mq_open(NOMBRE_COLA_ESTACION, O_CREAT | O_RDWR, 0666, &attr);
    if (cola_estacion == (mqd_t)-1) {
        perror("Estación: Error al abrir la cola de trueques");
        return NULL;
    }

    while (1) {
        ssize_t bytes_leidos = mq_receive(cola_estacion, buffer, TAMANIO_MAX_MSG, NULL);
        if (bytes_leidos == -1) continue;

        buffer[bytes_leidos] = '\0';
        int minerales_recibidos = 0;
        pid_t pid_nave = 0; // <-- NUEVO: necesitamos el PID para responder a la nave correcta  

        // CAMBIO: el formato ahora incluye PID, como manda nave.c
        // antes: sscanf(buffer, "MINERALES:%d", &minerales_recibidos)
        if (sscanf(buffer, "PID:%d;MINERALES:%d", &pid_nave, &minerales_recibidos) != 2) {
            fprintf(stderr, "Mensaje inválido: %s\n", buffer);
            continue; // <-- NUEVO: si el mensaje no matchea, lo descartamos y seguimos
        }

        // NUEVO: armamos el nombre de la cola privada de la nave (como en estacion.c)
        char nombre_cola_nave[64];
        snprintf(nombre_cola_nave, sizeof(nombre_cola_nave), NOMBRE_COLA_NAVE_SERVIDOR, pid_nave);

        char respuesta[TAMANIO_MAX_MSG];
        
        sem_wait(est->sem_mutex);

        // --- AJUSTE: Si está inactiva, rechazamos la transacción ---
        if (!est->activa) {
            sem_post(est->sem_mutex);
            
             // CAMBIO: la respuesta ya no se manda por cola_estacion,
            // se manda por la cola privada de la nave
            snprintf(respuesta, TAMANIO_MAX_MSG, "FUEL:0;OXY:0;STATUS:INACTIVE");

            // NUEVO: abrir cola privada de la nave para responder
            struct mq_attr attr_resp = {0, 3, TAMANIO_MAX_MSG, 0};
            mqd_t cola_nave = mq_open(nombre_cola_nave, O_CREAT | O_WRONLY, PERMISOS_COLA, &attr_resp);
            if (cola_nave != (mqd_t)-1) {
                mq_send(cola_nave, respuesta, strlen(respuesta), 0);
                mq_close(cola_nave);
            }
            continue; // Saltamos a la siguiente petición sin procesar minerales
        }

        //if (sscanf(buffer, "MINERALES:%d", &minerales_recibidos) > 0) {
            
            // --- AJUSTE: El mineral recibido actúa como combustible (Deuterio) ---
            // Cada unidad de mineral nos devuelve un % de combustible, por ejemplo, 15% por viaje
            est->bodegaMinerales[0] += minerales_recibidos; 
            est->combustible += (minerales_recibidos * 5); // 5% de combustible por cada mineral
            if (est->combustible > 100) est->combustible = 100;

            // La estación transfiere oxígeno a la nave a cambio
            int enviar_oxigeno = 50; // Le damos bastante oxígeno ya que nos sobra
            sem_post(est->sem_mutex);


            snprintf(respuesta, TAMANIO_MAX_MSG, "FUEL:0;OXY:%d", enviar_oxigeno);

        // CAMBIO: en vez de mq_send(cola_estacion, ...), abrimos la cola privada de la nave
        struct mq_attr attr_resp = {0, 3, TAMANIO_MAX_MSG, 0};
        mqd_t cola_nave = mq_open(nombre_cola_nave, O_CREAT | O_WRONLY, PERMISOS_COLA, &attr_resp);
        if (cola_nave == (mqd_t)-1) {
            perror("Error al abrir cola de nave específica");
            continue;
        }
        mq_send(cola_nave, respuesta, strlen(respuesta), 0);
        mq_close(cola_nave); // <-- NUEVO: cerrar la cola de la nave (no la de la estación)


        
            

        
    }
    mq_close(cola_estacion);
    return NULL;
}

/**
 * Hilo gráfico de la estación. 
 * Muestra el mapa y renderiza la estación en sus coordenadas FIJAS (representada por una "E").
 */
void* hilo_grafico_estacion(void* arg) {
    struct EstacionProceso* est = (struct EstacionProceso*) arg;
    int counter = 0;
    int fila = 1, columna = 45; 

    int OFFSET_X = 30; 
    int OFFSET_Y = 2;
    int ancho_mapa = 40 * 3;
    int alto_mapa = 15 * 2;

    while (1) {
        sem_wait(est->mutex_pantalla);
        clear();

        // --- DIBUJO DEL MAPA ---
        for (int i = OFFSET_X; i < OFFSET_X + ancho_mapa; i++) {
            mvprintw(OFFSET_Y, i, "-");
        }
        for (int j = OFFSET_Y; j < OFFSET_Y + alto_mapa; j++) {
            mvprintw(j, OFFSET_X, "|");
        } 

        sem_wait(est->sem_mutex);
       /* // --- RENDERS DE CONTROL ---
        mvprintw(fila, columna, "--- CENTRAL DE LA ESTACIÓN ---");
        mvprintw(fila + 1, columna, "Ubicación Fija X: %d, Y: %d", est->posX, est->posY);
        mvprintw(fila + 2, columna, "Minerales Totales en Hangar: %d", est->bodegaMinerales[0]);
        mvprintw(fila + 3, columna, "Tiempo de Actividad: %ds", counter);*/

        // Estadísticas de pantalla
        mvprintw(1, 45, "--- CENTRAL DE LA ESTACIÓN ---");
        mvprintw(2, 45, "Ubicación Fija X: %d, Y: %d", est->posX, est->posY);

        // --- RENDERS DE LA ESTACIÓN EN EL MAPA ---
        // Dibujamos una "E" en lugar de la "A" de la nave, y al ser fija NO incrementa sus posiciones
       if (est->activa) {
            mvprintw(3, 45, "Estado: ONLINE");
            // Se renderiza como 'E' si está operativa
            mvprintw(est->posY + OFFSET_Y + 1, est->posX + OFFSET_X + 1, "E");
        } else {
            mvprintw(3, 45, "Estado: CEMENTERIO ESPACIAL (OFFLINE)");
            // --- AJUSTE: Se queda en el mapa, pero pintamos una '#' (Chatarra) ---
            mvprintw(est->posY + OFFSET_Y + 1, est->posX + OFFSET_X + 1, "#");
        }

        sem_post(est->sem_mutex);

        refresh();
        sem_post(est->mutex_pantalla);

        usleep(200000); // Actualización de pantalla cada 0.2 segundos
        counter++;
    }
    return NULL;
    
}

/*// Lógica conceptual para meter en nave.c
void* hilo_escucha_sos(void* arg) {
    mqd_t cola_auxilio = mq_open("/cola_auxilio_deuterio", O_RDONLY);
    char buffer[256];
    
    while(1) {
        // Se queda esperando por si alguna estación grita por combustible
        ssize_t bytes = mq_receive(cola_auxilio, buffer, 256, NULL);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            int est_x, est_y;
            if (sscanf(buffer, "SOS_DEUTERIO:POS_X:%d;POS_Y:%d", &est_x, &est_y) > 0) {
                // ¡Alerta recibida! 
                // Aquí puedes cambiar la Inteligencia Artificial de la nave o tu destino actual
                // para obligarla a viajar a (est_x, est_y) si tiene deuterio en su bodega.
            }
        }
    }
}
// Lógica a colocar en tu archivo nave.c al hacer el trueque
if (strstr(respuesta_buffer, "STATUS:INACTIVE") != NULL) {
    printf("[Nave] ¡La estación está muerta! No pudimos conseguir oxígeno.\n");
    // La nave no gana recursos y sus minerales siguen en la bodega.
}*/