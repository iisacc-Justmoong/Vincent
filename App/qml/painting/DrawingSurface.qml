pragma ComponentBehavior: Bound

import QtQuick
import Vincent 2.0

Rectangle {
    id: surface
    color: "white"
    focus: true
    clip: true

    property color brushColor: "#1a1a1a"
    property real brushSize: 2
    property real brushFlow: 1
    property real brushOpacity: 1
    property real brushHardness: 1
    property real brushSpacing: 0
    property real brushSpacingRatio: 0
    property real pressureCurveMinimum: 0
    property real pressureCurveCenter: 0.5
    property real pressureCurveMaximum: 1
    property real stabilizerStrength: 0
    property var documentViewModel: null
    property string viewId: ""
    property int canvasWidth: 1
    property int canvasHeight: 1
    property string toolMode: "brush"
    property bool canvasItemReady: false
    property bool canvasSizeCreated: false

    signal brushDeltaRequested(int delta)
    signal toolShortcutRequested(string tool)

    function windowCanvasWidth() {
        return Math.max(1, Math.round(surface.width));
    }

    function windowCanvasHeight() {
        return Math.max(1, Math.round(surface.height));
    }

    function syncCanvasItemSizeToWindow() {
        if (!canvasItemReady || surface.width <= 0 || surface.height <= 0) {
            return;
        }
        canvasSurface.resizeCanvasSurface(windowCanvasWidth(), windowCanvasHeight());
        canvasSizeCreated = true;
    }

    function newCanvas() {
        syncCanvasItemSizeToWindow();
        canvasSurface.newCanvas();
    }

    function clearCanvas() {
        syncCanvasItemSizeToWindow();
        canvasSurface.clearCanvas();
    }

    function openRaster(fileUrl) {
        return canvasSurface.openRaster(fileUrl ? fileUrl.toString() : "");
    }

    function saveToFile(fileUrl) {
        return canvasSurface.saveToFile(fileUrl ? fileUrl.toString() : "");
    }

    onWidthChanged: {
        if (!surface.canvasSizeCreated) {
            syncCanvasItemSizeToWindow();
        }
    }

    onHeightChanged: {
        if (!surface.canvasSizeCreated) {
            syncCanvasItemSizeToWindow();
        }
    }

    DrawingSurfaceItem {
        id: canvasSurface
        anchors.centerIn: parent
        width: 1
        height: 1
        brushColor: surface.brushColor
        brushSize: surface.brushSize
        brushFlow: surface.brushFlow
        brushOpacity: surface.brushOpacity
        brushHardness: surface.brushHardness
        brushSpacing: surface.brushSpacing
        brushSpacingRatio: surface.brushSpacingRatio
        pressureCurveMinimum: surface.pressureCurveMinimum
        pressureCurveCenter: surface.pressureCurveCenter
        pressureCurveMaximum: surface.pressureCurveMaximum
        stabilizerStrength: surface.stabilizerStrength
        toolMode: surface.toolMode
        documentViewModel: surface.documentViewModel
        viewId: surface.viewId

        Component.onCompleted: {
            surface.canvasItemReady = true;
            surface.syncCanvasItemSizeToWindow();
        }
    }

    MouseArea {
        parent: canvasSurface
        anchors.fill: parent
        z: 3
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        cursorShape: surface.toolMode === "eraser" ? Qt.PointingHandCursor : Qt.CrossCursor

        onWheel: function (wheel) {
            surface.brushDeltaRequested(wheel.angleDelta.y > 0 ? 1 : -1);
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
