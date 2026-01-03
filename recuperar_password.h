#ifndef RECUPERAR_PASSWORD_H
#define RECUPERAR_PASSWORD_H

#include <time.h>
#include "usuario.h"

// Estructura para manejar tokens de recuperación de contraseña
typedef struct {
    char user[MAX_CHAIN_SIZE];
    char token[33]; // Para almacenar un hash SHA256 en hexadecimal
    time_t timestamp;
    int usado;
} TokenRecuperacion;

// Funciones para la recuperación de contraseña
int generar_token_recuperacion(const char* user);
int validar_token_recuperacion(const char* user, const char* token);
int cambiar_password_por_token(const char* user, const char* token, const char* new_pass);
int enviar_notificacion_recuperacion(const char* user);

#endif // RECUPERAR_PASSWORD_H