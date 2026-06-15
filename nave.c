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

WINDOW *win;

struct Nave{
    int oxigeno;
    int combustible;
    int posX, posY;
    int velocidadMovimiento; // rocio
    int combustibleGastadoMovimiento; // rocio
    int bodegaMinerales[4];
    sem_t *sem_mutex;
    int ancho, largo;
    sem_t *mutex_pantalla; // mutex para sincronizar acceso a la pantalla
};
struct Asteroides {
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


/*hilos del cliente nave*/
void* hilo_soporte_vital(void* arg);
void* hilo_propulsion(void* arg);
void* hilo_extraccion(void* arg);
void* hilo_radar(void* arg);
void *hilo_grafico(void *arg);
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
    /*inicializa los hilos*/
    //pthread_t hiloExtraccion;
    pthread_t hiloOxigeno;

    //la cantidad de naves es 
    /*Inicializa la nave*/
    nave.oxigeno = 100;
    nave.combustible = 100;
    nave.posX = 5;
    nave.posY = 5;
    nave.velocidadMovimiento=1;
    nave.combustibleGastadoMovimiento=1;
    for (int i = 0; i < 4; i++) {
        nave.bodegaMinerales[i] = 0;
    }
    
    sem_unlink("mutex_nave"); // Elimina el semáforo si ya existe
    sem_unlink("mutex_pantalla"); // Elimina el semáforo de pantalla si ya existe
    
    /*inicializa el mutex*/
    if ((nave.sem_mutex = sem_open( "mutex_nave", O_CREAT, 0666, 1)) == (sem_t *) -1) {
        perror("No se pudo crear el semáforo");
        exit(EXIT_FAILURE);
    }
    if ((nave.mutex_pantalla = sem_open("mutex_pantalla", O_CREAT, 0666, 1)) == (sem_t *) -1) {
        perror("No se pudo crear el semáforo para la pantalla");
        exit(EXIT_FAILURE);
    }
    

    initscr();
    cbreak();
    noecho();
    curs_set(FALSE);
    keypad(stdscr, TRUE);

    win = newwin(5, 40, 5, 5);
    if (win == NULL) {
        endwin();
        perror("No se pudo crear la ventana");
        exit(EXIT_FAILURE);
    }

    box(win, 0, 0);
    mvwprintw(win, 1, 1, "Oxigeno: %3d", nave.oxigeno);
    wrefresh(win);

    int rc;
    
    rc = pthread_create(&hiloOxigeno, NULL, hilo_soporte_vital, &nave);
    if (rc != 0) {
        endwin();
        perror("pthread_create hilo_soporte_vital");
        exit(EXIT_FAILURE);
    }

    //pthread_join(hiloExtraccion, NULL); // Espera a que el hilo de extracción termine (en este caso, no terminará).
    pthread_join(hiloOxigeno, NULL);
    //endwin(); // finaliza ncurses
    //sem_close(nave.sem_mutex); // Cierra el semáforo mutex

    //endwin();               // finaliza ncurses
    // Termina la ejecución del programa.
    // este es el loop principal de movimiento,imprime la x segun el movimiento de
  // las teclas
  /*INICIO TEST MOVIMIENTO*/
  /*Creo ventana*/
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  WINDOW *ventana = newwin(10, // alto
                           10, // ancho
                           1,  // fila inicial (y)
                           1);

  box(ventana, 0, 0); // Dibuja bordes
  // inicializo hilo movimiento
  pthread_t hiloMovimiento;
  // creo hilo movimiento
  int errorHiloMovimiento =
      pthread_create(&hiloMovimiento, NULL, hilo_propulsion, &nave);
  if (errorHiloMovimiento != 0) {
    perror("pthread_create");
    exit(EXIT_FAILURE);
  }
  

