pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Las tres formas de organizar las órdenes: por tiempo, por mesa y cuenta, y
// por mesero. Lo usan el mesero, cocina/bar y caja; agrupar() hace el trabajo.
RowLayout {
    id: selector

    property string modo: "tiempo"     // "tiempo" | "mesa" | "mesero"
    // En caja cada elemento ya es una cuenta: "Mesa y cuenta" agrupa solo por mesa
    property bool soloMesa: false

    // rondas (ya ordenadas) → [{ titulo, rondas }]. Dentro de cada grupo se
    // conserva el orden recibido. Por tiempo hay un solo grupo sin título.
    function agrupar(rondas) {
        if (modo === "tiempo")
            return [{ clave: "todas", titulo: "", rondas: rondas }]
        const grupos = []
        const porClave = {}
        for (const r of rondas) {
            const clave = modo === "mesa" ? (soloMesa ? String(r.numero_mesa) : r.numero_mesa + "|" + r.secuencial)
                                          : String(r.id_mesero)
            if (!porClave[clave]) {
                porClave[clave] = {
                    clave: clave,
                    titulo: modo === "mesa" ? "Mesa " + r.numero_mesa + (soloMesa ? "" : " · " + r.cuenta) : r.mesero,
                    mesa: r.numero_mesa, secuencial: r.secuencial, id_mesero: r.id_mesero,
                    rondas: []
                }
                grupos.push(porClave[clave])
            }
            porClave[clave].rondas.push(r)
        }
        const yo = Api.usuarioActual ? Api.usuarioActual.id_usuario : -1
        if (modo === "mesa")
            grupos.sort((a, b) => (a.mesa - b.mesa) || (a.secuencial - b.secuencial))
        else   // las del usuario con sesión primero, luego por nombre
            grupos.sort((a, b) => (Number(b.id_mesero === yo) - Number(a.id_mesero === yo))
                                  || a.titulo.localeCompare(b.titulo, "es"))
        return grupos
    }

    // Un layout dentro de otro se estira por defecto; así conserva su ancho
    Layout.fillWidth: false
    spacing: 4

    Label {
        text: "Organizar:"
        color: Tema.textoSuave
        font.pixelSize: Tema.letraChica
    }

    Repeater {
        model: [
            { valor: "tiempo", texto: "Tiempo" },
            { valor: "mesa", texto: "Mesa y cuenta" },
            { valor: "mesero", texto: "Mesero" }
        ]
        delegate: Button {
            id: opcion
            required property var modelData
            text: opcion.modelData.texto
            highlighted: selector.modo === opcion.modelData.valor
            flat: !highlighted
            onClicked: selector.modo = opcion.modelData.valor
        }
    }
}
