# Sistema de autentificación
```
Este sistema recibe y maneja los usuarios de un sistema de comandas, maneja la información:
```
- Usuario
- Contraseña
- Correo
- Numero teléfono
- Tipo usuarios (Cocina o Mesero)

```
Toda la navegación es con las flechas, enter y esc.
```

## Funciones
```
**void encriptar(entrada, salida)**
    Recibe una cadena a encriptar y otra vacía para guardar la salida, usa el método de encriptado EVP.

**void cargar_usuarios()**
    Toma los datos en usuarios.txt y los guarda en memoria con la estructura USUARIO a partir de la estructura `Nombre | Usuario | Contraseña Encriptada | correo | Numero Teléfono | Tipo Usuario`.

**void guardad_usuarios()**
    Envía los datos de los usuarios a `usuarios.txt`

**void leer_input(fila, columna, cadena buffer, maxima longitud, ocultar)**
    Recibe la posición de la caja de texto, el buffer de texto, la longitud maxima de la cadena y si esta se debe ocultar o no para la contraseña, con esto permite la escritura para el usuario, verificando que tecla es usada.

**int check_pass(contraseña)**
    Recibe la contraseña antes de encriptar y verifica que sea valida, o sea que tenga un numero, una mayúscula, una minúscula, dos caracteres especiales y al menos 8 caracteres en total.

**int check_mail(correo)**
    Verifica que el correo sea valida teniendo exactamente un arroba y un mínimo de un punto.

**int check_telf(teléfono)**
    Verifica que el teléfono sea valido teniendo solo numero y un mínimo de ocho caracteres.

**void iniciar_sesion()**

    Esta es la interfaz para iniciar sesión donde se pide el usuario y la contraseña.

**void crear_usuario**
    Esta es la interfaz para crear un usuario nuevo, se solicita el nombre, usuario, contraseña, correo, teléfono y tipo usuario, las verifica y de ser validos agrega el usuario nuevo.

**void main_menu**
    Menu principal con las opciones de iniciar sesión, crear usuario o salir.

**int main()**
    Se cargan los usuarios, carga y descarga ncurses ademas maneja parte del menu principal.
```

### Requisitos
- **GCC o similar**

### Compilación
- En terminal ejecutar `gcc IniciadorSesion.c -o IniciadorSesion -lncurses -lssl -lcrypto`