  while (nave.combustible>0) {
    werase(ventana);
    box(ventana, 0, 0);
    mvwprintw(ventana, nave.posY, nave.posX, "X");
    wrefresh(ventana);
    
    mvprintw(0, 0, "Pos(%d,%d) combustible:%d ",
             nave.posX, nave.posY, nave.combustible);
    refresh();
    napms(20);
  }
  //--- parte luis ----
  pthread_t hiloRadar;
  int errorHiloRadar = pthread_create(&hiloRadar, NULL, hilo_radar, &nave);
  if (errorHiloRadar != 0) {
    perror("pthread_create");
    exit(EXIT_FAILURE);
  }
    pthread_join(hiloRadar, NULL);
    endwin(); // finaliza ncurses
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


//este hilo soporte
void* hilo_soporte_vital(void* arg){
    struct Nave* nave = (struct Nave*) arg;

    while (1) {
        sem_wait(nave->sem_mutex);
        if (nave->oxigeno <= 0) {
            nave->oxigeno = 0;
            sem_post(nave->sem_mutex);
            break;
        }

        nave->oxigeno -= 5;
        if (nave->oxigeno < 0) {
            nave->oxigeno = 0;
        }

        int oxigeno_actual = nave->oxigeno;
        sem_post(nave->sem_mutex);

        werase(win);
        box(win, 0, 0);
        mvwprintw(win, 1, 1, "Oxigeno: %3d", oxigeno_actual);
        wrefresh(win);

        sleep(1);
    }

    werase(win);
    box(win, 0, 0);
    mvwprintw(win, 1, 1, "Oxigeno:   0");
    mvwprintw(win, 2, 1, "GAME OVER");
    wrefresh(win);

    return NULL;
}

//----- parte rocio ----

/*metodo importante aca primero me fijo si hay combustible, en el futuro supongo
que si se queda sin combustible game over. bloquea cuando se mueve si hay
combustible, actualiza posicion resta combustible configurado desde la variable
global y luego desbloquea
*/
void movimientoPorTecla(void *arg, int xPos, int yPos) {
  struct Nave *nave = arg;
  if (nave->combustible == 0) {
    printf("Combustible agotado,no puedo moverme");

    return;
  }

  sem_wait(nave->sem_mutex);
  nave->posX = xPos;
  nave->posY = yPos;
  nave->combustible -= nave->combustibleGastadoMovimiento;
  sem_post(nave->sem_mutex);
  return;
}
/*Aca solo se ve que tecla para mandar las nuevas coordenadas actualizadas, se
 * le suma/resta la velocidad de movimiento*/
void *hilo_propulsion(void *arg) {
  struct Nave *nave = arg;
  // w a s d
  int tecla;

  while (1) {
    tecla = getch();
    switch (tecla) {
    case 'w':
      movimientoPorTecla(nave, nave->posX,
                         nave->posY - nave->velocidadMovimiento);
      break;
    case 's':
      movimientoPorTecla(nave, nave->posX,
                         nave->posY + nave->velocidadMovimiento);
      break;
    case 'a':
      movimientoPorTecla(nave, nave->posX - nave->velocidadMovimiento,
                         nave->posY);
      break;
    case 'd':
      movimientoPorTecla(nave, nave->posX + nave->velocidadMovimiento,
                         nave->posY);
      break;
    }
  }
  return NULL;
}

void* hilo_radar(void *arg) {
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
        sem_wait(nave->mutex_pantalla);
        clear();

        
        // --- DIBUJO DEL MAPA ---
        for(int i = OFFSET_X; i < OFFSET_X + ancho_mapa; i++){
            mvprintw(OFFSET_Y, i, "-");
        }
        
        for(int j = OFFSET_Y; j < OFFSET_Y + alto_mapa; j++){
            mvprintw(j, OFFSET_X, "|");
        } 
        
        sem_wait(nave->sem_mutex); // Bloqueo de datos
        
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
        sem_post(nave->sem_mutex);
        refresh();
        
        // Desbloqueo de pantalla
        sem_post(nave->mutex_pantalla);
        
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