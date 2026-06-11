
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

struct Asteroides {
  int minerales;
  int posX, posY;
  int ancho, largo;
  sem_t *sem_mutex;
};
int main(int argc, char *argv[]) {}

void inicializarMutexAsteroide(struct Asteroides *asteroide, int id) {
  char nombre[32];
  snprintf(nombre, sizeof(nombre), "mutex_asteroide_%d", id); 

  sem_unlink(nombre); 

  asteroide->sem_mutex = sem_open(nombre, O_CREAT, 0666, 1);
  if (asteroide->sem_mutex == SEM_FAILED) { 
      perror("No se pudo crear el semáforo");
      exit(EXIT_FAILURE);
  }
}
void bloquearAsteroide(struct Asteroides *asteroide) {
  if (sem_wait(asteroide->sem_mutex) == -1) {
      perror("sem_wait asteroide");
      exit(EXIT_FAILURE);
  }
}

void desbloquearAsteroide(struct Asteroides *asteroide) {
  if (sem_post(asteroide->sem_mutex) == -1) {
      perror("sem_post asteroide");
      exit(EXIT_FAILURE);
  }
}


