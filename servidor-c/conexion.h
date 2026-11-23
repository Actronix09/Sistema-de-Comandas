#ifndef CONEXION_H
#define CONEXION_H

struct mg_context;

#define MAX_ORIGENES 8

// Páginas web que pueden conectarse, por ejemplo
// "https://actronix09.github.io". Lo revisa el navegador por nosotros: manda
// el encabezado Origin con la página que abrió la conexión.
typedef struct {
    char *origenes[MAX_ORIGENES];
    int num_origenes;
} ConfigConexion;

// Lee la lista separada por comas de COMANDAS_ORIGENES
void conexion_leer_origenes(ConfigConexion *config, const char *lista);
void conexion_liberar_origenes(ConfigConexion *config);

// Atiende los WebSocket que lleguen a la ruta "/"
void conexion_registrar(struct mg_context *contexto, ConfigConexion *config);

#endif
