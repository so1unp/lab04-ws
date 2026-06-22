#include "comun.h"
#include <fcntl.h>
#include <mqueue.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
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
void *hilo_comercio(void *arg);
/*funciones*/
int trueque_estacion(struct Nave *nave);
int minerales_totales(struct Nave *nave);
void inicializarVentanas_nave(struct Nave *arg);
void inicializarHilos_nave(struct Nave *arg);
void inicializar_nave(struct Nave *nave);
void gameOver_nave(struct Nave *nave);
void enviar_movimiento(struct Nave *nave, int newPosX, int newPosY);
int calcular_total_minerales(struct Nave *nave);
void movimientoPorTecla(void *arg, int xPos, int yPos);
static int abrir_cola_escritura(mqd_t *cola, const char *nombre);
static int enviar_minerales_a_estacion(int totalMinerales);
static int recibir_respuesta_estacion(char *respuesta_buffer,
                                      size_t tam_buffer);
static int parsear_respuesta_estacion(const char *respuesta, int *combustible,
                                      int *oxigeno);
static void aplicar_trueque(struct Nave *nave, int combustible_recibido,
                            int oxigeno_recibido);

// esto para el cierre abrupto
static struct Nave *g_nave = NULL;
static pthread_t g_hilos[5];

// handler para nave
void manejador_sigint(int sig) {
  // ctrl+c del nave, va al gameover

  gameOver_nave(g_nave);
  exit(EXIT_SUCCESS);
}

// snprintf(buffer, TAMANIO_MAX_MSG, "PID:%d;MINERALES:%d", getpid(),
// totalMinerales); esa linea esta en la linea
static int ya_murio = 0;

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  struct Nave nave;
  g_nave = &nave;
  signal(SIGINT, manejador_sigint);
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
  // cola = mq_open(NOMBRE_COLA_ESTACION, O_WRONLY, ...);
  // esa linea de arriba esta
  nave.oxigeno = 100;
  nave.combustible = 100;
  nave.en_trueque =
      0; // Nuevo campo para indicar si la nave está en medio de un trueque
  nave.velocidadMovimiento = 1;
  nave.combustibleGastadoMovimiento = 1;
  nave.ancho = 1;
  nave.largo = 1;
  nave.posX = -1;
  nave.posY = -1;
  memset(nave.bodegaMinerales, 0, sizeof(nave.bodegaMinerales));

  // SEMAFOROS
  // en main de nave.c
  char nombre_mutex[32];
  char nombre_pantalla[32];
  snprintf(nombre_mutex, sizeof(nombre_mutex), "mutex_nave_%d", getpid());
  snprintf(nombre_pantalla, sizeof(nombre_pantalla), "mutex_pantalla_%d",
           getpid());

  sem_unlink(nombre_mutex);
  sem_unlink(nombre_pantalla);

  if ((nave.sem_mutex = sem_open(nombre_mutex, O_CREAT, 0666, 1)) ==
      (sem_t *)-1) {
    perror("sem_open mutex_nave");
    exit(EXIT_FAILURE);
  }
  if ((nave.mutex_pantalla = sem_open(nombre_pantalla, O_CREAT, 0666, 1)) ==
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
  pthread_t hiloSoporteVital, hiloPropulsion, hiloExtraccion, hiloGrafico,
      hiloComercio;

  if (pthread_create(&hiloSoporteVital, NULL, hilo_soporte_vital, &nave) != 0) {
    perror("pthread_create hilo_soporte_vital");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloPropulsion, NULL, hilo_propulsion, &args) != 0) {
    perror("pthread_create hilo_propulsion");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloExtraccion, NULL, hilo_extraccion, &args) != 0) {
    perror("pthread_create hilo_extraccion");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloGrafico, NULL, hilo_grafico_nave, &args) != 0) {
    perror("pthread_create hilo_grafico");
    exit(EXIT_FAILURE);
  }
  if (pthread_create(&hiloComercio, NULL, hilo_comercio, &args) != 0) {
    perror("pthread_create hilo_comercio");
    exit(EXIT_FAILURE);
  }

  g_hilos[0] = hiloSoporteVital;
  g_hilos[1] = hiloPropulsion;
  g_hilos[2] = hiloExtraccion;
  g_hilos[3] = hiloGrafico;
  g_hilos[4] = hiloComercio;
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
  // 1. Avisamos al servidor

  gameOver_nave(&nave);
  exit(EXIT_SUCCESS);
}

int calcular_total_minerales(struct Nave *nave) {
  int total = 0;
  for (int i = 0; i < 4; i++) {
    total += nave->bodegaMinerales[i];
  }
  return total;
}

int minerales_totales(struct Nave *nave) { // no esta
  int total = 0;
  for (int i = 0; i < 4; i++) {
    total += nave->bodegaMinerales[i];
  }
  return total;
}

