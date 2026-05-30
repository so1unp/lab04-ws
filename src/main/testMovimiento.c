#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <pthread.h>

struct Nave {
    int oxigeno;
    int combustible;
    int posX, posY;
    int bodegaMinerales[4];
    pthread_mutex_t mutex;
};

extern struct Nave nave;

void *hilo_propulsion(void *arg);
void *hilo_soporte_vital(void *arg);
void *hilo_extraccion(void *arg);
void *hilo_radar(void *arg);

int main(void) {
    nave.posX        = 10;
    nave.posY        = 5;
    nave.combustible = 100;
    nave.oxigeno     = 100;
    pthread_mutex_init(&nave.mutex, NULL);

    // INicializo hilos
    initscr();
    cbreak();
    noecho();

    pthread_t t_propulsion, t_soporte, t_extraccion, t_radar;
    pthread_create(&t_propulsion, NULL, hilo_propulsion,   NULL);
    pthread_create(&t_soporte,   NULL, hilo_soporte_vital, NULL);
    pthread_create(&t_extraccion,NULL, hilo_extraccion,    NULL);
    pthread_create(&t_radar,     NULL, hilo_radar,         NULL);
    //loop principal
    while (1) {
        clear();
        mvaddch(nave.posY, nave.posX, 'X');
        mvprintw(0, 0, "Pos(%d,%d) combustible:%d ",
                 nave.posX, nave.posY, nave.combustible);
        refresh();
        napms(50);  // refresca la pantalla cada 50ms
    }

    endwin();
    //destruyo los hilos de la nave
    pthread_mutex_destroy(&nave.mutex);
    exit(EXIT_SUCCESS);
}