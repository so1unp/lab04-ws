#include "comun.h"
#include <fcntl.h>
#include <math.h>
#include <mqueue.h>
#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// para ser mas claro para la generacion de mapa el 0 es para vacio, 1 para el
// jugador, 2 para la nave y 3 para asteroides

// hilos
void *hilo_grafico(void *arg);
void *hilo_cola_respawn(void *arg);
void *hilo_cola_movimiento(void *arg);
void *hilo_combustible_estacion(void *arg);

// funciones
struct NaveConectada *
containsNavesConectadas(struct NaveConectada naves_conectadas[], int size,
                        int valor);
void rellenarMapa(struct Mapa *mapa, struct MatrizCompartida *shm);
void inicializarVentanas(struct Mapa *arg);
void gameOver(struct ArgsMapa *args, struct NaveConectada *nave);
void respawnNave(struct ArgsMapa *args, pid_t pid);
void moverNave(struct ArgsMapa *args, struct MensajeMovimiento msg);
void colocarEstaciones(struct Mapa *mapa, struct MatrizCompartida *shm);
void matarServidorYprocesos(struct Mapa *mapa, struct MatrizCompartida *shm);
static struct Mapa *s_mapa = NULL;
static struct MatrizCompartida *s_shm = NULL;
// handler para cierre abrupto, control
void manejador_sigint(int sig) {
  (void)sig; // Evita advertencia de variable no usada
  if (s_mapa && s_shm)
    matarServidorYprocesos(s_mapa, s_shm);
  exit(EXIT_SUCCESS);
}
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  sem_unlink(SEM_ESTACION_CONTADOR);

  // --- AGREGAR ESTO AL PRINCIPIO ---
  // Limpieza preventiva de la caché/restos de ejecuciones anteriores
  // shm_unlink(NOMBRE_SHM_MAPA);
  // mq_unlink(NOMBRE_COLA_SERVIDOR_RESPAWN);
  // mq_unlink(NOMBRE_COLA_SERVIDOR_MOVIMIENTO);
  // ---------------------------------
  //[ROCIO] agregue la funcion de limpieza que se ejecuta en cierre abrupto o
  // comun, ya no se usa

  // Inicializa la semilla aleatoria usando la hora actual del sistema
  srand((unsigned int)time(NULL));
  struct Mapa mapa;

  // ACA SE INICIALIZA LA MATRIZ COMPARTIDA
  struct MatrizCompartida *shm;

  signal(SIGINT, manejador_sigint);
  int fd = shm_open(NOMBRE_SHM_MAPA, O_CREAT | O_RDWR, 0666);
  ftruncate(fd, sizeof(struct MatrizCompartida));
  shm = mmap(NULL, sizeof(struct MatrizCompartida), PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, 0);
  close(fd);
  sem_init(&shm->mutex, 1, 1); // 1 = compartido entre procesos

  // esto para cuando cierra abrupto
  s_mapa = &mapa;
  s_shm = shm;

  rellenarMapa(&mapa, shm);
  inicializarVentanas(&mapa);

  pthread_t hiloGrafico, hiloRespawn, hiloMovimiento, hilosEstaciones[ESTACION_MAX_SV];

  struct ArgsMapa args = {&mapa, shm}; 
  /*  servidor.c:89:19: error: redefinition of ‘args’
   89 |   struct ArgsMapa args = {&mapa, shm};*/

   //lo que podemos hacer
  for (int i = 0; i < ESTACION_MAX_SV; i++) {
  if (pthread_create(&hilosEstaciones[i], NULL, hilo_combustible_estacion, &args) != 0) {
  perror("pthread_create hilosEstaciones");
  exit(EXIT_FAILURE);
  }
  }

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
  bool finCondicion = true;

  while (finCondicion) {
    // asumo que ninguna tiene
    bool algunaTieneCombustible = false;

    for (int i = 0; i < ESTACION_MAX_SV; i++) {
      if (mapa.estaciones[i].combustible > 0) {
        algunaTieneCombustible = true;
        break;
      }
    }
    finCondicion = algunaTieneCombustible;
  }

  pthread_join(hiloGrafico, NULL);
  pthread_join(hiloRespawn, NULL);
  pthread_join(hiloMovimiento, NULL);

  for (int i = 0; i < ESTACION_MAX_SV; i++) {
  pthread_join(hilosEstaciones[i], NULL);
  }

  // DE ACA EN ADELANTE LIMPIEZA
  matarServidorYprocesos(&mapa, shm);
  exit(EXIT_SUCCESS);
}

