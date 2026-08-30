# Sistema de Comandas

Sistema cliente-servidor para la gestión de comandas de un restaurante, escrito en C sobre Linux. La comunicación entre procesos se resuelve con **memoria compartida System V y semáforos**, el servidor atiende a cada cliente en un **hilo POSIX** dedicado, y la interfaz corre en terminal con **ncurses**.

El sistema distingue tres roles _mesero, cocina y administrador_ cada uno con su propia interfaz y permisos.

```
┌──────────────┐   ┌──────────────┐   ┌──────────────┐
│   Cliente    │   │   Cliente    │   │   Cliente    │
│   (Mesero)   │   │   (Cocina)   │   │   (Admin)    │
└──────┬───────┘   └──────┬───────┘   └──────┬───────┘
       │                  │                  │
       └──────────────────┼──────────────────┘
                          │
             Memoria compartida + semáforos
                          │
                 ┌────────┴────────┐
                 │    Servidor     │
                 │  1 hilo/cliente │
                 │  2 hilos de log │
                 └────────┬────────┘
                          │
              ┌───────────┴───────────┐
              │   Archivos de datos   │
              │  usuarios · productos │
              │  pedidos · inventario │
              │  producto_ingrediente │
              │        *.log          │
              └───────────────────────┘
```

## Tabla de contenidos

