import QtQuick
import QtQuick.Controls as Controls
import "."

Controls.ApplicationWindow {
    id: window
    readonly property int initialWidth: 1400
    readonly property int initialHeight: 880
    width: initialWidth
    height: initialHeight
    minimumWidth: initialWidth
    minimumHeight: initialHeight
    visible: true
    title: qsTr("Vincent")

    property var canvasPage: null

    header: Controls.ToolBar {
        id: mainToolBar

        background: Rectangle {
            color: window.palette.window
            border.width: 0
        }
    }

    PainterCanvasPage {
        id: painterPage
        anchors.fill: parent
        onPageReady: window.canvasPage = painterPage
    }
}