// temporal,solo por ahora, despues se reemplazaria por la logica de
// inicializacion del mapa
void rellenarMapa(struct Mapa *mapa, struct MatrizCompartida *shm) {

  memset(shm->MatrizMapa, 0, sizeof(shm->MatrizMapa));
  mapa->cant_naves = 0;

  int cantidad_asteroides = ASTEROIDE_MAX_SV;

  // --- 2. GENERAR ASTEROIDES CON TUS MINERALES ---
  for (int i = 0; i < cantidad_asteroides; i++) {
    int x, y;

    // Esta es la parte vital del servidor: busca un lugar vacío
    do {
      //queremos que la posicion, no sea ni el 0, ni el 79, ni el 24, para evitar los bordes**
      x = rand() % (VENTANA_SIZE_X - 2) + 1;
      y = rand() % (VENTANA_SIZE_Y - 2) + 1;
    } while (shm->MatrizMapa[y][x].estructuraMapa != 0);

    // Guardamos la posición
    mapa->asteroides[i].posX = x;
    mapa->asteroides[i].posY = y;
    mapa->asteroides[i].ancho = 1;
    mapa->asteroides[i].largo = 1;

    // AQUI AGREGAMOS TU LÓGICA DE PRUEBA.C:
    mapa->asteroides[i].Deuterio = rand() % 100 + 1;
    mapa->asteroides[i].Mutexio = rand() % 100 + 1;
    mapa->asteroides[i].semaforita = rand() % 100 + 1;
    mapa->asteroides[i].kernelio = rand() % 100 + 1;

    // Inicializamos el semáforo del asteroide
    sem_init(&mapa->asteroides[i].sem_mutex, 1, 1);

    // Finalmente, lo registramos en la matriz compartida
    shm->MatrizMapa[y][x].estructuraMapa = ASTEROIDE;
  }
  mapa->cant_asteroides = cantidad_asteroides;

  // aca
  for (int y = 0; y < VENTANA_SIZE_Y; y++) {
    for (int x = 0; x < VENTANA_SIZE_X; x++) {
      shm->MatrizMapa[y][x].pid_nave = -1;
    }
  }

  // Inicializar estaciones
  // la var Estacion_max esta inicia
  colocarEstaciones(mapa, shm);
}

// el servidro muere
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
 struct mq_attr attr = {0};
  attr.mq_maxmsg = 3;
  attr.mq_msgsize = sizeof(struct MensajeConexion);
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

// aca se elimina la nave del mapa, y del array de naves conectadas, y se
// decrementa la cantidad de naves, para que el servidor sepa que esa nave ya no
// esta mas, y pueda volver a aparecer en el mapa si se vuelve a conectar las
// coordenadas de esa nave no son g
void gameOver(struct ArgsMapa *args, struct NaveConectada *nave) {
  struct Mapa *mapa = args->mapa;
  struct MatrizCompartida *shm = args->shm;

  // 1. GUARDAMOS LAS COORDENADAS ANTES DE BORRAR LA NAVE
  int x_muerte = nave->posX;
  int y_muerte = nave->posY;

  // 2. BORRAMOS LA NAVE DE LA MATRIZ (Tu código original)
  shm->MatrizMapa[y_muerte][x_muerte].pid_nave = -1;

  // ---------------------------------------------------------
  // 3. INICIO DE LA TRANSFORMACIÓN: DE NAVE A ESCOMBROS
  // ---------------------------------------------------------

  // Usamos la cantidad actual de asteroides como el "índice" para el nuevo
  int indice_nuevo_ast = mapa->cant_asteroides;

  // A. Asignamos la posición exacta donde murió la nave
  mapa->asteroides[indice_nuevo_ast].posX = x_muerte;
  mapa->asteroides[indice_nuevo_ast].posY = y_muerte;
  mapa->asteroides[indice_nuevo_ast].ancho = 1;
  mapa->asteroides[indice_nuevo_ast].largo = 1;

  // B. Le damos minerales (puede ser chatarra valiosa por ser una nave)
  mapa->asteroides[indice_nuevo_ast].Deuterio = rand() % 50 + 10;
  mapa->asteroides[indice_nuevo_ast].Mutexio = rand() % 50 + 10;
  mapa->asteroides[indice_nuevo_ast].semaforita = rand() % 50 + 10;
  mapa->asteroides[indice_nuevo_ast].kernelio = rand() % 50 + 10;

  // C. Inicializamos su semáforo para que otros puedan minarlo sin problemas
  sem_init(&mapa->asteroides[indice_nuevo_ast].sem_mutex, 1, 1);

  // D. ¡El toque mágico! Escribimos en la matriz compartida que ahora hay un
  // asteroide El hilo_grafico del servidor y de los clientes verán esto y lo
  // dibujarán automáticamente.
  shm->MatrizMapa[y_muerte][x_muerte].estructuraMapa = ASTEROIDE;

  // E. Actualizamos nuestro contador global
  mapa->cant_asteroides++;

  // ---------------------------------------------------------
  // FIN DE LA TRANSFORMACIÓN
  // ---------------------------------------------------------

  // Al final de la inyección del asteroide en gameOver...
  FILE *log = fopen("servidor_eventos.log", "a"); // "a" para añadir al final
  if (log != NULL) {
    fprintf(log,
            "[SISTEMA] Nave PID %d destruida sin recursos. Convertida en "
            "asteroide en pos (%d, %d).\n",
            nave->pid, x_muerte, y_muerte);
    fclose(log);
  }

  // 4. ELIMINAMOS LA NAVE DEL ARRAY DE CONECTADAS (Tu código original)
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

  mapa->cant_naves--;
}

