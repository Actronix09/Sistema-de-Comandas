#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/sem.h>

// Variables globales
ColaAuth cola_auth = {.inicio = 0, .fin = 0, .count = 0};
ColaCocina cola_cocina = {.inicio = 0, .fin = 0, .count = 0};
int sem_auth = -1;
int sem_cocina = -1;
int logger_activo = 1;

// Mutex para proteger las colas
static pthread_mutex_t mutex_auth = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_cocina = PTHREAD_MUTEX_INITIALIZER;

// Inicializar sistema de logging
void logger_init() {
    logger_activo = 1;
    
    // Crear archivos de log si no existen
    FILE* f_auth = fopen("autenticacion.log", "a");
    FILE* f_cocina = fopen("cocina.log", "a");
    
    if (f_auth) {
        fprintf(f_auth, "\n========================================\n");
        fprintf(f_auth, "NUEVO INICIO DE SERVIDOR\n");
        time_t now = time(NULL);
        char timestamp[64];
        obtener_timestamp_formateado(now, timestamp, sizeof(timestamp));
        fprintf(f_auth, "Fecha: %s\n", timestamp);
        fprintf(f_auth, "========================================\n\n");
        fclose(f_auth);
    }
    
    if (f_cocina) {
        fprintf(f_cocina, "\n========================================\n");
        fprintf(f_cocina, "NUEVO INICIO DE SERVIDOR\n");
        time_t now = time(NULL);
        char timestamp[64];
        obtener_timestamp_formateado(now, timestamp, sizeof(timestamp));
        fprintf(f_cocina, "Fecha: %s\n", timestamp);
        fprintf(f_cocina, "========================================\n\n");
        fclose(f_cocina);
    }
}

// Limpiar sistema de logging
void logger_cleanup() {
    logger_activo = 0;
    
    FILE* f_auth = fopen("autenticacion.log", "a");
    FILE* f_cocina = fopen("cocina.log", "a");
    
    if (f_auth) {
        fprintf(f_auth, "\n========================================\n");
        fprintf(f_auth, "SERVIDOR DETENIDO\n");
        time_t now = time(NULL);
        char timestamp[64];
        obtener_timestamp_formateado(now, timestamp, sizeof(timestamp));
        fprintf(f_auth, "Fecha: %s\n", timestamp);
        fprintf(f_auth, "========================================\n\n");
        fclose(f_auth);
    }
    
    if (f_cocina) {
        fprintf(f_cocina, "\n========================================\n");
        fprintf(f_cocina, "SERVIDOR DETENIDO\n");
        time_t now = time(NULL);
        char timestamp[64];
        obtener_timestamp_formateado(now, timestamp, sizeof(timestamp));
        fprintf(f_cocina, "Fecha: %s\n", timestamp);
        fprintf(f_cocina, "========================================\n\n");
        fclose(f_cocina);
    }
}

// Encolar evento de autenticación
void log_evento_auth(TipoEventoAuth tipo, const char* usuario, const char* detalles, int cliente_id) {
    pthread_mutex_lock(&mutex_auth);
    
    if (cola_auth.count < MAX_LOG_QUEUE) {
        EventoAuth evento;
        evento.tipo = tipo;
        evento.timestamp = time(NULL);
        strncpy(evento.usuario, usuario ? usuario : "N/A", 99);
        evento.usuario[99] = '\0';
        strncpy(evento.detalles, detalles ? detalles : "", 255);
        evento.detalles[255] = '\0';
        evento.cliente_id = cliente_id;
        
        cola_auth.eventos[cola_auth.fin] = evento;
        cola_auth.fin = (cola_auth.fin + 1) % MAX_LOG_QUEUE;
        cola_auth.count++;
    }
    
    pthread_mutex_unlock(&mutex_auth);
}

// Encolar evento de cocina
void log_evento_cocina(TipoEventoCocina tipo, int pedido_id, const char* mesa, const char* mesero, int num_items, const char* detalles) {
    pthread_mutex_lock(&mutex_cocina);
    
    if (cola_cocina.count < MAX_LOG_QUEUE) {
        EventoCocina evento;
        evento.tipo = tipo;
        evento.timestamp = time(NULL);
        evento.pedido_id = pedido_id;
        strncpy(evento.mesa, mesa ? mesa : "N/A", 49);
        evento.mesa[49] = '\0';
        strncpy(evento.mesero, mesero ? mesero : "N/A", 99);
        evento.mesero[99] = '\0';
        evento.num_items = num_items;
        strncpy(evento.detalles, detalles ? detalles : "", 255);
        evento.detalles[255] = '\0';
        
        cola_cocina.eventos[cola_cocina.fin] = evento;
        cola_cocina.fin = (cola_cocina.fin + 1) % MAX_LOG_QUEUE;
        cola_cocina.count++;
    }
    
    pthread_mutex_unlock(&mutex_cocina);
}

