# Sistema de Comandas

Sistema para tomar, preparar y cobrar las comandas de un salón de eventos. Es la migración del programa original en C y ncurses a una aplicación con interfaz en Qt Quick, un servidor en C y una base de datos en PostgreSQL.

**Todo se compila y se ejecuta con Podman.** No hace falta instalar Qt, CMake ni el kit de Android en el equipo.

> **Estado actual:** la aplicación funciona de punta a punta: el cliente habla con el servidor en C por WebSocket y el servidor con PostgreSQL. Hay una **demo pública** que corre en el navegador: <https://actronix09.github.io/Sistema-de-Comandas/> (ver [Demo pública](#demo-pública)).

## Contenido

- [Arquitectura](#arquitectura)
- [Estado del proyecto](#estado-del-proyecto)
- [Estructura del repositorio](#estructura-del-repositorio)
- [Requisitos](#requisitos)
- [Probar el cliente en la computadora](#probar-el-cliente-en-la-computadora)
- [Descargar el APK](#descargar-el-apk)
- [Generar el APK de Android](#generar-el-apk-de-android)
- [Publicar el APK en GitHub](#publicar-el-apk-en-github)
- [Levantar la base y el servidor](#levantar-la-base-y-el-servidor)
- [Demo pública](#demo-pública)
- [Servidor](#servidor)
- [Base de datos](#base-de-datos)
- [Problemas conocidos](#problemas-conocidos)
- [Sistema original en C](#sistema-original-en-c)
- [Licencia](#licencia)

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

## Estado del proyecto

| Parte | Ya funciona | Falta |
| :-- | :-- | :-- |
| **Base de datos** | 17 tablas, 6 vistas y 13 disparadores: máquina de estados de la comanda, stock, reglas del cobro, estación por categoría y cobro de los paquetes (migración 003) | Pagos a cuentas cerradas y decisiones de rondas |
| **Cliente** | Login, mesero, cocina/bar, caja y administración (eventos con calendario, productos con receta, paquetes, ingredientes, usuarios y logs), conectado al servidor y con diseño adaptable a teléfono | Elegir el servidor desde la app (hoy con `--servidor`) |
| **Servidor** | 39 operaciones con transacciones, sesión por conexión, login con Argon2id, avisos en tiempo real, bitácora de actividad y modo demo | Conexión cifrada en la red local |
| **Contenedores** | Base, servidor, cliente, cliente web y Android | Arranque automático en la PC de caja |
| **Plataformas** | Linux, navegador y APK de Android | Instalador de Windows; WebSockets en el APK |

La lista detallada de pendientes, con responsables, está en `PENDIENTES.md`, en el repositorio de la base de datos.

### Qué puede hacer cada rol en el cliente

| Rol | Pantallas |
| :-- | :-- |
| Mesero | Elegir mesa → cuenta → productos, confirmar la orden y seguir sus órdenes, con las listas primero. Ve qué viene incluido en el paquete del evento |
| Cocina y bar | Rondas de su estación y cambio de estado de cada comanda |
| Cajero | Cuentas, detalle, pagos con propina, saldo, cierre de cuenta y cancelar comandas sueltas |
| Administrador | Lo del cajero, cancelar cuentas completas, eventos (calendario, repetición, mesas, paquetes con su modo de cobro, anticipos, iniciar/finalizar/cancelar, y consultar las cuentas de cualquier evento, también los pasados), productos con receta y variantes, paquetes, ingredientes, usuarios y logs |

## Estructura del repositorio

```
├── cliente/      Aplicación en Qt Quick: pantallas en QML, arranque en C++
│   └── Comandas/    Pantallas, componentes y la Api simulada
├── servidor/     Servidor en C++ y Qt
├── comun/        Código que comparten el cliente, el servidor y el sistema en C
├── android/      Contenedor que compila el APK y lo prueba en un emulador
│   └── salida/      Aquí queda el APK (no se sube a git)
├── db/init/      Contraseñas de desarrollo de la base
├── archivo/      Lo que ya no se usa; se conserva como referencia
│   ├── sistema-c/       Sistema original en C y ncurses
│   └── prototipo-qml/   Prototipo de las pantallas
├── compose.yaml  Definición de todos los contenedores
└── .env.example  Plantilla de la configuración
```

El esquema y los datos de la base están en otro repositorio, `desarrollo-proyecto`, en su carpeta `sql/`.

## Requisitos

| Para qué | Qué se necesita |
| :-- | :-- |
| Todo | Podman con `podman compose`, que usa `docker-compose` por debajo, y `systemctl --user enable --now podman.socket` |
| Todo | El repositorio `desarrollo-proyecto` junto a este, según la ruta `COMANDAS_SQL` de `.env` |
| Cliente en la computadora | Una sesión gráfica de Wayland |
| Probar en el emulador de Android | `/dev/kvm` |
| Android con ventana | X11 o XWayland, y `/dev/dri` |

Antes de la primera ejecución, crea la configuración y cambia la contraseña:

```bash
cp .env.example .env
```

## Probar el cliente en la computadora

```bash
podman compose --profile cliente up --build cliente
```

Abre la aplicación en una ventana. Para terminar, ciérrala o presiona `Ctrl+C` en la terminal. No necesita la base ni el servidor.

Como usa datos simulados, lo que cambies se pierde al cerrarla. La contraseña de todos es `1234`:

| Usuario | Pantalla |
| :-- | :-- |
| `lmendez`, `operez` | Mesero |
| `rgamez` | Cocina |
| `isoto` | Bar |
| `mrivas` | Caja |
| `dcampos` | Administrador |

## Descargar el APK

**[Descargar `cliente_comandas.apk`](https://github.com/Actronix09/Sistema-de-Comandas/releases/latest/download/cliente_comandas.apk)** (teléfonos ARM64)

Abre el archivo en el teléfono; Android te pedirá permitir la instalación de esa fuente. Es una versión de prueba: usa datos simulados, y lo que cambies se pierde al cerrarla. Los usuarios y la contraseña están en [Probar el cliente en la computadora](#probar-el-cliente-en-la-computadora).

## Generar el APK de Android

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

**Para usarla con el mouse en el emulador:**

```bash
podman compose --profile android-ventana up --build android-ventana
```

Termina al cerrar la ventana.

## Publicar el APK en GitHub

El enlace de descarga siempre apunta al APK de la versión más reciente. Para publicar una versión nueva:

1. Genera el APK con el comando de arriba, siempre en el mismo equipo para conservar la clave de firma.
2. En GitHub, abre **Releases → Draft a new release** y crea una etiqueta nueva, por ejemplo `v0.1.0`, sobre la rama que corresponda.
3. Sube `android/salida/cliente_comandas.apk` sin cambiarle el nombre, porque el enlace depende de él.
4. Deja activada **Set as the latest release** y desactivada **Set as a pre-release**, y publícala. El enlace ignora las versiones marcadas como *pre-release*.

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

## Demo pública

Para la presentación, cualquiera puede usar el sistema desde el navegador, sin instalar nada: <https://actronix09.github.io/Sistema-de-Comandas/>.

```
Navegador → GitHub Pages (la app en Qt compilada a WebAssembly)
          → wss://lilyth.tail2ed638.ts.net (Tailscale Funnel)
          → servidor en C en la PC de casa, con COMANDAS_DEMO=1
          → PostgreSQL con los datos de la demo
```

- **La página** la publica `.github/workflows/pages.yml` en cada push a `main` que toque `cliente/` o `comun/`. Compila con `cliente/Containerfile.web` (Qt 6.8.3 y Emscripten 3.1.56).
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

## Servidor

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

Los mensajes viajan como JSON por un solo WebSocket. Cada petición lleva un `id`, que la respuesta repite. Los **avisos** no llevan `id`: el servidor los manda cuando otra persona cambia algo, y la app vuelve a pedir esa lista.

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

Las reglas del negocio las revisa la base (restricciones y disparadores). El servidor revisa la forma de los datos y quién llama, y traduce los errores de la base a mensajes en español. Los logs son las últimas 500 líneas de actividad en memoria; el log completo queda en el del contenedor.

Las pruebas del servidor corren contra una base cargada:

```bash
python3 servidor-c/pruebas/probar.py 9000 --bd          # conexión, login y pedidos (datos de ejemplo)
python3 servidor-c/pruebas/probar_operaciones.py 9000   # una noche completa del salón (datos de ejemplo)
python3 servidor-c/pruebas/probar_admin.py 9100         # administración (datos de la demo, base desechable)
```

## Base de datos

El repositorio `desarrollo-proyecto` tiene el esquema (`sql/esquema.sql`), los datos de ejemplo (`sql/datos-ejemplo.sql`), los diagramas y la documentación del modelo. `compose.yaml` monta esos archivos para no tener dos copias.

La migración `003-cobro-paquetes.sql` hace que lo que incluye el paquete de un evento cueste 0 hasta la cantidad acordada; al acabarse, según el producto, se cobra, ya no se vende o se carga a la cuenta del anfitrión. La explicación está en su diccionario de datos.

Sus pruebas se corren desde ese repositorio (39 rechazos y 16 casos válidos):

```bash
make -C sql pruebas
```

Todavía usan el contenedor `practica1-postgres`, en Docker. Pasarlas a Podman es el punto 5.5 de `PENDIENTES.md`.

## Problemas conocidos

- **En la red local las contraseñas viajan sin cifrar.** El servidor usa `ws://`; la demo pública sí va cifrada (`wss://`, lo pone Tailscale). Es el punto 6.11 de `PENDIENTES.md`.
- **El APK de Android todavía no trae WebSockets.** Su compilación necesita el módulo `qtwebsockets`.
- **La dirección del servidor no se elige desde la app.** En escritorio se pasa con `--servidor ws://IP:9000`; en el navegador, con `?servidor=`.

## Sistema original en C

`archivo/sistema-c/` conserva el programa en C con ncurses, memoria compartida y archivos de texto, con su propio README. Ya no forma parte del sistema: sirve de referencia mientras su lógica pasa al servidor. También está en la rama `Sistema-en-ncurses`.

`archivo/prototipo-qml/` conserva el prototipo con el que se definieron las pantallas. El cliente real ya lo reemplazó.

## Licencia

Distribuido bajo la [Licencia MIT](LICENSE).
