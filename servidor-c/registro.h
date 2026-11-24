#ifndef REGISTRO_H
#define REGISTRO_H

#include <cjson/cJSON.h>

// Escribe una línea con fecha y hora en la salida de errores, que es lo que
// guarda el contenedor como su log. La usan varios hilos a la vez, así que
// cada línea se escribe completa sin mezclarse con otras.
//
// registrar() además la guarda en la bitácora: las últimas líneas en memoria,
// que el administrador ve en la app (OP_LISTAR_LOGS). Lo que solo sirve para
// depurar, como cada petición que salió bien, va con registrar_solo_log().
void registrar(const char *formato, ...) __attribute__((format(printf, 1, 2)));
void registrar_solo_log(const char *formato, ...) __attribute__((format(printf, 1, 2)));

// La bitácora, de la más reciente a la más vieja: [{"fecha", "texto"}, ...]
cJSON *registro_bitacora(void);

#endif
