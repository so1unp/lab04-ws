#include <mqueue.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "comun.h"

// para ser mas claro para la generacion de mapa el 0 es para vacio, 1 para el
// jugador, 2 para la nave y 3 para asteroides
// hilos
void *hilo_grafico(void *arg);

void *hilo_cola_respawn(void *arg);

void *hilo_cola_movimiento(void *arg);
// funciones
void rellenarMapa(struct Mapa *arg);
void inicializarVentanas(struct Mapa *arg);
void inicializarHilos(struct Mapa *arg);
struct MensajeServidor respawnNave(struct Mapa *arg, pid_t pid);
struct MensajeServidor moverNave(struct Mapa *arg,
                                 struct MensajeMovimiento msg);

int main(int argc, char *argv[]) {
  struct Mapa mapa;

  rellenarMapa(&mapa);
  inicializarVentanas(&mapa);

  pthread_t hiloGrafico, hiloRespawn, hiloMovimiento;

  if (pthread_create(&hiloGrafico, NULL, hilo_grafico, &mapa) != 0) {
    perror("pthread_create hilo_grafico");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloRespawn, NULL, hilo_cola_respawn, &mapa) != 0) {
    perror("pthread_create hilo_cola_respawn");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloMovimiento, NULL, hilo_cola_movimiento, &mapa) != 0) {
    perror("pthread_create hilo_cola_movimiento");
    exit(EXIT_FAILURE);
  }
  pthread_join(hiloGrafico, NULL);
  pthread_join(hiloRespawn, NULL);
  pthread_join(hiloMovimiento, NULL);

  void rellenarMapa(struct Mapa * arg);
  exit(EXIT_SUCCESS);
}
// temporal,solo por ahora, despues se reemplazaria por la logica de
// inicializacion del mapa
void rellenarMapa(struct Mapa *mapa) {
  memset(mapa->MatrizMapa, 0, sizeof(mapa->MatrizMapa));
  mapa->cant_naves = 0;

  // Inicializar asteroides
  for (int i = 0; i < ASTEROIDE_MAX_SV; i++) {
    int x, y;
    do {
      x = rand() % VENTANA_SIZE_X;
      y = rand() % VENTANA_SIZE_Y;
    } while (mapa->MatrizMapa[y][x] != 0); // si está ocupado, vuelve a elegir

    mapa->asteroides[i].posX = x;
    mapa->asteroides[i].posY = y;
    mapa->asteroides[i].ancho = 1;
    mapa->asteroides[i].largo = 1;
    mapa->asteroides[i].minerales = 100;
    mapa->MatrizMapa[y][x] = ASTEROIDE;
  }

  // Inicializar estaciones
  for (int i = 0; i < ESTACION_MAX_SV; i++) {
    int x, y;
    do {
      x = rand() % 25;
      y = rand() % 25;
    } while (mapa->MatrizMapa[y][x] != 0);

    // borrar despues
    mapa->estaciones[i].posX = x;
    mapa->estaciones[i].posY = y;
    mapa->estaciones[i].ancho = 1;
    mapa->estaciones[i].largo = 1;
    mapa->MatrizMapa[y][x] = ESTACION;
  }
}
void *hilo_grafico(void *arg) {
  struct Mapa *mapa = (struct Mapa *)arg;

  while (1) {
    pthread_mutex_lock(&mapa->mutex_grafico);

    werase(mapa->grafico.ventanaHud);
    box(mapa->grafico.ventanaHud, 0, 0);

    werase(mapa->grafico.ventana);
    box(mapa->grafico.ventana, 0, 0);

    for (int y = 0; y < VENTANA_SIZE_Y; y++) {
      for (int x = 0; x < VENTANA_SIZE_X; x++) {
        switch (mapa->MatrizMapa[y][x]) {
        case NAVE:
          mvwprintw(mapa->grafico.ventana, y, x, "x");
          break;
        case ASTEROIDE:
          mvwprintw(mapa->grafico.ventana, y, x, "*");
          break;
        case ESTACION:
          mvwprintw(mapa->grafico.ventana, y, x, "E");
          break;
        }
      }
    }

    wrefresh(mapa->grafico.ventana);
    wrefresh(mapa->grafico.ventanaHud);

    pthread_mutex_unlock(&mapa->mutex_grafico);

    usleep(100000);
  }

  return NULL;
}