static int abrir_cola_escritura(mqd_t *cola, const char *nombre) {
  // aca
  struct mq_attr attr = {0, 3, TAMANIO_MAX_MSG, 0};
  *cola = mq_open(nombre, O_WRONLY | O_CREAT, 0666, &attr);
  if (*cola == (mqd_t)-1) {
    perror("Error al abrir la cola de escritura");
    return -1;
  }
  return 0;
}

static int enviar_minerales_a_estacion(int totalMinerales) {
  mqd_t cola_estacion;
  char buffer[TAMANIO_MAX_MSG];

  if (abrir_cola_escritura(&cola_estacion, NOMBRE_COLA_ESTACION) == -1) {
    return -1;
  }

  // Empaquetamos el PID junto con los minerales usando getpid()
  // el pid es necesario para que la estacion sepa a quien responder, y los
  // minerales para el trueque cada interaccion con la estacion es
  // independiente, no hay un "contrato" previo, entonces el pid es la forma de
  // identificarnos cada pid es una nave diferente, y la estacion puede recibir
  // mensajes de varias naves, por eso el pid es crucial para que la estacion
  // sepa a quien responder
  snprintf(buffer, TAMANIO_MAX_MSG, "PID:%d;MINERALES:%d", getpid(),
           totalMinerales);

  if (mq_send(cola_estacion, buffer, strlen(buffer), 0) == -1) {
    perror("Error al enviar mensaje a la estación");
    mq_close(cola_estacion);
    return -1;
  }

  mq_close(cola_estacion);
  return 0;
}

static int recibir_respuesta_estacion(char *respuesta_buffer,
                                      size_t tam_buffer) {
  mqd_t cola_respuesta;
  ssize_t bytes_leidos;
  char nombre_cola[64];
  struct mq_attr attr = {0, 3, TAMANIO_MAX_MSG, 0};

  // Calculamos el nombre de nuestra cola privada
  snprintf(nombre_cola, sizeof(nombre_cola), NOMBRE_COLA_NAVE_SERVIDOR,
           getpid());

  // Abrimos la cola en modo lectura
  cola_respuesta =
      mq_open(nombre_cola, O_CREAT | O_RDONLY, PERMISOS_COLA, &attr);
  if (cola_respuesta == (mqd_t)-1) {
    perror("Error al abrir cola privada de respuesta");
    return -1;
  }

  bytes_leidos = mq_receive(cola_respuesta, respuesta_buffer, tam_buffer, NULL);
  if (bytes_leidos == -1) {
    perror("Error al recibir mensaje de la estación");
    mq_close(cola_respuesta);
    return -1;
  }

  respuesta_buffer[bytes_leidos] = '\0';

  // Limpieza total: Cerramos y DESTRUIMOS la cola (unlink) para no dejar
  // rastros
  mq_close(cola_respuesta);
  mq_unlink(nombre_cola);

  return 0;
}

static int parsear_respuesta_estacion(const char *respuesta, int *combustible,
                                      int *oxigeno) {
  if (sscanf(respuesta, "FUEL:%d;OXY:%d", combustible, oxigeno) != 2) {
    fprintf(stderr, "Respuesta inválida de la estación: %s\n", respuesta);
    return -1;
  }
  return 0;
}

static void aplicar_trueque(struct Nave *nave, int combustible_recibido,
                            int oxigeno_recibido) {
  sem_wait(nave->sem_mutex);

  nave->combustible += combustible_recibido;
  nave->oxigeno += oxigeno_recibido;

  for (int i = 0; i < 4; i++) {
    nave->bodegaMinerales[i] = 0;
  }

  sem_post(nave->sem_mutex);
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

// mailbox para el respawn
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
  static int cerrando = 0;

  if (cerrando)
    return;

  cerrando = 1;

  // Avisar al servidor
  mqd_t mq_servidor = mq_open(NOMBRE_COLA_SERVIDOR_RESPAWN, O_WRONLY);
  if (mq_servidor != (mqd_t)-1) {
    struct MensajeConexion msg;
    msg.pid = getpid();
    mq_send(mq_servidor, (char *)&msg, sizeof(msg), 0);
    mq_close(mq_servidor);
  }

  // Matar hilos
  for (int i = 0; i < 5; i++)
    pthread_cancel(g_hilos[i]);

  for (int i = 0; i < 5; i++)
    pthread_join(g_hilos[i], NULL);

  // Limpiar semaforos
  char nombre_mutex[32];
  char nombre_pantalla[32];

  snprintf(nombre_mutex, sizeof(nombre_mutex), "mutex_nave_%d", getpid());

  snprintf(nombre_pantalla, sizeof(nombre_pantalla), "mutex_pantalla_%d",
           getpid());

  sem_close(nave->sem_mutex);
  sem_close(nave->mutex_pantalla);

  sem_unlink(nombre_mutex);
  sem_unlink(nombre_pantalla);
  sem_unlink(SEM_ESTACION_CONTADOR);
  // cartel muerte
  fprintf(stderr,
          "\n\n===== GAME OVER =====\n"
          "Oxigeno: %d\n"
          "Combustible: %d\n\n",
          nave->oxigeno, nave->combustible);

  sleep(5);

  endwin();
}
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

