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
    property bool textEditingActive: false
    property color textToolAccentColor: "#A571E6"
    property int textToolFramePadding: 8
    readonly property real workspaceCanvasHorizontalInsetRatio: 0.09
    readonly property real workspaceCanvasTopInsetRatio: 0.12
    readonly property real workspaceCanvasBottomInsetRatio: 0.10
    readonly property int workspaceCanvasMinimumInset: 24
    readonly property int workspaceCanvasHorizontalInset: Math.max(workspaceCanvasMinimumInset, Math.round(width * workspaceCanvasHorizontalInsetRatio))
    readonly property int workspaceCanvasTopInset: Math.max(workspaceCanvasMinimumInset, Math.round(height * workspaceCanvasTopInsetRatio))
    readonly property int workspaceCanvasBottomInset: Math.max(workspaceCanvasMinimumInset, Math.round(height * workspaceCanvasBottomInsetRatio))
    readonly property int workspaceCanvasWidth: Math.max(1, Math.round(width) - workspaceCanvasHorizontalInset * 2)
    readonly property int workspaceCanvasHeight: Math.max(1, Math.round(height) - workspaceCanvasTopInset - workspaceCanvasBottomInset)
    readonly property int minimumTextToolFontPixelSize: 8
    readonly property int textToolFontPixelSize: Math.max(surface.minimumTextToolFontPixelSize, Math.round(surface.brushSize))
    readonly property int textToolMinimumWidth: 96

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
        cancelActiveText();
        syncCanvasItemSizeToWorkspace();
        canvasSurface.newCanvas();
    }

    function clearCanvas() {
        cancelActiveText();
        syncCanvasItemSizeToWorkspace();
        canvasSurface.clearCanvas();
    }

    function openRaster(fileUrl) {
        cancelActiveText();
        return canvasSurface.openRaster(fileUrl ? fileUrl.toString() : "");
    }

    function saveToFile(fileUrl) {
        commitActiveText();
        return canvasSurface.saveToFile(fileUrl ? fileUrl.toString() : "");
    }

    function longestTextLine(textValue) {
        const lines = (textValue && textValue.length ? textValue : " ").split(/\r?\n/);
        var longestLine = " ";
        for (let index = 0; index < lines.length; ++index) {
            if (lines[index].length > longestLine.length) {
                longestLine = lines[index];
            }
        }
        return longestLine.length ? longestLine : " ";
    }

    function textToolAvailableWidth() {
        return Math.max(surface.textToolMinimumWidth, canvasSurface.width - textToolEditorFrame.x);
    }

    function textToolMeasuredWidth() {
        return Math.ceil(textToolLineMetrics.advanceWidth) + surface.textToolFramePadding * 2;
    }

    function textToolResponsiveWidth() {
        return Math.min(surface.textToolAvailableWidth(), Math.max(surface.textToolMinimumWidth, surface.textToolMeasuredWidth()));
    }

    function beginTextPlacement(pointX, pointY) {
        if (surface.toolMode !== "text") {
            return;
        }

        commitActiveText();
        const maxX = Math.max(0, canvasSurface.width - surface.textToolMinimumWidth);
        const maxY = Math.max(0, canvasSurface.height - surface.textToolFontPixelSize - surface.textToolFramePadding * 2);
        textToolEditorFrame.x = Math.max(0, Math.min(maxX, pointX));
        textToolEditorFrame.y = Math.max(0, Math.min(maxY, pointY));
        textToolEditor.text = "";
        surface.textEditingActive = true;
        textToolEditor.forceActiveFocus();
    }

    function commitActiveText() {
        if (!surface.textEditingActive) {
            return;
        }

        const committedText = textToolEditor.text;
        const shouldCommit = committedText.trim().length > 0;
        surface.textEditingActive = false;
        if (shouldCommit) {
            canvasSurface.commitText(textToolEditorFrame.x, textToolEditorFrame.y, textToolEditorFrame.width, committedText, surface.textToolFontPixelSize, surface.brushColor);
        }
        textToolEditor.text = "";
        textToolEditor.focus = false;
    }

    function cancelActiveText() {
        if (!surface.textEditingActive) {
            return;
        }

        surface.textEditingActive = false;
        textToolEditor.text = "";
        textToolEditor.focus = false;
    }

    onToolModeChanged: {
        if (toolMode !== "text") {
            commitActiveText();
        }
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

        Rectangle {
            id: textToolEditorFrame
            parent: canvasSurface
            visible: surface.textEditingActive
            z: 4
            width: surface.textToolResponsiveWidth()
            height: Math.max(surface.textToolFontPixelSize + surface.textToolFramePadding * 2, textToolEditor.contentHeight + surface.textToolFramePadding * 2)
            color: Qt.rgba(255, 255, 255, 0.88)
            border.width: 1
            border.color: surface.textToolAccentColor

            TextMetrics {
                id: textToolLineMetrics
                font.pixelSize: surface.textToolFontPixelSize
                text: surface.longestTextLine(textToolEditor.text)
            }

            TextEdit {
                id: textToolEditor
                anchors.fill: parent
                anchors.margins: surface.textToolFramePadding
                visible: surface.textEditingActive
                color: surface.brushColor
                selectionColor: surface.textToolAccentColor
                selectedTextColor: "#ffffff"
                font.pixelSize: surface.textToolFontPixelSize
                wrapMode: TextEdit.Wrap
                focus: surface.textEditingActive
                selectByMouse: true

                onActiveFocusChanged: {
                    if (!activeFocus && surface.textEditingActive) {
                        surface.commitActiveText();
                    }
                }

                Keys.onPressed: function (event) {
                    if (event.key === Qt.Key_Escape) {
                        surface.cancelActiveText();
                        event.accepted = true;
                        return;
                    }

                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier))) {
                        surface.commitActiveText();
                        event.accepted = true;
                    }
                }
            }
        }
    }

    MouseArea {
        parent: canvasSurface
        anchors.fill: parent
        z: 3
        hoverEnabled: true
        acceptedButtons: surface.toolMode === "text" ? Qt.LeftButton : Qt.NoButton
        cursorShape: surface.toolMode === "text" ? Qt.IBeamCursor : surface.toolMode === "eraser" ? Qt.PointingHandCursor : Qt.CrossCursor

        onPressed: function (mouse) {
            if (surface.toolMode === "text") {
                surface.beginTextPlacement(mouse.x, mouse.y);
                mouse.accepted = true;
            }
        }

        onWheel: function (wheel) {
            surface.brushDeltaRequested(wheel.angleDelta.y > 0 ? 1 : -1);
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["B", "ㅠ"]
        enabled: !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("brush")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["E", "ㄷ"]
        enabled: !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("eraser")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["T", "ㅅ"]
        enabled: !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("text")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["["]
        enabled: !surface.textEditingActive
        onActivated: surface.brushDeltaRequested(-1)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["]"]
        enabled: !surface.textEditingActive
        onActivated: surface.brushDeltaRequested(1)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.Undo
        enabled: !surface.textEditingActive
        onActivated: canvasSurface.undo()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.Redo
        enabled: !surface.textEditingActive
        onActivated: canvasSurface.redo()
    }
}
