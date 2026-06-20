#include "comun.h"
#include <fcntl.h>
#include <mqueue.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
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
void inicializar_nave(struct Nave *nave);
void gameOver_nave(struct Nave *nave);
void enviar_movimiento(struct Nave *nave, int newPosX, int newPosY);

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
  struct MatrizCompartida *shm;

  int fd = shm_open(NOMBRE_SHM_MAPA, O_RDWR, 0666); // sin O_CREAT
  if (fd == -1) {
    perror("shm_open");
    exit(1);
  }

  shm = mmap(NULL, sizeof(struct MatrizCompartida), PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  // Inicializar nave

  nave.oxigeno = 100;
  nave.combustible = 100;
  nave.velocidadMovimiento = 1;
  nave.combustibleGastadoMovimiento = 1;
  nave.ancho = 1;
  nave.largo = 1;
  memset(nave.bodegaMinerales, 0, sizeof(nave.bodegaMinerales));

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

  //

  // GRAFICOS
  struct ArgsNave args = {&nave, shm};
  inicializarVentanas_nave(&nave);
  // RECIBO MAPA SERVIDOR
  inicializar_nave(&nave);
  // esperar a que el servidor me coloque
  int found = 0;

  while (!found) {
    for (int y = 0; y < VENTANA_SIZE_Y; y++) {
      for (int x = 0; x < VENTANA_SIZE_X; x++) {
        if (shm->MatrizMapa[y][x].pid_nave == getpid()) {
          nave.posX = x;
          nave.posY = y;
          found = 1;
          break;
        }
      }
      if (found)
        break;
    }
    usleep(100000);
  }
  // HILOS
  pthread_t hiloSoporteVital, hiloPropulsion, hiloExtraccion, hiloGrafico;

  if (pthread_create(&hiloSoporteVital, NULL, hilo_soporte_vital, &nave) != 0) {
    perror("pthread_create hilo_soporte_vital");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloPropulsion, NULL, hilo_propulsion, &args) != 0) {
    perror("pthread_create hilo_propulsion");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloExtraccion, NULL, hilo_extraccion, &nave) != 0) {
    perror("pthread_create hilo_extraccion");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloGrafico, NULL, hilo_grafico_nave, &args) != 0) {
    perror("pthread_create hilo_grafico");
    exit(EXIT_FAILURE);
  }

  // Loop principal — termina cuando muere la nave
  // en main, proteger la actualización de posición
  while (nave.oxigeno > 0 && nave.combustible > 0) {
    napms(100);
    found = 0;
    for (int y = 0; y < VENTANA_SIZE_Y && !found; y++) {
      for (int x = 0; x < VENTANA_SIZE_X; x++) {
        if (shm->MatrizMapa[y][x].pid_nave == getpid()) {
          sem_wait(nave.sem_mutex);
          nave.posX = x;
          nave.posY = y;
          sem_post(nave.sem_mutex);
          found = 1;
        }
      }
    }
  }

  // Game over
  gameOver_nave(&nave);
  sem_wait(nave.mutex_pantalla);
  mvprintw(0, 0, "GAME OVER - oxigeno:%d combustible:%d", nave.oxigeno,
           nave.combustible);
  refresh();
  sem_post(nave.mutex_pantalla);
  napms(20000);

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
  char nombre[64];

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
  if (nave->combustible == 0)
    return;
  enviar_movimiento(nave, xPos, yPos); 

  sem_wait(nave->sem_mutex);
  
  nave->combustible -= nave->combustibleGastadoMovimiento;

  sem_post(nave->sem_mutex);
}
/*Aca solo se ve que tecla para mandar las nuevas coordenadas actualizadas, se
 * le suma/resta la velocidad de movimiento*/
