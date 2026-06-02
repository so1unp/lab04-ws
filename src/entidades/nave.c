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
struct Nave {
  int oxigeno;
  int combustible;
  int posX, posY;
  int bodegaMinerales[4];
  sem_t *sem_mutex;
};
struct Nave nave;
int combustibleGastadoMovimiento = 1;
int velocidadMovimiento = 1;

/*metodo importante aca primero me fijo si hay combustible, en el futuro supongo
que si se queda sin combustible game over. bloquea cuando se mueve si hay
combustible, actualiza posicion resta combustible configurado desde la variable
global y luego desbloquea
*/
void movimientoPorTecla(int xPos, int yPos) {
  if (nave.combustible == 0) {
    printf("Combustible agotado,no puedo moverme");

    return;
  }

  sem_wait(nave.sem_mutex);
  nave.posX = xPos;
  nave.posY = yPos;
  nave.combustible -= combustibleGastadoMovimiento;
  sem_post(nave.sem_mutex);
  return;
}
/*Aca solo se ve que tecla para mandar las nuevas coordenadas actualizadas, se
 * le suma/resta la velocidad de movimiento*/
void *hilo_propulsion(void *param) {
  // w a s d
  int tecla;

  while (1) {
    tecla = getch();
    switch (tecla) {
    case 'w':
      movimientoPorTecla(nave.posX, nave.posY - velocidadMovimiento);
      break;
    case 's':
      movimientoPorTecla(nave.posX, nave.posY + velocidadMovimiento);
      break;
    case 'a':
      movimientoPorTecla(nave.posX - velocidadMovimiento, nave.posY);
      break;
    case 'd':
      movimientoPorTecla(nave.posX + velocidadMovimiento, nave.posY);
      break;
    }
  }
  return NULL;
}

void *hilo_soporte_vital(void *param) {
  // TODO: lógica de oxígeno
  return NULL;
}

void *hilo_extraccion(void *param) {
  // TODO: lógica de minería
  return NULL;
}

void *hilo_radar(void *param) {
  // TODO: lógica de radar
  return NULL;
}
