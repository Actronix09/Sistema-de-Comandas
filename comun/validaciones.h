#ifndef VALIDACIONES_H
#define VALIDACIONES_H

// Lo comparten el sistema en C, el cliente y el servidor en Qt. No depende de
// archivos, memoria compartida ni OpenSSL, así que también compila para Android.
#ifdef __cplusplus
extern "C" {
#endif

// Retorna 0 si la contraseña es válida y 1 si no
int validar_password(const char* pass);

#ifdef __cplusplus
}
#endif

#endif