- [Características](#características)
- [Requisitos](#requisitos)
- [Inicio rápido](#inicio-rápido)
- [Compilación](#compilación)
- [Uso por rol](#uso-por-rol)
- [Arquitectura](#arquitectura)
- [Protocolo de comunicación](#protocolo-de-comunicación)
- [Seguridad y autenticación](#seguridad-y-autenticación)
- [Recuperación de contraseña](#recuperación-de-contraseña)
- [Sistema de logging](#sistema-de-logging)
- [Inventario y recetas](#inventario-y-recetas)
- [Formato de los archivos de datos](#formato-de-los-archivos-de-datos)
- [Estructura del proyecto](#estructura-del-proyecto)
- [Módulos](#módulos)
- [Límites del sistema](#límites-del-sistema)
- [Resolución de problemas](#resolución-de-problemas)
- [Limitaciones conocidas](#limitaciones-conocidas)
- [Licencia](#licencia)

---

## Características

- **Autenticación con hash SHA-256** y validación de contraseñas fuertes.
- **Tres interfaces según rol**: mesero, cocina y administración.
- **Arquitectura cliente-servidor** sobre memoria compartida System V.
- **Concurrencia real**: un hilo por cliente, hasta 10 clientes simultáneos.
- **Pedidos en tiempo real** con tres estados: Pendiente → En progreso → Listo.
- **Logging asíncrono** en dos colas independientes, sin bloquear la atención de clientes.
- **Inventario con recetas**: cada producto declara qué ingredientes consume, y el stock se descuenta al crear el pedido.
- **Administración completa** de usuarios, productos e ingredientes desde la interfaz de admin.
- **Recuperación de contraseña** validada contra el servidor, bloqueada para cuentas de administrador.
- **Visor de logs** integrado en la interfaz de administración.

---

## Requisitos

| Componente | Versión / paquete |
|---|---|
| Sistema operativo | Linux o Unix con IPC System V |
| Compilador | GCC 7.0 o superior |
| ncurses | Interfaz de terminal |
| OpenSSL (libcrypto) | Hash SHA-256 |
| pthread | Hilos del servidor y del logger |

### Instalación de dependencias

```bash
# Ubuntu / Debian
sudo apt-get install build-essential libncurses-dev libssl-dev

# Fedora / RHEL / CentOS
sudo dnf install gcc make ncurses-devel openssl-devel

# Arch Linux
sudo pacman -S base-devel ncurses openssl
```

---

## Inicio rápido

```bash
make fresh          # limpia, compila y genera los datos de ejemplo
./Servidor          # terminal 1
./Cliente           # terminal 2 (y las que hagan falta)
```

`make fresh` crea tres usuarios de prueba:

| Usuario | Contraseña | Rol |
|---|---|---|
| `admin` | `admin123` | Administrador |
| `cocina` | `1234` | Cocina |
| `mesero` | `1234` | Mesero |

> Estas contraseñas son solo para pruebas: no cumplen los requisitos que el sistema exige al registrar usuarios nuevos, porque se insertan ya hasheadas directamente en el archivo de datos.

El cliente espera hasta 30 segundos a que el servidor esté listo, así que el orden de arranque no es crítico mientras el servidor llegue dentro de ese margen.

---

## Compilación

### Con make

| Comando | Qué hace |
|---|---|
| `make` | Compila `Servidor` y `Cliente` y crea los archivos de llave IPC |
| `make clean` | Borra objetos, binarios y archivos de llave |
| `make cleanall` | `clean` + borra datos y logs |
| `make data` | Genera datos de ejemplo (usuarios + menú) |
| `make datauser` | Genera solo los usuarios de prueba |
| `make datamenu` | Genera solo productos, inventario y recetas |
| `make fresh` | `cleanall` + compilar + `data` |
| `make run-servidor` | Compila y ejecuta el servidor |
| `make run-cliente` | Compila y ejecuta el cliente |
| `make help` | Lista los comandos disponibles |

### Compilación manual

```bash
# Servidor
gcc -Wall -g -o Servidor Servidor.c logger.c usuario.c productos.c \
    pedidos.c inventario.c -lpthread -lssl -lcrypto

# Cliente
gcc -Wall -g -o Cliente Cliente.c interfaces.c ui.c usuario.c productos.c \
    pedidos.c inventario.c logger.c recuperar_password.c -lncurses -lssl -lcrypto
```

El servidor no enlaza `ui.c`, `interfaces.c` ni `recuperar_password.c`: no dibuja nada, solo escribe en stdout.

---

## Uso por rol

### Mesero

1. Iniciar sesión con una cuenta de tipo Mesero.
2. **Tomar Nueva Orden**: navegar el catálogo con las flechas.
   - `ENTER` o `+` agrega una unidad
   - `-` quita una unidad
   - `O` confirma la orden y pide el número de mesa
   - `ESC` cancela
3. **Ver Mis Ordenes**: consultar el estado de los pedidos enviados.

Un producto sin ingredientes suficientes en inventario aparece marcado como no disponible y no puede ordenarse.

### Cocina

1. Iniciar sesión con una cuenta de tipo Cocina.
2. La pantalla lista los pedidos activos con mesa, mesero e items, y un contador de pendientes y en progreso.
   - Flechas / `Re Pág` / `Av Pág` / `Inicio` / `Fin`: navegar
   - `1`: marcar el pedido como **En progreso**
   - `2`: marcar el pedido como **Listo**
   - `R`: refrescar la lista
   - `ESC`: cerrar sesión

### Administrador

Menú con cuatro secciones:

1. **Administrador de usuarios** — listar, crear, editar y eliminar cuentas, incluido el rol.
2. **Administrador de productos** — alta, edición y baja de productos, y edición de su receta (qué ingredientes consume y en qué cantidad).
3. **Administrador de ingredientes** — alta, ajuste de stock y baja de ingredientes.
4. **Ver logs** — últimas líneas de `autenticacion.log` y de `cocina.log`.

### Cerrar el sistema

- **Cliente**: opción *Salir* del menú, o `ESC`.
- **Servidor**: `Ctrl+C`. El manejador de señal libera memoria compartida y semáforos antes de terminar.

---

## Arquitectura

### Comunicación IPC

El servidor crea un segmento de memoria compartida con un arreglo de 10 slots. Cada cliente toma un slot libre al conectarse y lo libera al salir.

```c
typedef struct {
    int cliente_conectado;
    int peticion_lista;      // el cliente publicó una petición
    int respuesta_lista;     // el servidor publicó la respuesta
    Peticion peticion;
    Respuesta respuesta;
    int cliente_id;          // PID del cliente
    int hilo_asignado;
} DatosCliente;
```

El ciclo de una operación:

1. El cliente escribe su `Peticion` en el slot y levanta `peticion_lista`.
2. El hilo del servidor asignado a ese slot detecta la bandera, ejecuta la operación y escribe la `Respuesta`.
3. El servidor levanta `respuesta_lista`; el cliente la consume y baja la bandera.

Todo acceso al segmento está protegido por un semáforo binario (`down`/`up`). Un segundo semáforo, el de estado, sirve de señal de "servidor listo" para que el cliente sepa cuándo puede conectarse.

Los archivos `servidor_usuarios_mem`, `servidor_usuarios_sem` y `servidor_estado_sem` son archivos vacíos que solo existen para generar llaves IPC estables con `ftok()`.

### Concurrencia

- **Un hilo por cliente** conectado, creado bajo demanda.
- **Dos hilos de logging**, uno para autenticación y otro para cocina, que consumen sus colas circulares.
- **Espera con sondeo** (`usleep`) tanto en el cliente como en el servidor, con un límite de 10 segundos por operación.

---

## Protocolo de comunicación

Cliente y servidor intercambian las estructuras `Peticion` y `Respuesta` definidas en `protocolo.h`.

### Operaciones

| Código | Operación |
|---|---|
| `OP_LOGIN` | Autenticar usuario y contraseña |
| `OP_CREAR_USUARIO` | Registrar un usuario nuevo |
| `OP_RECUPERAR_PASS` | Asignar una contraseña nueva |
| `OP_VERIFICAR_USUARIO` | Comprobar si un usuario existe y de qué tipo es |
| `OP_LOGOUT` | Cerrar sesión |
| `OP_LISTAR_PRODUCTOS` | Catálogo completo |
| `OP_CREAR_PEDIDO` | Registrar una orden |
| `OP_LISTAR_PEDIDOS` | Pedidos activos |
| `OP_CAMBIAR_ESTADO_PEDIDO` | Cambiar el estado de un pedido |
| `OP_ELIMINAR_PEDIDO` / `OP_OBTENER_PEDIDO` | Baja y consulta de pedidos |
| `OP_LISTAR_USUARIOS` | Lista de usuarios |
| `OP_CREAR_USUARIO` / `OP_MODIFICAR_USUARIO` / `OP_ELIMINAR_USUARIO` / `OP_OBTENER_USUARIO` | ABM de usuarios |
| `OP_CREAR_PRODUCTO` / `OP_MODIFICAR_PRODUCTO` / `OP_ELIMINAR_PRODUCTO` / `OP_OBTENER_PRODUCTO` | ABM de productos |
| `OP_LISTAR_INGREDIENTES` / `OP_CREAR_INGREDIENTE` / `OP_MODIFICAR_INGREDIENTE` / `OP_ELIMINAR_INGREDIENTE` / `OP_OBTENER_INGREDIENTE` | ABM de inventario |
| `OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO` | Comprobar stock para un producto y cantidad |
| `OP_LISTAR_LOGS` / `OP_VER_LOG` | Listar y leer archivos de log |

### Códigos de respuesta

| Código | Significado |
|---|---|
| `RESP_OK` | Operación exitosa |
| `RESP_ERROR` | Error general |
| `RESP_USUARIO_EXISTE` | El usuario ya está registrado |
| `RESP_USUARIO_NO_EXISTE` | Usuario no encontrado |
| `RESP_CREDENCIALES_INVALIDAS` | Usuario o contraseña incorrectos |
| `RESP_LIMITE_ALCANZADO` | Se alcanzó un límite del sistema |
| `RESP_VALIDACION_FALLIDA` | Los datos no cumplen los requisitos |

`OP_VERIFICAR_USUARIO` devuelve `RESP_USUARIO_EXISTE` junto con el `tipo_usuario` y el `nombre` de la cuenta cuando existe, y `RESP_OK` con `tipo_usuario = -1` cuando el nombre está libre. Lo usan tanto el alta de usuarios (para rechazar duplicados) como la recuperación de contraseña (para confirmar la cuenta y su rol).

---

## Seguridad y autenticación

### Hash de contraseñas

Las contraseñas se almacenan como **SHA-256 en hexadecimal** (64 caracteres), calculado con la API `EVP` de OpenSSL. La contraseña en claro nunca se guarda en disco ni se registra en los logs.

```c
void encriptar_password(const char* input, char* output);  // usuario.c
```

El campo `pass` mide 65 bytes (64 caracteres más el terminador) tanto en `USUARIO` como en `UsuarioMsg`.

### Requisitos de contraseña

Toda contraseña nueva, ya sea al crear la cuenta o al recuperarla, debe cumplir:

- Mínimo 8 caracteres
- Al menos 1 mayúscula (A–Z)
- Al menos 1 minúscula (a–z)
- Al menos 1 número (0–9)
- Al menos 2 símbolos de este conjunto: `*` `/` `_` `-` `+` `.`

La validación se aplica en el cliente y **se repite en el servidor**, de modo que una petición manipulada no puede saltársela.

### Roles

| Valor | Rol | Interfaz |
|---|---|---|
| `0` | Mesero | Toma de órdenes y consulta de las propias |
| `1` | Cocina | Tablero de pedidos y cambio de estado |
| `2` | Administrador | Gestión de usuarios, productos, inventario y logs |

---

## Recuperación de contraseña

El flujo vive en `recuperar_password.c` y se apoya por completo en el servidor:

1. **Identificación** — el usuario escribe su nombre de cuenta.
2. **Verificación** — el cliente envía `OP_VERIFICAR_USUARIO`. Se rechaza si la cuenta no existe o si es de tipo Administrador.
3. **Nueva contraseña** — se muestra la misma interfaz que usa el alta de usuarios, con el panel de requisitos, y se pide la contraseña oculta.
4. **Aplicación** — el cliente envía `OP_RECUPERAR_PASS`. El servidor vuelve a comprobar que la cuenta existe, que no es administrador y que la contraseña cumple los requisitos antes de guardar el nuevo hash.

Ante cualquier error se ofrece reintentar sin salir de la pantalla.

Las cuentas de administrador quedan fuera del flujo a propósito: al no haber verificación de identidad por correo ni por token, permitirlo dejaría el rol con más privilegios al alcance de cualquiera que conociera el nombre de usuario. La contraseña de un administrador se cambia desde la interfaz de administración de otra cuenta admin.

API del módulo:

```c
int  verificar_usuario_recuperable(const char* user, char* motivo, int motivo_len);
int  aplicar_nueva_password(const char* user, const char* new_pass, char* motivo, int len);
void recuperar_password(void);
```

---

## Sistema de logging

Dos colas circulares en memoria, cada una con su hilo consumidor y su mutex. Los productores (los hilos que atienden clientes) solo encolan, así que el logging nunca bloquea la atención.

### `autenticacion.log`

```
[30-08-2026 12:04:11] [CLIENTE_CONECTADO] Cliente ID: 1 | Slot: 0, PID: 4821
[30-08-2026 12:04:23] [LOGIN_EXITOSO] Usuario: mesero | Cliente ID: 1 | Tipo: Mesero
[30-08-2026 12:05:02] [LOGIN_FALLIDO] Usuario: mesero | Cliente ID: 1 | Credenciales incorrectas
[30-08-2026 12:06:40] [PASSWORD_RECUPERADA] Usuario: cocina | Cliente ID: 2 | Contraseña actualizada exitosamente
```

Eventos: `CLIENTE_CONECTADO`, `CLIENTE_DESCONECTADO`, `LOGIN_EXITOSO`, `LOGIN_FALLIDO`, `USUARIO_CREADO`, `USUARIO_YA_EXISTE`, `PASSWORD_RECUPERADA`, `PASSWORD_RECUPERACION_FALLIDA`.

### `cocina.log`

```
[30-08-2026 12:10:00] [PEDIDO_CREADO] Pedido #1 | Mesa: 5 | Mesero: MeseroTEST | Items: 3 | Creado por mesero
[30-08-2026 12:12:15] [PEDIDO_EN_PROGRESO] Pedido #1 | Mesa: 5 | Mesero: MeseroTEST | Items: 3
[30-08-2026 12:18:30] [PEDIDO_COMPLETADO] Pedido #1 | Mesa: 5 | Mesero: MeseroTEST | Items: 3
```

Eventos: `PEDIDO_CREADO`, `PEDIDO_EN_PROGRESO`, `PEDIDO_COMPLETADO`, `PEDIDO_ERROR`.

Ambos archivos se consultan desde *Ver logs* en la interfaz de administrador.

---

## Inventario y recetas

Cada producto tiene una **receta**: la lista de ingredientes que consume y en qué cantidad. La relación vive en el archivo `producto_ingrediente` y se edita desde *Administrador de productos → Editar ingredientes*.

- Al listar el catálogo, el mesero ve marcados los productos sin stock suficiente.
- Al crear un pedido, `pedidos_crear()` descuenta del inventario los ingredientes de cada item según su receta.
- El administrador ajusta el stock manualmente desde *Administrador de ingredientes*.

---

## Formato de los archivos de datos

Todos son de texto plano, un registro por línea, con `|` como separador.

| Archivo | Formato |
|---|---|
| `usuarios` | `nombre\|usuario\|hash_sha256\|correo\|telefono\|tipo` |
| `productos` | `id\|nombre\|descripcion\|precio` |
| `inventario` | `id\|nombre\|cantidad` |
| `producto_ingrediente` | `id_producto\|id_ingrediente\|cantidad_necesaria` |
| `pedidos` | `id\|mesa\|mesero\|num_items\|(id\|nombre\|cantidad)×N\|estado\|fecha` |

Ejemplo de `usuarios`:

```
Administrador|admin|240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9|admin@test.com|12345678|2
```

Ejemplo de `pedidos` (dos items, estado 0 = pendiente):

```
1|Mesa 3|MeseroTEST|2|1|Producto 1|2|2|Producto 2|1|0|30-08-2026 12:00
```

---

## Estructura del proyecto

```
Sistema-de-Comandas/
│
├── Servidor.c                    # Servidor: IPC, hilos y router de operaciones
├── Cliente.c                     # Cliente: conexión, menú y autenticación
├── protocolo.h                   # Contrato cliente-servidor
│
├── usuario.c / usuario.h         # Usuarios, hash SHA-256 y validaciones
├── productos.c / productos.h     # Catálogo de productos y recetas
├── pedidos.c / pedidos.h         # Pedidos y descuento de inventario
├── inventario.c / inventario.h   # Ingredientes y stock
├── interfaces.c / interfaces.h   # Interfaces de mesero, cocina y admin
├── ui.c / ui.h                   # Primitivas de UI sobre ncurses
├── logger.c / logger.h           # Logging asíncrono
├── recuperar_password.c / .h     # Flujo de recuperación de contraseña
│
├── makefile
├── LICENSE
├── README.md
│
├── usuarios                      # Datos (generados)
├── productos
├── inventario
├── producto_ingrediente
├── pedidos
├── autenticacion.log
├── cocina.log
│
├── servidor_usuarios_mem         # Archivos de llave IPC (vacíos)
├── servidor_usuarios_sem
└── servidor_estado_sem
```

El repositorio conserva además `usuarios.c` y `sesion.c` / `sesion.h`, que **no forman parte de la compilación**: son restos de una versión anterior del módulo de usuarios y de un manejo de sesión que hoy resuelven `usuario.c` y `Cliente.c`.

---

## Módulos

### `Servidor.c`

Crea la memoria compartida y los semáforos, acepta clientes, lanza un hilo por cada uno y enruta las operaciones del protocolo. Maneja `SIGINT` para liberar los recursos IPC al cerrar.

Funciones clave: `Crea_semaforo()`, `down()` / `up()`, `procesarPeticion()`, `AtenderCliente()`, `limpiar_recursos()`.

### `Cliente.c`

Espera a que el servidor esté listo, toma un slot, muestra el menú principal y lanza la interfaz que corresponda al rol autenticado.

Funciones clave: `esperar_servidor()`, `conectar_servidor()`, `enviar_peticion()`, `desconectar_servidor()`, `iniciar_sesion()`, `crear_usuario()`.

### `usuario.c`

Usuarios en memoria, persistencia, hash SHA-256 y validaciones.

Funciones clave: `usuario_init()`, `usuario_cargar()`, `usuario_guardar()`, `encriptar_password()`, `usuario_crear()`, `usuario_autenticar()`, `usuario_recuperar_pass()`, `usuario_existe()`, `usuario_eliminar_por_nombre()`, `validar_password()`, `validar_email()`, `validar_telefono()`.

### `productos.c`

Catálogo y recetas.

Funciones clave: `productos_cargar()` / `productos_guardar()`, `productos_get_by_id()`, `productos_agregar()`, `producto_agregar_ingrediente()`, `producto_verificar_disponibilidad()`, `producto_eliminar()`.

### `pedidos.c`

Alta, consulta y cambio de estado de pedidos; descuenta el inventario al crear la orden.

Funciones clave: `pedidos_cargar()` / `pedidos_guardar()`, `pedidos_crear()`, `pedidos_cambiar_estado()`, `pedidos_get_by_id()`.

### `inventario.c`

Ingredientes, stock y relación con productos.

Funciones clave: `inventario_cargar()` / `inventario_guardar()`, `inventario_agregar()`, `inventario_modificar_cantidad()`, `inventario_eliminar_ingrediente()`, `inventario_actualizar_stock()`, `producto_vincular_ingrediente()`.

### `interfaces.c`

Las tres interfaces completas y sus componentes de dibujo.

Funciones clave: `interfaz_mesero_ejecutar()`, `interfaz_cocina_ejecutar()`, `interfaz_admin_ejecutar()`, `dibujar_recuadro_pedido_horizontal()`, `dibujar_botones_si_no()`, `mostrar_menu_usuarios()`, `mostrar_menu_productos()`, `mostrar_menu_ingredientes()`.

### `ui.c`

Primitivas de interfaz sobre ncurses: marco, títulos, menús, campos de entrada y mensajes modales.

Funciones clave: `ui_init()` / `ui_cleanup()`, `ui_clear_screen()`, `ui_print_title()`, `ui_print_footer()`, `ui_draw_separator()`, `ui_clear_area()`, `ui_read_input()`, `ui_read_input_with_placeholder()`, `ui_show_menu()`, `ui_show_success()` / `ui_show_error()`.

Los campos de entrada calculan su ancho útil leyendo la pantalla, de modo que al escribir no borran el marco ni los paneles laterales; `ui_draw_separator()` y `ui_clear_area()` respetan los bordes por la misma razón, y `ui_print_footer()` recorta el texto al ancho de la terminal.

**Pares de color:**

| Par | Colores | Uso |
|---|---|---|
| 1 | Cian / negro | Títulos |
| 2 | Negro / magenta | Éxito |
| 3 | Blanco / rojo | Error |
| 4 | Magenta / negro | Fondo por defecto |
| 5 | Negro / amarillo | Resaltado |
| 6 | Blanco / magenta | Paneles de requisitos |
| 7 | Negro / cian | Pie de página |
| 8 | Amarillo / magenta | Bordes y placeholders |
| 9 | Verde / blanco | Confirmaciones |
| 10 | Blanco / negro | Texto general |

### `logger.c`

Colas circulares y hilos consumidores para los dos archivos de log.

Funciones clave: `logger_init()` / `logger_cleanup()`, `log_evento_auth()`, `log_evento_cocina()`, `hilo_logger_auth()`, `hilo_logger_cocina()`.

---

## Límites del sistema

| Constante | Valor | Definida en |
|---|---|---|
| `MAX_CLIENTES` | 10 | `Servidor.c`, `Cliente.c` |
| `MAX_USUARIOS` | 100 | `usuario.h` |
| `MAX_PRODUCTOS` | 100 | `productos.h` |
| `MAX_PEDIDOS` | 200 | `pedidos.h` |
| `MAX_ITEMS_PEDIDO` | 20 | `protocolo.h` |
| `MAX_INGREDIENTES` | 100 | `inventario.h` |
| `MAX_INGREDIENTES_PRODUCTO` | 20 | `productos.h` |
| `MAX_PRODUCTOS_RESPUESTA` | 50 | `protocolo.h` |
| `MAX_LOG_QUEUE` | 100 | `logger.h` |
| `MAX_CHAIN_SIZE` | 100 | `usuario.h`, `protocolo.h` |

---

## Resolución de problemas

### `fatal error: ncurses.h: No such file or directory`

Falta el paquete de desarrollo de ncurses. Ver [Requisitos](#requisitos).

### `fatal error: openssl/evp.h: No such file or directory`

Falta el paquete de desarrollo de OpenSSL. Ver [Requisitos](#requisitos).

### "Error: No se pudo acceder a la memoria compartida"

El servidor no está corriendo, o los archivos de llave no existen. Verificar que el servidor esté activo y, si hace falta, recrear las llaves con `make`.

### El cliente se queda en "Buscando servidor"

Espera hasta 30 segundos y aborta. Si el servidor sí está corriendo, lo más probable es que haya recursos IPC huérfanos de una ejecución anterior:

```bash
ipcs -m | grep $USER | awk '{print $2}' | xargs -r -n1 ipcrm -m
ipcs -s | grep $USER | awk '{print $2}' | xargs -r -n1 ipcrm -s
```

### El servidor no cierra limpiamente

Cerrar con `Ctrl+C`; si no responde, `kill -SIGTERM <PID>` y después limpiar los recursos IPC con los comandos de arriba.

### La terminal queda corrupta al salir

Ocurre si el cliente termina sin pasar por `ui_cleanup()`:

```bash
reset    # o bien: stty sane
```

### Un usuario no puede iniciar sesión tras actualizar el sistema

Las cuentas creadas antes del cambio a SHA-256 conservan un hash MD5 de 32 caracteres que ya no valida nunca. Esas cuentas deben pasar por *Recuperar Contraseña* o regenerarse con `make datauser`.

---

## Limitaciones conocidas

- **Los hashes no llevan sal**, así que dos usuarios con la misma contraseña comparten hash. Para un despliegue real corresponde una función de derivación con sal y coste configurable, como bcrypt o Argon2.
- **La recuperación de contraseña no verifica identidad**: basta conocer el nombre de usuario. Es aceptable para un sistema en red local cerrada; en producción exigiría confirmación por un canal aparte.
- **Los datos viven en archivos de texto** sin transacciones: dos escrituras simultáneas del mismo archivo pueden pisarse.
- **El sondeo con `usleep`** consume algo de CPU en reposo; unas variables de condición serían más eficientes.
- **La interfaz asume una terminal de al menos 80×24**; por debajo de eso el contenido se recorta.

---

## Licencia

Distribuido bajo la [Licencia MIT](LICENSE). Puedes usar, copiar, modificar, fusionar, publicar, distribuir, sublicenciar y vender copias del software, conservando el aviso de copyright y la licencia.
