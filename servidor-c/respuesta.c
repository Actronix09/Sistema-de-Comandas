#include "respuesta.h"

cJSON *respuesta_ok(cJSON *datos)
{
    cJSON *respuesta = cJSON_CreateObject();
    cJSON_AddBoolToObject(respuesta, "ok", 1);
    cJSON_AddItemToObject(respuesta, "datos", datos != NULL ? datos : cJSON_CreateNull());
    return respuesta;
}

cJSON *respuesta_error(const char *codigo, const char *mensaje)
{
    cJSON *respuesta = cJSON_CreateObject();
    cJSON_AddBoolToObject(respuesta, "ok", 0);
    cJSON *error = cJSON_AddObjectToObject(respuesta, "error");
    cJSON_AddStringToObject(error, "codigo", codigo);
    cJSON_AddStringToObject(error, "mensaje", mensaje);
    return respuesta;
}
