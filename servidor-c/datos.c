#include "datos.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "respuesta.h"

const char *dato_texto(const cJSON *objeto, const char *campo)
{
    const cJSON *valor = cJSON_GetObjectItemCaseSensitive(objeto, campo);
    return cJSON_IsString(valor) ? valor->valuestring : NULL;
}

int dato_entero(const cJSON *objeto, const char *campo, int minimo, int maximo, int *salida)
{
    const cJSON *valor = cJSON_GetObjectItemCaseSensitive(objeto, campo);
    if (!cJSON_IsNumber(valor))
        return 0;
    double numero = valor->valuedouble;
    // Con NaN las dos comparaciones son falsas y también se rechaza
    if (!(numero >= minimo && numero <= maximo) || numero != (int)numero)
        return 0;
    *salida = (int)numero;
    return 1;
}

int dato_numero(const cJSON *objeto, const char *campo, double maximo, int decimales, char *salida,
                size_t tam)
{
    const cJSON *valor = cJSON_GetObjectItemCaseSensitive(objeto, campo);
    if (!cJSON_IsNumber(valor))
        return 0;
    double numero = valor->valuedouble;
    if (!(numero >= 0 && numero <= maximo))
        return 0;
    int largo = snprintf(salida, tam, "%.*f", decimales, numero);
    return largo > 0 && (size_t)largo < tam;
}

int dato_ids(const cJSON *objeto, const char *campo, int maximo, char *salida, size_t tam)
{
    const cJSON *arreglo = cJSON_GetObjectItemCaseSensitive(objeto, campo);
    int total = cJSON_GetArraySize(arreglo);
    if (!cJSON_IsArray(arreglo) || total == 0 || total > maximo)
        return 0;

    int *vistos = malloc((size_t)total * sizeof *vistos);
    if (vistos == NULL)
        return 0;
    int leidos = 0;
    size_t usado = 0;
    salida[usado++] = '{';
    const cJSON *elemento;
    cJSON_ArrayForEach(elemento, arreglo) {
        double numero = cJSON_IsNumber(elemento) ? elemento->valuedouble : 0;
        if (!(numero >= 1 && numero <= 2147483647.0) || numero != (int)numero) {
            free(vistos);
            return 0;
        }
        int id = (int)numero;
        int repetido = 0;
        for (int i = 0; i < leidos && !repetido; i++)
            repetido = vistos[i] == id;
        if (repetido)
            continue;
        int largo = snprintf(salida + usado, tam - usado, "%s%d", leidos > 0 ? "," : "", id);
        if (largo < 0 || (size_t)largo >= tam - usado - 1) {
            free(vistos);
            return 0;
        }
        usado += (size_t)largo;
        vistos[leidos++] = id;
    }
    free(vistos);
    salida[usado++] = '}';
    salida[usado] = '\0';
    return leidos;
}

int recortar(const char *texto, char *salida, size_t tam)
{
    while (isspace((unsigned char)*texto))
        texto++;
    size_t largo = strlen(texto);
    while (largo > 0 && isspace((unsigned char)texto[largo - 1]))
        largo--;
    if (largo >= tam)
        return 0;
    memcpy(salida, texto, largo);
    salida[largo] = '\0';
    return 1;
}

size_t caracteres_utf8(const char *cadena)
{
    size_t total = 0;
    for (; *cadena != '\0'; cadena++) {
        if (((unsigned char)*cadena & 0xC0) != 0x80)
            total++;
    }
    return total;
}

int fecha_iso(const char *texto)
{
    int anio, mes, dia;
    char sobra;
    if (strlen(texto) != 10 || sscanf(texto, "%4d-%2d-%2d%c", &anio, &mes, &dia, &sobra) != 3)
        return 0;
    static const int DIAS[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int bisiesto = (anio % 4 == 0 && anio % 100 != 0) || anio % 400 == 0;
    return mes >= 1 && mes <= 12 && dia >= 1 && dia <= DIAS[mes - 1] + (mes == 2 && bisiesto);
}

int usuario_con_formato(const char *usuario)
{
    size_t largo = strlen(usuario);
    if (largo < 3 || largo > 40 || !isalpha((unsigned char)usuario[0]))
        return 0;
    for (size_t i = 1; i < largo; i++) {
        if (!isalnum((unsigned char)usuario[i]))
            return 0;
    }
    return 1;
}

cJSON *exigir_rol(const Cliente *cliente, const char *rol)
{
    if (strcmp(cliente->sesion.rol, rol) != 0)
        return respuesta_error("NO_AUTORIZADO", "Tu rol no puede hacer esta operación");
    return NULL;
}

cJSON *exigir_administrador(const Cliente *cliente)
{
    if (strcmp(cliente->sesion.rol, "cajero_admin") != 0 || strcmp(cliente->sesion.nivel_acceso, "administrador") != 0)
        return respuesta_error("NO_AUTORIZADO", "Solo un administrador puede hacer esta operación");
    return NULL;
}

int modo_demo(void)
{
    const char *valor = getenv("COMANDAS_DEMO");
    return valor != NULL && strcmp(valor, "1") == 0;
}