void respawnNave(struct ArgsMapa *args, pid_t pid) {
  struct Mapa *mapa = args->mapa;
  struct MatrizCompartida *shm = args->shm;

  int i = mapa->cant_naves;
  if (mapa->cant_naves >= NAVE_MAX_SV) {
    return;
  }

  int estacion = rand() % ESTACION_MAX_SV;
  int ex = mapa->estaciones[estacion].posX;
  int ey = mapa->estaciones[estacion].posY;

  int posX = -1, posY = -1;

  // busca en radio creciente alrededor de la estacion
  for (int radio = 1; radio < 10 && posX == -1; radio++) {
    for (int dx = -radio; dx <= radio && posX == -1; dx++) {
      for (int dy = -radio; dy <= radio && posX == -1; dy++) {
        // solo el borde del cuadrado actual
        if (abs(dx) != radio && abs(dy) != radio)
          continue;

        int nx = ex + dx;
        int ny = ey + dy;

        if (nx >= 1 && nx < VENTANA_SIZE_X - 1 && ny >= 1 &&
            ny < VENTANA_SIZE_Y - 1 && shm->MatrizMapa[ny][nx].pid_nave == -1 &&
            shm->MatrizMapa[ny][nx].estructuraMapa == 0) {
          posX = nx;
          posY = ny;
        }
      }
    }
  }

  if (posX == -1) {
    kill(pid, SIGINT);
    return;
  }

  mapa->naves_conectadas[i].pid = pid;
  mapa->naves_conectadas[i].posX = posX;
  mapa->naves_conectadas[i].posY = posY;
  shm->MatrizMapa[posY][posX].pid_nave = pid;
  mapa->cant_naves++;
}
// aca a medida que la nave se mueve, se actualiza su posicion en la matriz
// compartida, y se verifica que no choque con los bordes ni con otras naves, si
// choca con los bordes, aparece del otro lado, y si choca con otra nave, no
// deja avanzar osea que el mapa
//  aca calculo la nueva pos para devolver el movimiento a la nave
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
  if (naveExiste == -1)
    return;

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
  struct ArgsMapa *args = (struct ArgsMapa *)arg;//esta variable puede no ser usada, por
  //struct Mapa *mapa = args->mapa; //esta variable 
  struct MatrizCompartida *shm = args->shm;
  struct MensajeMovimiento msg;

  struct mq_attr attr = {0};
  attr.mq_maxmsg = 3;
  attr.mq_msgsize = sizeof(struct MensajeMovimiento);
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

