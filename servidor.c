#include "comun.h"
#include <fcntl.h>
#include <mqueue.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

// para ser mas claro para la generacion de mapa el 0 es para vacio, 1 para el
// jugador, 2 para la nave y 3 para asteroides

// hilos
void *hilo_grafico(void *arg);
void *hilo_cola_respawn(void *arg);
void *hilo_cola_movimiento(void *arg);

// funciones
struct NaveConectada *
containsNavesConectadas(struct NaveConectada naves_conectadas[], int size,
                        int valor);
void rellenarMapa(struct Mapa *mapa, struct MatrizCompartida *shm);
void inicializarVentanas(struct Mapa *arg);
void gameOver(struct ArgsMapa *args, struct NaveConectada *nave);
void respawnNave(struct ArgsMapa *args, pid_t pid);
void moverNave(struct ArgsMapa *args, struct MensajeMovimiento msg);

int main(int argc, char *argv[]) {
  struct Mapa mapa;

  // ACA SE INICIALIZA LA MATRIZ COMPARTIDA
  struct MatrizCompartida *shm;
  int fd = shm_open(NOMBRE_SHM_MAPA, O_CREAT | O_RDWR, 0666);
  ftruncate(fd, sizeof(struct MatrizCompartida));
  shm = mmap(NULL, sizeof(struct MatrizCompartida), PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
  close(fd);
  sem_init(&shm->mutex, 1, 1); // 1 = compartido entre procesos

  rellenarMapa(&mapa, shm);
  inicializarVentanas(&mapa);

  pthread_t hiloGrafico, hiloRespawn, hiloMovimiento;

  struct ArgsMapa args = {&mapa, shm};

  if (pthread_create(&hiloGrafico, NULL, hilo_grafico, &args) != 0) {
    perror("pthread_create hilo_grafico");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloRespawn, NULL, hilo_cola_respawn, &args) != 0) {
    perror("pthread_create hilo_cola_respawn");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloMovimiento, NULL, hilo_cola_movimiento, &args) != 0) {
    perror("pthread_create hilo_cola_movimiento");
    exit(EXIT_FAILURE);
  }

  pthread_join(hiloGrafico, NULL);
  pthread_join(hiloRespawn, NULL);
  pthread_join(hiloMovimiento, NULL);

  sem_destroy(&shm->mutex);
  munmap(shm, sizeof(struct MatrizCompartida));
  shm_unlink(NOMBRE_SHM_MAPA);

  endwin();
  exit(EXIT_SUCCESS);
}

// temporal,solo por ahora, despues se reemplazaria por la logica de
// inicializacion del mapa
void rellenarMapa(struct Mapa *mapa, struct MatrizCompartida *shm) {
  memset(shm->MatrizMapa, 0, sizeof(shm->MatrizMapa));
  mapa->cant_naves = 0;

  // Inicializar asteroides
  for (int i = 0; i < ASTEROIDE_MAX_SV; i++) {
    int x, y;
    do {
      x = rand() % VENTANA_SIZE_X;
      y = rand() % VENTANA_SIZE_Y;
    } while (shm->MatrizMapa[y][x].estructuraMapa !=
             0); // SI ESTA OCUPADO VUELVE A ELEGIR

    mapa->asteroides[i].posX = x;
    mapa->asteroides[i].posY = y;
    mapa->asteroides[i].ancho = 1;
    mapa->asteroides[i].largo = 1;
    mapa->asteroides[i].minerales = 100;
    shm->MatrizMapa[y][x].estructuraMapa = ASTEROIDE;
  }
  for (int y = 0; y < VENTANA_SIZE_Y; y++) {
    for (int x = 0; x < VENTANA_SIZE_X; x++) {
      shm->MatrizMapa[y][x].pid_nave = -1;
    }
  }

  // Inicializar estaciones
  for (int i = 0; i < ESTACION_MAX_SV; i++) {
    int x, y;
    do {
      x = rand() % VENTANA_SIZE_X;
      y = rand() % VENTANA_SIZE_Y;
    } while (shm->MatrizMapa[y][x].estructuraMapa != 0);

    mapa->estaciones[i].posX = x;
    mapa->estaciones[i].posY = y;
    mapa->estaciones[i].ancho = 1;
    mapa->estaciones[i].largo = 1;
    shm->MatrizMapa[y][x].estructuraMapa = ESTACION;
  }
}

