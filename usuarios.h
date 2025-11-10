#ifndef USUARIO_H
#define USUARIO_H

#define MAX_CHAIN_SIZE 100
#define MAX_USUARIOS   100

typedef struct
{
    char name[MAX_CHAIN_SIZE];
    char user[MAX_CHAIN_SIZE];
    char pass[33];
    char mail[MAX_CHAIN_SIZE];
    char telf[MAX_CHAIN_SIZE];
    int  tipo;
} USUARIO;

// Variables globales
extern USUARIO usuarios[MAX_USUARIOS];
extern int total_usuarios;

// Funciones de gestión de usuarios
void cargar_usuarios();
void guardar_usuarios();
void encriptar(const char* input, char* output);

// Funciones de validación
int check_pass(char pass[MAX_CHAIN_SIZE]);
int check_mail(char mail[MAX_CHAIN_SIZE]);
int check_telf(char telf[MAX_CHAIN_SIZE]);

#endif