void colocarEstaciones(struct Mapa *mapa, struct MatrizCompartida *shm) {
// Definimos PI si no está disponible
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

  // Un radio de 10 da una separación masiva en el eje Y (ocupa casi toda la
  // pantalla) En ncurses, los caracteres son más altos que anchos, así que
  // escalamos el radio en X
  double radioY = 9.0;
  double radioX = 22.0;

  int coordenadasValidas = 0;
  int x0, y0, x1, y1, x2, y2;

  do {
    // 1. Elegimos un centro ligeramente aleatorio para el triángulo, cerca del
    // centro del mapa (40, 12)
    int centroX = 35 + (rand() % 11); // Entre 35 y 45
    int centroY = 11 + (rand() % 4);  // Entre 11 y 14

    // 2. Elegimos un ángulo inicial completamente aleatorio (en radianes)
    // Esto hará que el triángulo apunte hacia arriba, abajo, los lados, etc.
    double anguloInicial = ((double)rand() / RAND_MAX) * 2 * M_PI;

    // 3. Calculamos los tres ángulos separados por 120 grados (2*PI / 3)
    double a0 = anguloInicial;
    double a1 = anguloInicial + (2 * M_PI / 3);
    double a2 = anguloInicial + (4 * M_PI / 3);

    // 4. Convertimos coordenadas polares a cartesianas enteros
    x0 = centroX + (int)(radioX * cos(a0));
    y0 = centroY + (int)(radioY * sin(a0));

    x1 = centroX + (int)(radioX * cos(a1));
    y1 = centroY + (int)(radioY * sin(a1));

    x2 = centroX + (int)(radioX * cos(a2));
    y2 = centroY + (int)(radioY * sin(a2));

    // 5. Control de límites estricto para evitar desbordes en la matriz (80x25)
    if (x0 < 2 || x0 >= VENTANA_SIZE_X - 2 || y0 < 2 ||
        y0 >= VENTANA_SIZE_Y - 2 || x1 < 2 || x1 >= VENTANA_SIZE_X - 2 ||
        y1 < 2 || y1 >= VENTANA_SIZE_Y - 2 || x2 < 2 ||
        x2 >= VENTANA_SIZE_X - 2 || y2 < 2 || y2 >= VENTANA_SIZE_Y - 2) {
      continue; // Si se sale de la pantalla, re-calcula
    }

    // 6. Verificar en la SHM que ninguna posición pise un asteroide
    if (shm->MatrizMapa[y0][x0].estructuraMapa == 0 &&
        shm->MatrizMapa[y1][x1].estructuraMapa == 0 &&
        shm->MatrizMapa[y2][x2].estructuraMapa == 0) {

      coordenadasValidas = 1;

      // Asignar las posiciones finales rotadas
      mapa->estaciones[0].posX = x0;
      mapa->estaciones[0].posY = y0;

      mapa->estaciones[1].posX = x1;
      mapa->estaciones[1].posY = y1;

      mapa->estaciones[2].posX = x2;
      mapa->estaciones[2].posY = y2;
    }
  } while (!coordenadasValidas);

  // Inicializar atributos comunes y registrar en memoria compartida
  for (int i = 0; i < ESTACION_MAX_SV; i++) {
    mapa->estaciones[i].ancho = 1;
    mapa->estaciones[i].largo = 1;
    mapa->estaciones[i].minerales = 0;
    mapa->estaciones[i].combustible = 1000;
    mapa->estaciones[i].sem_mutex = NULL;

    int x = mapa->estaciones[i].posX;
    int y = mapa->estaciones[i].posY;

    shm->MatrizMapa[y][x].estructuraMapa = ESTACION;
  }
}
// este es el metodo para limpiar todo cuando muere el sv, no importa si sea de
// forma natural(estaciones sin combustible o por el control + c)
void matarServidorYprocesos(struct Mapa *mapa, struct MatrizCompartida *shm) {

  for (int i = 0; i < mapa->cant_naves; i++) {

    kill(mapa->naves_conectadas[i].pid, SIGINT);
  }
  // mientras que haya naves, no se cierra el sv
  while (mapa->cant_naves > 0) {
    usleep(100000);
  }
  for (int i = 0; i < mapa->cant_asteroides; i++) {
    sem_destroy(&mapa->asteroides[i].sem_mutex);
  }

  for (int i = 0; i < ESTACION_MAX_SV; i++) {
    if (mapa->estaciones[i].sem_mutex != NULL) {
      sem_destroy(mapa->estaciones[i].sem_mutex);
    }
  }
  mq_unlink(NOMBRE_COLA_SERVIDOR_RESPAWN);
  mq_unlink(NOMBRE_COLA_SERVIDOR_MOVIMIENTO);

  sem_destroy(&shm->mutex);
  munmap(shm, sizeof(struct MatrizCompartida));
  shm_unlink(NOMBRE_SHM_MAPA);

  endwin();
}