void inicializarVentanas(struct Mapa *mapa) {
  initscr();
  cbreak();
  noecho();
  curs_set(FALSE);
  keypad(stdscr, TRUE);

  // ventana principal del jugador
  mapa->grafico.ventana = newwin(VENTANA_SIZE_Y, // alto
                                 VENTANA_SIZE_X, // ancho
                                 0,              // y
                                 0);             // x
  box(mapa->grafico.ventana, 0, 0);
  wrefresh(mapa->grafico.ventana);

  // ventana de estadisticas
  mapa->grafico.ventanaHud =
      newwin(10,             // alto
             VENTANA_SIZE_X, // ancho
             VENTANA_SIZE_Y, // para que este abajo de la ventana principal
             0);             // x (al lado del mapa)
  box(mapa->grafico.ventanaHud, 0, 0);
  wrefresh(mapa->grafico.ventanaHud);
}

void *hilo_cola_respawn(void *arg) {
  struct Mapa *mapa = (struct Mapa *)arg;
  struct MensajeConexion msg;

  fprintf(stderr, "[RESPAWN] Hilo iniciado\n");
  // formato de la cola de respawn servior y abirla

  struct mq_attr attr = {0, 10, sizeof(struct MensajeConexion), 0};

  mqd_t mq_conexiones =
      mq_open(NOMBRE_COLA_SERVIDOR_RESPAWN, O_CREAT | O_RDONLY, 0644, &attr);

  if (mq_conexiones == (mqd_t)-1) {
    exit(1);
  }

  // loop spawn recibe la id de la nave y devuelve la matriz del mapa

  while (1) {
    // espero las id de las naves
    if (mq_receive(mq_conexiones, (char *)&msg, sizeof(msg), NULL) == -1) {
      perror("[RESPAWN] mq_receive");
      continue;
    }

    pthread_mutex_lock(&mapa->mutex_grafico);

    // armo la respuesta para enviar a la nave
    struct MensajeServidor respuesta = respawnNave(mapa, msg.pid);

    pthread_mutex_unlock(&mapa->mutex_grafico);

    char nombre_cola[64];
    // armo nombre de la cola a la que tengo que enviar
    snprintf(nombre_cola, sizeof(nombre_cola), NOMBRE_NAVE_RESPAWN, msg.pid);

    mqd_t mq_nave = mq_open(nombre_cola, O_WRONLY);

    if (mq_nave == (mqd_t)-1) {
      perror("[RESPAWN] mq_open nave");
      continue;
    }

    if (mq_send(mq_nave, (char *)&respuesta, sizeof(respuesta), 0) == -1) {
      perror("[RESPAWN] mq_send");
    }

    mq_close(mq_nave);
  }

  return NULL;
}

// void *hilo_cola_movimiento(void *arg) {
//   struct Mapa *mapa = (struct Mapa *)arg;
//   struct MensajeMovimiento msg;

//   fprintf(stderr, "[Movimiento] Hilo iniciado\n");

//   struct mq_attr attr = {0, 10, sizeof(struct MensajeConexion), 0};

//   mqd_t mq_conexiones =
//       mq_open(NOMBRE_COLA_SERVIDOR_MOVIMIENTO, O_CREAT | O_RDONLY, 0644, &attr);

//   if (mq_conexiones == (mqd_t)-1) {
//     exit(1);
//   }

//   // loop spawn recibe la id de la nave y devuelve la matriz del mapa

//   while (1) {
//     // espero las id de las naves
//     if (mq_receive(mq_conexiones, (char *)&msg, sizeof(msg), NULL) == -1) {
//       perror("[RESPAWN] mq_receive");
//       continue;
//     }

//     pthread_mutex_lock(&mapa->mutex_grafico);

//     // armo la respuesta para enviar a la nave
//     struct MensajeServidor respuesta = moverNave(mapa, msg);

//     pthread_mutex_unlock(&mapa->mutex_grafico);

//     char nombre_cola[64];
//     // armo nombre de la cola a la que tengo que enviar
//     snprintf(nombre_cola, sizeof(nombre_cola), NOMBRE_NAVE_MOVIMIENTO, msg.pid);

//     mqd_t mq_nave = mq_open(nombre_cola, O_WRONLY);

//     if (mq_nave == (mqd_t)-1) {
//       perror("[RESPAWN] mq_open nave");
//       continue;
//     }

//     if (mq_send(mq_nave, (char *)&respuesta, sizeof(respuesta), 0) == -1) {
//       perror("[RESPAWN] mq_send");
//     }

//     mq_close(mq_nave);
//   }

//   return NULL;
// }
struct MensajeServidor respawnNave(struct Mapa *mapa, pid_t pid) {
  struct MensajeServidor respuesta;

  int i = mapa->cant_naves;
  int estacion_Comienzo = rand() % ESTACION_MAX_SV;
  int posX = mapa->estaciones[estacion_Comienzo].posX;
  int posY = mapa->estaciones[estacion_Comienzo].posY;

  mapa->naves_conectadas[i].pid = pid;
  mapa->naves_conectadas[i].posX = posX;
  mapa->naves_conectadas[i].posY = posY;
  mapa->MatrizMapa[posY][posX] = NAVE;

