pragma Singleton
import QtQuick

// ---------------------------------------------------------------------------
//  Tema: colores, medidas y formatos compartidos por todas las pantallas.
//
//  Los controles (botones, campos, combos) los dibuja el estilo elegido al
//  arrancar; las superficies propias (tarjetas, paneles, tablero) usan estos
//  colores. Por ahora la app es solo en claro (Theme=Light en
//  qtquickcontrols2.conf); los colores oscuros se quedan para cuando se
//  decida seguir el tema del sistema.
// ---------------------------------------------------------------------------
QtObject {
    id: tema

    readonly property bool oscuro: false

    // Superficies y texto
    readonly property color fondo:          oscuro ? "#14161a" : "#f3f4f6"
    readonly property color superficie:     oscuro ? "#1d2026" : "#ffffff"
    readonly property color superficieAlta: oscuro ? "#262a32" : "#eef0f3"
    readonly property color borde:          oscuro ? "#363b46" : "#d9dde3"
    readonly property color texto:          oscuro ? "#e8eaed" : "#1f2328"
    readonly property color textoSuave:     oscuro ? "#a3a9b3" : "#5c6470"

    // Acento de la marca y colores con significado
    readonly property color acento:           oscuro ? "#ff8a4c" : "#d9531e"
    // Texto sobre un fondo de color: en oscuro los tonos son claros, así que va oscuro
    readonly property color textoSobreAcento: oscuro ? "#1a1205" : "#ffffff"
    readonly property color exito:            oscuro ? "#4cc38a" : "#1f8a4c"
    readonly property color advertencia:      oscuro ? "#f2c14e" : "#b7791f"
    readonly property color peligro:          oscuro ? "#ff6b6b" : "#c53030"
    readonly property color info:             oscuro ? "#6cb6ff" : "#2563eb"

    // Medidas
    readonly property int radio: 10
    readonly property int espacio: 12
    readonly property int margen: 16
    readonly property int letraChica: 12
    readonly property int letraNormal: 14
    readonly property int letraMediana: 16
    readonly property int letraTitulo: 24

    // Estados de comanda. 'lista' todavía no está en ck_comanda_estado de tu esquema.
    function nombreEstado(estado) {
        switch (estado) {
        case "pendiente":      return "Pendiente"
        case "en_preparacion": return "En preparación"
        case "lista":          return "Lista"
        case "entregada":      return "Entregada"
        case "cancelada":      return "Cancelada"
        }
        return estado
    }

    function colorEstado(estado) {
        switch (estado) {
        case "pendiente":      return info
        case "en_preparacion": return advertencia
        case "lista":          return exito
        case "cancelada":      return peligro
        }
        return textoSuave
    }

    // Categorías del CHECK ck_producto_categoria
    function nombreCategoria(categoria) {
        switch (categoria) {
        case "alimentos": return "Alimentos"
        case "bebidas":   return "Bebidas"
        case "dulceria":  return "Dulcería"
        }
        return categoria
    }

    function colorCategoria(categoria) {
        switch (categoria) {
        case "alimentos": return oscuro ? "#f0a05a" : "#c46a1c"
        case "bebidas":   return oscuro ? "#6cb6ff" : "#2563eb"
        case "dulceria":  return oscuro ? "#e58ac4" : "#b03a87"
        }
        return textoSuave
    }

    // Texto del rol a partir del usuario (supertipo + subtipo del esquema)
    function nombreRol(usuario) {
        if (!usuario)
            return ""
        switch (usuario.rol) {
        case "mesero":       return "Mesero · turno " + usuario.turno_asignado
        case "bar_cocina":   return usuario.estacion_asignada === "bar" ? "Bar" : "Cocina"
        case "cajero_admin": return usuario.nivel_acceso === "administrador" ? "Administrador" : "Cajero"
        }
        return usuario.rol
    }

    // Color del tiempo de espera de una ronda en cocina
    function colorEspera(minutos) {
        if (minutos >= 20)
            return peligro
        if (minutos >= 10)
            return advertencia
        return textoSuave
    }

    // Formatos
    function dinero(valor) {
        return Number(valor).toLocaleCurrencyString(Qt.locale("es_MX"), "$")
    }

    function hora(milisegundos) {
        return Qt.formatTime(new Date(milisegundos), "HH:mm")
    }
}
