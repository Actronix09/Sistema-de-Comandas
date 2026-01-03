#include "recuperar_password.h"
#include "usuario.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <unistd.h>

// Función para generar un hash SHA256
void hash_string(const char* input, char* output) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_sha256();
    
    EVP_DigestInit_ex(ctx, md, NULL);
    EVP_DigestUpdate(ctx, input, strlen(input));
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);
    
    // Convertir a hexadecimal
    for(unsigned int i = 0; i < digest_len; i++) {
        sprintf(output + (i * 2), "%02x", digest[i]);
    }
    output[digest_len * 2] = '\0';
}

// Función para generar un token aleatorio
void generar_token_aleatorio(char* token, size_t length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for(size_t i = 0; i < length - 1; i++) {
        int key = rand() % (int)(sizeof(charset) - 1);
        token[i] = charset[key];
    }
    token[length - 1] = '\0';
}

// Guardar token en archivo
int guardar_token(const TokenRecuperacion* token_rec) {
    FILE* archivo = fopen("tokens_recuperacion.txt", "a");
    if (!archivo) {
        return -1;
    }
    
    fprintf(archivo, "%s|%s|%ld|%d\n", 
            token_rec->user, 
            token_rec->token, 
            (long)token_rec->timestamp, 
            token_rec->usado);
    
    fclose(archivo);
    return 0;
}

// Buscar token en archivo
TokenRecuperacion* buscar_token(const char* user, const char* token) {
    FILE* archivo = fopen("tokens_recuperacion.txt", "r");
    if (!archivo) {
        return NULL;
    }
    
    TokenRecuperacion* temp_token = malloc(sizeof(TokenRecuperacion));
    char linea[256];
    
    while (fgets(linea, sizeof(linea), archivo)) {
        // Parsear la línea
        char* user_guardado = strtok(linea, "|");
        char* token_guardado = strtok(NULL, "|");
        char* timestamp_str = strtok(NULL, "|");
        char* usado_str = strtok(NULL, "|");
        
        if (user_guardado && token_guardado && timestamp_str && usado_str) {
            // Remover salto de línea del último campo
            if (usado_str[strlen(usado_str)-1] == '\n') {
                usado_str[strlen(usado_str)-1] = '\0';
            }
            
            if (strcmp(user_guardado, user) == 0 && strcmp(token_guardado, token) == 0) {
                strcpy(temp_token->user, user_guardado);
                strcpy(temp_token->token, token_guardado);
                temp_token->timestamp = atol(timestamp_str);
                temp_token->usado = atoi(usado_str);
                
                fclose(archivo);
                return temp_token;
            }
        }
    }
    
    fclose(archivo);
    free(temp_token);
    return NULL;
}

// Marcar token como usado
int marcar_token_usado(const char* user, const char* token) {
    FILE* archivo_lectura = fopen("tokens_recuperacion.txt", "r");
    if (!archivo_lectura) {
        return -1;
    }
    
    FILE* archivo_temp = fopen("tokens_temp.txt", "w");
    if (!archivo_temp) {
        fclose(archivo_lectura);
        return -1;
    }
    
    char linea[256];
    int token_actualizado = 0;
    
    while (fgets(linea, sizeof(linea), archivo_lectura)) {
        char* user_guardado = strtok(linea, "|");
        char* token_guardado = strtok(NULL, "|");
        char* timestamp_str = strtok(NULL, "|");
        char* usado_str = strtok(NULL, "|");
        
        if (user_guardado && token_guardado && timestamp_str && usado_str) {
            // Remover salto de línea del último campo
            if (usado_str[strlen(usado_str)-1] == '\n') {
                usado_str[strlen(usado_str)-1] = '\0';
            }
            
            if (strcmp(user_guardado, user) == 0 && strcmp(token_guardado, token) == 0) {
                fprintf(archivo_temp, "%s|%s|%s|%d\n", 
                        user_guardado, token_guardado, timestamp_str, 1);
                token_actualizado = 1;
            } else {
                fprintf(archivo_temp, "%s|%s|%s|%s", 
                        user_guardado, token_guardado, timestamp_str, usado_str);
                if (linea[strlen(linea)-1] != '\n') {
                    fprintf(archivo_temp, "\n");
                }
            }
        }
    }
    
    fclose(archivo_lectura);
    fclose(archivo_temp);
    
    // Reemplazar archivo original
    if (token_actualizado) {
        remove("tokens_recuperacion.txt");
        rename("tokens_temp.txt", "tokens_recuperacion.txt");
    } else {
        remove("tokens_temp.txt");
        return -1;
    }
    
    return token_actualizado ? 0 : -1;
}

