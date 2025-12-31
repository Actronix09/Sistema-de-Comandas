#ifndef PEDIDOS_H
#define PEDIDOS_H

#define MAX_PEDIDOS 200
#define MAX_ITEMS_PEDIDO 20
#define MAX_MESA 50

typedef enum {
    ESTADO_NO_EMPEZADO = 0,
    ESTADO_EN_PROGRESO = 1,
    ESTADO_LISTO = 2,
    ESTADO_PAGADO = 3  // NUEVO
} EstadoPedido;

typedef struct {
    int producto_id;
    char nombre_producto[100];
    int cantidad;
} ItemPedido;

typedef struct {
    int id;
    char mesa[MAX_MESA];
    char mesero[100];
    ItemPedido items[MAX_ITEMS_PEDIDO];
    int num_items;
    EstadoPedido estado;
    char fecha[20];
} PEDIDO;

// Gestión de pedidos
void pedidos_init();
void pedidos_cargar();
void pedidos_guardar();
int pedidos_get_total();
PEDIDO* pedidos_get_by_index(int index);
PEDIDO* pedidos_get_by_id(int id);
int pedidos_crear(const char* mesa, const char* mesero, ItemPedido* items, int num_items);
int pedidos_cambiar_estado(int id, EstadoPedido nuevo_estado);
int pedidos_contar_por_estado(EstadoPedido estado);

#endif