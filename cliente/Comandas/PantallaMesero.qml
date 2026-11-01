import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Menú del mesero: "Tomar Nueva Orden" y "Ver Mis Ordenes", ahora como pestañas.
Item {
    id: pantalla

    signal salir()

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
            TabButton { text: "Mis órdenes" }
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