// Generar token de recuperación
int generar_token_recuperacion(const char* user) {
    // Verificar que el usuario existe
    if (!usuario_existe(user)) {
        // No se puede usar logger_escribir, se registrará en el log de autenticación
        log_evento_auth(LOG_PASSWORD_FALLIDA, user, "Usuario no existe para recuperación de contraseña", 0);
        return -1;
    }

    TokenRecuperacion token_rec;
    strcpy(token_rec.user, user);
    token_rec.timestamp = time(NULL);
    token_rec.usado = 0;

    // Generar token aleatorio
    char token_temp[33];
    generar_token_aleatorio(token_temp, 32);

    // Crear hash del token aleatorio para mayor seguridad
    hash_string(token_temp, token_rec.token);

    // Guardar token
    if (guardar_token(&token_rec) != 0) {
        log_evento_auth(LOG_PASSWORD_FALLIDA, user, "No se pudo guardar el token de recuperación", 0);
        return -1;
    }

    // Enviar notificación (simulado)
    enviar_notificacion_recuperacion(user);

    log_evento_auth(LOG_PASSWORD_RECUPERADA, user, "Se generó token de recuperación", 0);
    return 0;
}

// Validar token de recuperación
int validar_token_recuperacion(const char* user, const char* token) {
    TokenRecuperacion* token_encontrado = buscar_token(user, token);

    if (!token_encontrado) {
        log_evento_auth(LOG_PASSWORD_FALLIDA, user, "Token no encontrado", 0);
        free(token_encontrado);
        return -1;
    }

    // Verificar si ya fue usado
    if (token_encontrado->usado) {
        log_evento_auth(LOG_PASSWORD_FALLIDA, user, "Token ya fue usado", 0);
        free(token_encontrado);
        return -2;
    }

    // Verificar expiración (1 hora)
    time_t ahora = time(NULL);
    if (ahora - token_encontrado->timestamp > 3600) { // 1 hora en segundos
        log_evento_auth(LOG_PASSWORD_FALLIDA, user, "Token expirado", 0);
        free(token_encontrado);
        return -3;
    }

    free(token_encontrado);
    return 0;
}

// Cambiar contraseña usando token
int cambiar_password_por_token(const char* user, const char* token, const char* new_pass) {
    // Validar token
    int validacion = validar_token_recuperacion(user, token);
    if (validacion != 0) {
        return validacion;
    }

    // Validar nueva contraseña
    if (validar_password(new_pass) != 0) {
        log_evento_auth(LOG_PASSWORD_FALLIDA, user, "Nueva contraseña inválida", 0);
        return -4;
    }

    // Cambiar contraseña
    int resultado = usuario_recuperar_pass(user, new_pass);
    if (resultado != 0) {
        log_evento_auth(LOG_PASSWORD_FALLIDA, user, "No se pudo cambiar la contraseña", 0);
        return -5;
    }

    // Marcar token como usado
    if (marcar_token_usado(user, token) != 0) {
        log_evento_auth(LOG_PASSWORD_FALLIDA, user, "No se pudo marcar token como usado", 0);
        return -6;
    }

    log_evento_auth(LOG_PASSWORD_RECUPERADA, user, "Contraseña cambiada exitosamente", 0);
    return 0;
}

// Enviar notificación de recuperación (simulado)
int enviar_notificacion_recuperacion(const char* user) {
    // En un sistema real, aquí se enviaría un correo o notificación
    // Por ahora, solo registramos la acción
    log_evento_auth(LOG_PASSWORD_RECUPERADA, user, "Solicitud de recuperación de contraseña", 0);
    return 0;
}