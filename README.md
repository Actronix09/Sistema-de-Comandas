# Sistema de Comandas

Sistema para tomar y cobrar las comandas de un salón de eventos. Se está migrando del programa original en C y ncurses a una aplicación con interfaz en Qt Quick para Linux, Windows y Android, con un servidor en C++ y PostgreSQL.

> **Estado:** el cliente todavía trabaja con datos simulados (`cliente/Comandas/Api.qml`), y el servidor solo atiende el inicio y el cierre de sesión y la lista de productos. La lista completa de lo que falta está en `PENDIENTES.md`, en el repositorio de la base de datos.

## Estructura

```
├── cliente/      Aplicación en Qt Quick: pantallas en QML, arranque en C++
├── servidor/     Servidor en C++ y Qt: WebSocket con JSON hacia PostgreSQL
├── comun/        Código que comparten el cliente, el servidor y el sistema en C
├── android/      Contenedor para compilar el cliente y probarlo en un emulador
├── db/init/      Contraseñas de desarrollo para la base en contenedor
├── archivo/      Lo que ya no se usa; se conserva como referencia
│   ├── sistema-c/       Sistema original en C y ncurses, con su README
│   └── prototipo-qml/   Prototipo de las pantallas, con datos simulados
└── compose.yaml  Base de datos, servidor, cliente y Android
```

El esquema y los datos de la base están en otro repositorio, `desarrollo-proyecto`, en su carpeta `sql/`. `compose.yaml` los toma de ahí con la variable `COMANDAS_SQL` de `.env`.

## Cómo levantarlo

Se necesita Podman con `podman compose`.

```bash
cp .env.example .env                                  # y cambiar la contraseña
podman compose up -d --build                          # base y servidor
podman compose --profile cliente up --build cliente   # cliente, en Wayland
```

Para Android (necesita KVM):

```bash
podman compose --profile android up --build android                  # compila y prueba
podman compose --profile android-ventana up --build android-ventana  # con ventana
```

La base queda en el puerto 5433 y el servidor en el 9000. Los usuarios de ejemplo entran con la contraseña `Demo_123`, que solo existe en desarrollo.

> `podman compose down -v` borra el volumen con todos los datos.

## Licencia

Distribuido bajo la [Licencia MIT](LICENSE).
