#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define PERMISOS 0644
#define FILAS 3
#define COLUMNAS 9

// Estructura para compartir datos
typedef struct {
    int matriz[FILAS][COLUMNAS];
    int resultados[FILAS];
    int resultados_listos[FILAS];
} DatosCompartidos;

// Función para crear semáforo
int Crea_semaforo(key_t llave, int valor_inicial) {
    int semid = semget(llave, 1, IPC_CREAT | PERMISOS);
    if (semid == -1) {
        return -1;
    }
    semctl(semid, 0, SETVAL, valor_inicial);
    return semid;
}

// DOWN
void down(int semid) {
    struct sembuf op_p[] = {0, -1, 0};
    semop(semid, op_p, 1);
}

// UP
void up(int semid) {
    struct sembuf op_v[] = {0, +1, 0};
    semop(semid, op_v, 1);
}

int main() {
    int shmid, semid;
    DatosCompartidos *datos;
    key_t llave_mem, llave_sem;
    pid_t pid1, pid2, pid3;
    int i;

    // Crear llaves para memoria compartida y semáforo
    llave_mem = ftok("archivo_mem", 'm');
    llave_sem = ftok("archivo_sem", 's');

    // Crear memoria compartida
    shmid = shmget(llave_mem, sizeof(DatosCompartidos), IPC_CREAT | PERMISOS);
    if (shmid == -1) {
        perror("Error al crear memoria compartida");
        exit(-1);
    }

    // Adjuntar memoria compartida
    datos = (DatosCompartidos *)shmat(shmid, 0, 0);
    if (datos == (void *)-1) {
        perror("Error al adjuntar memoria compartida");
        exit(-1);
    }

    // Crear semáforo
    semid = Crea_semaforo(llave_sem, 1);
    if (semid == -1) {
        perror("Error al crear semáforo");
        exit(-1);
    }

    // Inicializar banderas de resultados
    for (i = 0; i < FILAS; i++) {
        datos->resultados_listos[i] = 0;
    }

    // PROCESO PADRE: Llenar la matriz
    printf("\n========== PROCESO PADRE ==========\n");
    printf("PID Padre: %d\n", getpid());
    printf("\nInicializando matriz 9x3:\n");

    printf("\n");
    // Fila 1 (Numeros Normales)
    for(i = 0; i < COLUMNAS; i++){
        datos -> matriz[0][i] = i+1;
        printf("\t%d", datos -> matriz[0][i]);
    }

    printf("\n");
    // Fila 2 (Numeros Impares)
    for(i = 0; i < COLUMNAS; i++){
        datos -> matriz[1][i] = (i+1)*2-1;
        printf("\t%d", datos -> matriz[1][i]);
    }

    printf("\n");
    // Fila 3 (Numeros Pares)
    for(i = 0; i < COLUMNAS; i++){
        datos -> matriz[2][i] = (i+1)*2;
        printf("\t%d", datos -> matriz[2][i]);
    }

    printf("\n");

    // Crear tres procesos hijos
    printf("\n========== CREANDO PROCESOS HIJOS ==========\n");

    // HIJO 1 - Procesa fila 0
    pid1 = fork();
    if (pid1 == -1) {
        perror("Error al crear proceso hijo 1");
        exit(-1);
    }

    if (pid1 == 0) {
        // Código del hijo 1
        printf("\n--- HIJO 1 ---\n");
        printf("Mi PID: %d\n", getpid());
        printf("PID de mi padre: %d\n", getppid());
        
        int suma = 0;

        printf("\n Suma = ");
        for (i = 0; i < COLUMNAS; i++){
            suma += datos -> matriz[0][i];
            printf("%d", datos -> matriz[0][i]);
            if(i < COLUMNAS - 1) printf(" + ");
        }

        // Envio resultados y terminacion hilo
        down(semid);
        datos->resultados[0] = suma;
        datos->resultados_listos[0] = 1;
        printf(" = %d (guardado en memoria)\n", suma);
        up(semid);
        
        sleep(1);
        
        shmdt(datos);
        exit(0);
    }

    // HIJO 2 - Procesa fila 1
    pid2 = fork();
    if (pid2 == -1) {
        perror("Error al crear proceso hijo 2");
        exit(-1);
    }

    if (pid2 == 0) {
        // Código del hijo 2
        printf("\n--- HIJO 2 ---\n");
        printf("Mi PID: %d\n", getpid());
        printf("PID de mi padre: %d\n", getppid());
        
        int suma = 0;

        printf("\n Suma = ");
        for (i = 0; i < COLUMNAS; i++){
            suma += datos -> matriz[1][i];
            printf("%d", datos -> matriz[1][i]);
            if(i < COLUMNAS - 1) printf(" + ");
        }

        // Envio resultados y terminación hilo
        down(semid);
        datos->resultados[1] = suma;
        datos->resultados_listos[1] = 1;
        printf(" = %d (guardado en memoria)\n", suma);
        up(semid);
        
        sleep(1);
        
        shmdt(datos);
        exit(0);
    }

    // HIJO 3 - Procesa fila 2
    pid3 = fork();
    if (pid3 == -1) {
        perror("Error al crear proceso hijo 3");
        exit(-1);
    }

    if (pid3 == 0) {
        // Código del hijo 3
        printf("\n--- HIJO 3 ---\n");
        printf("Mi PID: %d\n", getpid());
        printf("PID de mi padre: %d\n", getppid());
        
        int suma = 0;

        printf("\n Suma = ");
        for (i = 0; i < COLUMNAS; i++){
            suma += datos -> matriz[2][i];
            printf("%d", datos -> matriz[2][i]);
            if(i < COLUMNAS - 1) printf(" + ");
        }

        // Envio resultados y terminacion hilo
        down(semid);
        datos->resultados[2] = suma;
        datos->resultados_listos[2] = 1;
        printf(" = %d (guardado en memoria)\n", suma);
        up(semid);
        
        sleep(1);
        
        shmdt(datos);
        exit(0);
    }

    // Esperar a que todos los hijos terminen
    wait(NULL);
    wait(NULL);
    wait(NULL);

    // Leer e imprimir resultados
    printf("\n========== RESULTADOS FINALES ==========\n");
    printf("Padre leyendo resultados de memoria compartida:\n\n");

    for (i = 0; i < FILAS; i++) {
        down(semid);
        if (datos->resultados_listos[i]) {
            printf("Fila %d: Suma = %d\n", i, datos->resultados[i]);
        }
        up(semid);
    }

    printf("\n========== PROCESO COMPLETADO ==========\n");

    // Limpiar recursos
    shmdt(datos);
    shmctl(shmid, IPC_RMID, 0);
    semctl(semid, 0, IPC_RMID, 0);


    return 0;
}