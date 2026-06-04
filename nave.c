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
    pthread_mutex_t mutex_pantalla;
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
    curs_set(0); 
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
    int fila = 1, columna = 1; 
    
    
    int OFFSET_X = 30; 
    int OFFSET_Y = 2;
    int ancho_mapa = 40*3;
    int alto_mapa = 15*2;
    
    char arr[11];
    char arr2[11];
    arr2[10] = '\0';
    arr[10] = '\0';
    

    while (counter < 100) {

        // Bloqueo de pantalla
        pthread_mutex_lock(&nave->mutex_pantalla);
        clear();

        
        // --- DIBUJO DEL MAPA ---
        for(int i = OFFSET_X; i < OFFSET_X + ancho_mapa; i++){
            mvprintw(OFFSET_Y, i, "-");
        }
        
        for(int j = OFFSET_Y; j < OFFSET_Y + alto_mapa; j++){
            mvprintw(j, OFFSET_X, "|");
        } 
        
        pthread_mutex_lock(&nave->mutex);
        
        int mitad = (nave->oxigeno + 9) / 10;
        int mitad2 = (nave->combustible + 9) / 10;
        
        
        for(int k = 0; k < mitad2; k++){
            arr2[k] = '=';
        }
        arr2[mitad2] = '\0'; 

        
        for(int k = 0; k < mitad; k++){
            arr[k] = '=';
        }
        arr[mitad] = '\0'; 

        // --- DIBUJO DE ESTADÍSTICAS ---
        mvprintw(fila, columna, "Oxigeno: %s", arr);
        mvprintw(fila + 1, columna, "Combustible: %s", arr2);
        mvprintw(fila + 2, columna, "Posicion X: %d", nave->posX);
        mvprintw(fila + 3, columna, "Posicion Y: %d", nave->posY);
        mvprintw(fila + 4, columna, "Minerales 1: %d", nave->bodegaMinerales[0]);
        mvprintw(fila + 5, columna, "Minerales 2: %d", nave->bodegaMinerales[1]);
        mvprintw(fila + 6, columna, "Minerales 3: %d", nave->bodegaMinerales[2]);
        mvprintw(fila + 7, columna, "Minerales 4: %d", nave->bodegaMinerales[3]);
        mvprintw(fila + 8, columna, "Tiempo: %d segundos", counter);
       
        // --- DIBUJO DE LA NAVE ---
        mvprintw(nave->posY + OFFSET_Y + 1, nave->posX + OFFSET_X + 1, "A");

        // Desbloqueo de datos
        pthread_mutex_unlock(&nave->mutex);
        refresh();
        
        // Desbloqueo de pantalla
        pthread_mutex_unlock(&nave->mutex_pantalla);
        
        usleep(100000); // 0.1 segundos
        
        counter++;
        
        // Mocking
        nave->posX++;
        if (counter % 3 == 0) {
            nave->posY++; 
        }
        nave->oxigeno --; 
        nave->combustible --; 
    }
    return NULL;
}