#include "usuario.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>

USUARIO usuarios[MAX_USUARIOS];
int total_usuarios = 0;

// Función para encriptar cadena usando MD5
void encriptar(const char* input, char* output)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    EVP_DigestUpdate(ctx, input, strlen(input));
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    for(unsigned int i = 0; i < digest_len; i++){
        sprintf(&output[i*2], "%02x", (unsigned int)digest[i]);
    }
    output[32] = '\0';
}

// Función para cargar usuarios desde archivo
void cargar_usuarios()
{
    FILE *archivo = fopen("usuarios.txt", "r");
    if(archivo != NULL)
    {
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
}

// Función para guardar usuarios en archivo
void guardar_usuarios()
{
    FILE *archivo = fopen("usuarios.txt", "w");
    if(archivo != NULL)
    {
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
}

// Función para verificar contraseña
int check_pass(char pass[MAX_CHAIN_SIZE])
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

// Función para verificar correo
int check_mail(char mail[MAX_CHAIN_SIZE])
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

// Función para verificar número de teléfono
int check_telf(char telf[MAX_CHAIN_SIZE])
{   
    int longitud = 0;
    for(int i = 0; telf[i] != '\0' && i < MAX_CHAIN_SIZE; i++)
    {
        longitud++;
        if(telf[i] < '0' || telf[i] > '9') return 1;
    }
    return (longitud >= 8) ? 0 : 1;
}