  mapa->cant_naves++;
  memcpy(respuesta.MatrizMapa, mapa->MatrizMapa, sizeof(mapa->MatrizMapa));
  respuesta.posX = posX;
  respuesta.posY = posY;
  return respuesta;
}
// aca calculo la nueva pos para devolver el movimiento a la nave
struct MensajeServidor moverNave(struct Mapa *mapa,
                                 struct MensajeMovimiento msg) {
  int naveExiste = -1;
  int nuevaPosX = msg.posX;
  int nuevaPosY = msg.posY;

  for (int i = 0; i < mapa->cant_naves; i++) {
    if (mapa->naves_conectadas[i].pid == msg.pid) {
      naveExiste = i;
      break;
    }
  }
  if (naveExiste == -1) {
    perror("NAVE NO EXISTE");
    exit(EXIT_FAILURE);
  }
  struct MensajeServidor respuesta;

  // si choco con los vordes volteo y aparece del otro lado
  if (nuevaPosX >= VENTANA_SIZE_X)
    nuevaPosX = 0;
  if (nuevaPosX < 0)
    nuevaPosX = VENTANA_SIZE_X - 1;
  if (nuevaPosY >= VENTANA_SIZE_Y)
    nuevaPosY = 0;
  if (nuevaPosY < 0)
    nuevaPosY = VENTANA_SIZE_Y - 1;

  // Si hay otra nave, no deja avanzar
  int lugarMatriz = mapa->MatrizMapa[nuevaPosY][nuevaPosX];
  if (lugarMatriz != NAVE) {

    // borra posición vieja
    mapa->MatrizMapa[mapa->naves_conectadas[naveExiste].posY]
                    [mapa->naves_conectadas[naveExiste].posX] = 0;

    // actualiza posición
    mapa->naves_conectadas[naveExiste].posX = nuevaPosX;
    mapa->naves_conectadas[naveExiste].posY = nuevaPosY;

    // escribe posición nueva
    mapa->MatrizMapa[nuevaPosY][nuevaPosX] = NAVE;
  }
  // armo respuesta
  memcpy(respuesta.MatrizMapa, mapa->MatrizMapa, sizeof(mapa->MatrizMapa));
  respuesta.posX = mapa->naves_conectadas[naveExiste].posX;
  respuesta.posY = mapa->naves_conectadas[naveExiste].posY;
  return respuesta;
}
// crear la extructura logica mapa
// crear asteroides
// dibujar mapa
// memoria compartida
// mailbox+
void *hilo_cola_movimiento(void *arg) {
  struct Mapa *mapa = (struct Mapa *)arg;
  struct MensajeMovimiento msg;

  fprintf(stderr, "[SV-MOV] Hilo iniciado\n");

  struct mq_attr attr = {0, 10, sizeof(struct MensajeMovimiento), 0};
  mqd_t mq_conexiones =
      mq_open(NOMBRE_COLA_SERVIDOR_MOVIMIENTO, O_CREAT | O_RDONLY, 0644, &attr);

  if (mq_conexiones == (mqd_t)-1) {
    perror("[SV-MOV] mq_open");
    exit(1);
  }
  fprintf(stderr, "[SV-MOV] cola abierta, esperando mensajes...\n");

  while (1) {
    if (mq_receive(mq_conexiones, (char *)&msg, sizeof(msg), NULL) == -1) {
      perror("[SV-MOV] mq_receive");
      continue;
    }
    fprintf(stderr, "[SV-MOV] recibido pid=%d destino=(%d,%d)\n", msg.pid, msg.posX, msg.posY);

    pthread_mutex_lock(&mapa->mutex_grafico);
    struct MensajeServidor respuesta = moverNave(mapa, msg);
    pthread_mutex_unlock(&mapa->mutex_grafico);

    fprintf(stderr, "[SV-MOV] nueva pos=(%d,%d)\n", respuesta.posX, respuesta.posY);

    char nombre_cola[64];
    snprintf(nombre_cola, sizeof(nombre_cola), NOMBRE_NAVE_MOVIMIENTO, msg.pid);
    fprintf(stderr, "[SV-MOV] abriendo cola respuesta: %s\n", nombre_cola);

    mqd_t mq_nave = mq_open(nombre_cola, O_WRONLY);

    if (mq_nave == (mqd_t)-1) {
      perror("[SV-MOV] mq_open nave");
      continue;
    }

    if (mq_send(mq_nave, (char *)&respuesta, sizeof(respuesta), 0) == -1) {
      perror("[SV-MOV] mq_send");
    } else {
      fprintf(stderr, "[SV-MOV] respuesta enviada\n");
    }

    mq_close(mq_nave);
  }

  return NULL;
}