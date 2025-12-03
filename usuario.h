#ifndef USUARIO_H
#define USUARIO_H

#define MAX_CHAIN_SIZE 100
#define MAX_USUARIOS 100

typedef struct
{
    char name[MAX_CHAIN_SIZE];
    char user[MAX_CHAIN_SIZE];
    char pass[33];
    char mail[MAX_CHAIN_SIZE];
    char telf[MAX_CHAIN_SIZE];
    int tipo;
} USUARIO;

// Gestión de usuarios
void usuario_init();
void usuario_cargar();
void usuario_guardar();
int usuario_get_total();
USUARIO* usuario_get_by_index(int index);

// Operaciones de usuario
int usuario_crear(const char* name, const char* user, const char* pass, const char* mail, const char* telf, int tipo);
int usuario_autenticar(const char* user, const char* pass);
int usuario_recuperar_pass(const char* user, const char* new_pass);
int usuario_existe(const char* user);

// Validaciones
int validar_password(const char* pass);
int validar_email(const char* mail);
int validar_telefono(const char* telf);

// Encriptación
void encriptar_password(const char* input, char* output);

#endif