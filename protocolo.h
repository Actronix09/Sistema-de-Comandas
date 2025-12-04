#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#define TAM_MENSAJE 256
#define MAX_CHAIN_SIZE 100
#define MAX_PRODUCTOS_RESPUESTA 50
#define MAX_ITEMS_PEDIDO 20

// Tipos de operación
typedef enum {
    OP_LOGIN = 1,
    OP_CREAR_USUARIO = 2,
    OP_RECUPERAR_PASS = 3,
    OP_VERIFICAR_USUARIO = 4,
    OP_LOGOUT = 5,
    OP_LISTAR_PRODUCTOS = 6,
    OP_CREAR_PEDIDO = 7,
    OP_LISTAR_PEDIDOS = 8,
    OP_CAMBIAR_ESTADO_PEDIDO = 9
} TipoOperacion;

// Códigos de respuesta
typedef enum {
    RESP_OK = 0,
    RESP_ERROR = -1,
    RESP_USUARIO_EXISTE = -2,
    RESP_USUARIO_NO_EXISTE = -3,
    RESP_CREDENCIALES_INVALIDAS = -4,
    RESP_LIMITE_ALCANZADO = -5,
    RESP_VALIDACION_FALLIDA = -6
} CodigoRespuesta;

// Estructura para items de pedido
typedef struct {
    int producto_id;
    char nombre_producto[100];
    int cantidad;
} ItemPedidoMsg;

// Estructura para productos en respuesta
typedef struct {
    int id;
    char nombre[100];
    char descripcion[200];
    float precio;
} ProductoMsg;

// Estructura para pedidos en respuesta
typedef struct {
    int id;
    char mesa[50];
    char mesero[100];
    ItemPedidoMsg items[MAX_ITEMS_PEDIDO];
    int num_items;
    int estado; // 0=No empezado, 1=En progreso, 2=Listo
    char fecha[20];
} PedidoMsg;

// Estructura para peticiones
typedef struct {
    TipoOperacion operacion;
    char user[MAX_CHAIN_SIZE];
    char pass[MAX_CHAIN_SIZE];
    char name[MAX_CHAIN_SIZE];
    char mail[MAX_CHAIN_SIZE];
    char telf[MAX_CHAIN_SIZE];
    int tipo;
    
    // Para pedidos
    char mesa[50];
    ItemPedidoMsg items[MAX_ITEMS_PEDIDO];
    int num_items;
    int pedido_id;
    int nuevo_estado;
} Peticion;

// Estructura para respuestas
typedef struct {
    CodigoRespuesta codigo;
    int tipo_usuario;  // 0=Mesero, 1=Cocina
    char nombre[MAX_CHAIN_SIZE];
    char mensaje[TAM_MENSAJE];
    
    // Para productos
    ProductoMsg productos[MAX_PRODUCTOS_RESPUESTA];
    int num_productos;
    
    // Para pedidos
    PedidoMsg pedidos[50];
    int num_pedidos;
    int pedido_id_creado;
} Respuesta;

#endif