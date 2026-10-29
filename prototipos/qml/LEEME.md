# Prototipo QML · Sistema de Comandas

Prototipo **desechable** para ver cómo se vería la interfaz y elegir estilo y lenguaje anfitrión.
Los datos están simulados en memoria: al cerrar la ventana se pierde todo.

## Ejecutar

```sh
cd prototipos/qml
./ejecutar.sh                    # Material, claro
./ejecutar.sh Material oscuro
./ejecutar.sh FluentWinUI3       # aspecto de Windows 11
./ejecutar.sh Universal
./ejecutar.sh Fusion
```

- Usa `qml6`. El `qml` de este sistema es el de Qt 5.15 y no abre estos archivos.
- Material necesita aceleración gráfica. Sin GPU (por ejemplo, una VM sin 3D) sus botones salen sin fondo.
  FluentWinUI3, Universal y Fusion sí se dibujan completos.

## Usuarios de prueba

Contraseña `1234` para todos, o los botones de acceso rápido del login.

| Usuario | Rol | Pantalla |
|---|---|---|
| `lmendez`, `operez` | Mesero | Tomar nueva orden y Mis órdenes |
| `rgamez` | Cocina | Tablero de cocina |
| `isoto` | Bar | Tablero de bar |
| `mrivas` | Administrador | Menú de administración |

## Qué probar

1. **Mesero:** agrega productos (clic, Enter o `+`; `-` quita uno), escribe una nota, confirma con `O` y elige mesa y cuenta.
2. **Cocina** (`rgamez`): la orden aparece en Pendientes. Elige la tarjeta con un clic y usa `1` (Iniciar) y `2` (Lista),
   o haz clic en el estado de cada producto. Activa "Simular pedidos" para ver llegar órdenes de otros meseros.
3. **Mesero › Mis órdenes:** entrega lo que ya está listo.
4. **Admin › Ingredientes:** alta, edición, baja y `+`/`-`. Deja un ingrediente en 0 y vuelve como mesero:
   los productos que lo usan salen "Sin stock".
5. Repite con otros estilos y en oscuro.

## Qué decidir al terminar

- **D1 · Estilo y distribución:** qué estilo te gusta y qué cambiarías de cada pantalla.
- **D2 · Anfitrión:** el mismo `Main.qml` se carga desde Python o desde C++ (abajo). Compara instalación,
  arranque y qué tan cómodo te resulta cada uno.

## Anfitriones

**Python.** Usa PySide6 si está instalado; si no, el PyQt6 del sistema.

```sh
python3 anfitrion/main.py                      # admite --oscuro
# Con PySide6 (el recomendado para el sistema real), sin instalar nada global:
uv run --no-project --with pyside6 anfitrion/main.py
```

**C++.**

```sh
cmake -S anfitrion/cpp -B anfitrion/cpp/build
cmake --build anfitrion/cpp/build
./anfitrion/cpp/build/prototipo_comandas       # admite --oscuro
QT_QUICK_CONTROLS_STYLE=FluentWinUI3 ./anfitrion/cpp/build/prototipo_comandas
```

## Cómo está armado

```
Main.qml                   navegación por rol (StackView), avisos
Comandas/qmldir            declara el módulo y sus singletons
Comandas/ApiSimulada.qml   singleton "Api": la costura con el servidor (equivalencias REST en su encabezado)
Comandas/Tema.qml          colores claro/oscuro, medidas, formatos de dinero y hora
Comandas/Avisos.qml        avisos globales (reemplazan a ui_show_success/error/info)
Comandas/Pantalla*.qml     pantallas; Vista*.qml son las pestañas del mesero
Comandas/Tarjeta*.qml, Dialogo*.qml, Etiqueta.qml, ChipEstado.qml, ...   componentes
Comandas/Capturas.qml      modo --capturas
```

Patrones que conviene copiar:

- Las pantallas solo hablan con `Api` y reciben `{codigo, mensaje, datos}` en un callback.
  Para conectar el servidor real se cambia `Api`, no las pantallas.
- Las respuestas llegan después. Si la pantalla se cerró mientras tanto, el callback no hace nada (`if (!vista) return`).
- Las pantallas avisan con señales (`sesionIniciada`, `salir`, `volver`) y `Main.qml` decide a dónde ir.
- Delegados con `required property` y `pragma ComponentBehavior: Bound`.

Revisión estática (sin avisos). Usa rutas absolutas: con relativas, `qmllint` da un falso aviso sobre los singletons.

```sh
/usr/lib/qt6/bin/qmllint -I "$PWD" "$PWD/Main.qml" "$PWD"/Comandas/*.qml
```

## Capturas de todas las pantallas

```sh
mkdir -p /tmp/capturas
python3 anfitrion/main.py --capturas /tmp/capturas            # ventana completa, con diálogos
QT_QUICK_CONTROLS_STYLE=FluentWinUI3 python3 anfitrion/main.py --oscuro --capturas /tmp/capturas
```

## Simplificaciones del prototipo (no son decisiones)

- Una sola contraseña, `1234`, sin hash.
- Una comanda por unidad, porque tu esquema no tiene `cantidad`.
- El stock se descuenta al crear la orden. Cuándo descontarlo de verdad lo decides en tu esquema.
- La estación sale de la categoría: bebidas → bar; alimentos y dulcería → cocina.
- Una ronda son las comandas de la misma cuenta con la misma hora.
- Se usa el estado `lista`, que todavía falta en `ck_comanda_estado`.
- Las mesas 4 a 6, las existencias y casi todo el catálogo son inventados; lo demás viene de `datos-ejemplo.sql`.

## Ejercicios

Las pantallas marcadas "Por construir" traen su especificación:
1 Ver logs · 2 Recuperar contraseña · 3 Crear usuario · 4 Admin › Usuarios · 5 Admin › Productos.
