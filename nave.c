
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <unistd.h>
struct Nave{
    int oxigeno;
    int combustible;
    int posX, posY;
    int bodegaMinerales[4];
    pthread_mutex_t mutex;
} ;

int main(void)
{
    /*hilos del cliente nave*/
    void* hilo_soporte_vital(void* arg);
    void* hilo_propulsion(void* arg);
    void* hilo_extraccion(void* arg);
    void* hilo_radar(void* arg);

   
    //pthread_t es el identificador del hilo
    pthread_t id_hilo_radar;

    struct Nave mi_nave;
    mi_nave.oxigeno=100;
    mi_nave.combustible=100;
    mi_nave.posX=0;
    mi_nave.posY=0;
    mi_nave.bodegaMinerales[0]=0;
    mi_nave.bodegaMinerales[1]=0;
    mi_nave.bodegaMinerales[2]=0;
    mi_nave.bodegaMinerales[3]=0;    
    

     pthread_mutex_init(&mi_nave.mutex, NULL);
    pthread_create(&id_hilo_radar, NULL, hilo_radar, &mi_nave);
  

    pthread_join(id_hilo_radar, NULL);
    // Termina la ejecución del programa.
    exit(EXIT_SUCCESS);
}

//hilo pthread

   

void* hilo_radar(void* arg){
struct Nave* nave = (struct Nave*) arg;

int counter = 0;
    
//para ejecutar solo 10s , el while debe recibir
while (counter < 10){
    system("clear");
pthread_mutex_lock(&nave->mutex);
printf("Oxígeno: %d\n", nave->oxigeno);
printf("Combustible: %d\n", nave->combustible);
printf("Posición X: %d\n", nave->posX);
printf("Posición Y: %d\n", nave->posY);
printf("Minerales 1: %d\n", nave->bodegaMinerales[0]);
printf("Minerales 2: %d\n", nave->bodegaMinerales[1]);
printf("Minerales 3: %d\n", nave->bodegaMinerales[2]);
printf("Minerales 4: %d\n", nave->bodegaMinerales[3]);
     pthread_mutex_unlock(&nave->mutex);
     usleep(100000);
     printf("Tiempo: %d segundos\n", counter);


counter++;
}   
return NULL;
}
