#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
struct Nave {
  int oxigeno;
  int combustible;
  int posX, posY;
  int bodegaMinerales[4];
  sem_t *sem_mutex;
};

// extern struct Nave nave;
// struct Mapa {
//   Nave nave;


// };

// struct Mapa {};

int main(int argc, char *argv[]) {
  // Agregar código aquí.

  // Termina la ejecución del programa.
  exit(EXIT_SUCCESS);
}
// crear la extructura logica mapa
// crear asteroides
// dibujar mapa
// memoria compartida
// mailbox