import QtQuick
import QtQuick.Controls as Controls
import "."

Controls.ApplicationWindow {
    id: window
    width: 1080
    height: 720
    visible: true
    title: qsTr("Vincent")

    property var canvasPage: null

    header: Controls.ToolBar {
        id: mainToolBar
        contentHeight: implicitHeight

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
