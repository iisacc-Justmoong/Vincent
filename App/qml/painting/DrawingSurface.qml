pragma ComponentBehavior: Bound

import QtQuick
import LVRS 1.0 as LV
import Vincent 2.0

Rectangle {
    id: surface
    color: "transparent"
    focus: true
    clip: true

    property color brushColor: "#1a1a1a"
    property real brushSize: 2
    property var documentViewModel: null
    property string viewId: ""
    property int canvasWidth: 1
    property int canvasHeight: 1
    property string toolMode: "brush"

    signal brushDeltaRequested(int delta)
    signal toolShortcutRequested(string tool)

    function newCanvas() {
        canvasSurface.newCanvas()
    }

    function clearCanvas() {
        canvasSurface.clearCanvas()
    }

    function openRaster(fileUrl) {
        return canvasSurface.openRaster(fileUrl ? fileUrl.toString() : "")
    }

    function saveToFile(fileUrl) {
        return canvasSurface.saveToFile(fileUrl ? fileUrl.toString() : "")
    }

    DrawingSurfaceItem {
        id: canvasSurface
        anchors.centerIn: parent
        width: Math.max(1, surface.canvasWidth)
        height: Math.max(1, surface.canvasHeight)
        brushColor: surface.brushColor
        brushSize: surface.brushSize
        toolMode: surface.toolMode
        documentViewModel: surface.documentViewModel
        viewId: surface.viewId
    }

    PointHandler {
        id: stylusStrokeHandler
        parent: canvasSurface
        acceptedPointerTypes: PointerDevice.Pen | PointerDevice.Eraser
        enabled: surface.toolMode === "brush" || surface.toolMode === "eraser"
        target: null

        onActiveChanged: {
            if (active) {
                surface.forceActiveFocus()
                canvasSurface.beginStroke(point.position.x, point.position.y, point.pressure, true)
            } else {
                canvasSurface.endStroke(point.position.x, point.position.y, point.pressure, true)
            }
        }

        onPointChanged: {
            if (!active) {
                return
            }
            canvasSurface.appendStrokePoint(point.position.x, point.position.y, point.pressure, true)
        }
    }

    PointHandler {
        id: mouseStrokeHandler
        parent: canvasSurface
        acceptedDevices: PointerDevice.Mouse
        acceptedButtons: Qt.LeftButton
        enabled: surface.toolMode === "brush" || surface.toolMode === "eraser"
        target: null

        onActiveChanged: {
            if (active) {
                surface.forceActiveFocus()
                canvasSurface.beginStroke(point.position.x, point.position.y, 1.0, false)
            } else {
                canvasSurface.endStroke(point.position.x, point.position.y, 1.0, false)
            }
        }

        onPointChanged: {
            if (!active) {
                return
            }
            canvasSurface.appendStrokePoint(point.position.x, point.position.y, 1.0, false)
        }
    }

    MouseArea {
        parent: canvasSurface
        anchors.fill: parent
        z: 3
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: surface.toolMode === "eraser" ? Qt.PointingHandCursor : Qt.CrossCursor

        onWheel: function(wheel) {
            surface.brushDeltaRequested(wheel.angleDelta.y > 0 ? 1 : -1)
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["B", "ㅠ"]
        onActivated: surface.toolShortcutRequested("brush")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["E", "ㄷ"]
        onActivated: surface.toolShortcutRequested("eraser")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["["]
        onActivated: surface.brushDeltaRequested(-1)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["]"]
        onActivated: surface.brushDeltaRequested(1)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.Undo
        onActivated: canvasSurface.undo()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.Redo
        onActivated: canvasSurface.redo()
    }
}
