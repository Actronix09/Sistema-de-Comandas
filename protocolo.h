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
    OP_CAMBIAR_ESTADO_PEDIDO = 9,
    OP_LISTAR_USUARIOS = 10,
    OP_MODIFICAR_USUARIO = 11,
    OP_ELIMINAR_USUARIO = 12,
    OP_OBTENER_USUARIO = 13,
    OP_MODIFICAR_PRODUCTO = 14,
    OP_ELIMINAR_PRODUCTO = 15,
    OP_CREAR_PRODUCTO = 16,
    OP_OBTENER_PRODUCTO = 17,
    OP_LISTAR_INGREDIENTES = 18,
    OP_CREAR_INGREDIENTE = 19,
    OP_MODIFICAR_INGREDIENTE = 20,
    OP_ELIMINAR_INGREDIENTE = 21,
    OP_OBTENER_INGREDIENTE = 22,
    OP_ELIMINAR_PEDIDO = 23,
    OP_OBTENER_PEDIDO = 24,
    OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO = 25,
    OP_LISTAR_LOGS = 26,
    OP_VER_LOG = 27
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

// Estructura para ingredientes
typedef struct {
    int id;
    char nombre[100];
    int cantidad;
} IngredienteMsg;

// Estructura para usuarios en respuesta
typedef struct {
    char name[MAX_CHAIN_SIZE];
    char user[MAX_CHAIN_SIZE];
    char pass[33];  // MD5
    char mail[MAX_CHAIN_SIZE];
    char telf[MAX_CHAIN_SIZE];
    int tipo;
} UsuarioMsg;

// Estructura para peticiones
typedef struct {
    TipoOperacion operacion;
    char user[MAX_CHAIN_SIZE];
    char pass[MAX_CHAIN_SIZE];
    char name[MAX_CHAIN_SIZE];
    char mail[MAX_CHAIN_SIZE];
    char telf[MAX_CHAIN_SIZE];
    int tipo;
    int producto_id;
    int pedido_id;
    int ingrediente_id;
    int nuevo_estado;
    float precio;
    char descripcion[200];

    // Para pedidos
    char mesa[50];
    ItemPedidoMsg items[MAX_ITEMS_PEDIDO];
    int num_items;
    int cantidad;

    // Para ingredientes en productos
    struct {
        int id;
        int cantidad;
    } ingredientes[20];
    int num_ingredientes;

    // Para logs
    char nombre_archivo[100];
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
    ProductoMsg producto;

    // Para pedidos
    PedidoMsg pedidos[50];
    int num_pedidos;
    int pedido_id_creado;
    PedidoMsg pedido;

    // Para usuarios
    UsuarioMsg usuarios[100];
    int num_usuarios;
    UsuarioMsg usuario;

    // Para ingredientes
    IngredienteMsg ingredientes[100];
    int num_ingredientes;
    IngredienteMsg ingrediente;

    // Para logs
    char logs[20][100];  // Lista de nombres de archivos de log
    int num_logs;
    char contenido_log[5000];  // Contenido del archivo de log solicitado
} Respuesta;

#endif