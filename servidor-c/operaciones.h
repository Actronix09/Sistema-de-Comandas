#ifndef OPERACIONES_H
#define OPERACIONES_H

#include <cjson/cJSON.h>
#include <sodium.h>

#include "cliente.h"

// Arranca libsodium. Regresa 0 si todo bien.
int operaciones_iniciar(void);

// Guarda la contraseña con Argon2id (el formato que revisa el login). 0 si todo bien.
int operaciones_hash(const char *contrasena, char hash[crypto_pwhash_STRBYTES]);

// Cada una recibe el "datos" de la petición (puede ser NULL) y regresa la
// respuesta {ok, datos | error}; despacho.c le agrega el id.
cJSON *op_login(Cliente *cliente, const cJSON *datos);
cJSON *op_logout(Cliente *cliente);
cJSON *op_listar_productos(void);
cJSON *op_crear_pedido(Cliente *cliente, const cJSON *datos);

// op_salon.c: evento, mesas, cuentas y caja
cJSON *op_evento_en_curso(void);
cJSON *op_listar_mesas(Cliente *cliente);
cJSON *op_listar_cuentas(const cJSON *datos);
cJSON *op_crear_cuenta(Cliente *cliente, const cJSON *datos);
cJSON *op_listar_cuentas_caja(Cliente *cliente, const cJSON *datos);
cJSON *op_detalle_cuenta(Cliente *cliente, const cJSON *datos);
cJSON *op_registrar_pago(Cliente *cliente, const cJSON *datos);
cJSON *op_cerrar_cuenta(Cliente *cliente, const cJSON *datos);
cJSON *op_cancelar_cuenta(Cliente *cliente, const cJSON *datos);

// op_comandas.c
cJSON *op_listar_comandas(const cJSON *datos);
cJSON *op_cambiar_estado(Cliente *cliente, const cJSON *datos);
cJSON *op_cancelar_comandas(Cliente *cliente, const cJSON *datos);

// op_inventario.c
cJSON *op_listar_ingredientes(void);
cJSON *op_guardar_ingrediente(Cliente *cliente, const cJSON *datos, int es_nuevo);
cJSON *op_ajustar_stock(Cliente *cliente, const cJSON *datos);
cJSON *op_eliminar_ingrediente(Cliente *cliente, const cJSON *datos);
cJSON *op_reiniciar_stock(Cliente *cliente);

// op_catalogo.c
cJSON *op_guardar_producto(Cliente *cliente, const cJSON *datos, int es_nuevo);
cJSON *op_eliminar_producto(Cliente *cliente, const cJSON *datos);
cJSON *op_listar_paquetes(Cliente *cliente);
cJSON *op_guardar_paquete(Cliente *cliente, const cJSON *datos, int es_nuevo);
cJSON *op_eliminar_paquete(Cliente *cliente, const cJSON *datos);

// op_eventos.c
cJSON *op_listar_eventos(Cliente *cliente, const cJSON *datos);
cJSON *op_crear_evento(Cliente *cliente, const cJSON *datos);
cJSON *op_modificar_evento(Cliente *cliente, const cJSON *datos);
cJSON *op_cambiar_estado_evento(Cliente *cliente, const cJSON *datos);
cJSON *op_registrar_anticipo(Cliente *cliente, const cJSON *datos);

// op_usuarios.c
cJSON *op_listar_usuarios(Cliente *cliente);
cJSON *op_guardar_usuario(Cliente *cliente, const cJSON *datos, int es_nuevo);
cJSON *op_eliminar_usuario(Cliente *cliente, const cJSON *datos);

#endif