// 2
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
  struct ArgsNave *args = (struct ArgsNave *)arg; // [ORIGINAL]
  // struct MatrizCompartida *shm = args->shm; // [ALEX] la comenta porque no se
  // usa (evita warning)
  struct Nave *nave = args->nave; // [ORIGINAL]
  // w a s d
  int tecla;

  // [ALEX] Nuevo: hace que getch() no se quede esperando bloqueado.
  // Si no se toca nada en 50ms, getch() devuelve ERR y el loop puede seguir.
  timeout(50);

  while (1) {
    tecla = getch(); // [ORIGINAL]

    // [ALEX] Si no tocaste ninguna tecla, tecla vale ERR. Seguimos el loop sin
    // hacer nada.
    if (tecla == ERR) {
      continue;
    }

    // [ALEX] Si la nave está en medio de un trueque con la estación,
    // ignoramos la tecla (la nave queda "congelada")
    if (nave->en_trueque == 1) {
      continue;
    }

    refresh();       // [ORIGINAL]
    switch (tecla) { // [ORIGINAL] - el switch completo no cambia
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

/**
 * Función para el hilo de extracción de minerales.
 * Esta función simula la extracción de minerales de asteroides adyacentes, el
 * consumo de combustible y la actualización de la bodega de minerales de la
 * nave.
 */
/**
 * Función para el hilo de extracción de minerales.
 * [ORIGINAL - nave.c] Firma y estructura general del hilo
 * [ALEX] Lógica de detección de asteroide adyacente, límite de bodega
 *        y extracción aleatoria
 */
void *hilo_extraccion(void *arg) {
  // [ALEX] Antes recibía solo "struct Nave *nave", ahora necesita
  // también el shm para revisar la matriz compartida -> usamos ArgsNave
  struct ArgsNave *args = (struct ArgsNave *)arg;
  struct Nave *nave = args->nave;           // [ALEX]
  struct MatrizCompartida *shm = args->shm; // [ALEX]

  srand((unsigned int)time(NULL)); // [ORIGINAL] ya estaba en nave.c

  int limite_bodega = 50; // [ALEX] Capacidad máxima de la bodega de minerales

  while (1) {
    sleep(2); // [ORIGINAL] Simula el tiempo de extracción

    // [ALEX] Obtenemos posición actual y combustible de forma segura
    sem_wait(nave->sem_mutex);
    int x = nave->posX;
    int y = nave->posY;
    int combustible_actual = nave->combustible;

    // [ALEX] Calculamos cuántos minerales tenemos en total
    int minerales_actuales = 0;
    for (int i = 0; i < 4; i++) { // 4 = BODEGA_MINERALES_MAX
      minerales_actuales += nave->bodegaMinerales[i];
    }
    sem_post(nave->sem_mutex);

    // [ORIGINAL] Chequeo de combustible, antes estaba dentro del lock
    if (combustible_actual <= 0) {
      break; // No hay combustible para extraer
    }

    // [ALEX] Si la bodega está llena, no hace nada (no gasta combustible)
    if (minerales_actuales >= limite_bodega) {
      continue;
    }

    // [ALEX] Antes esto era "int asteroideAdyacente = 1;" fijo (siempre true)
    // en nave.c. Ahora se revisa de verdad contra la matriz compartida.
    int asteroideAdyacente = 0;

    sem_wait(&shm->mutex); // [ALEX] Bloqueo del mapa compartido
    if (y > 0 && shm->MatrizMapa[y - 1][x].estructuraMapa == ASTEROIDE) {
      asteroideAdyacente = 1; // Arriba
    } else if (y < VENTANA_SIZE_Y - 1 &&
               shm->MatrizMapa[y + 1][x].estructuraMapa == ASTEROIDE) {
      asteroideAdyacente = 1; // Abajo
    } else if (x > 0 && shm->MatrizMapa[y][x - 1].estructuraMapa == ASTEROIDE) {
      asteroideAdyacente = 1; // Izquierda
    } else if (x < VENTANA_SIZE_X - 1 &&
               shm->MatrizMapa[y][x + 1].estructuraMapa == ASTEROIDE) {
      asteroideAdyacente = 1; // Derecha
    }
    sem_post(&shm->mutex); // [ALEX]

    // [ORIGINAL] Estructura del bloque de extracción (sem_wait/sem_post sobre
    // nave) [ALEX] Pero ahora solo extrae SI hay asteroide adyacente, y en un
    // slot aleatorio
    if (asteroideAdyacente) {
      sem_wait(nave->sem_mutex); // --- BLOQUEO --- [ORIGINAL]

      if (nave->combustible > 0) {
        nave->combustible -= 1; // [ORIGINAL] gasta combustible

        // [ALEX] Antes (nave.c) llenaba los 4 minerales con un for.
        // Ahora extrae solo 1 tipo de mineral, elegido al azar
        int tipoMineral = rand() % 4;            // 4 = BODEGA_MINERALES_MAX
        nave->bodegaMinerales[tipoMineral] += 5; // [ALEX] +5 en vez de +1
      }

      sem_post(nave->sem_mutex); // --- DESBLOQUEO --- [ORIGINAL]
    }
  }

  return NULL;
}

void *hilo_comercio(void *arg) { // nuevo
  struct ArgsNave *args = (struct ArgsNave *)arg;
  struct Nave *nave = args->nave;
  struct MatrizCompartida *shm = args->shm;

  // Inicializamos el semáforo contador en 3 (Máximo 3 naves por estación)
  sem_t *sem_estacion_contador =
      sem_open(SEM_ESTACION_CONTADOR, O_CREAT, 0666, 3);

  while (1) {
    sleep(1);

    // Revisamos cuántos minerales tenemos
    sem_wait(nave->sem_mutex);
    int x = nave->posX;
    int y = nave->posY;
    int minerales = calcular_total_minerales(nave);
    sem_post(nave->sem_mutex);

    // Si la bodega está vacía, no hacemos nada
    if (minerales <= 0) {
      continue;
    }

    // Revisamos si hay una estación adyacente
    int estacionAdyacente = 0;
    sem_wait(&shm->mutex);
    if (y > 0 && shm->MatrizMapa[y - 1][x].estructuraMapa == ESTACION)
      estacionAdyacente = 1;
    else if (y < VENTANA_SIZE_Y - 1 &&
             shm->MatrizMapa[y + 1][x].estructuraMapa == ESTACION)
      estacionAdyacente = 1;
    else if (x > 0 && shm->MatrizMapa[y][x - 1].estructuraMapa == ESTACION)
      estacionAdyacente = 1;
    else if (x < VENTANA_SIZE_X - 1 &&
             shm->MatrizMapa[y][x + 1].estructuraMapa == ESTACION)
      estacionAdyacente = 1;
    sem_post(&shm->mutex);

    // Si hay una estación y tenemos minerales, arrancamos el proceso
    if (estacionAdyacente) {

      nave->en_trueque = 1; // Congelamos la nave (bloquea teclado)

      sem_wait(sem_estacion_contador); // Hacemos fila. Si ya hay 3 naves
                                       // adentro, el hilo se duerme acá.

      trueque_estacion(nave); // funciones de intercambio

      // agrego delay para simular que el trueque sea lento y no tan rapido que
      // flash :V
      sleep(3);

      sem_post(sem_estacion_contador); // Salimos de la estación y liberamos el
                                       // cupo para otra nave

      nave->en_trueque = 0; // Descongelamos la nave

      sleep(2); // Delay para darte tiempo a moverte antes de que vuelva a
                // intentar entrar
    }
  }

  return NULL;
}

/**
 * Función para realizar el trueque en la estación espacial.
 * Esta función se encarga de enviar los recursos de la nave a la estación
 * espacial a través de una cola de mensajes y recibir los recursos necesarios a
 * cambio.
 *
 */
int trueque_estacion(struct Nave *nave) { // nuevo
  int totalMinerales;
  int combustible_recibido = 0;
  int oxigeno_recibido = 0;
  char respuesta_buffer[TAMANIO_MAX_MSG + 1];

  sem_wait(nave->sem_mutex);
  totalMinerales = calcular_total_minerales(nave);
  sem_post(nave->sem_mutex);

  if (totalMinerales <= 0) {
    // printf("No hay minerales para intercambiar\n");
    return 0;
  }
  // esta funcion esta
  if (enviar_minerales_a_estacion(totalMinerales) == -1) {
    return -1;
  }

  if (recibir_respuesta_estacion(respuesta_buffer, TAMANIO_MAX_MSG) == -1) {
    return -1;
  }

  // printf("Respuesta de la estación: %s\n", respuesta_buffer);

  if (parsear_respuesta_estacion(respuesta_buffer, &combustible_recibido,
                                 &oxigeno_recibido) == -1) {
    return -1;
  }

  aplicar_trueque(nave, combustible_recibido, oxigeno_recibido);

  // printf("Trueque realizado: +%d combustible, +%d
  // oxígeno\n",combustible_recibido, oxigeno_recibido);

  return 0;
}

void *hilo_grafico_nave(void *arg) { // ojo
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