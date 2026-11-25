# Sistema de Comandas

Sistema para tomar, preparar y cobrar las comandas de un salón de eventos. Es la migración del programa original en C y ncurses a una aplicación con interfaz en Qt Quick, un servidor en C y una base de datos en PostgreSQL.

**Todo se compila y se ejecuta con Podman.** No hace falta instalar Qt, CMake ni el kit de Android en el equipo.

> **Estado actual:** la aplicación funciona de punta a punta: el cliente habla con el servidor en C por WebSocket y el servidor con PostgreSQL.
> - **Demo pública: lista.** Corre en el navegador, sin instalar nada: <https://actronix09.github.io/Sistema-de-Comandas/> (ver [Demo pública](#demo-pública)).
> - **Sistema del salón, en la red local: en desarrollo.** La base, el servidor y el cliente de escritorio en Linux ya funcionan, pero los meseros todavía no se pueden conectar desde el celular (ver [El sistema en el salón](#el-sistema-en-el-salón)).
>
> El esquema, las migraciones, los datos y la documentación de la base están en el repositorio de la base de datos: [BD-Sistema-de-Comandas](https://github.com/Actronix09/BD-Sistema-de-Comandas).

## Contenido

- [Qué hace el sistema](#qué-hace-el-sistema)
- [Arquitectura](#arquitectura)
  - [Recorrido de una petición](#recorrido-de-una-petición)
  - [Avisos en tiempo real](#avisos-en-tiempo-real)
  - [Seguridad](#seguridad)
- [Estado del proyecto](#estado-del-proyecto)
  - [El sistema en el salón](#el-sistema-en-el-salón)
- [Estructura del repositorio](#estructura-del-repositorio)
  - [Cliente](#cliente)
  - [Servidor en C](#servidor-en-c)
  - [Código común](#código-común)
  - [Base de datos](#base-de-datos)
  - [Contenedores y publicación](#contenedores-y-publicación)
- [Requisitos](#requisitos)
- [Levantar la base y el servidor](#levantar-la-base-y-el-servidor)
- [Probar el cliente en la computadora](#probar-el-cliente-en-la-computadora)
- [Android](#android)
- [Demo pública](#demo-pública)
- [Protocolo](#protocolo)
- [Configuración del servidor](#configuración-del-servidor)
- [Pruebas](#pruebas)
- [Problemas conocidos](#problemas-conocidos)
- [Sistema original en C](#sistema-original-en-c)
- [Licencia](#licencia)

## Qué hace el sistema

Una noche en el salón sigue este camino:

1. **El administrador prepara el evento.** Lo da de alta en el calendario, con sus organizadores, sus mesas (cada una con su mesero), los paquetes contratados con la cantidad acordada de cada producto y los anticipos. El día del evento lo pasa a *en curso*: todo el salón trabaja con ese evento.
2. **El mesero toma la orden.** Elige la mesa y después la cuenta (una abierta o una nueva con el nombre de quien paga) y al final los productos. Al confirmar, el servidor crea **una comanda por cada unidad**: «3× Cerveza» son tres comandas.
3. **Cocina y bar la preparan.** Cada estación ve solo lo suyo: la base decide si un producto va a cocina o a bar por su categoría. Cada comanda avanza `pendiente → en_preparacion → lista → entregada`. Al pasar a `en_preparacion`, el inventario descuenta los ingredientes de su receta.
4. **El mesero recibe un aviso** cuando su orden está lista, y la entrega.
5. **La caja cobra.** Ve el saldo de cada cuenta, registra pagos (efectivo o tarjeta, con propina para el mesero) y cierra la cuenta cuando ya no debe nada y no tiene comandas sin terminar. Puede cancelar comandas sueltas.
6. **Los paquetes se cobran solos.** Lo que incluye el paquete del evento sale en 0 hasta la cantidad acordada. Al acabarse, según lo que eligió el organizador, el producto se cobra normal, ya no se vende o se carga a la cuenta del anfitrión.

Cada rol ve solo sus pantallas:

| Rol | Pantallas |
| :-- | :-- |
| Mesero | Elegir mesa → cuenta → productos, confirmar la orden y seguir sus órdenes, con las listas primero. Ve qué viene incluido en el paquete del evento |
| Cocina y bar | Rondas de su estación y cambio de estado de cada comanda |
| Cajero | Cuentas, detalle, pagos con propina, saldo, cierre de cuenta y cancelar comandas sueltas |
| Administrador | Lo del cajero, cancelar cuentas completas, eventos (calendario, repetición, mesas, paquetes con su modo de cobro, anticipos, iniciar/finalizar/cancelar, y consultar las cuentas de cualquier evento, también los pasados), productos con receta y variantes, paquetes, ingredientes, usuarios y logs |

No hay «Crear usuario» ni «Recuperar contraseña» en el inicio de sesión: solo el administrador crea usuarios y cambia contraseñas.

## Arquitectura

```
┌───────────────────────────┐
│ Cliente en Qt Quick       │  Linux, Android y navegador (WebAssembly).
│ Mesero · Cocina/Bar · Caja│  Windows, pendiente.
│ Administración            │
└─────────────┬─────────────┘
              │  WebSocket con JSON · puerto 9000
              │  (wss:// por Tailscale Funnel en la demo pública)
┌─────────────┴─────────────┐
│ Servidor en C             │  civetweb, cJSON, libpq y libsodium.
│                           │  Lo único que toca la base
└─────────────┬─────────────┘
              │  SQL · puerto 5432 (5433 en el equipo)
┌─────────────┴─────────────┐
│ PostgreSQL 16             │  Reglas del negocio en restricciones y disparadores
└───────────────────────────┘
```

Cada capa tiene un trabajo:

- **El cliente** solo muestra y captura. No conoce la base: todo lo pide al servidor por medio de `Api.qml`. Agrupa los datos para mostrarlos (rondas, líneas «2× Cerveza») y revisa lo mínimo para no mandar formularios vacíos.
- **El servidor** es lo único que se conecta a la base. Revisa la forma de los datos y quién llama, corre cada operación en una transacción, traduce los errores de la base a mensajes en español y avisa a los demás clientes cuando algo cambia.
- **La base** hace cumplir las reglas del negocio con restricciones `CHECK`, claves foráneas y disparadores: la máquina de estados de la comanda, el inventario, el saldo de las cuentas, la estación por categoría y el cobro de los paquetes. Así, ninguna regla depende de que el cliente o el servidor se acuerden de revisarla.

### Recorrido de una petición

Por ejemplo, cuando el mesero confirma una orden:

1. **Pantalla** (`VistaTomarOrden.qml`): arma la orden y llama a `Api.crearPedido(...)` con un callback.
2. **`Api.qml`**: le pone un `id` y manda `{"id": 7, "op": "OP_CREAR_PEDIDO", "datos": {...}}` por el WebSocket. Si todavía no hay conexión, la deja en cola. Si en 15 s no hay respuesta, llama al callback con un error.
3. **`conexion.c`**: civetweb recibe la trama en el hilo de esa conexión. Al abrirse, la conexión pasó por la revisión del `Origin`.
4. **`despacho.c`**: lee el JSON, busca la operación por su nombre (el mismo identificador de `comun/protocolo.h`) y revisa que haya sesión. Sin sesión solo se permiten `OP_LOGIN` y `OP_EVENTO_EN_CURSO`.
5. **`operaciones.c` / `op_*.c`**: la operación revisa el rol y los datos (`datos.c`), toma una conexión de la base (`bd.c`) y corre su SQL en una transacción.
6. **PostgreSQL**: los disparadores fijan el precio cobrado (0 si entra en el paquete), revisan la cuenta y la estación. Si algo no se cumple, la transacción se deshace completa.
7. **`respuesta.c`**: arma `{"ok": true, "datos": ...}` o `{"ok": false, "error": {...}}`, y `despacho.c` le agrega el `id`.
8. **`avisos.c`**: manda `{"aviso": "comandas"}` a todos los que tienen sesión. Cocina y bar vuelven a pedir su lista y la orden aparece sola.
9. **`registro.c`**: deja una línea en la bitácora (la que ve el administrador en Logs) y en el log del contenedor.

### Avisos en tiempo real

Los avisos son mensajes que el servidor manda sin que se los pidan, cuando otra persona cambia algo. No llevan `id`, y así el cliente los distingue de las respuestas. Cada uno es una señal de `Api.qml`, y las pantallas que muestran esa lista la vuelven a pedir:

| Aviso | Cuándo | A quién |
| :-- | :-- | :-- |
| `comandas` | Se crea, avanza o cancela una comanda | Todos los que tienen sesión |
| `cuentas` | Se abre, paga, cierra o cancela una cuenta | Todos |
| `inventario` | Cambian los ingredientes o sus existencias | Todos |
| `catalogo` | Cambian los productos o los paquetes | Todos |
| `eventos` | Cambia un evento, sus mesas, paquetes o anticipos | Todos |
| `usuarios` | Se crea, modifica o elimina un usuario | Todos |
| `orden_lista` | Una orden quedó lista | Solo al mesero que la levantó |

### Seguridad

- **Contraseñas:** el servidor las guarda con Argon2id (libsodium) y las revisa contra ese hash al iniciar sesión. En la base no hay ninguna contraseña en claro.
- **Sesión por conexión:** quien inició sesión queda guardado en su conexión WebSocket. Si la conexión se cae, el servidor olvida la sesión y la app pide volver a entrar.
- **Mínimo privilegio en la base:** el servidor entra como `app_comandas`, con el rol `comandas_app` de la migración `099-permisos.sql`. Puede leer todo, pero eventos, mesas, cuentas y comandas no los puede borrar (cambian de estado o se cancelan), y los pagos y anticipos no los puede editar ni borrar.
- **Origen de la página:** el servidor solo acepta páginas web de la lista `COMANDAS_ORIGENES`. Así otra página no puede usar el navegador de alguien para hablar con el servidor. Las apps de escritorio y Android no mandan `Origin` y pasan, pero igual tienen que iniciar sesión.
- **Errores:** lo que rechazan las reglas de la base llega al usuario con su mensaje, porque está escrito para él. Cualquier otro error se registra completo en el log y al cliente le llega un mensaje genérico.
- **Credenciales:** ninguna está en el código. Todas salen de `.env`, que no se sube a git.

## Estado del proyecto

| Parte | Ya funciona | Falta |
| :-- | :-- | :-- |
| **Base de datos** | 17 tablas, 6 vistas y 13 disparadores: máquina de estados de la comanda, stock, reglas del cobro, estación por categoría y cobro de los paquetes (migración 003) | Pagos a cuentas cerradas y decisiones de rondas |
| **Cliente** | Login, mesero, cocina/bar, caja y administración (eventos con calendario, productos con receta, paquetes, ingredientes, usuarios y logs), conectado al servidor y con diseño adaptable a teléfono | Elegir el servidor desde la app (hoy con `--servidor`) |
| **Servidor** | 39 operaciones con transacciones, sesión por conexión, login con Argon2id, avisos en tiempo real, bitácora de actividad y modo demo | Conexión cifrada en la red local |
| **Contenedores** | Base, servidor, cliente, cliente web y Android | Arranque automático en la PC de caja |
| **Plataformas** | Linux y navegador | Instalador de Windows; WebSockets en el APK de Android |

### El sistema en el salón

El sistema se usa de dos formas, con el mismo código:

| | Demo pública | Sistema del salón |
| :-- | :-- | :-- |
| Para qué | Que cualquiera lo pruebe en la presentación | El trabajo real de cada evento |
| Dónde corre el servidor | En la PC de casa, expuesta con Tailscale Funnel | En la PC de caja del salón, sin salida a internet |
| Cómo se conectan los clientes | Navegador, por `wss://` | Celulares y PC en la WiFi del salón, por `ws://IP-de-la-caja:9000` |
| Datos | Los de la Práctica 4, se reinician cada noche | Los del salón, sin datos de prueba |
| Estado | Lista | En desarrollo |

**Lo que ya funciona en la red local:** la base y el servidor se levantan en una PC con `podman compose up -d` y escuchan en el puerto 9000. El cliente de escritorio en Linux se conecta con `cliente_comandas --servidor ws://IP-de-la-caja:9000`. Las pantallas y las operaciones son las mismas que en la demo.

**Lo que falta para usarlo en el salón**, en orden de importancia:

| Falta | Por qué importa |
| :-- | :-- |
| **APK de Android con WebSockets** | Los meseros trabajan desde el celular. El APK publicado es anterior a la conexión con el servidor y usa datos simulados; uno nuevo no compila porque `android/Containerfile` no instala `qtwebsockets` |
| **Elegir el servidor desde la app** | En Android no se puede pasar `--servidor`, y sin él la app busca `ws://localhost:9000`, que en el celular no existe. Hace falta una pantalla para escribir la IP de la caja y guardarla (`main.cpp` ya prepara `QSettings` para eso) |
| **Base de producción** | `compose.yaml` siempre carga los datos de ejemplo y les pone la contraseña `Demo_123.`. El salón necesita una carga limpia con un solo administrador inicial |
| **Arranque automático en la PC de caja** | Hoy hay que correr `podman compose up` cada vez que se prende la PC (por ejemplo, con Quadlet de systemd) |
| **Conexión cifrada** | En la WiFi del salón las contraseñas viajan sin cifrar (`ws://`) |
| **Instaladores** | Linux, Windows y una guía para preparar la PC de caja (Podman, firewall e IP fija). El cliente de Windows todavía no existe |

Mientras no esté el APK, una opción es servir la versión web desde la PC de caja en la red local y abrirla en el navegador del celular con `?servidor=ws://IP-de-la-caja:9000`. No está montada ni probada.

Lo que falta para la entrega final está en [`pendientes.md`](https://github.com/Actronix09/BD-Sistema-de-Comandas/blob/main/pendientes.md), en el repositorio de la base de datos. Los problemas que siguen abiertos en la aplicación están en [Problemas conocidos](#problemas-conocidos).

## Estructura del repositorio

```
├── cliente/                 Aplicación en Qt Quick (escritorio, navegador y Android)
│   ├── main.cpp                Arranque: carga Main.qml del módulo Comandas
│   ├── Main.qml                Ventana: elige la pantalla según el rol, avisos y letrero de la demo
│   ├── Comandas/               Pantallas, diálogos, componentes y la Api (ver Cliente)
│   ├── validaciones_qml.*      Deja usar desde QML las validaciones en C de comun/
│   ├── pagina.html             Página de carga de la versión web
│   ├── qtquickcontrols2.conf   Estilo Material claro
│   ├── CMakeLists.txt          Compilación con Qt 6.8
│   ├── Containerfile           Cliente de escritorio (Debian, Wayland)
│   └── Containerfile.web       Cliente web (Qt for WebAssembly y Emscripten)
├── servidor-c/              Servidor en C (ver Servidor en C)
│   └── pruebas/                Pruebas del servidor por WebSocket, en Python
├── comun/                   Código que comparten el cliente, el servidor y el sistema original
├── db/
│   ├── init/                   Scripts que se corren al crear la base de desarrollo
│   └── demo/                   Capa de datos de la demo pública
├── android/                 Contenedor que compila el APK y lo prueba en un emulador
│   └── salida/                 Aquí queda el APK (no se sube a git)
├── servidor/                Primer servidor, en C++ y Qt; ya no se usa
├── archivo/                 Lo que ya no se usa; se conserva como referencia
│   ├── sistema-c/              Sistema original en C y ncurses
│   └── prototipo-qml/          Prototipo de las pantallas
├── .github/workflows/
│   └── pages.yml               Publica la versión web en GitHub Pages
├── compose.yaml             Definición de todos los contenedores
└── .env.example             Plantilla de la configuración
```

### Cliente

Está en `cliente/Comandas/` y se compila dentro del ejecutable como el módulo QML `Comandas`. Tres archivos son *singleton*, es decir, existen una sola vez y todas las pantallas los comparten: `Api.qml`, `Tema.qml` y `Avisos.qml`.

| Grupo | Archivos | Para qué |
| :-- | :-- | :-- |
| Núcleo | `Api.qml` | La única puerta entre las pantallas y los datos: el WebSocket, las peticiones con su `id`, la cola mientras conecta, el tiempo de espera, los avisos y la agrupación de comandas en rondas y líneas. Las pantallas no saben que hay red |
| | `Tema.qml` | Colores, medidas y formatos (dinero, fechas) de todas las pantallas |
| | `Avisos.qml`, `Aviso.qml` | Mensajes flotantes de éxito, error o información que no bloquean la pantalla |
| Inicio | `PantallaLogin.qml` | Inicio de sesión. En la demo muestra los botones de acceso rápido |
| Mesero | `PantallaMesero.qml` | Pestañas «Tomar orden» y «Mis órdenes», con el contador de órdenes listas |
| | `VistaTomarOrden.qml`, `ElegirCuenta.qml` | Mesa → cuenta → productos, y la orden en curso |
| | `VistaMisOrdenes.qml` | Lo que levantó el mesero, por ronda, con el estado de cada comanda |
| Cocina y bar | `PantallaCocina.qml` | Tablero de tres columnas (pendiente, en preparación, lista). En teléfono, una columna a la vez |
| Caja | `PantallaCaja.qml` | Cuentas con su saldo, detalle, pagos con propina, cierre y cancelación de comandas |
| Administración | `PantallaAdmin.qml` | Menú del administrador. El nivel cajero entra directo a la caja |
| | `AdminEventos.qml`, `EditorEvento.qml` | Calendario de eventos y la ficha de cada uno: organizadores, repetición, mesas, paquetes, anticipos y estado |
| | `AdminProductos.qml`, `AdminPaquetes.qml` | Menú con precio, variantes y receta, y paquetes con lo que incluyen |
| | `AdminIngredientes.qml`, `AdminUsuarios.qml` | Inventario y usuarios, con el mismo patrón de lista, alta, edición y baja |
| | `AdminBitacora.qml` | Logs: las últimas 500 acciones del servidor |
| Diálogos | `DialogoConfirmarOrden.qml`, `DialogoCancelarComandas.qml`, `DialogoDetalleCuenta.qml`, `DialogoProducto.qml`, `DialogoPaquete.qml`, `DialogoIngrediente.qml`, `DialogoUsuario.qml`, `DialogoConfirmar.qml` | Formularios de alta y edición, confirmaciones y consulta de cuentas |
| Componentes | `Encabezado.qml`, `TarjetaProducto.qml`, `TarjetaRonda.qml`, `LineaOrden.qml`, `ChipEstado.qml`, `Etiqueta.qml`, `SelectorOrden.qml` | Piezas que se repiten: barra superior, tarjetas, estado de una comanda y formas de ordenar las listas (por tiempo, por mesa y cuenta, por mesero) |
| Herramientas | `Capturas.qml` | Recorrido que entra con cada rol y guarda una captura de cada pantalla |
| | `PantallaPendiente.qml` | Marcador para pantallas que todavía no existen |

**Opciones al abrir la app:**

| Opción | Para qué |
| :-- | :-- |
| `--servidor ws://IP:9000` | A qué servidor conectarse. Sin ella, el escritorio usa `ws://localhost:9000` y el navegador `wss://lilyth.tail2ed638.ts.net` |
| `--demo` | Muestra el letrero y los botones de la demo. En el navegador siempre está activo |
| `--capturas DIR` y `--tam ANCHOxALTO` | Corre el recorrido de capturas y las guarda en `DIR` |

En el navegador, el servidor se cambia con `?servidor=` en la dirección.

### Servidor en C

Está en `servidor-c/`. Cada conexión la atiende su propio hilo de civetweb, así que lo que comparten los hilos (la bitácora, la lista de conexiones y las conexiones a la base) va con candado.

| Archivo | Para qué |
| :-- | :-- |
| `main.c` | Arranque: lee la configuración, inicia libsodium, la base y civetweb, y espera la señal para cerrar |
| `conexion.c` | WebSocket: revisa el `Origin` al abrir, recibe cada mensaje y lo pasa a `despacho.c`, y cierra la sesión al caerse la conexión |
| `cliente.h` | Lo que sabe el servidor de cada conexión: quién inició sesión y con qué rol |
| `despacho.c` | Lee el JSON, busca la operación por su nombre, revisa que haya sesión y llama a la operación. Le agrega el `id` a la respuesta |
| `operaciones.c` | Login, logout, evento en curso, productos, mesas, cuentas y crear pedidos |
| `op_comandas.c` | Listar comandas, avanzar su estado y cancelarlas |
| `op_salon.c` | Cuentas de la caja, detalle, pagos, cierre y cancelación de cuentas |
| `op_eventos.c` | Calendario, alta con repetición, edición con organizadores, mesas y paquetes, cambios de estado y anticipos |
| `op_catalogo.c` | Productos con receta y variantes, y paquetes con sus productos |
| `op_inventario.c` | Ingredientes, ajustes de existencias y reinicio del stock |
| `op_usuarios.c` | Usuarios: la fila de `usuario` y la de su subtipo en la misma transacción |
| `datos.c` | Lectura y revisión de los datos de una petición (enteros, importes, fechas, listas de ids, formato del usuario), revisión del rol y del modo demo |
| `bd.c` | Conexiones a PostgreSQL compartidas por los hilos (4 por omisión). Si queda una transacción abierta la deshace, y si la conexión se rompe abre otra. Traduce los errores de la base |
| `respuesta.c` | Las dos formas de respuesta: `ok` con `datos`, o `error` con `codigo` y `mensaje` |
| `avisos.c` | Lista de conexiones abiertas y envío de avisos a todos o a un usuario |
| `registro.c` | Log con fecha y hora, y la bitácora de las últimas 500 líneas en memoria |
| `Makefile`, `Containerfile` | Compilación. El contenedor ya trae civetweb, cJSON, libpq y libsodium |

### Código común

`comun/` tiene lo que comparten el cliente, el servidor y el sistema original en C, sin copiarlo:

- `protocolo.h`: los identificadores de las operaciones (`OP_LOGIN`, `OP_CREAR_PEDIDO`…). El nombre que viaja en `"op"` es el mismo identificador.
- `validaciones.c` y `validaciones.h`: la validación de contraseñas. El cliente la usa desde QML por medio de `validaciones_qml`.
- `usuario.h`: la estructura del usuario del sistema original.

### Base de datos

El esquema no está en este repositorio: `compose.yaml` monta la carpeta `sql/` del repositorio de la base, según la ruta `COMANDAS_SQL` de `.env`, para no tener dos copias. Al crear el volumen se cargan, en orden:

| Orden | Archivo | Qué hace |
| :-- | :-- | :-- |
| 1 | `sql/esquema.sql` | Tablas, vistas, disparadores y restricciones |
| 2 | `db/init/02-migraciones.sh` | Aplica en orden las migraciones de `sql/migraciones/` (dominios, formato del usuario, cobro de los paquetes y permisos). Una migración nueva entra sola |
| 3 | `sql/datos-ejemplo.sql` | Un evento pequeño con 5 usuarios, productos, mesas y cuentas |
| 4 | `db/init/03-contrasenas-desarrollo.sql` | Les pone `Demo_123.` a los usuarios de ejemplo. Solo para desarrollo |
| 5 | `sql/carga/crear_usuarios.sql` | Usuarios de la base con permisos limitados (`app_comandas`, `consulta_comandas` y el administrador) |

La demo pública usa otra carga, descrita en [Demo pública](#demo-pública).

La migración `003-cobro-paquetes.sql` hace que lo que incluye el paquete de un evento cueste 0 hasta la cantidad acordada; al acabarse, según el producto, se cobra, ya no se vende o se carga a la cuenta del anfitrión. La explicación está en su diccionario de datos.

### Contenedores y publicación

`compose.yaml` define todo con perfiles: sin perfil solo levanta la base y el servidor.

| Servicio | Perfil | Qué hace |
| :-- | :-- | :-- |
| `db` | — | PostgreSQL 16.4. Antes de crear la base revisa que estén todos los archivos montados; si falta uno, se detiene con un mensaje y deja el volumen vacío |
| `servidor` | — | El servidor en C. Espera a que la base esté lista y entra como `app_comandas` |
| `cliente` | `cliente` | El cliente de escritorio, en una ventana de Wayland del equipo |
| `android-apk` | `android-apk` | Compila el APK para ARM64 |
| `android` | `android` | Compila el APK y lo prueba en un emulador |
| `android-ventana` | `android-ventana` | El mismo emulador con una ventana para usarlo con el mouse |

| Volumen | Qué guarda |
| :-- | :-- |
| `comandas-pgdata` | Los datos de la base |
| `comandas-gradle` | La caché de Gradle, para no bajar todo en cada compilación de Android |
| `comandas-android-clave` | La clave de firma del APK, para que uno nuevo actualice al anterior |

`.github/workflows/pages.yml` publica la versión web en GitHub Pages en cada push a `main` que toque `cliente/` o `comun/`. Compila con `cliente/Containerfile.web` en un trabajo y publica en otro.

## Requisitos

| Para qué | Qué se necesita |
| :-- | :-- |
| Todo | Podman con `podman compose`, que usa `docker-compose` por debajo, y `systemctl --user enable --now podman.socket` |
| Base y servidor | El repositorio de la base junto a este, según la ruta `COMANDAS_SQL` de `.env` |
| Cliente en la computadora | Una sesión gráfica de Wayland |
| Probar en el emulador de Android | `/dev/kvm` |
| Android con ventana | X11 o XWayland, y `/dev/dri` |

Antes de la primera ejecución, crea la configuración y cambia las contraseñas:

```bash
cp .env.example .env
```

| Variable de `.env` | Para qué |
| :-- | :-- |
| `COMANDAS_BD_USUARIO`, `COMANDAS_BD_CONTRASENA` | Dueño de la base, para administrarla con `psql` |
| `COMANDAS_SQL` | Ruta absoluta a la carpeta `sql/` del repositorio de la base |
| `COMANDAS_APP_CLAVE` | Contraseña de `app_comandas`, el usuario con el que entra el servidor |
| `COMANDAS_CONSULTA_CLAVE` | Contraseña de `consulta_comandas`, que solo puede leer |
| `COMANDAS_ADMIN_CLAVE` | Contraseña del usuario administrador de la base |
| `COMANDAS_ORIGENES` | Páginas web que pueden conectarse al servidor, separadas por comas |

## Levantar la base y el servidor

```bash
podman compose up -d --build        # levanta los dos, en segundo plano
podman compose ps                   # estado
podman compose logs -f servidor     # log del servidor
podman compose stop                 # los detiene y conserva los datos
```

La base queda en `localhost:5433` y el servidor en `localhost:9000`. Para abrir una consola de la base, con el usuario de tu `.env`:

```bash
podman compose exec db psql -U comandas -d comandas
```

El esquema y los datos solo se cargan cuando el volumen está vacío. Para cargar un esquema nuevo hay que borrar el volumen:

```bash
podman compose down -v              # BORRA todos los datos de la base
podman compose up -d --build
```

## Probar el cliente en la computadora

Con la base y el servidor levantados:

```bash
podman compose --profile cliente run --rm --build cliente cliente_comandas --servidor ws://servidor:9000
```

Abre la aplicación en una ventana. Para terminar, ciérrala o presiona `Ctrl+C` en la terminal. Dentro de compose, el servidor se llama `servidor`; sin `--servidor`, la app busca `ws://localhost:9000`, que dentro del contenedor no existe.

Los usuarios de los datos de ejemplo entran con la contraseña `Demo_123.`:

| Usuario | Pantalla |
| :-- | :-- |
| `lmendez`, `operez` | Mesero |
| `rgamez` | Cocina |
| `isoto` | Bar |
| `mrivas` | Administrador |

## Android

**Hoy el APK no compila:** el cliente necesita el módulo `qtwebsockets` y `android/Containerfile` todavía no lo instala. El APK publicado, [`cliente_comandas.apk`](https://github.com/Actronix09/Sistema-de-Comandas/releases/latest/download/cliente_comandas.apk) (versión `v0.1.0`, teléfonos ARM64), es anterior a la conexión con el servidor: usa datos simulados y lo que cambies se pierde al cerrarla.

Para instalarlo, abre el archivo en el teléfono; Android te pedirá permitir la instalación de esa fuente.

**Generar el APK:**

```bash
podman compose --profile android-apk up --build android-apk
```

Compila el APK para ARM64 y termina con el mensaje `APK listo`. Queda en `android/salida/cliente_comandas.apk`, firmado para desarrollo. La primera vez tarda bastante, porque descarga Qt y el kit de Android (la imagen pesa unos 11 GB).

La clave de firma se guarda en el volumen `comandas-android-clave`, así que un APK nuevo actualiza al anterior. Si borras ese volumen o compilas en otro equipo, hay que desinstalar la aplicación antes de instalar el APK nuevo.

**Para compilarlo y además probarlo en un emulador:**

```bash
podman compose --profile android up --build android
```

Lo instala en el emulador y comprueba que la aplicación arranque. Si todo salió bien, el último mensaje es `El cliente está corriendo en el emulador`. Deja también en `android/salida/` una captura (`captura.png`) y los logs (`logcat.txt` y `emulador.log`).

En una PC x86 el emulador no arranca imágenes ARM64. Por eso usa la imagen `google_apis` x86_64, que trae `libndk_translation` y corre el código ARM64 de la app.

**Para usarla con el mouse en el emulador:**

```bash
podman compose --profile android-ventana up --build android-ventana
```

Termina al cerrar la ventana.

**Publicar el APK en GitHub.** El enlace de descarga siempre apunta al APK de la versión más reciente. Para publicar una versión nueva:

1. Genera el APK con el comando de arriba, siempre en el mismo equipo para conservar la clave de firma.
2. En GitHub, abre **Releases → Draft a new release** y crea una etiqueta nueva, por ejemplo `v0.2.0`, sobre la rama que corresponda.
3. Sube `android/salida/cliente_comandas.apk` sin cambiarle el nombre, porque el enlace depende de él.
4. Deja activada **Set as the latest release** y desactivada **Set as a pre-release**, y publícala. El enlace ignora las versiones marcadas como *pre-release*.

## Demo pública

Para la presentación, cualquiera puede usar el sistema desde el navegador, sin instalar nada: <https://actronix09.github.io/Sistema-de-Comandas/>.

```
Navegador → GitHub Pages (la app en Qt compilada a WebAssembly)
          → wss://lilyth.tail2ed638.ts.net (Tailscale Funnel)
          → servidor en C en la PC de casa, con COMANDAS_DEMO=1
          → PostgreSQL con los datos de la demo
```

- **La página** la publica `.github/workflows/pages.yml`. Compila con `cliente/Containerfile.web` (Qt 6.8.3 y Emscripten 3.1.56). La página de carga es `cliente/pagina.html`.
- **La base de la demo** tiene los datos de la Práctica 4 y encima una capa propia:
  - `db/demo/03-poblar.sh` corre el mismo `sql/carga/poblar.sql` del repositorio de la base: unos 62 mil renglones, con 240 eventos, 200 usuarios, el menú del salón y 37 mil comandas. Así la demo muestra todo lo de las prácticas.
  - `db/demo/datos-demo.sql` agrega las cuentas de los botones del login: `meseroDemo1`, `meseroDemo2`, `cocinaDemo1`, `cocinaDemo2` (en el bar), `cajeroDemo` y `adminDemo`. Todas entran con la contraseña `Demo_123.`, igual que los 200 usuarios de la práctica.
  - El evento en curso de los datos pasa a la fecha de hoy y sus mesas 1 a 6 quedan con los meseros demo. Se le asignan como incluidos los dos paquetes que más se piden en él.
  - Se reinicia cada noche a las 3:30. Crearla tarda unos minutos.
- **El modo demo** se nota en tres cosas:
  - La app muestra un letrero arriba y los botones de acceso rápido.
  - El servidor no deja borrar usuarios ni cambiar contraseñas o nombres de acceso.
  - El servidor solo acepta páginas de `https://actronix09.github.io`.
- **La PC servidor** (Fedora CoreOS) está configurada en el repositorio privado `servidor-lilyth`.

Para probar la página contra un servidor local, compílala, sírvela y ábrela con `?servidor=`:

```bash
podman build -f cliente/Containerfile.web -o cliente/web .
python3 -m http.server 8000 --bind 127.0.0.1 -d cliente/web
# http://127.0.0.1:8000/?servidor=ws://127.0.0.1:9000
```

El servidor local también tiene que aceptar esa página: `COMANDAS_ORIGENES=https://actronix09.github.io,http://127.0.0.1:8000`.

**En el celular la app se usa en vertical.** Acostado deja unos 340 px de alto para las listas, así que muestra un aviso para girarlo. En tablet y computadora se usa en cualquier orientación.

Para revisar el diseño en un tamaño sin probar a mano, el cliente de escritorio tiene un recorrido de capturas (`cliente/Comandas/Capturas.qml`). Entra con cada rol de la demo, abre cada pantalla y guarda una imagen:

```bash
podman build -t comandas-cliente -f cliente/Containerfile .
podman run --rm --network <red-del-servidor> -e QT_QPA_PLATFORM=offscreen -v "$PWD/capturas:/salida:z" \
  comandas-cliente cliente_comandas --servidor ws://<servidor>:9000 --capturas /salida --tam 390x760 --demo
```

Sin pantalla, Qt no dibuja el fondo de los botones resaltados; en el navegador sí salen.

## Protocolo

Los mensajes viajan como JSON por un solo WebSocket. Cada petición lleva un `id`, que la respuesta repite. Los avisos no llevan `id` (ver [Avisos en tiempo real](#avisos-en-tiempo-real)).

```json
→ {"id": 1, "op": "OP_LOGIN", "datos": {"usuario": "cajeroDemo", "contrasena": "..."}}
← {"id": 1, "ok": true, "datos": {"id_usuario": 5, "rol": "cajero_admin", "nivel_acceso": "cajero", ...}}
← {"id": 1, "ok": false, "error": {"codigo": "CREDENCIALES_INVALIDAS", "mensaje": "Usuario o contraseña incorrectos"}}
← {"aviso": "comandas"}
← {"aviso": "orden_lista", "datos": {"id_mesero": 1, "numero_mesa": 2, "productos": ["2× Refresco"]}}
```

Sin sesión solo se puede hacer login y ver el evento en curso; lo demás responde `NO_AUTENTICADO`. Las operaciones de la caja trabajan con el evento en curso; `OP_LISTAR_CUENTAS_CAJA` y `OP_DETALLE_CUENTA` aceptan además `numero_evento` para consultar otro, solo el administrador. Las operaciones (el nombre es el de `comun/protocolo.h`):

| Área | Operaciones | Quién |
| :-- | :-- | :-- |
| Sesión | `OP_LOGIN`, `OP_LOGOUT`, `OP_EVENTO_EN_CURSO` | Cualquiera |
| Salón | `OP_LISTAR_PRODUCTOS`, `OP_LISTAR_MESAS`, `OP_LISTAR_CUENTAS`, `OP_CREAR_CUENTA`, `OP_CREAR_PEDIDO`, `OP_LISTAR_PEDIDOS`, `OP_CAMBIAR_ESTADO_PEDIDO` | Según el rol: el mesero pide y entrega, cocina y bar preparan |
| Caja | `OP_LISTAR_CUENTAS_CAJA`, `OP_DETALLE_CUENTA`, `OP_REGISTRAR_PAGO`, `OP_CERRAR_CUENTA`, `OP_ELIMINAR_PEDIDO` (cancela comandas) | Cajero y administrador |
| Administración | `OP_CANCELAR_CUENTA`, `OP_LISTAR_USUARIOS`, `OP_CREAR_USUARIO`, `OP_MODIFICAR_USUARIO`, `OP_ELIMINAR_USUARIO`, `OP_LISTAR_INGREDIENTES`, `OP_CREAR_INGREDIENTE`, `OP_MODIFICAR_INGREDIENTE`, `OP_AJUSTAR_STOCK`, `OP_ELIMINAR_INGREDIENTE`, `OP_REINICIAR_STOCK`, `OP_CREAR_PRODUCTO`, `OP_MODIFICAR_PRODUCTO`, `OP_ELIMINAR_PRODUCTO`, `OP_LISTAR_PAQUETES`, `OP_CREAR_PAQUETE`, `OP_MODIFICAR_PAQUETE`, `OP_ELIMINAR_PAQUETE`, `OP_LISTAR_EVENTOS`, `OP_CREAR_EVENTO`, `OP_MODIFICAR_EVENTO`, `OP_CAMBIAR_ESTADO_EVENTO`, `OP_REGISTRAR_ANTICIPO`, `OP_LISTAR_LOGS` | Administrador (la lista de ingredientes la ve cualquiera) |

Los demás identificadores de `protocolo.h` (`OP_RECUPERAR_PASS`, `OP_VERIFICAR_USUARIO`, `OP_OBTENER_USUARIO`, `OP_OBTENER_PRODUCTO`, `OP_OBTENER_INGREDIENTE`, `OP_OBTENER_PEDIDO`, `OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO` y `OP_VER_LOG`) están reservados y responden `NO_IMPLEMENTADA`.

## Configuración del servidor

Se configura con variables de entorno, que `compose.yaml` toma de `.env`:

| Variable | Valor por omisión | Para qué |
| :-- | :-- | :-- |
| `COMANDAS_BD_HOST` | `localhost` | Equipo de la base |
| `COMANDAS_BD_PUERTO` | `5432` | Puerto de la base |
| `COMANDAS_BD_NOMBRE` | `comandas` | Nombre de la base |
| `COMANDAS_BD_USUARIO` | `app_comandas` | Usuario de la base (rol `comandas_app`) |
| `COMANDAS_BD_CONTRASENA` | Obligatoria | Contraseña de la base |
| `COMANDAS_BD_CONEXIONES` | `4` | Conexiones a la base que comparten los hilos |
| `COMANDAS_PUERTO` | `9000` | Puerto para los clientes |
| `COMANDAS_ORIGENES` | Ninguna | Páginas web que pueden conectarse, separadas por comas |
| `COMANDAS_DEMO` | `0` | `1` en la demo pública |

Los logs son las últimas 500 líneas de actividad en memoria; el log completo queda en el del contenedor.

## Pruebas

Las pruebas del servidor son clientes WebSocket en Python (solo la biblioteca estándar, con `pruebas/cliente_ws.py`) y corren contra una base cargada:

```bash
python3 servidor-c/pruebas/probar.py 9000 --bd          # conexión, login y pedidos (datos de ejemplo)
python3 servidor-c/pruebas/probar_operaciones.py 9000   # una noche completa del salón (datos de ejemplo)
python3 servidor-c/pruebas/probar_admin.py 9100         # administración (datos de la demo, base desechable)
```

Las pruebas de la base (restricciones, disparadores y permisos) están en el repositorio de la base y se corren desde ahí (39 rechazos y 16 casos válidos):

```bash
make -C sql pruebas
```

Todavía usan el contenedor `practica1-postgres`, en Docker (`MOTOR=docker` en `sql/Makefile`). Falta probarlas con `make -C sql pruebas MOTOR=podman`.

## Problemas conocidos

- **En la red local las contraseñas viajan sin cifrar.** El servidor usa `ws://`; la demo pública sí va cifrada (`wss://`, lo pone Tailscale). Falta pasar el servidor local a `wss://` con un certificado que acepten los clientes.
- **El APK de Android no compila.** Su contenedor no instala el módulo `qtwebsockets`; el APK publicado es anterior a la conexión con el servidor.
- **La dirección del servidor no se elige desde la app.** En escritorio se pasa con `--servidor ws://IP:9000`; en el navegador, con `?servidor=`. En Android no hay forma de cambiarla.
- **No hay base de producción.** La base siempre se crea con los datos de ejemplo y la contraseña `Demo_123.`.
- **`servidor/` ya no se usa.** Es el primer servidor, en C++ y Qt; lo reemplazó el servidor en C. Falta pasarlo a `archivo/`.

## Sistema original en C

`archivo/sistema-c/` conserva el programa en C con ncurses, memoria compartida y archivos de texto, con su propio README. Ya no forma parte del sistema: el servidor en C es su sucesor y las pantallas QML conservan sus menús (varias dicen en su primer comentario a qué parte del original equivalen). También está en la rama `Sistema-en-ncurses`.

`archivo/prototipo-qml/` conserva el prototipo con el que se definieron las pantallas. El cliente real ya lo reemplazó.

## Licencia

Distribuido bajo la [Licencia MIT](LICENSE).
