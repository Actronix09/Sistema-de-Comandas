#ifndef REGISTRO_H
#define REGISTRO_H

// Escribe una línea con fecha y hora en la salida de errores, que es lo que
// guarda el contenedor como su log. La usan varios hilos a la vez, así que
// cada línea se escribe completa sin mezclarse con otras.
void registrar(const char *formato, ...) __attribute__((format(printf, 1, 2)));

#endif
