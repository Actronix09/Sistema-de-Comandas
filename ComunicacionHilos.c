#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define FILAS 3
#define COLUMNAS 9

// Estructura para compartir datos entre hilos
typedef struct {
    int matriz[FILAS][COLUMNAS];
    int resultados[FILAS];
    int resultados_listos[FILAS];
} DatosCompartidos;

// Estructura para pasar argumentos a cada hilo
typedef struct {
    DatosCompartidos *datos;
    int fila;
    pthread_mutex_t *mutex;
} ArgumentosHilo;

// Función que ejecutará cada hilo
void *ProcesarFila(void *argumentos) {
    ArgumentosHilo *args = (ArgumentosHilo*)argumentos;
    int i;
    int suma = 0;
    
    printf("\n--- HILO %ld (Fila %d) ---\n", pthread_self(), args->fila);
    printf("Procesando fila %d\n", args->fila);
    
    printf("\n Suma = ");
    for (i = 0; i < COLUMNAS; i++) {
        suma += args->datos->matriz[args->fila][i];
        printf("%d", args->datos->matriz[args->fila][i]);
        if(i < COLUMNAS - 1) printf(" + ");
    }
    
    // Sección crítica: guardar resultados
    pthread_mutex_lock(args->mutex);
    args->datos->resultados[args->fila] = suma;
    args->datos->resultados_listos[args->fila] = 1;
    printf(" = %d (guardado en memoria)\n", suma);
    pthread_mutex_unlock(args->mutex);
    
    sleep(1);
    
    pthread_exit(NULL);
}

int main() {
    pthread_t hilos[FILAS];
    pthread_mutex_t mutex;
    DatosCompartidos datos;
    ArgumentosHilo args[FILAS];
    int i, j;

    // Inicializar mutex
    pthread_mutex_init(&mutex, NULL);

    // Inicializar banderas de resultados
    for (i = 0; i < FILAS; i++) {
        datos.resultados_listos[i] = 0;
    }

    // HILO PRINCIPAL: Llenar la matriz
    printf("\n========== HILO PRINCIPAL ==========\n");
    printf("PID del proceso: %d\n", getpid());
    printf("ID del hilo principal: %ld\n", pthread_self());
    printf("\nInicializando matriz %dx%d:\n", COLUMNAS, FILAS);

    printf("\n");
    // Fila 0 (Números Normales: 1, 2, 3, ...)
    for(i = 0; i < COLUMNAS; i++){
        datos.matriz[0][i] = i + 1;
        printf("\t%d", datos.matriz[0][i]);
    }

    printf("\n");
    // Fila 1 (Números Impares: 1, 3, 5, ...)
    for(i = 0; i < COLUMNAS; i++){
        datos.matriz[1][i] = (i + 1) * 2 - 1;
        printf("\t%d", datos.matriz[1][i]);
    }

    printf("\n");
    // Fila 2 (Números Pares: 2, 4, 6, ...)
    for(i = 0; i < COLUMNAS; i++){
        datos.matriz[2][i] = (i + 1) * 2;
        printf("\t%d", datos.matriz[2][i]);
    }

    printf("\n");

    // Crear tres hilos
    printf("\n========== CREANDO HILOS ==========\n");

    for (i = 0; i < FILAS; i++) {
        args[i].datos = &datos;
        args[i].fila = i;
        args[i].mutex = &mutex;
        
        if (pthread_create(&hilos[i], NULL, ProcesarFila, (void*)&args[i]) != 0) {
            fprintf(stderr, "Error al crear hilo %d\n", i);
            exit(-1);
        }
        printf("Hilo %d creado con ID: %ld\n", i, hilos[i]);
    }

    // Esperar a que todos los hilos terminen
    printf("\n========== ESPERANDO HILOS ==========\n");
    for (i = 0; i < FILAS; i++) {
        pthread_join(hilos[i], NULL);
        printf("Hilo %d finalizado\n", i);
    }

    // Leer e imprimir resultados
    printf("\n========== RESULTADOS FINALES ==========\n");
    printf("Hilo principal leyendo resultados:\n\n");

    for (i = 0; i < FILAS; i++) {
        if (datos.resultados_listos[i]) {
            printf("Fila %d: Suma = %d\n", i, datos.resultados[i]);
        }
    }

    printf("\n========== PROCESO COMPLETADO ==========\n");

    // Destruir mutex
    pthread_mutex_destroy(&mutex);

    return 0;
}