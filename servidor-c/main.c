// Servidor del Sistema de Comandas, en C.
//
// Sucesor de archivo/sistema-c/Servidor.c: en lugar de memoria compartida y
// semáforos, los clientes se conectan por WebSocket desde la red local o desde
// un navegador, con mensajes JSON {id, op, datos} → {id, ok, datos | error}.
//
// Variables de entorno:
//   COMANDAS_PUERTO     puerto donde escucha (9000)
//   COMANDAS_ORIGENES   páginas web permitidas, separadas por comas
//   COMANDAS_BD_*       conexión a PostgreSQL (ver bd.h)

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <civetweb.h>

#include "bd.h"
#include "conexion.h"
#include "operaciones.h"
#include "registro.h"

static volatile sig_atomic_t seguir = 1;

static void al_recibir_senal(int senal)
{
    (void)senal;
    seguir = 0;
}

// Los errores internos de civetweb van al mismo log que los nuestros
static int registrar_civetweb(const struct mg_connection *conexion, const char *mensaje)
{
    (void)conexion;
    registrar("civetweb: %s", mensaje);
    return 1;
}

static const char *variable(const char *nombre, const char *por_omision)
{
    const char *valor = getenv(nombre);
    return (valor != NULL && valor[0] != '\0') ? valor : por_omision;
}

int main(void)
{
    const char *puerto = variable("COMANDAS_PUERTO", "9000");

    ConfigConexion config;
    conexion_leer_origenes(&config, getenv("COMANDAS_ORIGENES"));

    // Ctrl+C o "podman stop" detienen el servidor de forma ordenada
    struct sigaction accion;
    memset(&accion, 0, sizeof accion);
    accion.sa_handler = al_recibir_senal;
    sigaction(SIGINT, &accion, NULL);
    sigaction(SIGTERM, &accion, NULL);

    if (operaciones_iniciar() != 0) {
        registrar("No se pudo iniciar libsodium");
        return 1;
    }
    bd_iniciar();

    mg_init_library(0);
    if (!mg_check_feature(MG_FEATURES_WEBSOCKET)) {
        registrar("Esta civetweb se compiló sin WebSocket");
        return 1;
    }

    // ping cada 30 s: mantiene viva la conexión aunque nadie mande nada, y
    // cierra las de los clientes que ya no responden
    const char *opciones[] = {
        "listening_ports", puerto,
        "num_threads", "64",
        "websocket_timeout_ms", "30000",
        "enable_websocket_ping_pong", "yes",
        NULL,
    };
    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof callbacks);
    callbacks.log_message = registrar_civetweb;

    struct mg_context *contexto = mg_start(&callbacks, NULL, opciones);
    if (contexto == NULL) {
        registrar("No se pudo escuchar en el puerto %s", puerto);
        return 1;
    }
    conexion_registrar(contexto, &config);

    registrar("Servidor escuchando en el puerto %s", puerto);
    for (int i = 0; i < config.num_origenes; i++)
        registrar("Página permitida: %s", config.origenes[i]);

    // Los hilos de civetweb atienden a los clientes; este solo espera la señal
    while (seguir)
        pause();

    registrar("Cerrando servidor...");
    mg_stop(contexto);
    mg_exit_library();
    bd_cerrar();
    conexion_liberar_origenes(&config);
    return 0;
}
