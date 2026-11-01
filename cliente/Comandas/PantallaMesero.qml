import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Menú del mesero: "Tomar Nueva Orden" y "Ver Mis Ordenes", ahora como pestañas.
// Cuando cocina o bar terminan una orden suya, el mesero recibe un aviso y la
// pestaña "Mis órdenes" cuenta cuántos avisos no ha visto.
Item {
    id: pantalla

    signal salir()

    property int sinVer: 0

    Connections {
        target: Api
        function onOrdenLista(datos) {
            if (!Api.usuarioActual || datos.id_mesero !== Api.usuarioActual.id_usuario)
                return
            Avisos.exito("¡Orden lista! Mesa " + datos.numero_mesa + " · " + datos.cuenta + ": " + datos.productos.join(", "))
            if (pestanas.currentIndex !== 1)
                pantalla.sinVer++
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Tema.fondo
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Encabezado {
            Layout.fillWidth: true
            titulo: "Mesero"
            onSalir: pantalla.salir()
        }

        TabBar {
            id: pestanas
            Layout.leftMargin: Tema.margen
            Layout.topMargin: 8
            Layout.preferredWidth: Math.min(420, pantalla.width - 2 * Tema.margen)
            TabButton { text: "Tomar nueva orden" }
            TabButton { text: pantalla.sinVer > 0 ? "Mis órdenes (" + pantalla.sinVer + " listas)" : "Mis órdenes" }
            onCurrentIndexChanged: if (currentIndex === 1) pantalla.sinVer = 0
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: pestanas.currentIndex

            VistaTomarOrden {
                id: vistaOrden
            }
            VistaMisOrdenes {
                activa: pestanas.currentIndex === 1
            }
        }
    }
}
