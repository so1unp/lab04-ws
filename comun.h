#ifndef COMUN_H
#define COMUN_H

#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>

/* Colas y permisos */
#define NOMBRE_COLA_ESTACION "/cola_estacion"
#define PERMISOS_COLA 0666
#define TAMANIO_MAX_MSG 256

#define NOMBRE_COLA_ESTACION "/cola_estacion"
#define NOMBRE_COLA_NAVE_SERVIDOR "/cola_nave_%d"
#define NOMBRE_COLA_SERVIDOR_RESPAWN "/cola_servidor_respawn"
#define NOMBRE_COLA_SERVIDOR_MOVIMIENTO "/cola_servidor_movimiento"
#define NOMBRE_NAVE_RESPAWN "/cola_respawn_%d"
#define NOMBRE_NAVE_MOVIMIENTO "/cola_moviento_%d"

#define BODEGA_MINERALES_MAX 4
//VARIABLE MEMORIA COMPARTIDA MAPA
#define NOMBRE_SHM_MAPA "/shm_mapa"

// PA LO GRAFICO

#define VENTANA_SIZE_Y 25
#define VENTANA_SIZE_X 80
#define ESTACION 2
#define ASTEROIDE 3
#define NAVE 1
// servidor ajustes
#define ESTACION_MAX_SV 3
#define ASTEROIDE_MAX_SV 6
#define NAVE_MAX_SV 3

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

struct Grafico {
  WINDOW *ventana;
  WINDOW *ventanaHud;
};
struct NaveConectada {
  pid_t pid;
  int posX, posY;
};
struct LugarMatriz {
  int estructuraMapa;
  pid_t pid_nave;
};
struct Nave {
  int oxigeno;
  int combustible;
  int posX, posY;
  int velocidadMovimiento;
  int combustibleGastadoMovimiento;
  int bodegaMinerales[BODEGA_MINERALES_MAX];
  sem_t *sem_mutex;
  int ancho, largo;
  sem_t *mutex_pantalla;
  struct Grafico grafico;
};
struct MatrizCompartida {
    struct LugarMatriz MatrizMapa[VENTANA_SIZE_Y][VENTANA_SIZE_X];
    sem_t mutex;  
};
struct ArgsMapa {
  struct Mapa *mapa;
  struct MatrizCompartida *shm;
};
struct ArgsNave {
  struct Nave *nave;
  struct MatrizCompartida *shm;
};

struct Mapa {
  struct Asteroides asteroides[ASTEROIDE_MAX_SV];
  struct Estacion estaciones[ESTACION_MAX_SV];
  struct Grafico grafico;
  pthread_mutex_t mutex_grafico;
  struct NaveConectada naves_conectadas[NAVE_MAX_SV];
  int cant_naves;
};




struct MensajeConexion {
  pid_t pid;
};

struct MensajeMovimiento {
  pid_t pid;
  int posX, posY;
};

#endif /* COMUN_H */