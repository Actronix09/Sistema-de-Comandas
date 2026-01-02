#include "usuario.h"
#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>

static USUARIO usuarios[MAX_USUARIOS];
int total_usuarios = 0;

// Inicializar sistema de usuarios
void usuario_init()
{
    total_usuarios = 0;
}

// Encriptar password usando MD5
void encriptar_password(const char* input, char* output)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    EVP_DigestUpdate(ctx, input, strlen(input));
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    for(unsigned int i = 0; i < digest_len; i++)
    {
        sprintf(&output[i*2], "%02x", (unsigned int)digest[i]);
    }
    output[32] = '\0';
}

// Cargar usuarios desde archivo
void usuario_cargar()
{
    FILE *archivo = fopen("usuarios", "r");
    if(archivo == NULL)
        return;

    total_usuarios = 0;
    char linea[512];

    while(fgets(linea, sizeof(linea), archivo) && total_usuarios < MAX_USUARIOS)
    {
        linea[strcspn(linea, "\n")] = 0;

        char *token = strtok(linea, "|");
        if(token) strncpy(usuarios[total_usuarios].name, token, MAX_CHAIN_SIZE-1);

        token = strtok(NULL, "|");
        if(token) strncpy(usuarios[total_usuarios].user, token, MAX_CHAIN_SIZE-1);

        token = strtok(NULL, "|");
        if(token) strncpy(usuarios[total_usuarios].pass, token, 32);

        token = strtok(NULL, "|");
        if(token) strncpy(usuarios[total_usuarios].mail, token, MAX_CHAIN_SIZE-1);

        token = strtok(NULL, "|");
        if(token) strncpy(usuarios[total_usuarios].telf, token, MAX_CHAIN_SIZE-1);

        token = strtok(NULL, "|");
        if(token) usuarios[total_usuarios].tipo = atoi(token);

        total_usuarios++;
    }
    
    fclose(archivo);
}

// Guardar usuarios en archivo
void usuario_guardar()
{
    FILE *archivo = fopen("usuarios", "w");
    if(archivo == NULL)
        return;

    for(int i = 0; i < total_usuarios; i++)
    {
        fprintf(archivo, "%s|%s|%s|%s|%s|%d\n",
                usuarios[i].name,
                usuarios[i].user,
                usuarios[i].pass,
                usuarios[i].mail,
                usuarios[i].telf,
                usuarios[i].tipo);
    }
    
    fclose(archivo);
}

// Obtener total de usuarios
int usuario_get_total()
{
    return total_usuarios;
}

// Obtener usuario por índice
USUARIO* usuario_get_by_index(int index)
{
    if(index >= 0 && index < total_usuarios)
        return &usuarios[index];
    return NULL;
}

// Verificar si usuario existe
int usuario_existe(const char* user)
{
    for(int i = 0; i < total_usuarios; i++)
    {
        if(strcmp(usuarios[i].user, user) == 0)
            return 1;
    }
    return 0;
}

// Crear nuevo usuario
int usuario_crear(const char* name, const char* user, const char* pass, const char* mail, const char* telf, int tipo)
{
    if(total_usuarios >= MAX_USUARIOS)
        return -1; // Límite alcanzado

    if(usuario_existe(user))
        return -2; // Usuario ya existe

    // Crear nuevo usuario
    USUARIO nuevo;
    strncpy(nuevo.name, name, MAX_CHAIN_SIZE-1);
    strncpy(nuevo.user, user, MAX_CHAIN_SIZE-1);
    encriptar_password(pass, nuevo.pass);
    strncpy(nuevo.mail, mail, MAX_CHAIN_SIZE-1);
    strncpy(nuevo.telf, telf, MAX_CHAIN_SIZE-1);
    nuevo.tipo = tipo;

    usuarios[total_usuarios] = nuevo;
    total_usuarios++;

    usuario_guardar();
    return 0; // Éxito
}

// Autenticar usuario
int usuario_autenticar(const char* user, const char* pass)
{
    char pass_encriptada[33];
    encriptar_password(pass, pass_encriptada);

    for(int i = 0; i < total_usuarios; i++)
    {
        if(strcmp(usuarios[i].user, user) == 0 && strcmp(usuarios[i].pass, pass_encriptada) == 0)
        {
            return i; // Retorna índice del usuario
        }
    }
    return -1; // No encontrado
}

// Recuperar contraseña
int usuario_recuperar_pass(const char* user, const char* new_pass)
{
    for(int i = 0; i < total_usuarios; i++)
    {
        if(strcmp(usuarios[i].user, user) == 0)
        {
            encriptar_password(new_pass, usuarios[i].pass);
            usuario_guardar();
            return 0; // Éxito
        }
    }
    return -1; // Usuario no encontrado
}

// Validar contraseña
int validar_password(const char* pass)
{
    int charespc = 0;
    int num = 0;
    int mayus = 0;
    int minus = 0;
    int longitud = 0;

    for(int i = 0; pass[i] != '\0' && i < MAX_CHAIN_SIZE; i++)
    {
        longitud++;
        if(pass[i] >= '0' && pass[i] <= '9') num++;
        else if(pass[i] >= 'A' && pass[i] <= 'Z') mayus++;
        else if(pass[i] >= 'a' && pass[i] <= 'z') minus++;
        else if(pass[i] == '*' || pass[i] == '/' || pass[i] == '-' ||
                pass[i] == '_' || pass[i] == '+' || pass[i] == '.') charespc++;
    }
    
    return (charespc >= 2 && num > 0 && mayus > 0 && minus > 0 && longitud >= 8) ? 0 : 1;
}

// Validar email
int validar_email(const char* mail)
{
    int arroba = 0;
    int punto = 0;

    for(int i = 0; mail[i] != '\0' && i < MAX_CHAIN_SIZE; i++)
    {
        if(mail[i] == '@') arroba++;
        if(mail[i] == '.') punto++;
    }
    
    return (arroba == 1 && punto > 0) ? 0 : 1;
}

// Validar teléfono
int validar_telefono(const char* telf)
{
    int longitud = 0;

    for(int i = 0; telf[i] != '\0' && i < MAX_CHAIN_SIZE; i++)
    {
        longitud++;
        if(telf[i] < '0' || telf[i] > '9')
            return 1;
    }

    return (longitud >= 8) ? 0 : 1;
}

// Eliminar usuario por nombre
int usuario_eliminar_por_nombre(const char* user)
{
    for(int i = 0; i < total_usuarios; i++)
    {
        if(strcmp(usuarios[i].user, user) == 0)
        {
            // Mover todos los usuarios después de i una posición hacia atrás
            for(int j = i; j < total_usuarios - 1; j++)
            {
                usuarios[j] = usuarios[j + 1];
            }
            total_usuarios--;
            return 0; // Éxito
        }
    }
    return -1; // Usuario no encontrado
}