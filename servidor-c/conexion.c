#include "conexion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>
#include <cjson/cJSON.h>

#include "avisos.h"
#include "cliente.h"
#include "despacho.h"
#include "registro.h"

// Un mensaje de este sistema nunca se acerca a este tamaño
#define TAM_MAXIMO_MENSAJE (1024 * 1024)

void conexion_leer_origenes(ConfigConexion *config, const char *lista)
{
    config->num_origenes = 0;
    if (lista == NULL)
        return;

    char *copia = strdup(lista);
    char *resto = copia;
    char *origen;
    while ((origen = strtok_r(resto, ",", &resto)) != NULL && config->num_origenes < MAX_ORIGENES) {
        while (*origen == ' ')
            origen++;
        size_t largo = strlen(origen);
        while (largo > 0 && origen[largo - 1] == ' ')
            origen[--largo] = '\0';
        if (largo > 0)
            config->origenes[config->num_origenes++] = strdup(origen);
    }
    free(copia);
}

void conexion_liberar_origenes(ConfigConexion *config)
{
    for (int i = 0; i < config->num_origenes; i++)
        free(config->origenes[i]);
    config->num_origenes = 0;
}

static int origen_permitido(const ConfigConexion *config, const char *origen)
{
    for (int i = 0; i < config->num_origenes; i++) {
        if (strcmp(config->origenes[i], origen) == 0)
            return 1;
    }
    return 0;
}

// Llega la solicitud para abrir el WebSocket. Regresar 0 la acepta.
//
// Un navegador siempre manda Origin. Si la página no está en la lista, se
// rechaza: así otra página no puede usar el navegador de alguien para hablar
// con este servidor. Las apps de Qt y Android no mandan Origin y pasan; igual
// tienen que iniciar sesión.
//
// Al rechazar, civetweb solo cierra la conexión, así que aquí se manda un 403
// para que quede claro por qué. La respuesta se escribe completa a mano: para
// este momento civetweb ya marcó la conexión como WebSocket y sus funciones de
// cabeceras (mg_send_http_error) se niegan a escribir la línea de estado.
// civetweb pasa la conexión como const, pero el cliente todavía espera una
// respuesta HTTP normal.
static void responder_403(const struct mg_connection *conexion)
{
    static const char cuerpo[] = "Esta página no tiene permiso de conectarse al servidor\n";
    char respuesta[256];
    int largo = snprintf(respuesta, sizeof respuesta,
                         "HTTP/1.1 403 Forbidden\r\n"
                         "Content-Type: text/plain; charset=utf-8\r\n"
                         "Content-Length: %zu\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "%s",
                         sizeof cuerpo - 1, cuerpo);
    mg_write((struct mg_connection *)conexion, respuesta, (size_t)largo);
}

static int al_conectar(const struct mg_connection *conexion, void *datos_registro)
{
    const ConfigConexion *config = datos_registro;
    const char *origen = mg_get_header(conexion, "Origin");
    if (origen != NULL && !origen_permitido(config, origen)) {
        registrar("Conexión rechazada de %s: el origen %s no está permitido",
                  mg_get_request_info(conexion)->remote_addr, origen);
        responder_403(conexion);
        return 1;
    }
    return 0;
}

// El WebSocket ya está abierto: se crea el Cliente de esta conexión
static void al_estar_listo(struct mg_connection *conexion, void *datos_registro)
{
    (void)datos_registro;
    Cliente *cliente = calloc(1, sizeof *cliente);
    if (cliente == NULL)
        return; // al_recibir() cierra la conexión si no hay Cliente

    cliente->conexion = conexion;
    snprintf(cliente->ip, sizeof cliente->ip, "%s", mg_get_request_info(conexion)->remote_addr);
    mg_set_user_connection_data(conexion, cliente);
    avisos_agregar(cliente);
    registrar("Cliente conectado: %s", cliente->ip);
}

// Llega una trama. Regresar 1 deja la conexión abierta y 0 la cierra.
//
// Los bits bajos de "banderas" dicen el tipo de trama y el bit 0x80 dice si es
// la última parte del mensaje. Un mensaje de texto puede llegar partido: la
// primera trama es TEXT y las demás CONTINUATION.
static int al_recibir(struct mg_connection *conexion, int banderas, char *datos, size_t largo,
                      void *datos_registro)
{
    (void)datos_registro;
    Cliente *cliente = mg_get_user_connection_data(conexion);
    int tipo = banderas & 0x0f;
    int es_final = (banderas & 0x80) != 0;

    if (cliente == NULL || tipo == MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE)
        return 0;

    if (tipo == MG_WEBSOCKET_OPCODE_TEXT) {
        free(cliente->mensaje);
        cliente->mensaje = NULL;
        cliente->largo = 0;
        cliente->recibiendo_texto = 1;
    } else if (tipo != MG_WEBSOCKET_OPCODE_CONTINUATION || !cliente->recibiendo_texto) {
        return 1; // Binario, ping o pong: el protocolo solo usa texto
    }

    if (cliente->largo + largo > TAM_MAXIMO_MENSAJE) {
        registrar("Mensaje demasiado grande de %s: se cierra la conexión", cliente->ip);
        return 0;
    }
    char *nuevo = realloc(cliente->mensaje, cliente->largo + largo + 1);
    if (nuevo == NULL)
        return 0;
    memcpy(nuevo + cliente->largo, datos, largo);
    cliente->mensaje = nuevo;
    cliente->largo += largo;
    cliente->mensaje[cliente->largo] = '\0';

    if (!es_final)
        return 1;

    cliente->recibiendo_texto = 0;
    char *respuesta = despachar(cliente, cliente->mensaje, cliente->largo);
    free(cliente->mensaje);
    cliente->mensaje = NULL;
    cliente->largo = 0;

    if (respuesta == NULL)
        return 0;
    mg_websocket_write(conexion, MG_WEBSOCKET_OPCODE_TEXT, respuesta, strlen(respuesta));
    cJSON_free(respuesta);
    return 1;
}

static void al_cerrar(const struct mg_connection *conexion, void *datos_registro)
{
    (void)datos_registro;
    Cliente *cliente = mg_get_user_connection_data(conexion);
    if (cliente == NULL)
        return;

    registrar("Cliente desconectado: %s", cliente->ip);
    // Después de esto ningún aviso la toca, y ya se puede liberar
    avisos_quitar(cliente);
    free(cliente->mensaje);
    free(cliente);
}

void conexion_registrar(struct mg_context *contexto, ConfigConexion *config)
{
    mg_set_websocket_handler(contexto, "/", al_conectar, al_estar_listo, al_recibir, al_cerrar, config);
}
