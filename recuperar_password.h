#ifndef RECUPERAR_PASSWORD_H
#define RECUPERAR_PASSWORD_H

#include "usuario.h"
#include "protocolo.h"

// Comunicación con el servidor (implementada en Cliente.c)
int enviar_peticion(Peticion *pet, Respuesta *resp);

// Consulta al servidor si el usuario puede recuperar su contraseña.
// Retorna 0 si es válido; en caso contrario escribe el motivo en 'motivo'.
//   -1 = error de comunicacion
//   -2 = el usuario no existe
//   -3 = el usuario es administrador
int verificar_usuario_recuperable(const char* user, char* motivo, int motivo_len);

// Envía la nueva contraseña al servidor para el usuario indicado.
// Retorna 0 si se actualizó; -1 si hubo error (motivo en 'motivo').
int aplicar_nueva_password(const char* user, const char* new_pass, char* motivo, int motivo_len);

// Interfaz completa de recuperación de contraseña
void recuperar_password();

#endif // RECUPERAR_PASSWORD_H