void *hilo_propulsion(void *arg) {
  struct ArgsNave *args = (struct ArgsNave *)arg;
  struct MatrizCompartida *shm = args->shm;
  struct Nave *nave = args->nave;
  // w a s d
  int tecla;

  while (1) {
    tecla = getch();
    mvprintw(0, 0, "tecla=%d   ", tecla);
    refresh();
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
void *hilo_grafico_nave(void *arg) {
  struct ArgsNave *args = (struct ArgsNave *)arg;
  struct MatrizCompartida *shm = args->shm;
  struct Nave *nave = args->nave;
  char arr[11];
  char arr2[11];
  int iteracion = 0;


  refresh();
  sleep(1);

  while (1) {
    refresh();

    sem_wait(nave->sem_mutex);
    refresh();

    
    int oxigeno = nave->oxigeno;
    int combustible = nave->combustible;
    int posX = nave->posX;
    int posY = nave->posY;
    int minerales = calcular_total_minerales(nave);

    sem_post(nave->sem_mutex);  
    refresh();

    int mitad = (oxigeno + 9) / 10;
    int mitad2 = (combustible + 9) / 10;
    for (int k = 0; k < 10; k++)
      arr[k] = k < mitad ? '=' : ' ';
    for (int k = 0; k < 10; k++)
      arr2[k] = k < mitad2 ? '=' : ' ';
    arr[10] = arr2[10] = '\0';

    mvprintw(2, 0, "[GRAFICO] ox=%d comb=%d pos=(%d,%d) min=%d     ",
             oxigeno, combustible, posX, posY, minerales);
    refresh();

    werase(nave->grafico.ventanaHud);
    box(nave->grafico.ventanaHud, 0, 0);
    mvwprintw(nave->grafico.ventanaHud, 1, 1, "Oxigeno:     [%s]", arr);
    mvwprintw(nave->grafico.ventanaHud, 2, 1, "Combustible: [%s]", arr2);
    mvwprintw(nave->grafico.ventanaHud, 3, 1, "Pos: (%2d, %2d)  ", posX, posY);
    mvwprintw(nave->grafico.ventanaHud, 4, 1, "Minerales:   %d", minerales);
    wrefresh(nave->grafico.ventanaHud);

    werase(nave->grafico.ventana);
    box(nave->grafico.ventana, 0, 0);
    for (int y = 0; y < VENTANA_SIZE_Y; y++) {
      for (int x = 0; x < VENTANA_SIZE_X; x++) {
        if (shm->MatrizMapa[y][x].pid_nave != -1) {
          mvwprintw(nave->grafico.ventana, y, x, "x");
        } else {
          switch (shm->MatrizMapa[y][x].estructuraMapa) {
          case ASTEROIDE:
            mvwprintw(nave->grafico.ventana, y, x, "*");
            break;
          case ESTACION:
            mvwprintw(nave->grafico.ventana, y, x, "E");
            break;
          }
        }
      }
    }
    wrefresh(nave->grafico.ventana);

    refresh();

    usleep(100000);
  }
  return NULL;
}
// version de hilo grafico para la nave, se fusiono radar con el nuevo
// hilografico para manejar todo de una
// void *hilo_grafico_nave(void *arg) {
//   struct ArgsNave *args = (struct ArgsNave *)arg;
//   struct MatrizCompartida *shm = args->shm;
//   struct Nave *nave = args->nave;
//   char arr[11];
//   char arr2[11];

//   while (1) {
//     sem_wait(nave->sem_mutex);

//     int mitad = (nave->oxigeno + 9) / 10;
//     int mitad2 = (nave->combustible + 9) / 10;
//     for (int k = 0; k < 10; k++)
//       arr[k] = k < mitad ? '=' : ' ';
//     for (int k = 0; k < 10; k++)
//       arr2[k] = k < mitad2 ? '=' : ' ';
//     arr[10] = arr2[10] = '\0';

//     werase(nave->grafico.ventanaHud);
//     box(nave->grafico.ventanaHud, 0, 0);
//     mvwprintw(nave->grafico.ventanaHud, 1, 1, "Oxigeno:     [%s]", arr);
//     mvwprintw(nave->grafico.ventanaHud, 2, 1, "Combustible: [%s]", arr2);
//     mvwprintw(nave->grafico.ventanaHud, 3, 1, "Pos: (%2d, %2d)  ", nave->posX,
//               nave->posY);
//     mvwprintw(nave->grafico.ventanaHud, 4, 1, "Minerales:   %d",
//               calcular_total_minerales(nave));
//     wrefresh(nave->grafico.ventanaHud);

//     werase(nave->grafico.ventana);
//     box(nave->grafico.ventana, 0, 0);
//     for (int y = 0; y < VENTANA_SIZE_Y; y++) {
//       for (int x = 0; x < VENTANA_SIZE_X; x++) {
//         if (shm->MatrizMapa[y][x].pid_nave != -1) {
//           mvwprintw(nave->grafico.ventana, y, x, "x");
//         } else {
//           switch (shm->MatrizMapa[y][x].estructuraMapa) {

//           case ASTEROIDE:
//             mvwprintw(nave->grafico.ventana, y, x, "*");
//             break;
//           case ESTACION:
//             mvwprintw(nave->grafico.ventana, y, x, "E");
//             break;
//           }
//         }
//       }
//     }
//     wrefresh(nave->grafico.ventana);

//     usleep(100000);
//   }
//   return NULL;
// }

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
// mailbox para el respawn (esta deveria morirse aca ya que solo se usa una
// vez)
void inicializar_nave(struct Nave *nave) {
  struct MensajeConexion msg;
  msg.pid = getpid();

  mqd_t mq_servidor = mq_open(NOMBRE_COLA_SERVIDOR_RESPAWN, O_WRONLY);
  if (mq_servidor == (mqd_t)-1) {
    perror("mq_open servidor respawn");
    exit(EXIT_FAILURE);
  }
  mq_send(mq_servidor, (char *)&msg, sizeof(msg), 0);
  mq_close(mq_servidor);
}

// Pal despawn
void gameOver_nave(struct Nave *nave) {

  struct MensajeConexion msg;
  msg.pid = getpid();

  mqd_t mq_servidor = mq_open(NOMBRE_COLA_SERVIDOR_RESPAWN, O_WRONLY);
  if (mq_servidor == (mqd_t)-1) {
    exit(EXIT_FAILURE);
  }

  if (mq_send(mq_servidor, (char *)&msg, sizeof(msg), 0) == -1) {
  }
  mq_close(mq_servidor);
}
//

void enviar_movimiento(struct Nave *nave, int newPosX, int newPosY) {
  struct MensajeMovimiento mensaje;
  mensaje.pid = getpid();
  mensaje.posX = newPosX;
  mensaje.posY = newPosY;

  mqd_t mq_servidor = mq_open(NOMBRE_COLA_SERVIDOR_MOVIMIENTO, O_WRONLY);
  if (mq_servidor == (mqd_t)-1) {
    perror("mq_open servidor movimiento");
    return;
  }
  mq_send(mq_servidor, (char *)&mensaje, sizeof(mensaje), 0);
  mq_close(mq_servidor);
}
