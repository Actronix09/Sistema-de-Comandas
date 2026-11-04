# Sistema de Comandas

Sistema para tomar, preparar y cobrar las comandas de un salón de eventos. Se está migrando del programa original en C y ncurses a una aplicación con interfaz en Qt Quick, un servidor en C++ y una base de datos en PostgreSQL.

**Todo se compila y se ejecuta con Podman.** No hace falta instalar Qt, CMake ni el kit de Android en el equipo.

> **Estado actual:** el cliente todavía no se conecta al servidor. Funciona con datos simulados (`cliente/Comandas/Api.qml`), así que sus pantallas se pueden probar sin levantar la base. El servidor solo atiende 3 de sus 27 operaciones.

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
- [Servidor](#servidor)
- [Base de datos](#base-de-datos)
- [Problemas conocidos](#problemas-conocidos)
- [Sistema original en C](#sistema-original-en-c)
- [Licencia](#licencia)

## Arquitectura

```
┌───────────────────────────┐
│ Cliente en Qt Quick       │  Linux y Android. Windows, pendiente.
│ Mesero · Cocina/Bar · Caja│
└─────────────┬─────────────┘
              │  Pendiente: hoy usa datos simulados
              │  WebSocket con JSON · puerto 9000
┌─────────────┴─────────────┐
│ Servidor en C++ y Qt      │  Lo único que toca la base
└─────────────┬─────────────┘
              │  SQL · puerto 5432 (5433 en el equipo)
┌─────────────┴─────────────┐
│ PostgreSQL 16             │  Reglas del negocio en restricciones y disparadores
└───────────────────────────┘
```

## Estado del proyecto

| Parte | Ya funciona | Falta |
| :-- | :-- | :-- |
| **Base de datos** | 17 tablas, 6 vistas y 13 disparadores: máquina de estados de la comanda, stock, reglas del cobro y estación por categoría | Pagos a cuentas cerradas, reglas para cancelar comandas y decisiones de rondas y logs |
| **Cliente** | Login, mesero, cocina/bar, caja, usuarios e ingredientes, con diseño adaptable a teléfono | Conexión real al servidor, ver logs, recuperar contraseña, crear usuario y productos con receta |
| **Servidor** | WebSocket, conexión a la base, sesión por conexión, login con Argon2id, logout y lista de productos | Las otras 24 operaciones, transacciones, avisos en tiempo real y conexión cifrada |
| **Contenedores** | Base, servidor, cliente y Android | Arranque automático en la PC de caja |
| **Plataformas** | Linux y APK de Android | Instalador de Windows |

La lista detallada de pendientes, con responsables, está en `PENDIENTES.md`, en el repositorio de la base de datos.

### Qué puede hacer cada rol en el cliente

| Rol | Pantallas |
| :-- | :-- |
| Mesero | Elegir mesa → cuenta → productos, confirmar la orden y seguir sus órdenes, con las listas primero |
| Cocina y bar | Rondas de su estación y cambio de estado de cada comanda |
| Cajero | Cuentas abiertas, detalle, pagos con propina, saldo y cierre de cuenta |
| Administrador | Lo del cajero, usuarios e ingredientes |

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

## Servidor

Se configura con variables de entorno, que `compose.yaml` toma de `.env`:

| Variable | Valor por omisión | Para qué |
| :-- | :-- | :-- |
| `COMANDAS_BD_HOST` | `localhost` | Equipo de la base |
| `COMANDAS_BD_PUERTO` | `5433` | Puerto de la base |
| `COMANDAS_BD_NOMBRE` | `comandas` | Nombre de la base |
| `COMANDAS_BD_USUARIO` | Obligatoria | Usuario de la base |
| `COMANDAS_BD_CONTRASENA` | Obligatoria | Contraseña de la base |
| `COMANDAS_PUERTO` | `9000` | Puerto para los clientes |

Los mensajes viajan como JSON por un solo WebSocket. Cada petición lleva un `id`, que la respuesta repite:

```json
→ {"id": 1, "op": "OP_LOGIN", "datos": {"usuario": "mrivas", "contrasena": "..."}}
← {"id": 1, "ok": true, "datos": {"id_usuario": 5, "rol": "cajero_admin"}}
← {"id": 1, "ok": false, "error": {"codigo": "CREDENCIALES_INVALIDAS", "mensaje": "Usuario o contraseña incorrectos"}}
```

Hoy funcionan `OP_LOGIN`, `OP_LOGOUT` y `OP_LISTAR_PRODUCTOS`. Las demás operaciones de `comun/protocolo.h` responden `NO_IMPLEMENTADA`, y sin sesión la respuesta es `NO_AUTENTICADO`.

Para generar el hash de una contraseña:

```bash
podman compose build servidor
podman run --rm -i localhost/comandas-servidor servidor_comandas --generar-hash
```

Escribe la contraseña y presiona Enter. Debe tener 8 caracteres o más, una mayúscula, una minúscula, un número y 2 símbolos de `* / _ - + .`.

## Base de datos

El repositorio `desarrollo-proyecto` tiene el esquema (`sql/esquema.sql`), los datos de ejemplo (`sql/datos-ejemplo.sql`), los diagramas y la documentación del modelo. `compose.yaml` monta esos archivos para no tener dos copias.

Sus pruebas se corren desde ese repositorio y hoy pasan todas (34 rechazos y 13 casos válidos):

```bash
make -C sql pruebas
```

Todavía usan el contenedor `practica1-postgres`, en Docker. Pasarlas a Podman es el punto 5.5 de `PENDIENTES.md`.

## Problemas conocidos

- **La contraseña de desarrollo de la base no se conoce.** `db/init/03-contrasenas-desarrollo.sql` dice que es `Demo_123`, pero ese valor no coincide con el hash del archivo y tampoco cumple las reglas de contraseña. Hace falta generar un hash nuevo y actualizar ese archivo.
- **Las contraseñas viajan sin cifrar.** El servidor usa `ws://`; pasar a `wss://` es el punto 6.11 de `PENDIENTES.md`.
- **`dcampos` solo existe en los datos simulados.** Falta en `datos-ejemplo.sql`, igual que el nivel `cajero` de `mrivas` (punto 3.12).

## Sistema original en C

`archivo/sistema-c/` conserva el programa en C con ncurses, memoria compartida y archivos de texto, con su propio README. Ya no forma parte del sistema: sirve de referencia mientras su lógica pasa al servidor. También está en la rama `Sistema-en-ncurses`.

`archivo/prototipo-qml/` conserva el prototipo con el que se definieron las pantallas. El cliente real ya lo reemplazó.

## Licencia

Distribuido bajo la [Licencia MIT](LICENSE).
