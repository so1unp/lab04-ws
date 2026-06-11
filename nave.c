#include "comun.h"
#include <fcntl.h>
#include <mqueue.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*hilos del cliente nave*/
void *hilo_soporte_vital(void *arg);
void *hilo_propulsion(void *arg);
void *hilo_extraccion(void *arg);
void *hilo_grafico_nave(void *arg);
/*funciones*/
int trueque_estacion(struct Nave *nave);
int minerales_totales(struct Nave *nave);
void inicializarVentanas_nave(struct Nave *arg);
void inicializarHilos_nave(struct Nave *arg);

int calcular_total_minerales(struct Nave *nave) {
  int total = 0;
  for (int i = 0; i < 4; i++) {
    total += nave->bodegaMinerales[i];
  }
  return total;
}
int minerales_totales(struct Nave *nave) {
  int total = 0;
  for (int i = 0; i < 4; i++) {
    total += nave->bodegaMinerales[i];
  }
  return total;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  struct Nave nave;

  // Inicializar nave

  nave.oxigeno = 100;
  nave.combustible = 100;
  nave.posX = 5;
  nave.posY = 5;
  nave.velocidadMovimiento = 1;
  nave.combustibleGastadoMovimiento = 1;
  nave.ancho = 1;
  nave.largo = 1;
  // esto de aca inicializa el array en puros 0
  memset(nave.bodegaMinerales, 0, sizeof(nave.bodegaMinerales));
  // temporal, eliminar cuando se conecte el mailbox
  memset(nave.MatrizMapa, 0, sizeof(nave.MatrizMapa));
  nave.MatrizMapa[nave.posX][nave.posY] = NAVE;

  // SEMAFOROS
  sem_unlink("mutex_nave");
  sem_unlink("mutex_pantalla");
  if ((nave.sem_mutex = sem_open("mutex_nave", O_CREAT, 0666, 1)) ==
      (sem_t *)-1) {
    perror("sem_open mutex_nave");
    exit(EXIT_FAILURE);
  }
  if ((nave.mutex_pantalla = sem_open("mutex_pantalla", O_CREAT, 0666, 1)) ==
      (sem_t *)-1) {
    perror("sem_open mutex_pantalla");
    exit(EXIT_FAILURE);
  }
  // COLA MENSAJES
  //  crear cola propia para recibir
  char nombre_cola_nave[64];
  snprintf(nombre_cola_nave, sizeof(nombre_cola_nave),NOMBRE_COLA_NAVE_SERVIDOR,
           getpid());

  struct mq_attr attr_nave = {0, 10, sizeof(struct MensajeServidor), 0};
  mqd_t mq_nave =
      mq_open(nombre_cola_nave, O_CREAT | O_RDONLY, 0644, &attr_nave);
  if (mq_nave == (mqd_t)-1) {
    perror("mq_open nave");
    exit(EXIT_FAILURE);
  }

  // notificar sv que se creo una nueva nave
  struct MensajeConexion conexion;
  conexion.pid = getpid();

  mqd_t mq_servidor = mq_open(NOMBRE_COLA_SERVIDOR, O_WRONLY);
  if (mq_servidor == (mqd_t)-1) {
    perror("mq_open servidor");
    exit(EXIT_FAILURE);
  }

  mq_send(mq_servidor, (char *)&conexion, sizeof(conexion), 0);
  mq_close(mq_servidor);

  // GRAFICOS
  inicializarVentanas_nave(&nave);
  // HILOS
  pthread_t hiloSoporteVital, hiloPropulsion, hiloExtraccion, hiloGrafico;

  if (pthread_create(&hiloSoporteVital, NULL, hilo_soporte_vital, &nave) != 0) {
    perror("pthread_create hilo_soporte_vital");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloPropulsion, NULL, hilo_propulsion, &nave) != 0) {
    perror("pthread_create hilo_propulsion");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloExtraccion, NULL, hilo_extraccion, &nave) != 0) {
    perror("pthread_create hilo_extraccion");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloGrafico, NULL, hilo_grafico_nave, &nave) != 0) {
    perror("pthread_create hilo_grafico");
    exit(EXIT_FAILURE);
  }

  // Loop principal — termina cuando muere la nave
  while (nave.oxigeno > 0 && nave.combustible > 0) {
    napms(100);
  }

  // Game over
  sem_wait(nave.mutex_pantalla);
  mvprintw(0, 0, "GAME OVER - oxigeno:%d combustible:%d", nave.oxigeno,
           nave.combustible);
  refresh();
  sem_post(nave.mutex_pantalla);
  napms(2000);

  // Cleanup
  pthread_cancel(hiloSoporteVital);
  pthread_cancel(hiloPropulsion);
  pthread_cancel(hiloExtraccion);
  pthread_cancel(hiloGrafico);

  pthread_join(hiloSoporteVital, NULL);
  pthread_join(hiloPropulsion, NULL);
  pthread_join(hiloExtraccion, NULL);
  pthread_join(hiloGrafico, NULL);

  sem_close(nave.sem_mutex);
  sem_close(nave.mutex_pantalla);
  sem_unlink("mutex_nave");
  sem_unlink("mutex_pantalla");

  endwin();
  exit(EXIT_SUCCESS);
}

