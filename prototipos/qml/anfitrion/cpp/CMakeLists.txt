# Anfitrión en C++ del prototipo QML.
#
#   cmake -S . -B build
#   cmake --build build
#   ./build/prototipo_comandas
cmake_minimum_required(VERSION 3.21)
project(PrototipoComandas LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Gui Qml Quick)
qt_standard_project_setup()

qt_add_executable(prototipo_comandas main.cpp)

# El prototipo lee los .qml desde la carpeta del código para compartirlos con
# ejecutar.sh. En el sistema real se compilarían dentro del ejecutable con
# qt_add_qml_module, que además revisa los .qml al compilar.
get_filename_component(DIR_QML "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
target_compile_definitions(prototipo_comandas PRIVATE DIR_QML="${DIR_QML}")

target_link_libraries(prototipo_comandas PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick)
