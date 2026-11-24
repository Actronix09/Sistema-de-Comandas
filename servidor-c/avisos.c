#include "avisos.h"

#include <pthread.h>
#include <string.h>

#include <civetweb.h>

// Las conexiones abiertas. Cada una la atiende su propio hilo de civetweb, y
// cualquiera puede avisar a las demás, así que la lista va con candado. Escribir
// en otra conexión es seguro: mg_websocket_write bloquea esa conexión mientras
// manda la trama.
static Cliente *clientes;
static pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;

void avisos_agregar(Cliente *cliente)
{
    pthread_mutex_lock(&candado);
    cliente->siguiente = clientes;
    clientes = cliente;
    pthread_mutex_unlock(&candado);
}

void avisos_quitar(Cliente *cliente)
{
    pthread_mutex_lock(&candado);
    for (Cliente **actual = &clientes; *actual != NULL; actual = &(*actual)->siguiente) {
        if (*actual == cliente) {
            *actual = cliente->siguiente;
            break;
        }
    }
    pthread_mutex_unlock(&candado);
}

void avisos_bloquear(void)
{
    pthread_mutex_lock(&candado);
}

void avisos_desbloquear(void)
{
    pthread_mutex_unlock(&candado);
}

static char *armar(const char *aviso, const cJSON *datos)
{
    cJSON *mensaje = cJSON_CreateObject();
    cJSON_AddStringToObject(mensaje, "aviso", aviso);
    if (datos != NULL)
        cJSON_AddItemToObject(mensaje, "datos", cJSON_Duplicate(datos, 1));
    char *texto = cJSON_PrintUnformatted(mensaje);
    cJSON_Delete(mensaje);
    return texto;
}

// id_usuario 0 = a todos los que tienen sesión
static void enviar(int id_usuario, const char *aviso, const cJSON *datos)
{
    char *texto = armar(aviso, datos);
    if (texto == NULL)
        return;
    size_t largo = strlen(texto);

    pthread_mutex_lock(&candado);
    for (Cliente *c = clientes; c != NULL; c = c->siguiente) {
        if (c->sesion.activa && (id_usuario == 0 || c->sesion.id_usuario == id_usuario))
            mg_websocket_write(c->conexion, MG_WEBSOCKET_OPCODE_TEXT, texto, largo);
    }
    pthread_mutex_unlock(&candado);
    cJSON_free(texto);
}

void avisar_todos(const char *aviso)
{
    enviar(0, aviso, NULL);
}

void avisar_usuario(int id_usuario, const char *aviso, const cJSON *datos)
{
    if (id_usuario > 0)
        enviar(id_usuario, aviso, datos);
}
