#ifndef TICKET_H
#define TICKET_H

#include "pedidos.h"

#define MAX_TICKETS 200
#define MAX_TICKETS_HISTORIAL 1000
#define NOMBRE_RESTAURANTE "RESTAURANTE DELICIAS"

typedef enum {
    TICKET_PENDIENTE_PAGO = 0,
    TICKET_PAGADO = 1
} EstadoTicket;

typedef struct {
    int id;
    int pedido_id;
    char mesa[MAX_MESA];
    char mesero[100];
    ItemPedido items[MAX_ITEMS_PEDIDO];
    int num_items;
    float total;
    EstadoTicket estado;
    char fecha[20];
    char hora_pago[20]; // Para tickets pagados
} TICKET;

// Gestión de tickets
void ticket_init();
void ticket_cargar();
void ticket_guardar();
int ticket_get_total();
TICKET* ticket_get_by_index(int index);
TICKET* ticket_get_by_id(int id);
TICKET* ticket_get_by_pedido_id(int pedido_id);
int ticket_crear_desde_pedido(PEDIDO* pedido);
int ticket_marcar_pagado(int id);
void ticket_mover_a_historial(TICKET* ticket);
int ticket_contar_por_estado(EstadoTicket estado);

#endif