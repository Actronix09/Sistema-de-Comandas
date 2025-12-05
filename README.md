# Sistema de Comandas

## Estructura
```
SistemasOperativosTrabajoFinalDEV/
|
├── interfaz_cocina.c
├── interfaz_cocina.h
|
├── interfaz_mesero.c
├── interfaz_mesero.h
|
├── logger.c
├── logger.h
|
├── pedidos.c
├── pedidos.h
|
├── productos.c
├── productos.h
|
├── sesion.c
├── sesion.h
|
├── ui.c
├── ui.h
|
├── usuarios.c
├── usuarios.h
|
├── Cliente.c
├── Servidor.c
|
├── makefile
|
├── productos
|
├── .gitgnore
└── README.md
```

---

## Partes

### Cliente.c

**Estructuras**
Aquí se encuentran las estructuras para los datos compartidos, comunicación con el servidor.

**void down, void up y int try_down**
Funciones para cambiar el estado de los semáforos.

**int esperar_servidor**
Función usada para esperar la conexión con el servidor por medio de semáforos.

**int conectar_servidor**
Función que se encarga de conectarse con el servidor con la secuencia para se asignados a un slot y a su hilo respectivo.

**int enviar_peticion**
Envía un paquete de solicitud y espera confirmación de parte del servidor.

**void desconectar_servidor**
Detiene la conexión con el servidor y cierra la memoria.

**int iniciar_sesion**
Se crea la interfaz de inicio de sesión y manejo de datos.

**void crear_usuario**
Crea la interfaz de creación de usuario y manejo de las entradas.

**void recuperar_password**
Crea la interfaz de recuperación de contraseña y lo maneja.

**int main**
Ejecución del sistema principal.

### Servidor.c

**Estructuras**
Aquí se encuentran las estructuras para los datos compartidos, comunicación con el cliente.

**int Crea_semaforo, void down y void up**
Funciones para cambiar el estado de los semáforos y creación de semáforos.

**void limpiar_recursos**
Se usa para limpiar memoria y datos almacenados durante la ejecución.

**void procesarPeticion**
Manejo de peticiones y registro en el log.

**void *AtenderCliente**
Función con apuntador para los hilos de atención de clientes.

**int main**
Ejecución del sistema principal.

### interfaz_cocina.c

**Estructuras**
Aquí se encuentran las estructuras para los datos compartidos, comunicación con el servidor.

**int enviar_peticion_cocina**
Solicita los datos para la cocina.

**void dibujar_recuadro_pedido_horizontal**
Crea el recuadro de los pedidos de manera dinámica acorde al tamaño de la ventana.

**void interfaz_cocina_ejecutar**
Genera la interfaz de cocina y maneja los eventos de botones.

### interfaz_mesero

**Estructuras**
Aquí se encuentran las estructuras para los datos compartidos, comunicación con el servidor.

**int enviar_peticion_mesero**
Solicita los datos para el mesero.

**void calcular_layout**
Calcula el layout de las opciones de manera dinámica acorde al tamaño de la venta en el eje x e y.

**void dibujar_recuadro_producto**
Crea el recuadro del producto acorde al calculo dinámico.

**void interfaz_mesero_ejecutar**
Genera la interfaz de mesero y maneja los eventos de botones.

### logger.c

---

## Compilación

### Con makefile

Compilación inicial: 
    `make`

Ejecución:
    `make run-servidor` "Solo una vez"
    `make run-cliente` "Al una vez o dos para manejar cocina y meseros a la par"

Re-compilar:
    `make clean`
    `make`

Eliminar archivos de compilación generados:
    `make clean`

Eliminar todos los archivos generados:
    `make cleanall`

## Manual

Servidor: 
`gcc -o Servidor Servidor.c usuario.c productos.c pedidos.c logger.c -lpthread -lssl -lcrypto`

Cliente:
`gcc -o Cliente Cliente.c usuario.c ui.c interfaz_mesero.c interfaz_cocina.c -lncurses -lssl -lcrypto`