# Sistema de Comandas - Restaurante

Sistema cliente-servidor para la gestión de comandas en restaurantes, desarrollado en C con comunicación mediante memoria compartida y semáforos. Implementa interfaces separadas para meseros, cocina y administración con autenticación de usuarios.

## Tabla de Contenidos

- [Características](#características)
- [Arquitectura](#arquitectura)
- [Requisitos](#requisitos)
- [Estructura del Proyecto](#estructura-del-proyecto)
- [Compilación](#compilación)
- [Uso](#uso)
- [Documentación de Módulos](#documentación-de-módulos)
- [Protocolo de Comunicación](#protocolo-de-comunicación)
- [Sistema de Logging](#sistema-de-logging)
- [Gestión de Inventario](#gestión-de-inventario)

---

## Características

- **Autenticación de usuarios** con encriptación MD5
- **Interfaz triple**: Meseros, Cocina y Administración con funcionalidades específicas
- **Arquitectura cliente-servidor** usando memoria compartida IPC
- **Gestión de pedidos en tiempo real** con estados (Pendiente, En Progreso, Completado)
- **Sistema de logging asíncrono** para auditoría
- **Interfaz de usuario en terminal** usando ncurses
- **Validación robusta** de datos (contraseñas, emails, teléfonos)
- **Soporte multi-cliente** (hasta 10 clientes simultáneos)
- **Gestión de inventario** con control de ingredientes
- **Gestión de usuarios** con roles y permisos
- **Recuperación de contraseña** con tokens
- **Gestión completa de productos e ingredientes** desde la interfaz de administrador

---

## Arquitectura

### Modelo Cliente-Servidor

```
┌─────────────┐         ┌──────────────────┐         ┌─────────────┐
│  Cliente 1  │◄───────►│                  │◄───────►│  Cliente 2  │
│  (Mesero)   │         │    Servidor      │         │  (Cocina)   │
└─────────────┘         │                  │         └─────────────┘
                        │  - Memoria       │
┌─────────────┐         │    Compartida    │         ┌─────────────┐
│  Cliente 3  │◄───────►│  - Semáforos     │◄───────►│  Cliente 4  │
│  (Admin)    │         │  - Hilos         │         │  (...)      │
└─────────────┘         └──────────────────┘         └─────────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  Sistema de     │
                        │  Archivos       │
                        │  - usuarios     │
                        │  - productos    │
                        │  - pedidos      │
                        │  - inventario   │
                        │  - *.log        │
                        └─────────────────┘
```

### Comunicación IPC

- **Memoria Compartida**: Intercambio de peticiones y respuestas
- **Semáforos**: Sincronización de acceso a recursos compartidos
- **Hilos (pthread)**: Un hilo por cliente conectado
- **Sistema de colas**: Logging asíncrono no bloqueante

---

## Requisitos

### Dependencias del Sistema

- **Sistema Operativo**: Linux/Unix
- **Compilador**: GCC 7.0 o superior
- **Bibliotecas**:
  - `ncurses` - Interfaz de usuario en terminal
  - `openssl` - Encriptación MD5
  - `pthread` - Manejo de hilos

### Instalación de Dependencias

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential libncurses5-dev libncursesw5-dev libssl-dev
```

#### Fedora/RHEL/CentOS
```bash
sudo dnf install gcc make ncurses-devel openssl-devel
```

#### Arch Linux
```bash
sudo pacman -S base-devel ncurses openssl
```

---

## Estructura del Proyecto

```
SistemasOperativosTrabajoFinalDEV/
│
├── Servidor.c                 # Servidor principal (gestión de clientes)
├── Cliente.c                  # Cliente principal (menú de autenticación)
│
├── usuario.c / usuario.h      # Gestión de usuarios y autenticación
├── usuarios.c / usuarios.h    # Funciones auxiliares de usuario
├── productos.c / productos.h  # Catálogo de productos
├── pedidos.c / pedidos.h      # Gestión de pedidos
├── inventario.c / inventario.h # Gestión de inventario y stock
├── interfaces.c / interfaces.h # Funciones comunes de las interfaces
├── ui.c / ui.h                # Interfaz de usuario (ncurses)
├── logger.c / logger.h        # Sistema de logging asíncrono
├── sesion.c / sesion.h        # Gestión de sesiones
├── recuperar_password.c / recuperar_password.h # Recuperación de contraseña
│
├── protocolo.h                # Definiciones del protocolo de comunicación
│
├── makefile                   # Script de compilación
├── productos                  # Archivo de datos de productos
├── usuarios                   # Archivo de datos de usuarios (generado)
├── pedidos                    # Archivo de datos de pedidos (generado)
├── inventario                 # Archivo de datos de ingredientes
├── producto_ingrediente       # Archivo de relaciones producto-ingredientes
│
├── autenticacion.log          # Log de eventos de autenticación (generado)
├── cocina.log                 # Log de eventos de cocina (generado)
│
├── .gitignore
└── README.md
```

---

## Compilación

### Usando Makefile (Recomendado)

#### Compilación inicial
```bash
make
```

#### Limpiar archivos compilados
```bash
make clean
```

#### Limpiar todo (incluye datos y logs)
```bash
make cleanall
```

#### Crear datos iniciales
```bash
make data
```

#### Ejecutar servidor
```bash
make run-servidor
```

#### Ejecutar cliente
```bash
make run-cliente
```

#### Ejecutar todo el proceso (limpiar, compilar, crear datos, correr servidor)
```bash
make fresh
```

### Compilación Manual

#### Servidor
```bash
gcc -Wall -g -o Servidor Servidor.c usuario.c productos.c pedidos.c logger.c inventario.c -lpthread -lssl -lcrypto
```

#### Cliente
```bash
gcc -Wall -g -o Cliente Cliente.c usuario.c ui.c interfaces.c productos.c pedidos.c inventario.c logger.c recuperar_password.c -lncurses -lssl -lcrypto
```

---

## Uso

### 1. Iniciar el Servidor

En una terminal:
```bash
./Servidor
```

El servidor mostrará:
```
+---------------------------------------+
|  SERVIDOR DE USUARIOS INICIADO        |
+---------------------------------------+

PID del Servidor: 12345
Sistema de usuarios inicializado
Usuarios registrados: 0
Productos cargados: 10
Ingredientes cargados: 5
Pedidos cargados: 0
Sistema de logging inicializado

*** SERVIDOR LISTO PARA ACEPTAR CONEXIONES ***

Esperando clientes...
(Presione Ctrl+C para detener)
```

### 2. Iniciar Cliente(s)

En otra(s) terminal(es):
```bash
./Cliente
```

El cliente esperará la conexión con el servidor y mostrará el menú principal.

### 3. Flujo de Trabajo

#### Para Meseros:
1. Crear usuario (tipo Mesero) o Iniciar sesión
2. Seleccionar "Tomar Nueva Orden"
3. Navegar por productos con flechas
4. Presionar ENTER para agregar (+1)
5. Presionar '-' para quitar (-1)
6. Presionar 'O' para confirmar orden
7. Ingresar número de mesa

#### Para Cocina:
1. Crear usuario (tipo Cocina) o Iniciar sesión
2. Ver pedidos pendientes y en progreso
3. Presionar '1' para marcar como "En Progreso"
4. Presionar '2' para marcar como "Listo"
5. Presionar 'R' para actualizar vista

#### Para Administración:
1. Crear usuario (tipo Administrador) o Iniciar sesión
2. Acceder a la interfaz de administración
3. Gestionar usuarios (crear, modificar, eliminar)
4. Gestionar productos (crear, modificar, eliminar)
5. Gestionar inventario (ingredientes y stock)
6. Ver reportes de pedidos y actividad

### 4. Detener el Sistema

- **Cliente**: Navegar a "Salir" en el menú o presionar ESC
- **Servidor**: Presionar `Ctrl+C` (cierre limpio con liberación de recursos)

---

## Documentación de Módulos

### Servidor.c

**Responsabilidades principales:**
- Crear y gestionar memoria compartida IPC
- Asignar slots a clientes conectados
- Crear hilos para atender cada cliente
- Procesar peticiones (login, crear usuario, pedidos, etc.)
- Coordinar sistema de logging
- Gestionar inventario y productos

**Funciones clave:**
- `Crea_semaforo()` - Crea semáforos con valor inicial
- `down()` / `up()` - Operaciones P y V sobre semáforos
- `limpiar_recursos()` - Liberación limpia de recursos al cerrar
- `procesarPeticion()` - Router de operaciones del protocolo
- `AtenderCliente()` - Función de hilo para cada cliente
- `main()` - Loop principal del servidor

### Cliente.c

**Responsabilidades principales:**
- Conectar al servidor mediante memoria compartida
- Obtener slot asignado por el servidor
- Mostrar menú de autenticación
- Lanzar interfaz apropiada (mesero/cocina/admin) según tipo de usuario
- Enviar peticiones y recibir respuestas

**Funciones clave:**
- `esperar_servidor()` - Espera activa hasta que servidor esté listo
- `conectar_servidor()` - Establece conexión y obtiene slot
- `enviar_peticion()` - Comunicación sincrónica con servidor
- `desconectar_servidor()` - Cierre limpio de conexión
- `iniciar_sesion()` - Flujo de autenticación
- `crear_usuario()` - Flujo de registro con validaciones
- `recuperar_password()` - Cambio de contraseña

### usuario.c

**Responsabilidades:**
- Gestión de usuarios en memoria
- Encriptación de contraseñas con MD5
- Validación de datos (password, email, teléfono)
- Persistencia en archivo

**Funciones clave:**
- `usuario_init()` - Inicializar sistema
- `usuario_cargar()` / `usuario_guardar()` - I/O de archivo
- `encriptar_password()` - Hash MD5 de contraseñas
- `usuario_crear()` - Registro de nuevo usuario
- `usuario_autenticar()` - Verificar credenciales
- `validar_password()` - Validar requisitos de seguridad
- `validar_email()` - Validar formato de correo
- `validar_telefono()` - Validar formato de teléfono
- `usuario_eliminar_por_nombre()` - Eliminar usuario por nombre

**Requisitos de contraseña:**
- Mínimo 8 caracteres
- Al menos 1 mayúscula (A-Z)
- Al menos 1 minúscula (a-z)
- Al menos 1 número (0-9)
- Al menos 2 símbolos especiales (* / _ - + .)

### productos.c

**Responsabilidades:**
- Gestión de catálogo de productos
- Persistencia en archivo
- Gestión de relaciones producto-ingredientes

**Funciones clave:**
- `productos_init()` / `productos_cargar()` / `productos_guardar()`
- `productos_get_total()` - Cantidad de productos
- `productos_get_by_index()` - Acceso por índice
- `productos_get_by_id()` - Búsqueda por ID
- `productos_agregar()` - Añadir nuevo producto
- `producto_agregar_ingrediente()` - Vincular producto con ingrediente
- `producto_verificar_disponibilidad()` - Verificar disponibilidad de producto
- `producto_eliminar()` - Eliminar un producto

### inventario.c

**Responsabilidades:**
- Gestión de ingredientes y stock
- Vinculación con productos
- Control de disponibilidad de ingredientes

**Funciones clave:**
- `inventario_init()` / `inventario_cargar()` / `inventario_guardar()`
- `inventario_get_total()` - Cantidad de ingredientes
- `inventario_get_by_index()` - Acceso por índice
- `inventario_get_by_id()` - Búsqueda por ID
- `inventario_agregar()` - Añadir nuevo ingrediente
- `inventario_modificar_cantidad()` - Modificar stock de ingrediente
- `inventario_eliminar_ingrediente()` - Eliminar ingrediente
- `producto_vincular_ingrediente()` - Vincular producto con ingrediente
- `inventario_actualizar_stock()` - Actualizar stock tras crear pedido

### interfaces.c

**Responsabilidades:**
- Funciones comunes de las interfaces de usuario (mesero, cocina, admin)
- Funciones para dibujar componentes de UI ncurses
- Gestión de la interfaz de administración

**Funciones clave:**
- `dibujar_botones_si_no()` - Dibujar botones de selección
- `dibujar_botones_confirmacion()` - Dibujar botones de confirmación
- `dibujar_recuadro_usuario_horizontal()` - Dibujar tarjeta de usuario
- `dibujar_recuadro_producto_admin()` - Dibujar tarjeta de producto para admin
- `dibujar_recuadro_ingrediente_admin()` - Dibujar tarjeta de ingrediente para admin
- `mostrar_menu_usuarios()` - Interfaz de gestión de usuarios
- `mostrar_menu_productos()` - Interfaz de gestión de productos
- `mostrar_menu_ingredientes()` - Interfaz de gestión de ingredientes
- `interfaz_admin_ejecutar()` - Punto de entrada de la interfaz de administrador

### ui.c

**Responsabilidades:**
- Inicializar y configurar ncurses
- Funciones de dibujo y entrada de usuario
- Menús interactivos con navegación por flechas
- Mensajes de éxito/error/información

**Funciones clave:**
- `ui_init()` / `ui_cleanup()` - Inicialización y limpieza de ncurses
- `ui_clear_screen()` - Limpiar pantalla con bordes
- `ui_print_title()` - Título centrado con color
- `ui_read_input()` - Lectura de texto (con opción de ocultar)
- `ui_show_menu()` - Menú navegable con flechas
- `ui_show_success()` / `ui_show_error()` - Mensajes modales

**Esquema de colores:**
- Par 1: Cyan/Negro - Títulos
- Par 2: Negro/Magenta - Éxito
- Par 3: Blanco/Rojo - Error
- Par 4: Magenta/Negro - Texto normal
- Par 5: Negro/Amarillo - Resaltado
- Par 6: Blanco/Magenta - Requisitos
- Par 9: Verde/Blanco - Confirmaciones
- Par 10: Blanco/Negro - Texto general

### logger.c

**Responsabilidades:**
- Sistema de logging asíncrono con colas
- Dos hilos independientes (autenticación y cocina)
- Escritura en archivos de log con timestamps
- No bloquea operaciones principales

**Funciones clave:**
- `logger_init()` / `logger_cleanup()` - Inicialización y cierre
- `log_evento_auth()` - Encolar evento de autenticación
- `log_evento_cocina()` - Encolar evento de cocina
- `hilo_logger_auth()` - Hilo consumidor para auth
- `hilo_logger_cocina()` - Hilo consumidor para cocina
- `obtener_timestamp_formateado()` - Formato de fecha/hora

**Eventos registrados:**

*Autenticación:*
- Cliente conectado/desconectado
- Login exitoso/fallido
- Usuario creado
- Usuario duplicado
- Recuperación de contraseña

*Cocina:*
- Pedido creado
- Pedido en progreso
- Pedido completado
- Errores en pedidos

---

## Protocolo de Comunicación

### Operaciones Soportadas

| Código | Operación | Descripción |
|--------|-----------|-------------|
| `OP_LOGIN` | Iniciar sesión | Autenticar usuario y contraseña |
| `OP_CREAR_USUARIO` | Crear usuario | Registrar nuevo usuario |
| `OP_RECUPERAR_PASS` | Recuperar contraseña | Cambiar contraseña de usuario |
| `OP_VERIFICAR_USUARIO` | Verificar usuario | Comprobar si usuario existe |
| `OP_LOGOUT` | Cerrar sesión | Desconectar usuario |
| `OP_LISTAR_PRODUCTOS` | Listar productos | Obtener catálogo completo |
| `OP_CREAR_PEDIDO` | Crear pedido | Registrar nueva orden |
| `OP_LISTAR_PEDIDOS` | Listar pedidos | Obtener pedidos activos |
| `OP_CAMBIAR_ESTADO_PEDIDO` | Cambiar estado | Actualizar estado de pedido |
| `OP_LISTAR_USUARIOS` | Listar usuarios | Obtener lista de usuarios |
| `OP_MODIFICAR_USUARIO` | Modificar usuario | Actualizar datos de usuario |
| `OP_ELIMINAR_USUARIO` | Eliminar usuario | Borrar usuario del sistema |
| `OP_OBTENER_USUARIO` | Obtener usuario | Recuperar datos de usuario |
| `OP_MODIFICAR_PRODUCTO` | Modificar producto | Actualizar datos de producto |
| `OP_ELIMINAR_PRODUCTO` | Eliminar producto | Borrar producto del sistema |
| `OP_CREAR_PRODUCTO` | Crear producto | Registrar nuevo producto |
| `OP_OBTENER_PRODUCTO` | Obtener producto | Recuperar datos de producto |
| `OP_LISTAR_INGREDIENTES` | Listar ingredientes | Obtener lista de ingredientes |
| `OP_CREAR_INGREDIENTE` | Crear ingrediente | Registrar nuevo ingrediente |
| `OP_MODIFICAR_INGREDIENTE` | Modificar ingrediente | Actualizar stock de ingrediente |
| `OP_ELIMINAR_INGREDIENTE` | Eliminar ingrediente | Borrar ingrediente del sistema |
| `OP_OBTENER_INGREDIENTE` | Obtener ingrediente | Recuperar datos de ingrediente |
| `OP_ELIMINAR_PEDIDO` | Eliminar pedido | Borrar pedido del sistema |
| `OP_OBTENER_PEDIDO` | Obtener pedido | Recuperar datos de pedido |
| `OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO` | Verificar disponibilidad | Comprobar si producto está disponible |
| `OP_LISTAR_LOGS` | Listar logs | Obtener lista de archivos de log |
| `OP_VER_LOG` | Ver log | Leer contenido de archivo de log |

### Códigos de Respuesta

| Código | Significado |
|--------|-------------|
| `RESP_OK` | Operación exitosa |
| `RESP_ERROR` | Error general |
| `RESP_USUARIO_EXISTE` | Usuario ya registrado |
| `RESP_USUARIO_NO_EXISTE` | Usuario no encontrado |
| `RESP_CREDENCIALES_INVALIDAS` | Usuario/contraseña incorrectos |
| `RESP_LIMITE_ALCANZADO` | Límite alcanzado (usuarios/pedidos/productos) |
| `RESP_VALIDACION_FALLIDA` | Datos no cumplen requisitos |

### Estructura de Comunicación

```c
// Petición del cliente
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

// Respuesta del servidor
typedef struct {
    CodigoRespuesta codigo;
    int tipo_usuario;  // 0=Mesero, 1=Cocina, 2=Administrador
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
```

---

## Sistema de Logging

### Archivos de Log

#### autenticacion.log
Registra todos los eventos relacionados con usuarios:
```
[05-12-2024 14:23:15] [LOGIN_EXITOSO] Usuario: juan | Cliente ID: 1 | Tipo: Mesero
[05-12-2024 14:25:30] [USUARIO_CREADO] Usuario: maria | Cliente ID: 2 | Nombre: Maria Garcia, Tipo: Cocina, Email: maria@example.com
[05-12-2024 14:27:45] [LOGIN_FALLIDO] Usuario: pedro | Cliente ID: 1 | Credenciales incorrectas
```

#### cocina.log
Registra todos los eventos de pedidos:
```
[05-12-2024 14:30:00] [PEDIDO_CREADO] Pedido #1 | Mesa: 5 | Mesero: Juan Perez | Items: 3 | Creado por mesero
[05-12-2024 14:32:15] [PEDIDO_EN_PROGRESO] Pedido #1 | Mesa: 5 | Mesero: Juan Perez | Items: 3 | Estado cambiado a EN PROGRESO
[05-12-2024 14:35:20] [PEDIDO_COMPLETADO] Pedido #1 | Mesa: 5 | Mesero: Juan Perez | Items: 3 | Estado cambiado a COMPLETADO
```

### Arquitectura de Logging

- **Colas circulares**: Buffer de 1000 eventos por tipo
- **Hilos independientes**: Escritura asíncrona sin bloqueo
- **Mutex**: Protección de colas en acceso concurrente
- **Formato consistente**: Timestamp + Evento + Detalles

---

## Configuración Avanzada

### Límites del Sistema

En `protocolo.h` y archivos de cabecera:

```c
#define MAX_CLIENTES 10              // Clientes simultáneos
#define MAX_USUARIOS 100             // Usuarios totales
#define MAX_PRODUCTOS 100            // Productos en catálogo
#define MAX_PEDIDOS 500              // Pedidos históricos
#define MAX_ITEMS_PEDIDO 20          // Items por pedido
#define MAX_LOG_QUEUE 1000           // Buffer de logs
#define MAX_INGREDIENTES 200         // Ingredientes en inventario
#define MAX_INGREDIENTES_PRODUCTO 20 // Ingredientes por producto
```

### Archivos de Persistencia

- `usuarios` - Base de datos de usuarios (formato: name|user|pass_hash|mail|telf|tipo)
- `productos` - Catálogo de productos (formato: id|nombre|descripcion|precio)
- `pedidos` - Histórico de pedidos (formato complejo con items embebidos)
- `inventario` - Datos de ingredientes y stock (formato: id|nombre|stock|unidad)
- `producto_ingrediente` - Relaciones producto-ingrediente (formato: id_producto|id_ingrediente|cantidad_necesaria)

---

## Resolución de Problemas

### Problemas comunes de compilación

**Error con biblioteca ncurses**:
```
fatal error: ncurses.h: No such file or directory
```
**Solución**:
- Ubuntu/Debian: `sudo apt-get install libncurses5-dev libncursesw5-dev`
- Fedora/RHEL/CentOS: `sudo dnf install ncurses-devel`
- Arch Linux: `sudo pacman -S ncurses`

### Problemas de encriptación

**Error con OpenSSL**:
```
fatal error: openssl/evp.h: No such file or directory
```
**Solución**:
- Ubuntu/Debian: `sudo apt-get install libssl-dev`
- Fedora/RHEL/CentOS: `sudo dnf install openssl-devel`
- Arch Linux: `sudo pacman -S openssl`

### El cliente no puede conectarse

**Problema**: "Error: No se pudo acceder a la memoria compartida"

**Solución**:
1. Verificar que el servidor esté ejecutándose
2. Esperar unos segundos tras iniciar el servidor
3. Verificar permisos de archivos de llave (servidor_usuarios_*)

### Errores de semáforo

**Problema**: "Error al crear semáforo"

**Solución**:
```bash
# Limpiar semáforos huérfanos
ipcs -s | grep $USER | awk '{print $2}' | xargs -n1 ipcrm -s

# Limpiar memoria compartida huérfana
ipcs -m | grep $USER | awk '{print $2}' | xargs -n1 ipcrm -m
```

### Servidor no responde

**Solución**:
1. Cerrar servidor con `Ctrl+C` (cierre limpio)
2. Si no responde, usar `kill -SIGTERM <PID>`
3. Limpiar recursos IPC (ver comando anterior)
4. Reiniciar servidor

### Problemas de ncurses

**Problema**: Terminal corrupta tras cerrar cliente

**Solución**:
```bash
reset
# o
stty sane
```

---

## Gestión de Inventario

El sistema incluye una funcionalidad avanzada de gestión de inventario que permite a los administradores:

- **Registrar ingredientes**: Nombre, ID, stock actual y unidad de medida
- **Actualizar stock**: Ajustar cantidades según abastecimiento
- **Relacionar productos con ingredientes**: Definir qué ingredientes y cuánta cantidad se necesita para cada producto
- **Control de stock en tiempo real**: Al crear pedidos, se reducen automáticamente los ingredientes utilizados

### Funcionalidades de Inventario

#### En interfaz de administración:
1. **Lista de ingredientes**: Visualización de todos los ingredientes con stock actual
2. **Agregar ingrediente**: Crear nuevos ingredientes con nombre, unidad y stock inicial
3. **Modificar stock**: Actualizar cantidades disponibles
4. **Eliminar ingrediente**: Remover ingredientes no utilizados
5. **Asociar ingredientes a productos**: Definir la receta de cada producto
6. **Reporte de stock bajo**: Alerta cuando el stock está por debajo del umbral mínimo

#### Control automático:
- Al crear un pedido, el sistema verifica disponibilidad de ingredientes
- Si no hay suficiente stock, se impide la creación del pedido
- Actualización automática de inventario al confirmar pedidos
- Registro de movimientos de inventario en logs

### Archivo de persistencia

- `inventario` - Almacena la información de ingredientes en formato: `id|nombre|stock|unidad`
- `producto_ingrediente` - Almacena las relaciones entre productos e ingredientes en formato: `id_producto|id_ingrediente|cantidad_necesaria`
- El sistema carga estos archivos al iniciar y los actualiza periódicamente
- Los cambios en el inventario son persistentes entre reinicios del sistema