// Obtener nombre del evento de autenticación
const char* obtener_nombre_evento_auth(TipoEventoAuth tipo) {
    switch(tipo) {
        case LOG_CLIENTE_CONECTADO: return "CLIENTE_CONECTADO";
        case LOG_CLIENTE_DESCONECTADO: return "CLIENTE_DESCONECTADO";
        case LOG_LOGIN_EXITOSO: return "LOGIN_EXITOSO";
        case LOG_LOGIN_FALLIDO: return "LOGIN_FALLIDO";
        case LOG_USUARIO_CREADO: return "USUARIO_CREADO";
        case LOG_USUARIO_EXISTE: return "USUARIO_YA_EXISTE";
        case LOG_PASSWORD_RECUPERADA: return "PASSWORD_RECUPERADA";
        case LOG_PASSWORD_FALLIDA: return "PASSWORD_RECUPERACION_FALLIDA";
        default: return "DESCONOCIDO";
    }
}

// Obtener nombre del evento de cocina
const char* obtener_nombre_evento_cocina(TipoEventoCocina tipo) {
    switch(tipo) {
        case LOG_PEDIDO_CREADO: return "PEDIDO_CREADO";
        case LOG_PEDIDO_EN_PROGRESO: return "PEDIDO_EN_PROGRESO";
        case LOG_PEDIDO_COMPLETADO: return "PEDIDO_COMPLETADO";
        case LOG_PEDIDO_ERROR: return "PEDIDO_ERROR";
        default: return "DESCONOCIDO";
    }
}

// Formatear timestamp
void obtener_timestamp_formateado(time_t timestamp, char* buffer, int size) {
    struct tm* tm_info = localtime(&timestamp);
    strftime(buffer, size, "%d-%m-%Y %H:%M:%S", tm_info);
}

// Hilo para logging de autenticación
void* hilo_logger_auth(void* arg) {
    FILE* archivo = NULL;
    
    printf("Hilo de logging de autenticacion iniciado (ID: %ld)\n", pthread_self());
    
    while (logger_activo || cola_auth.count > 0) {
        pthread_mutex_lock(&mutex_auth);
        
        if (cola_auth.count > 0) {
            EventoAuth evento = cola_auth.eventos[cola_auth.inicio];
            cola_auth.inicio = (cola_auth.inicio + 1) % MAX_LOG_QUEUE;
            cola_auth.count--;
            pthread_mutex_unlock(&mutex_auth);
            
            // Abrir archivo en modo append
            archivo = fopen("autenticacion.log", "a");
            if (archivo) {
                char timestamp[64];
                obtener_timestamp_formateado(evento.timestamp, timestamp, sizeof(timestamp));
                
                fprintf(archivo, "[%s] ", timestamp);
                fprintf(archivo, "[%s] ", obtener_nombre_evento_auth(evento.tipo));
                
                if (strlen(evento.usuario) > 0 && strcmp(evento.usuario, "N/A") != 0) {
                    fprintf(archivo, "Usuario: %s | ", evento.usuario);
                }
                
                if (evento.cliente_id > 0) {
                    fprintf(archivo, "Cliente ID: %d | ", evento.cliente_id);
                }
                
                if (strlen(evento.detalles) > 0) {
                    fprintf(archivo, "%s", evento.detalles);
                }
                
                fprintf(archivo, "\n");
                fflush(archivo);
                fclose(archivo);
            }
        } else {
            pthread_mutex_unlock(&mutex_auth);
            usleep(100000); // Dormir 100ms si no hay eventos
        }
    }
    
    printf("Hilo de logging de autenticacion finalizado\n");
    pthread_exit(NULL);
}

// Hilo para logging de cocina
void* hilo_logger_cocina(void* arg) {
    FILE* archivo = NULL;
    
    printf("Hilo de logging de cocina iniciado (ID: %ld)\n", pthread_self());
    
    while (logger_activo || cola_cocina.count > 0) {
        pthread_mutex_lock(&mutex_cocina);
        
        if (cola_cocina.count > 0) {
            EventoCocina evento = cola_cocina.eventos[cola_cocina.inicio];
            cola_cocina.inicio = (cola_cocina.inicio + 1) % MAX_LOG_QUEUE;
            cola_cocina.count--;
            pthread_mutex_unlock(&mutex_cocina);
            
            // Abrir archivo en modo append
            archivo = fopen("cocina.log", "a");
            if (archivo) {
                char timestamp[64];
                obtener_timestamp_formateado(evento.timestamp, timestamp, sizeof(timestamp));
                
                fprintf(archivo, "[%s] ", timestamp);
                fprintf(archivo, "[%s] ", obtener_nombre_evento_cocina(evento.tipo));
                
                if (evento.pedido_id > 0) {
                    fprintf(archivo, "Pedido #%d | ", evento.pedido_id);
                }
                
                if (strlen(evento.mesa) > 0 && strcmp(evento.mesa, "N/A") != 0) {
                    fprintf(archivo, "Mesa: %s | ", evento.mesa);
                }
                
                if (strlen(evento.mesero) > 0 && strcmp(evento.mesero, "N/A") != 0) {
                    fprintf(archivo, "Mesero: %s | ", evento.mesero);
                }
                
                if (evento.num_items > 0) {
                    fprintf(archivo, "Items: %d | ", evento.num_items);
                }
                
                if (strlen(evento.detalles) > 0) {
                    fprintf(archivo, "%s", evento.detalles);
                }
                
                fprintf(archivo, "\n");
                fflush(archivo);
                fclose(archivo);
            }
        } else {
            pthread_mutex_unlock(&mutex_cocina);
            usleep(100000); // Dormir 100ms si no hay eventos
        }
    }
    
    printf("Hilo de logging de cocina finalizado\n");
    pthread_exit(NULL);
}