void *hilo_grafico(void *arg) {
  struct ArgsMapa *args = (struct ArgsMapa *)arg;
  struct Mapa *mapa = args->mapa;
  struct MatrizCompartida *shm = args->shm;

  while (1) {
    
    struct LugarMatriz snapshot[VENTANA_SIZE_Y][VENTANA_SIZE_X];
    sem_wait(&shm->mutex);
    memcpy(snapshot, shm->MatrizMapa, sizeof(snapshot));
    sem_post(&shm->mutex); 

    werase(mapa->grafico.ventanaHud);
    box(mapa->grafico.ventanaHud, 0, 0);

    werase(mapa->grafico.ventana);
    box(mapa->grafico.ventana, 0, 0);

    for (int y = 0; y < VENTANA_SIZE_Y; y++) {
      for (int x = 0; x < VENTANA_SIZE_X; x++) {
        if (snapshot[y][x].pid_nave != -1) {
          mvwprintw(mapa->grafico.ventana, y, x, "x");
        } else {
          switch (snapshot[y][x].estructuraMapa) {
          case ASTEROIDE:
            mvwprintw(mapa->grafico.ventana, y, x, "*");
            break;
          case ESTACION:
            mvwprintw(mapa->grafico.ventana, y, x, "E");
            break;
          }
        }
      }
    }

    wrefresh(mapa->grafico.ventana);
    wrefresh(mapa->grafico.ventanaHud);

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
  pthread_mutex_init(&mapa->mutex_grafico, NULL);

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
  struct ArgsMapa *args = (struct ArgsMapa *)arg;
  struct Mapa *mapa = args->mapa;
  struct MatrizCompartida *shm = args->shm;
  struct MensajeConexion msg;

  // formato de la cola de respawn servidor y abirla
  struct mq_attr attr = {0, 10, sizeof(struct MensajeConexion), 0};
  mqd_t mq_conexiones =
      mq_open(NOMBRE_COLA_SERVIDOR_RESPAWN, O_CREAT | O_RDONLY, 0644, &attr);
  if (mq_conexiones == (mqd_t)-1) {
    perror("[RESPAWN] mq_open");
    exit(1);
  }

  // loop spawn recibe la id de la nave y devuelve la matriz del mapa
  while (1) {
    // espero las id de las naves
    if (mq_receive(mq_conexiones, (char *)&msg, sizeof(msg), NULL) == -1) {
      perror("[RESPAWN] mq_receive");
      continue;
    }

    struct NaveConectada *nave = containsNavesConectadas(
        mapa->naves_conectadas, mapa->cant_naves, msg.pid);

    if (nave == NULL) {
      sem_wait(&shm->mutex);
      respawnNave(args, msg.pid);
      sem_post(&shm->mutex);

    } else {
      sem_wait(&shm->mutex);
      gameOver(args, nave);
      sem_post(&shm->mutex);
    }
  }

  return NULL;
}

struct NaveConectada *
containsNavesConectadas(struct NaveConectada naves_conectadas[], int size,
                        int valor) {
  for (int i = 0; i < size; i++)
    if (naves_conectadas[i].pid == valor)
      return &naves_conectadas[i];
  return NULL;
}

void gameOver(struct ArgsMapa *args, struct NaveConectada *nave) {
  struct Mapa *mapa = args->mapa;
  struct MatrizCompartida *shm = args->shm;
  // luego lo elimino del mapa
  shm->MatrizMapa[nave->posY][nave->posX].pid_nave = -1;
  // encuentro y lo saco del array de naves.
  int desdeAcaAcomodar = -1;
  for (int i = 0; i < mapa->cant_naves; i++) {
    if (mapa->naves_conectadas[i].pid == nave->pid) {
      desdeAcaAcomodar = i;
      break;
    }
  }
  if (desdeAcaAcomodar == -1)
    return;

  for (int i = desdeAcaAcomodar; i < mapa->cant_naves - 1; i++) {
    mapa->naves_conectadas[i] = mapa->naves_conectadas[i + 1];
  }
  // decremento la cantidad de naves
  mapa->cant_naves--;

}

void respawnNave(struct ArgsMapa *args, pid_t pid) {
  struct Mapa *mapa = args->mapa;
  struct MatrizCompartida *shm = args->shm;

  int i = mapa->cant_naves;
  int estacion_Comienzo = rand() % ESTACION_MAX_SV;
  int posX = mapa->estaciones[estacion_Comienzo].posX;
  int posY = mapa->estaciones[estacion_Comienzo].posY;

  mapa->naves_conectadas[i].pid = pid;
  mapa->naves_conectadas[i].posX = posX;
  mapa->naves_conectadas[i].posY = posY;
  shm->MatrizMapa[posY][posX].pid_nave = pid;

  mapa->cant_naves++;
}

// aca calculo la nueva pos para devolver el movimiento a la nave
void moverNave(struct ArgsMapa *args, struct MensajeMovimiento msg) {
  struct Mapa *mapa = args->mapa;
  struct MatrizCompartida *shm = args->shm;
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

  // si choco con los bordes volteo y aparece del otro lado
  if (nuevaPosX >= VENTANA_SIZE_X - 1)
    nuevaPosX = 1;
  if (nuevaPosX < 1)
    nuevaPosX = VENTANA_SIZE_X - 1;
  if (nuevaPosY >= VENTANA_SIZE_Y - 1)
    nuevaPosY = 1;
  if (nuevaPosY < 1)
    nuevaPosY = VENTANA_SIZE_Y - 1;

  // Si hay otra nave, no deja avanzar
  if (shm->MatrizMapa[nuevaPosY][nuevaPosX].pid_nave == -1) {
    // borra posición vieja
    shm->MatrizMapa[mapa->naves_conectadas[naveExiste].posY]
                   [mapa->naves_conectadas[naveExiste].posX]
                       .pid_nave = -1;

    // actualiza posición
    mapa->naves_conectadas[naveExiste].posX = nuevaPosX;
    mapa->naves_conectadas[naveExiste].posY = nuevaPosY;

    // escribe posición nueva
    shm->MatrizMapa[nuevaPosY][nuevaPosX].pid_nave = msg.pid;
  }
}


void *hilo_cola_movimiento(void *arg) {
  struct ArgsMapa *args = (struct ArgsMapa *)arg;
  struct Mapa *mapa = args->mapa;
  struct MatrizCompartida *shm = args->shm;
  struct MensajeMovimiento msg;

  struct mq_attr attr = {0, 10, sizeof(struct MensajeMovimiento), 0};
  mqd_t mq_conexiones =
      mq_open(NOMBRE_COLA_SERVIDOR_MOVIMIENTO, O_CREAT | O_RDONLY, 0644, &attr);
  if (mq_conexiones == (mqd_t)-1) {
    perror("[SV-MOV] mq_open");
    exit(1);
  }



  while (1) {

    if (mq_receive(mq_conexiones, (char *)&msg, sizeof(msg), NULL) == -1) {
      perror("[SV-MOV] mq_receive");
      continue;
    }

 

    sem_wait(&shm->mutex);
    moverNave(args, msg);
    sem_post(&shm->mutex);

  }
  return NULL;
}