#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>

#define MAX_LOG_QUEUE 100
#define MAX_LOG_MESSAGE 512

// Tipos de eventos de autenticación
typedef enum {
    LOG_CLIENTE_CONECTADO,
    LOG_CLIENTE_DESCONECTADO,
    LOG_LOGIN_EXITOSO,
    LOG_LOGIN_FALLIDO,
    LOG_USUARIO_CREADO,
    LOG_USUARIO_EXISTE,
    LOG_PASSWORD_RECUPERADA,
    LOG_PASSWORD_FALLIDA
} TipoEventoAuth;

// Tipos de eventos de cocina
typedef enum {
    LOG_PEDIDO_CREADO,
    LOG_PEDIDO_EN_PROGRESO,
    LOG_PEDIDO_COMPLETADO,
    LOG_PEDIDO_ERROR
} TipoEventoCocina;

// Estructura de evento de autenticación
typedef struct {
    TipoEventoAuth tipo;
    time_t timestamp;
    char usuario[100];
    char detalles[256];
    int cliente_id;
} EventoAuth;

// Estructura de evento de cocina
typedef struct {
    TipoEventoCocina tipo;
    time_t timestamp;
    int pedido_id;
    char mesa[50];
    char mesero[100];
    int num_items;
    char detalles[256];
} EventoCocina;

// Colas circulares para los logs
typedef struct {
    EventoAuth eventos[MAX_LOG_QUEUE];
    int inicio;
    int fin;
    int count;
} ColaAuth;

typedef struct {
    EventoCocina eventos[MAX_LOG_QUEUE];
    int inicio;
    int fin;
    int count;
} ColaCocina;

// Variables globales para el sistema de log
extern ColaAuth cola_auth;
extern ColaCocina cola_cocina;
extern int sem_auth;
extern int sem_cocina;
extern int logger_activo;

// Funciones de inicialización
void logger_init();
void logger_cleanup();

// Funciones para encolar eventos
void log_evento_auth(TipoEventoAuth tipo, const char* usuario, const char* detalles, int cliente_id);
void log_evento_cocina(TipoEventoCocina tipo, int pedido_id, const char* mesa, const char* mesero, int num_items, const char* detalles);

// Hilos de logging
void* hilo_logger_auth(void* arg);
void* hilo_logger_cocina(void* arg);

// Funciones auxiliares
const char* obtener_nombre_evento_auth(TipoEventoAuth tipo);
const char* obtener_nombre_evento_cocina(TipoEventoCocina tipo);
void obtener_timestamp_formateado(time_t timestamp, char* buffer, int size);

#endif