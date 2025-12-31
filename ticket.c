#include "ticket.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static TICKET tickets[MAX_TICKETS];
static int total_tickets = 0;
static int siguiente_id = 1;

void ticket_init() {
    total_tickets = 0;
    siguiente_id = 1;
}

void ticket_cargar() {
    FILE *archivo = fopen("tickets", "r");
    if(archivo == NULL) {
        printf("Archivo de tickets no encontrado, creando uno nuevo...\n");
        return;
    }
    
    total_tickets = 0;
    char linea[1024];
    
    while(fgets(linea, sizeof(linea), archivo) && total_tickets < MAX_TICKETS) {
        linea[strcspn(linea, "\n")] = 0;
        
        char *token = strtok(linea, "|");
        if(token) tickets[total_tickets].id = atoi(token);
        
        token = strtok(NULL, "|");
        if(token) tickets[total_tickets].pedido_id = atoi(token);
        
        token = strtok(NULL, "|");
        if(token) strncpy(tickets[total_tickets].mesa, token, MAX_MESA-1);
        
        token = strtok(NULL, "|");
        if(token) strncpy(tickets[total_tickets].mesero, token, 99);
        
        token = strtok(NULL, "|");
        if(token) tickets[total_tickets].num_items = atoi(token);
        
        // Leer items
        for(int i = 0; i < tickets[total_tickets].num_items && i < MAX_ITEMS_PEDIDO; i++) {
            token = strtok(NULL, "|");
            if(token) tickets[total_tickets].items[i].producto_id = atoi(token);
            
            token = strtok(NULL, "|");
            if(token) strncpy(tickets[total_tickets].items[i].nombre_producto, token, 99);
            
            token = strtok(NULL, "|");
            if(token) tickets[total_tickets].items[i].cantidad = atoi(token);
        }
        
        token = strtok(NULL, "|");
        if(token) tickets[total_tickets].total = atof(token);
        
        token = strtok(NULL, "|");
        if(token) tickets[total_tickets].estado = atoi(token);
        
        token = strtok(NULL, "|");
        if(token) strncpy(tickets[total_tickets].fecha, token, 19);
        
        token = strtok(NULL, "|");
        if(token) strncpy(tickets[total_tickets].hora_pago, token, 19);
        
        if(tickets[total_tickets].id >= siguiente_id)
            siguiente_id = tickets[total_tickets].id + 1;
        
        total_tickets++;
    }
    
    fclose(archivo);
    printf("Tickets cargados: %d\n", total_tickets);
}

void ticket_guardar() {
    FILE *archivo = fopen("tickets", "w");
    if(archivo == NULL) return;
    
    for(int i = 0; i < total_tickets; i++) {
        fprintf(archivo, "%d|%d|%s|%s|%d",
                tickets[i].id,
                tickets[i].pedido_id,
                tickets[i].mesa,
                tickets[i].mesero,
                tickets[i].num_items);
        
        // Guardar items
        for(int j = 0; j < tickets[i].num_items; j++) {
            fprintf(archivo, "|%d|%s|%d",
                    tickets[i].items[j].producto_id,
                    tickets[i].items[j].nombre_producto,
                    tickets[i].items[j].cantidad);
        }
        
        fprintf(archivo, "|%.2f|%d|%s|%s\n",
                tickets[i].total,
                tickets[i].estado,
                tickets[i].fecha,
                tickets[i].hora_pago);
    }
    
    fclose(archivo);
}

int ticket_get_total() {
    return total_tickets;
}

TICKET* ticket_get_by_index(int index) {
    if(index >= 0 && index < total_tickets)
        return &tickets[index];
    return NULL;
}

TICKET* ticket_get_by_id(int id) {
    for(int i = 0; i < total_tickets; i++) {
        if(tickets[i].id == id)
            return &tickets[i];
    }
    return NULL;
}

TICKET* ticket_get_by_pedido_id(int pedido_id) {
    for(int i = 0; i < total_tickets; i++) {
        if(tickets[i].pedido_id == pedido_id)
            return &tickets[i];
    }
    return NULL;
}

int ticket_crear_desde_pedido(PEDIDO* pedido) {
    if(total_tickets >= MAX_TICKETS)
        return -1;
    
    // Verificar si ya existe ticket para este pedido
    if(ticket_get_by_pedido_id(pedido->id) != NULL)
        return -2;
    
    tickets[total_tickets].id = siguiente_id++;
    tickets[total_tickets].pedido_id = pedido->id;
    strncpy(tickets[total_tickets].mesa, pedido->mesa, MAX_MESA-1);
    strncpy(tickets[total_tickets].mesero, pedido->mesero, 99);
    tickets[total_tickets].num_items = pedido->num_items;
    
    // Calcular total
    float total = 0.0;
    for(int i = 0; i < pedido->num_items; i++) {
        tickets[total_tickets].items[i] = pedido->items[i];
        
        // Obtener precio del producto (aquí necesitaríamos acceso a productos)
        // Por ahora usaremos un valor temporal, será calculado en el servidor
    }
    
    tickets[total_tickets].total = total; // Se calculará en el servidor
    tickets[total_tickets].estado = TICKET_PENDIENTE_PAGO;
    strncpy(tickets[total_tickets].fecha, pedido->fecha, 19);
    tickets[total_tickets].hora_pago[0] = '\0';
    
    int id_creado = tickets[total_tickets].id;
    total_tickets++;
    ticket_guardar();
    
    return id_creado;
}

int ticket_marcar_pagado(int id) {
    for(int i = 0; i < total_tickets; i++) {
        if(tickets[i].id == id && tickets[i].estado == TICKET_PENDIENTE_PAGO) {
            tickets[i].estado = TICKET_PAGADO;
            
            // Obtener fecha/hora actual para pago
            time_t t = time(NULL);
            struct tm *tm_info = localtime(&t);
            strftime(tickets[i].hora_pago, 20, "%d-%m-%Y %H:%M:%S", tm_info);
            
            ticket_guardar();
            
            // Mover a historial
            ticket_mover_a_historial(&tickets[i]);
            
            return 0;
        }
    }
    return -1;
}

void ticket_mover_a_historial(TICKET* ticket) {
    FILE *historial = fopen("tickets_historial.txt", "a");
    if(historial == NULL) return;
    
    fprintf(historial, "=== TICKET #%d ===\n", ticket->id);
    fprintf(historial, "Pedido: #%d\n", ticket->pedido_id);
    fprintf(historial, "Mesa: %s | Mesero: %s\n", ticket->mesa, ticket->mesero);
    fprintf(historial, "Fecha: %s | Pagado: %s\n", ticket->fecha, ticket->hora_pago);
    fprintf(historial, "Estado: %s\n", ticket->estado == TICKET_PAGADO ? "PAGADO" : "PENDIENTE");
    fprintf(historial, "Items:\n");
    
    for(int i = 0; i < ticket->num_items; i++) {
        fprintf(historial, "  - %dx %s\n", 
                ticket->items[i].cantidad,
                ticket->items[i].nombre_producto);
    }
    
    fprintf(historial, "Total: $%.2f\n", ticket->total);
    fprintf(historial, "====================\n\n");
    
    fclose(historial);
}

int ticket_contar_por_estado(EstadoTicket estado) {
    int count = 0;
    for(int i = 0; i < total_tickets; i++) {
        if(tickets[i].estado == estado)
            count++;
    }
    return count;
}