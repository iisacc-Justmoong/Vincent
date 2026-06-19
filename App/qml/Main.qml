import QtQuick
import LVRS 1.0 as LV
import "./canvas" as CanvasViews

LV.ApplicationWindow {
    id: window
    readonly property int initialWidth: 1400
    readonly property int initialHeight: 880
    width: initialWidth
    height: initialHeight
    minimumWidth: initialWidth
    minimumHeight: initialHeight
    visible: true
    windowColor: LV.Theme.window
    solidChrome: true
    windowDragHandleEnabled: true
    navigationEnabled: false
    autoAttachRuntimeEvents: true

    property var canvasPage: null

    CanvasViews.PainterCanvasPage {
        id: painterPage
        anchors.fill: parent
        topChromeReservedHeight: window.windowDragHandleEnabled ? window.windowDragHandleHeight : 0
        onPageReady: window.canvasPage = painterPage
    }
}
