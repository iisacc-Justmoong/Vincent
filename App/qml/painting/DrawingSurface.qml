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
    property string shapeKind: "rectangle"
    property bool shapeDraggingActive: false
    property real shapeStartX: 0
    property real shapeStartY: 0
    property real shapeCurrentX: 0
    property real shapeCurrentY: 0
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
    readonly property int shapeToolMinimumDragDistance: 2
    readonly property int shapeToolStrokeWidth: Math.max(1, Math.round(surface.brushSize))

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
        cancelActiveShape();
        syncCanvasItemSizeToWorkspace();
        canvasSurface.newCanvas();
    }

    function clearCanvas() {
        cancelActiveText();
        cancelActiveShape();
        syncCanvasItemSizeToWorkspace();
        canvasSurface.clearCanvas();
    }

    function openRaster(fileUrl) {
        cancelActiveText();
        cancelActiveShape();
        return canvasSurface.openRaster(fileUrl ? fileUrl.toString() : "");
    }

    function saveToFile(fileUrl) {
        commitActiveText();
        commitActiveShape();
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

    function clampedShapePointX(pointX) {
        return Math.max(0, Math.min(canvasSurface.width, pointX));
    }

    function clampedShapePointY(pointY) {
        return Math.max(0, Math.min(canvasSurface.height, pointY));
    }

    function shapePreviewRect() {
        const left = Math.min(surface.shapeStartX, surface.shapeCurrentX);
        const top = Math.min(surface.shapeStartY, surface.shapeCurrentY);
        return {
            x: left,
            y: top,
            width: Math.abs(surface.shapeCurrentX - surface.shapeStartX),
            height: Math.abs(surface.shapeCurrentY - surface.shapeStartY)
        };
    }

    function requestShapePreviewPaint() {
        if (shapePreviewCanvas.visible) {
            shapePreviewCanvas.requestPaint();
        }
    }

    function beginShapeDrag(pointX, pointY) {
        if (surface.toolMode !== "shape") {
            return;
        }

        commitActiveText();
        surface.shapeStartX = clampedShapePointX(pointX);
        surface.shapeStartY = clampedShapePointY(pointY);
        surface.shapeCurrentX = surface.shapeStartX;
        surface.shapeCurrentY = surface.shapeStartY;
        surface.shapeDraggingActive = true;
        requestShapePreviewPaint();
    }

    function updateShapeDrag(pointX, pointY) {
        if (!surface.shapeDraggingActive) {
            return;
        }

        surface.shapeCurrentX = clampedShapePointX(pointX);
        surface.shapeCurrentY = clampedShapePointY(pointY);
        requestShapePreviewPaint();
    }

    function commitActiveShape() {
        if (!surface.shapeDraggingActive) {
            return;
        }

        const bounds = shapePreviewRect();
        surface.shapeDraggingActive = false;
        if (bounds.width >= surface.shapeToolMinimumDragDistance && bounds.height >= surface.shapeToolMinimumDragDistance) {
            canvasSurface.commitShape(bounds.x, bounds.y, bounds.width, bounds.height, surface.shapeKind, surface.brushColor, surface.shapeToolStrokeWidth);
        }
        requestShapePreviewPaint();
    }

    function cancelActiveShape() {
        if (!surface.shapeDraggingActive) {
            return;
        }

        surface.shapeDraggingActive = false;
        requestShapePreviewPaint();
    }

    function traceEllipsePath(context, x, y, widthValue, heightValue) {
        const kappa = 0.5522847498;
        const ox = widthValue / 2 * kappa;
        const oy = heightValue / 2 * kappa;
        const xe = x + widthValue;
        const ye = y + heightValue;
        const xm = x + widthValue / 2;
        const ym = y + heightValue / 2;

        context.moveTo(x, ym);
        context.bezierCurveTo(x, ym - oy, xm - ox, y, xm, y);
        context.bezierCurveTo(xm + ox, y, xe, ym - oy, xe, ym);
        context.bezierCurveTo(xe, ym + oy, xm + ox, ye, xm, ye);
        context.bezierCurveTo(xm - ox, ye, x, ym + oy, x, ym);
        context.closePath();
    }

    function traceStarPath(context, x, y, widthValue, heightValue) {
        const centerX = x + widthValue / 2;
        const centerY = y + heightValue / 2;
        const outerRadiusX = widthValue / 2;
        const outerRadiusY = heightValue / 2;
        const innerRadiusX = outerRadiusX * 0.45;
        const innerRadiusY = outerRadiusY * 0.45;

        for (let index = 0; index < 10; ++index) {
            const outerPoint = index % 2 === 0;
            const radiusX = outerPoint ? outerRadiusX : innerRadiusX;
            const radiusY = outerPoint ? outerRadiusY : innerRadiusY;
            const angle = -Math.PI / 2 + index * Math.PI / 5;
            const px = centerX + Math.cos(angle) * radiusX;
            const py = centerY + Math.sin(angle) * radiusY;
            if (index === 0) {
                context.moveTo(px, py);
            } else {
                context.lineTo(px, py);
            }
        }
        context.closePath();
    }

    function traceShapePath(context, shapeValue, x, y, widthValue, heightValue) {
        const shape = shapeValue === "triagle" ? "triangle" : shapeValue;
        if (shape === "ellipse") {
            traceEllipsePath(context, x, y, widthValue, heightValue);
            return;
        }
        if (shape === "triangle") {
            context.moveTo(x + widthValue / 2, y);
            context.lineTo(x + widthValue, y + heightValue);
            context.lineTo(x, y + heightValue);
            context.closePath();
            return;
        }
        if (shape === "diamond") {
            context.moveTo(x + widthValue / 2, y);
            context.lineTo(x + widthValue, y + heightValue / 2);
            context.lineTo(x + widthValue / 2, y + heightValue);
            context.lineTo(x, y + heightValue / 2);
            context.closePath();
            return;
        }
        if (shape === "star") {
            traceStarPath(context, x, y, widthValue, heightValue);
            return;
        }
        if (shape === "rectanglebubble" || shape === "ellipsebubble") {
            const tailHeight = Math.min(Math.max(4, heightValue * 0.22), heightValue * 0.35);
            const bodyHeight = Math.max(surface.shapeToolMinimumDragDistance, heightValue - tailHeight);
            if (shape === "ellipsebubble") {
                traceEllipsePath(context, x, y, widthValue, bodyHeight);
            } else {
                context.rect(x, y, widthValue, bodyHeight);
            }
            context.moveTo(x + widthValue * 0.26, y + bodyHeight);
            context.lineTo(x + widthValue * 0.18, y + heightValue);
            context.lineTo(x + widthValue * 0.44, y + bodyHeight);
            context.closePath();
            return;
        }

        context.rect(x, y, widthValue, heightValue);
    }

    function paintShapePreview(context, previewWidth, previewHeight) {
        context.clearRect(0, 0, previewWidth, previewHeight);
        if (!surface.shapeDraggingActive) {
            return;
        }

        const inset = surface.shapeToolStrokeWidth / 2;
        const pathWidth = Math.max(1, previewWidth - surface.shapeToolStrokeWidth);
        const pathHeight = Math.max(1, previewHeight - surface.shapeToolStrokeWidth);
        context.beginPath();
        traceShapePath(context, surface.shapeKind, inset, inset, pathWidth, pathHeight);
        context.lineWidth = surface.shapeToolStrokeWidth;
        context.lineJoin = "round";
        context.lineCap = "round";
        context.strokeStyle = surface.brushColor.toString();
        context.stroke();
    }

    onToolModeChanged: {
        if (toolMode !== "text") {
            commitActiveText();
        }
        if (toolMode !== "shape") {
            cancelActiveShape();
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

        Canvas {
            id: shapePreviewCanvas
            parent: canvasSurface
            visible: surface.shapeDraggingActive
            z: 4
            x: surface.shapePreviewRect().x
            y: surface.shapePreviewRect().y
            width: Math.max(1, surface.shapePreviewRect().width)
            height: Math.max(1, surface.shapePreviewRect().height)
            renderTarget: Canvas.Image
            opacity: 0.9

            onPaint: {
                const context = getContext("2d");
                surface.paintShapePreview(context, width, height);
            }
            onVisibleChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
        }
    }

    MouseArea {
        parent: canvasSurface
        anchors.fill: parent
        z: 3
        hoverEnabled: true
        acceptedButtons: surface.toolMode === "shape" ? Qt.LeftButton : surface.toolMode === "text" ? Qt.LeftButton : Qt.NoButton
        cursorShape: surface.toolMode === "shape" ? Qt.CrossCursor : surface.toolMode === "text" ? Qt.IBeamCursor : surface.toolMode === "eraser" ? Qt.PointingHandCursor : Qt.CrossCursor

        onPressed: function (mouse) {
            if (surface.toolMode === "shape") {
                surface.beginShapeDrag(mouse.x, mouse.y);
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "text") {
                surface.beginTextPlacement(mouse.x, mouse.y);
                mouse.accepted = true;
            }
        }

        onPositionChanged: function (mouse) {
            if (surface.toolMode === "shape" && surface.shapeDraggingActive) {
                surface.updateShapeDrag(mouse.x, mouse.y);
                mouse.accepted = true;
            }
        }

        onReleased: function (mouse) {
            if (surface.toolMode === "shape") {
                surface.updateShapeDrag(mouse.x, mouse.y);
                surface.commitActiveShape();
                mouse.accepted = true;
            }
        }

        onCanceled: surface.cancelActiveShape()

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
