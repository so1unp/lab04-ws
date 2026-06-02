#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <ncurses.h>

struct Nave {
    int oxigeno;
    int combustible;
    int posX, posY;
    int bodegaMinerales[4];
    pthread_mutex_t mutex;
    pthread_mutex_t mutex_pantalla; // <-- Punto final eliminado
};

void* hilo_soporte_vital(void* arg);
void* hilo_propulsion(void* arg);
void* hilo_extraccion(void* arg);
void* hilo_radar(void* arg);

int main(void) {
    pthread_t id_hilo_radar;
    struct Nave mi_nave;

    mi_nave.oxigeno = 100;
    mi_nave.combustible = 100;
    mi_nave.posX = 0;
    mi_nave.posY = 0;
    mi_nave.bodegaMinerales[0] = 0;
    mi_nave.bodegaMinerales[1] = 0;
    mi_nave.bodegaMinerales[2] = 0;
    mi_nave.bodegaMinerales[3] = 0;
   
    // 1. Inicializamos herramientas y mutex ANTES de crear el hilo
    initscr();
    pthread_mutex_init(&mi_nave.mutex, NULL);
    pthread_mutex_init(&mi_nave.mutex_pantalla, NULL);
    
    // 2. Creamos el hilo
    pthread_create(&id_hilo_radar, NULL, hilo_radar, &mi_nave);

    // 3. Esperamos a que termine
    pthread_join(id_hilo_radar, NULL);
    
    // 4. Destruimos los mutex en líneas separadas
    pthread_mutex_destroy(&mi_nave.mutex);
    pthread_mutex_destroy(&mi_nave.mutex_pantalla);

    // 5. Cerramos ncurses
    endwin();
   
    exit(EXIT_SUCCESS);
}

void* hilo_radar(void* arg) {
    struct Nave* nave = (struct Nave*) arg;
    int counter = 0;
    int fila = 1, columna = 1; // Posición inicial

    while (counter < 100) {
        
        // Bloqueo de pantalla
        pthread_mutex_lock(&nave->mutex_pantalla);
        clear();
        
        // Bloqueo de datos de la nave
        pthread_mutex_lock(&nave->mutex);
        
        // Impresión en coordenadas (Y, X) incrementando la fila (Y) y sin saltos de línea (\n)
        mvprintw(fila, columna, "Oxígeno: %d", nave->oxigeno);
        mvprintw(fila + 1, columna, "Combustible: %d", nave->combustible);
        mvprintw(fila + 2, columna, "Posición X: %d", nave->posX);
        mvprintw(fila + 3, columna, "Posición Y: %d", nave->posY);
        mvprintw(fila + 4, columna, "Minerales 1: %d", nave->bodegaMinerales[0]);
        mvprintw(fila + 5, columna, "Minerales 2: %d", nave->bodegaMinerales[1]);
        mvprintw(fila + 6, columna, "Minerales 3: %d", nave->bodegaMinerales[2]);
        mvprintw(fila + 7, columna, "Minerales 4: %d", nave->bodegaMinerales[3]);
        mvprintw(fila + 8, columna, "Tiempo: %d segundos", counter);
       
        // Desbloqueo de datos
        pthread_mutex_unlock(&nave->mutex);
        
        // Volcado a pantalla
        refresh();
        
        // Desbloqueo de pantalla
        pthread_mutex_unlock(&nave->mutex_pantalla);
        
        // El hilo descansa fuera de los bloqueos
        usleep(100000);
        
        counter++;
    }
    return NULL;
}