// /**
//  * Función para realizar el trueque en la estación espacial.
//  * Esta función se encarga de enviar los recursos de la nave a la estación
//  espacial a través de una cola de mensajes y recibir los recursos necesarios
//  a cambio.
//  *
//  */

/**
 * Función para el hilo de extracción de minerales.
 * Esta función simula la extracción de minerales de asteroides adyacentes, el
 * consumo de combustible y la actualización de la bodega de minerales de la
 * nave.
 */
void *hilo_extraccion(void *arg) {
  struct Nave *nave = (struct Nave *)arg;
  // Simula la extracción de minerales y el consumo de combustible

  srand((unsigned int)time(
      NULL)); // Inicializa la semilla para la generación de números aleatorios
  int asteroideAdyacente = 1;

  while (1) {
    sleep(2); // Simula el tiempo de extracción

    /* verifica si hay un asteroide adyacente */
    if (!asteroideAdyacente) {
      continue; // Intenta de nuevo en la siguiente iteración
    }

    sem_wait(nave->sem_mutex); // --- BLOQUEO ---

    if (nave->combustible <= 0) {
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

    sem_post(nave->sem_mutex); // --- DESBLOQUEO ---
  }
  return NULL;
}

void *hilo_soporte_vital(void *arg) {
  struct Nave *nave = (struct Nave *)arg;

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

    sem_post(nave->sem_mutex);

    sleep(1);
  }

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
  // borro la posicion vieja de la matriz
  nave->MatrizMapa[nave->posY][nave->posX] = 0;
  nave->posX = xPos;
  nave->posY = yPos;
  // escribo la nueva
  nave->MatrizMapa[nave->posY][nave->posX] = NAVE;

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
// version de hilo grafico para la nave, se fusiono radar con el nuevo
// hilografico para manejar todo de una
void *hilo_grafico_nave(void *arg) {
  struct Nave *nave = (struct Nave *)arg;
  char arr[11];
  char arr2[11];

  while (1) {
    sem_wait(nave->sem_mutex);

    int mitad = (nave->oxigeno + 9) / 10;
    int mitad2 = (nave->combustible + 9) / 10;
    for (int k = 0; k < 10; k++)
      arr[k] = k < mitad ? '=' : ' ';
    for (int k = 0; k < 10; k++)
      arr2[k] = k < mitad2 ? '=' : ' ';
    arr[10] = arr2[10] = '\0';

    werase(nave->grafico.ventanaHud);
    box(nave->grafico.ventanaHud, 0, 0);
    mvwprintw(nave->grafico.ventanaHud, 1, 1, "Oxigeno:     [%s]", arr);
    mvwprintw(nave->grafico.ventanaHud, 2, 1, "Combustible: [%s]", arr2);
    mvwprintw(nave->grafico.ventanaHud, 3, 1, "Pos: (%2d, %2d)  ", nave->posX,
              nave->posY);
    mvwprintw(nave->grafico.ventanaHud, 4, 1, "Minerales:   %d",
              calcular_total_minerales(nave));
    wrefresh(nave->grafico.ventanaHud);

    werase(nave->grafico.ventana);
    box(nave->grafico.ventana, 0, 0);
    for (int y = 0; y < VENTANA_SIZE_Y; y++) {
      for (int x = 0; x < VENTANA_SIZE_X; x++) {
        switch (nave->MatrizMapa[y][x]) {
        case NAVE:
          mvwprintw(nave->grafico.ventana, y, x, "x");
          break;
        case ASTEROIDE:
          mvwprintw(nave->grafico.ventana, y, x, "*");
          break;
        case ESTACION:
          mvwprintw(nave->grafico.ventana, y, x, "E");
          break;
        }
      }
    }
    wrefresh(nave->grafico.ventana);

    sem_post(nave->sem_mutex);
    usleep(100000);
  }
  return NULL;
}
// misma que servidor
void inicializarVentanas_nave(struct Nave *nave) {
  initscr();
  cbreak();
  noecho();
  curs_set(FALSE);
  keypad(stdscr, TRUE);

  nave->grafico.ventana = newwin(VENTANA_SIZE_Y, VENTANA_SIZE_X, 0, 0);
  box(nave->grafico.ventana, 0, 0);
  wrefresh(nave->grafico.ventana);

  nave->grafico.ventanaHud = newwin(6, VENTANA_SIZE_X, VENTANA_SIZE_Y, 0);
  box(nave->grafico.ventanaHud, 0, 0);
  wrefresh(nave->grafico.ventanaHud);
}
