pragma ComponentBehavior: Bound

import QtQuick
import Vincent 2.0

Rectangle {
    id: surface
    color: workspaceColor
    focus: true
    clip: true

    property color workspaceColor: "#1f1f20"
    property color canvasColor: "white"
    property color brushColor: "#1a1a1a"
    property real brushSize: 2
    property real brushFlow: 1
    property real brushOpacity: 1
    readonly property real maximumAntialiasingBrushHardness: 1
    property real brushHardness: maximumAntialiasingBrushHardness
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
    readonly property real workspaceCanvasHorizontalInsetRatio: 0.09
    readonly property real workspaceCanvasTopInsetRatio: 0.12
    readonly property real workspaceCanvasBottomInsetRatio: 0.10
    readonly property int workspaceCanvasMinimumInset: 24
    readonly property int workspaceCanvasHorizontalInset: Math.max(workspaceCanvasMinimumInset, Math.round(width * workspaceCanvasHorizontalInsetRatio))
    readonly property int workspaceCanvasTopInset: Math.max(workspaceCanvasMinimumInset, Math.round(height * workspaceCanvasTopInsetRatio))
    readonly property int workspaceCanvasBottomInset: Math.max(workspaceCanvasMinimumInset, Math.round(height * workspaceCanvasBottomInsetRatio))
    readonly property int workspaceCanvasWidth: Math.max(1, Math.round(width) - workspaceCanvasHorizontalInset * 2)
    readonly property int workspaceCanvasHeight: Math.max(1, Math.round(height) - workspaceCanvasTopInset - workspaceCanvasBottomInset)

    signal brushDeltaRequested(int delta)
    signal toolShortcutRequested(string tool)

    function syncCanvasItemSizeToWorkspace() {
        if (!canvasItemReady || surface.width <= 0 || surface.height <= 0) {
            return;
        }
        canvasSurface.resizeCanvasSurface(workspaceCanvasWidth, workspaceCanvasHeight);
        canvasSizeCreated = true;
    }

    function newCanvas() {
        syncCanvasItemSizeToWorkspace();
        canvasSurface.newCanvas();
    }

    function clearCanvas() {
        syncCanvasItemSizeToWorkspace();
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
            syncCanvasItemSizeToWorkspace();
        }
    }

    onHeightChanged: {
        if (!surface.canvasSizeCreated) {
            syncCanvasItemSizeToWorkspace();
        }
    }

    Item {
        id: canvasViewport
        objectName: "canvasViewport"
        x: surface.workspaceCanvasHorizontalInset
        y: surface.workspaceCanvasTopInset
        width: surface.workspaceCanvasWidth
        height: surface.workspaceCanvasHeight

        Rectangle {
            id: canvasPaper
            objectName: "canvasPaper"
            anchors.centerIn: parent
            width: canvasSurface.width
            height: canvasSurface.height
            color: surface.canvasColor
            border.color: "#b8bcc4"
            border.width: canvasPaper.width < surface.width || canvasPaper.height < surface.height ? 1 : 0
        }

        DrawingSurfaceItem {
            id: canvasSurface
            anchors.centerIn: parent
            z: 1
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
                surface.syncCanvasItemSizeToWorkspace();
            }
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
