#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>

struct Asteroides {
    int posX;
    int posY;
    sem_t sem_mutex;
    int Mutexio;
    int semaforita;
    int kernelio;
    int Deuterio;
};



struct Asteroides mapa_asteroides[100];
int contador_asteroides = 1;
void asignar_valores(struct Asteroides *asteroide);
void* hilo_extraccion(void* arg);

int main() {
    
    int cantidad_asteroides = 0;
    char linea[256]; // Aquí guardaremos cada renglón que vayamos leyendo

    // 1. Abrimos tu archivo de configuración
    FILE *archivo = fopen("config.txt", "r");

    if (archivo == NULL) {
        printf("Error: No se pudo abrir el archivo de configuracion.\n");
        return 1;
    }

    // 2. Leemos el archivo renglón por renglón
    // fgets toma el renglón y lo guarda en la variable 'linea'
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        
        // 3. Buscamos nuestro patrón exacto en la línea actual
        // Le decimos: "Busca 'CANT_ASTEROIDES=' y guárdame el entero (%d) que le sigue"
        if (sscanf(linea, "CANT_ASTEROIDES=%d", &cantidad_asteroides) == 1) {
            // Si sscanf devuelve 1, significa que encontró exitosamente la variable
            break; // ¡Ya encontramos lo que buscábamos! Rompemos el ciclo para no leer de más
        }
    }

    // 4. Cerramos el archivo
    fclose(archivo);

    // Solo para comprobar que funcionó:
    printf("Cantidad de asteroides leida del archivo: %d\n", cantidad_asteroides);
    
    for (int i = 0; i < cantidad_asteroides; i++) {
        asignar_valores(&mapa_asteroides[i]);
    }

   
    return 0;
}

//fabrica
void asignar_valores(struct Asteroides *asteroide) {
    //para un numero random entre 0 y 9, para asignar la posicion del asteroide, se usa la funcion rand() y el operador modulo
    asteroide->posX = rand() % 10; //aca 
    asteroide->posY = rand() % 10; //aca
    asteroide->Deuterio = rand() % 100 + 1; //aca, para que no sea 0, se le suma 1
    asteroide->Mutexio = rand() % 100 + 1; //aca, para que no sea 0, se le suma 1
    asteroide->semaforita = rand() % 100 + 1; //aca, para que no sea 0, se le suma 1
    asteroide->kernelio = rand() % 100 + 1; //aca, para que no sea 0, se le suma 1


    
    //para imprimir el numeo del asterioide que va a ser impreso, 
    printf("Asignando valores al asteroide numero %d\n", contador_asteroides);
    printf("posX: %d\n", asteroide->posX);
    printf("posY: %d\n", asteroide->posY);
    printf("Deuterio: %d\n", asteroide->Deuterio);
    printf("Mutexio: %d\n", asteroide->Mutexio);
    printf("semaforita: %d\n", asteroide->semaforita);
    printf("kernelio: %d\n", asteroide->kernelio);
    printf("Inicializando semáforo del asteroide...\n");
    
    //salto de linea , se usa
    printf("\n");
    printf("--------------------------------------------------------------\n");
    sem_init(&asteroide->sem_mutex, 1, 1);
    contador_asteroides++;
}














