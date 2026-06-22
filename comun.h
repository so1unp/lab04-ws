#ifndef COMUN_H
#define COMUN_H

#include <ncurses.h>
#include <pthread.h>
#include <semaphore.h>

/* Colas y permisos */
#define NOMBRE_COLA_ESTACION "/cola_estacion"
#define PERMISOS_COLA 0666
#define TAMANIO_MAX_MSG 256
#define NOMBRE_COLA_NAVE "/cola_nave" // nombre de la cola de la nave para recibir msj de la estacion
//#define ESTACION_MAX_SV 3
#define SEM_ESTACION_CONTADOR "/sem_estacion_contador" // semáforo para controlar el contador de naves en la estación


#define NOMBRE_COLA_ESTACION "/cola_estacion"
#define NOMBRE_COLA_NAVE_SERVIDOR "/cola_nave_%d"
#define NOMBRE_COLA_SERVIDOR_RESPAWN "/cola_servidor_respawn"
#define NOMBRE_COLA_SERVIDOR_MOVIMIENTO "/cola_servidor_movimiento"



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
    int posX;
    int posY;
    sem_t sem_mutex;
    int Mutexio;
    int semaforita;
    int kernelio;
    int Deuterio;
    int ancho, largo;
};

struct Estacion {
  int minerales;
  int combustible;
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
  int en_trueque; // Nuevo campo para indicar si la nave está en medio de un trueque
  
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
  struct Asteroides asteroides[ASTEROIDE_MAX_SV + NAVE_MAX_SV]; // Ajuste: Aumentamos el tamaño del array para incluir naves
  struct Estacion estaciones[ESTACION_MAX_SV];
  struct Grafico grafico;
  pthread_mutex_t mutex_grafico;
  struct NaveConectada naves_conectadas[NAVE_MAX_SV];
  int cant_naves;
  int cant_asteroides;

};




struct MensajeConexion {
  pid_t pid;
};

struct MensajeMovimiento {
  pid_t pid;
  int posX, posY;
};

#endif /* COMUN_H */