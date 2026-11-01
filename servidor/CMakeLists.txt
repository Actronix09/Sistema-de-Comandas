# Servidor del sistema
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/servidor_comandas
#
# Los datos de conexión van en variables de entorno; ver main.cpp.
cmake_minimum_required(VERSION 3.21)
project(ServidorComandas LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(
    Qt6 6.8
    REQUIRED COMPONENTS
    Core
    Sql
    WebSockets
)
qt_standard_project_setup(REQUIRES 6.8)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)

# validaciones.c y protocolo.h son los mismos del sistema en C, sin copiarlos
qt_add_executable(
    servidor_comandas
    main.cpp
    servidor.cpp
    servidor.h
    ../validaciones.c
)
target_include_directories(servidor_comandas PRIVATE ..)

target_link_libraries(
    servidor_comandas PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::WebSockets
    PkgConfig::SODIUM
)
