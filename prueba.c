#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>


struct Asteroides {
    
    int posX, posY;
    sem_t sem_mutex;
   // int flag;
   int Mutexio;
   int semaforita;
    int kernelio;
    int   Deuterio;
};

struct Nave{ 

    int combustible;
    int posX, posY;
    int bodegaMinerales[4];
};

   struct Asteroides mapa_asteroides[100];
void asignar_valores(struct Asteroides *asteroide);
void hilo_extraccion(void* arg, struct Asteroides *asteroide);

int main() {
    //para que sea una variab
     

    // 2. La Línea de Ensamblaje
    for (int i = 0; i < 5; i++) {
        // Delegamos la asignación enviando la dirección de memoria
        asignar_valores(&mapa_asteroides[i]); 
    }

    
    struct Nave nave;

    //la cantidad de naves es 
    /*Inicializa la nave*/
    nave.combustible = 2;
    nave.posX = 5;
    nave.posY = 5;
    for (int i = 0; i < 4; i++) {
        nave.bodegaMinerales[i] = 0;
    }

    //&nave es la dirección de memoria de la nave, que se pasa al hilo para que pueda modificar sus datos
    hilo_extraccion(&nave, &mapa_asteroides[0]);
    //para llamar a la funcion 
    
    return 0;



}


void asignar_valores(struct Asteroides *asteroide) {
    // Validación de espacio vacío
   
    //no sobreponer
        asteroide->posX = 3;
        asteroide->posY = 3;

        //random
    asteroide->Deuterio = 100;
    asteroide->Mutexio = 1;
    asteroide->semaforita = 1;
    asteroide->kernelio = 1;

    //%d es el formato para imprimir enteros
    printf("Asignando valores al asteroide...\n");
    printf("posX: %d\n", asteroide->posX);
    printf("posY: %d\n", asteroide->posY);
    printf("Deuterio: %d\n", asteroide->Deuterio);
    printf("Mutexio: %d\n", asteroide->Mutexio);
    printf("semaforita: %d\n", asteroide->semaforita);
    printf("kernelio: %d\n", asteroide->kernelio);
    printf("Inicializando semáforo del asteroide...\n");

    //asteroide->flag = 1;
    sem_init(&asteroide->sem_mutex, 1, 1);

    
}



void hilo_extraccion(void* arg, struct Asteroides *asteroide){
    struct Nave* nave = (struct Nave*) arg;
    

  
    //nave-> asteroide
    //el hilo recibe la nave, y el asteroide al cual va a minar

            //extraer
            int indiceMIneral;
            asteroide = &asteroide[0]; // Asumiendo que estás minando el primer asteroide del arreglo
            asteroide->Deuterio = 4;

   for(int i =0; i<10; i++){


     if (nave->combustible <= 0) {
            printf("Extracción: no hay combustible.\n");

            asignar_valores(&mapa_asteroides[6] ); // ‘mapa_asteroides’ undeclared (first use in this function); did you mean ‘Asteroides’?
            //el error se debe a que el arreglo mapa_asteroides no está declarado dentro de esta función, por lo que no es accesible. Para solucionar esto, puedes pasar el arreglo mapa_asteroides como argumento a la función hilo_extraccion, o declarar el arreglo como una variable global para que sea accesible desde cualquier función.
            break; // Sale del bucle si no hay combustible
        }

       
    
            indiceMIneral = i;
            nave->bodegaMinerales[indiceMIneral] ++;        
            asteroide->Deuterio -= 1; // Simula la extracción de un mineral del asteroide
            

            //Imprimir recursos extraidos
            printf("Extracción: numero %d, Combustible restante: %d\n", i, nave->combustible);
            printf("Minerales en bodega: %d, %d, %d, %d\n", nave->bodegaMinerales[0], nave->bodegaMinerales[1], nave->bodegaMinerales[2], nave->bodegaMinerales[3]);
            //imprimir ultimos datos del asteroide

            printf("Datos del asteroide después de la extracción:\n");
            printf("Deuterio: %d\n", asteroide->Deuterio);
            printf("Mutexio: %d\n", asteroide->Mutexio);
            printf("semaforita: %d\n", asteroide->semaforita);
            printf("kernelio: %d\n", asteroide->kernelio);
            printf("///////////////////////////////////////////\n");


         if (asteroide->Deuterio == 0) {
            printf("El asteroide se ha agotado.\n");
                break; // Sale del bucle si el asteroide se ha agotado
            }

        nave->combustible -= 1; // Simula el consumo de combustible por la extracción 
   }

   
 
   


}