void *hilo_combustible_estacion(void *arg) {
    struct ArgsMapa *args = (struct ArgsMapa *)arg;
    struct Mapa *mapa = args->mapa;
    
    // Usamos un truco seguro para asignar un ID único (0, 1, 2) a cada hilo
    static int generador_id = 0;
    static pthread_mutex_t mutex_id = PTHREAD_MUTEX_INITIALIZER;
    
    pthread_mutex_lock(&mutex_id);
    int mi_estacion_id = generador_id++;
    pthread_mutex_unlock(&mutex_id);
    
    int alerta_enviada = 0;
    int estacion_muerta_enviada = 0;

    while (1) {
        sleep(2); // Frecuencia de consumo de la estación (ajustable)

        // Bloqueamos la SHM antes de leer/modificar el combustible
        sem_wait(&args->shm->mutex);
        
        if (mapa->estaciones[mi_estacion_id].combustible > 0) {
            // El combustible decrece con el tiempo
            mapa->estaciones[mi_estacion_id].combustible -= 10;

            if (mapa->estaciones[mi_estacion_id].combustible < 0) {
                mapa->estaciones[mi_estacion_id].combustible = 0;
            }

            // Si una nave llena la estación, reseteamos el flag de alerta
            if (mapa->estaciones[mi_estacion_id].combustible > 200) {
                alerta_enviada = 0;
                estacion_muerta_enviada = 0;
            }

            // Umbral crítico: 20% (200 unidades de un máximo de 1000)
            if (mapa->estaciones[mi_estacion_id].combustible <= 200 && 
                mapa->estaciones[mi_estacion_id].combustible > 0 && 
                alerta_enviada == 0) {

                alerta_enviada = 1; // Se envía una sola vez por evento crítico

                // Preparamos el mensaje directo (MD)
                struct MensajeAlerta msg_alerta;
                snprintf(msg_alerta.texto, sizeof(msg_alerta.texto), 
                         "[ALERTA HUD] Estacion %d en pos (%d,%d) con combustible critico (20%% o menos)!", 
                         mi_estacion_id, 
                         mapa->estaciones[mi_estacion_id].posX, 
                         mapa->estaciones[mi_estacion_id].posY);

                // Notificar a todas las naves que figuran en el array de conectados del servidor
                for (int j = 0; j < mapa->cant_naves; j++) {
                    char nombre_cola_alerta[64];
                    snprintf(nombre_cola_alerta, sizeof(nombre_cola_alerta), NOMBRE_COLA_ALERTAS_NAVE, mapa->naves_conectadas[j].pid);

                    // O_NONBLOCK evita retrasar el servidor si una nave no procesa rápido sus mensajes
                    mqd_t mq_dest = mq_open(nombre_cola_alerta, O_WRONLY | O_NONBLOCK);
                    if (mq_dest != (mqd_t)-1) {
                        mq_send(mq_dest, (const char *)&msg_alerta, sizeof(msg_alerta), 0);
                        mq_close(mq_dest);
                    }
                }
            }
        }// --- NUEVO BLOQUE: La estación llegó exactamente a 0 (Estación Muerta) ---
        else if (mapa->estaciones[mi_estacion_id].combustible == 0 && estacion_muerta_enviada == 0) {
            estacion_muerta_enviada = 1; // Se envía una sola vez para no spamear

            struct MensajeAlerta msg_muerte;
            snprintf(msg_muerte.texto, sizeof(msg_muerte.texto), 
                     "[HUD] Estacion %d en (%d,%d) ha MUERTO (Sin combustible).", 
                     mi_estacion_id, 
                     mapa->estaciones[mi_estacion_id].posX, 
                     mapa->estaciones[mi_estacion_id].posY);

            // Avisar a las naves que la estación ya no da para más
            for (int j = 0; j < mapa->cant_naves; j++) {
                char nombre_cola_alerta[64];
                snprintf(nombre_cola_alerta, sizeof(nombre_cola_alerta), NOMBRE_COLA_ALERTAS_NAVE, mapa->naves_conectadas[j].pid);

                mqd_t mq_dest = mq_open(nombre_cola_alerta, O_WRONLY | O_NONBLOCK);
                if (mq_dest != (mqd_t)-1) {
                    mq_send(mq_dest, (const char *)&msg_muerte, sizeof(msg_muerte), 0);
                    mq_close(mq_dest);
                }
            }
        }

        sem_post(&args->shm->mutex);
    }
    return NULL;
}


