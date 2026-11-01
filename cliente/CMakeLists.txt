# Cliente del sistema
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/cliente_comandas
cmake_minimum_required(VERSION 3.21)
project(ClienteComandas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(
    Qt6 6.8
    REQUIRED COMPONENTS
    Gui
    Qml
    Quick
    QuickControls2
)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(cliente_comandas main.cpp)

# Hacen el trabajo que hacía el qmldir del prototipo: estos tres archivos
# existen una sola vez y todas las pantallas los comparten.
set_source_files_properties(
    Comandas/Tema.qml
    Comandas/Avisos.qml
    Comandas/Api.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

# Los .qml quedan dentro del ejecutable y Qt los revisa al compilar
qt_add_qml_module(
    cliente_comandas
    URI Comandas
    QML_FILES
    Main.qml
    Comandas/AdminIngredientes.qml
    Comandas/Api.qml
    Comandas/Aviso.qml
    Comandas/Avisos.qml
    Comandas/ChipEstado.qml
    Comandas/DialogoConfirmar.qml
    Comandas/DialogoConfirmarOrden.qml
    Comandas/DialogoIngrediente.qml
    Comandas/ElegirCuenta.qml
    Comandas/Encabezado.qml
    Comandas/Etiqueta.qml
    Comandas/LineaOrden.qml
    Comandas/PantallaAdmin.qml
    Comandas/PantallaCocina.qml
    Comandas/PantallaLogin.qml
    Comandas/PantallaMesero.qml
    Comandas/PantallaPendiente.qml
    Comandas/TarjetaProducto.qml
    Comandas/TarjetaRonda.qml
    Comandas/Tema.qml
    Comandas/VistaMisOrdenes.qml
    Comandas/VistaTomarOrden.qml
)

# El estilo va en la raíz de los recursos (:/qtquickcontrols2.conf), que es
# donde Qt lo busca; dentro del módulo quedaría en /qt/qml/Comandas/.
qt_add_resources(
    cliente_comandas "configuracion"
    PREFIX "/"
    FILES qtquickcontrols2.conf
)

target_link_libraries(
    cliente_comandas PRIVATE
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)
