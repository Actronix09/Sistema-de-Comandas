pragma Singleton
import QtQml

// Canal global de avisos: cualquier pantalla llama Avisos.exito("...") y
// Main.qml lo muestra abajo de la ventana. Reemplaza a ui_show_success,
// ui_show_error y ui_show_info, pero sin bloquear la pantalla.
QtObject {
    signal mostrar(string texto, string tipo)   // tipo: "exito", "error" o "info"

    function exito(texto) { mostrar(texto, "exito") }
    function error(texto) { mostrar(texto, "error") }
    function info(texto) { mostrar(texto, "info") }
}
