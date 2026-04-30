pragma ComponentBehavior: Bound

import QtQuick
import LVRS 1.0 as LV

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
    property var strokes: []
    property var currentStroke: null
    property bool fullRedrawPending: true
    property bool appendStrokePending: false
    property bool paintScheduled: false
    property int lastPaintedStrokeIndex: -1
    property int lastPaintedPointCount: 0
    property string toolMode: "brush"
    property string backgroundSource: ""
    property var backgroundPlacement: ({
            x: 0,
            y: 0,
            width: 0,
            height: 0
        })
    readonly property int maxUndoSteps: 64
    readonly property var canvasBackend: CanvasBackend
    readonly property var rasterDocumentIO: RasterDocumentIO

    function updateCanvasSizeFromViewport() {
        surface.canvasWidth = Math.max(1, Math.round(surface.width))
        surface.canvasHeight = Math.max(1, Math.round(surface.height))
        surface.updateDocumentProperty("canvasWidth", surface.canvasWidth)
        surface.updateDocumentProperty("canvasHeight", surface.canvasHeight)
    }

    function clearDocumentState() {
        if (!surface.canMutateDocument()) {
            return
        }
        surface.strokes = []
        surface.currentStroke = null
        surface.backgroundSource = ""
        surface.backgroundPlacement = {
            x: 0,
            y: 0,
            width: 0,
            height: 0
        }
        surface.schedulePaint(true)
    }

    function updateDocumentProperty(propertyName, value) {
        if (!documentViewModel) {
            return false
        }
        if (viewId.length && LV.ViewModels) {
            var boundViewModel = LV.ViewModels.getForView(viewId)
            if (boundViewModel) {
                if (!LV.ViewModels.canWrite(viewId)) {
                    console.warn("CanvasDocument is not writable for view:", viewId)
                    return false
                }
                if (!LV.ViewModels.updateProperty(viewId, propertyName, value)) {
                    console.warn(LV.ViewModels.lastError)
                    return false
                }
                return true
            }
        }
        documentViewModel[propertyName] = value
        return true
    }

    function canMutateDocument() {
        if (!documentViewModel) {
            return false
        }
        if (viewId.length && LV.ViewModels) {
            var boundViewModel = LV.ViewModels.getForView(viewId)
            if (boundViewModel && !LV.ViewModels.canWrite(viewId)) {
                console.warn("CanvasDocument is not writable for view:", viewId)
                return false
            }
        }
        return true
    }

    function syncStateFromViewModel() {
        if (!documentViewModel) {
            return
        }
        if (surface.canvasWidth !== documentViewModel.canvasWidth) {
            surface.canvasWidth = documentViewModel.canvasWidth
        }
        if (surface.canvasHeight !== documentViewModel.canvasHeight) {
            surface.canvasHeight = documentViewModel.canvasHeight
        }
        surface.schedulePaint(true)
    }

    function captureBackgroundSnapshot() {
        if (!surface.backgroundSource.length) {
            return ({})
        }
        return {
            source: surface.backgroundSource,
            x: surface.backgroundPlacement.x,
            y: surface.backgroundPlacement.y,
            width: surface.backgroundPlacement.width,
            height: surface.backgroundPlacement.height
        }
    }

    function applyBackgroundSnapshot(snapshot) {
        var nextBackground = snapshot && snapshot.source ? snapshot : ({})
        surface.backgroundSource = nextBackground.source !== undefined ? nextBackground.source : ""
        surface.backgroundPlacement = {
            x: nextBackground.x !== undefined ? nextBackground.x : 0,
            y: nextBackground.y !== undefined ? nextBackground.y : 0,
            width: nextBackground.width !== undefined ? nextBackground.width : 0,
            height: nextBackground.height !== undefined ? nextBackground.height : 0
        }
        if (surface.backgroundSource.length) {
            paintCanvas.loadImage(surface.backgroundSource)
        }
    }

    function captureSnapshot() {
        return surface.canvasBackend.captureSnapshot(
                    surface.canvasWidth,
                    surface.canvasHeight,
                    surface.strokes,
                    surface.captureBackgroundSnapshot())
    }

    function pushUndoState() {
        surface.canvasBackend.pushUndoState(surface.captureSnapshot(), surface.maxUndoSteps)
    }

    function applySnapshot(snapshot) {
        if (!surface.canMutateDocument()) {
            return
        }

        if (snapshot.canvasWidth !== undefined && snapshot.canvasHeight !== undefined) {
            surface.canvasWidth = Math.max(1, Math.round(snapshot.canvasWidth))
            surface.canvasHeight = Math.max(1, Math.round(snapshot.canvasHeight))
            surface.updateDocumentProperty("canvasWidth", surface.canvasWidth)
            surface.updateDocumentProperty("canvasHeight", surface.canvasHeight)
        }

        surface.strokes = snapshot.strokes !== undefined ? snapshot.strokes : []
        surface.currentStroke = null
        surface.applyBackgroundSnapshot(snapshot.background !== undefined ? snapshot.background : {})
        surface.schedulePaint(true)
    }

    function undo() {
        if (!surface.canMutateDocument() || !surface.canvasBackend.canUndo) {
            return
        }

        var snapshot = surface.canvasBackend.undo(surface.captureSnapshot(), surface.maxUndoSteps)
        if (snapshot && Object.keys(snapshot).length) {
            surface.applySnapshot(snapshot)
        }
    }

    function redo() {
        if (!surface.canMutateDocument() || !surface.canvasBackend.canRedo) {
            return
        }

        var snapshot = surface.canvasBackend.redo(surface.captureSnapshot(), surface.maxUndoSteps)
        if (snapshot && Object.keys(snapshot).length) {
            surface.applySnapshot(snapshot)
        }
    }

    Connections {
        target: surface.documentViewModel
        ignoreUnknownSignals: true

        function onCanvasWidthChanged() {
            if (surface.documentViewModel && surface.canvasWidth !== surface.documentViewModel.canvasWidth) {
                surface.canvasWidth = surface.documentViewModel.canvasWidth
                surface.schedulePaint(true)
            }
        }

        function onCanvasHeightChanged() {
            if (surface.documentViewModel && surface.canvasHeight !== surface.documentViewModel.canvasHeight) {
                surface.canvasHeight = surface.documentViewModel.canvasHeight
                surface.schedulePaint(true)
            }
        }
    }

    signal brushDeltaRequested(int delta)
    signal toolShortcutRequested(string tool)

    function schedulePaint(fullRedraw) {
        if (fullRedraw) {
            surface.fullRedrawPending = true
        }
        if (surface.paintScheduled) {
            return
        }
        surface.paintScheduled = true
        Qt.callLater(function() {
            surface.paintScheduled = false
            paintCanvas.requestPaint()
        })
    }

    function currentStrokeColor() {
        if (surface.toolMode === "eraser") {
            return "#000000"
        }
        return typeof surface.brushColor === "string" ? surface.brushColor : surface.brushColor.toString()
    }

    function strokePointSize(point, fallbackSize) {
        if (point && point.size !== undefined) {
            return point.size
        }
        return fallbackSize
    }

    function strokePointOpacity(point) {
        if (point && point.opacity !== undefined) {
            return point.opacity
        }
        if (point && point.pressure !== undefined) {
            return point.pressure
        }
        return 1.0
    }

    function createStrokePoint(pointX, pointY, rawPressure, pressureSensitive) {
        var baseSize = surface.currentStroke ? surface.currentStroke.size : surface.brushSize
        var pointSize = BrushEngine.sampleSize(baseSize, rawPressure, pressureSensitive)
        var pointOpacity = BrushEngine.resolvedOpacity(rawPressure, pressureSensitive)
        if (surface.currentStroke && surface.currentStroke.points.length) {
            var lastPoint = surface.currentStroke.points[surface.currentStroke.points.length - 1]
            var previousSize = surface.strokePointSize(lastPoint, baseSize)
            var previousOpacity = surface.strokePointOpacity(lastPoint)
            pointSize = BrushEngine.smoothedSampleSize(previousSize, pointSize)
            pointOpacity = BrushEngine.smoothedSampleOpacity(previousOpacity, pointOpacity)
        }
        return {
            x: pointX,
            y: pointY,
            pressure: BrushEngine.resolvedPressure(rawPressure, pressureSensitive),
            size: pointSize,
            opacity: pointOpacity
        }
    }

    function beginStroke(pointX, pointY, rawPressure, pressureSensitive) {
        if (surface.toolMode !== "brush" && surface.toolMode !== "eraser") {
            return
        }
        surface.pushUndoState()
        surface.currentStroke = {
            color: surface.currentStrokeColor(),
            size: surface.brushSize,
            points: [surface.createStrokePoint(pointX, pointY, rawPressure, pressureSensitive)],
            erase: surface.toolMode === "eraser",
            pressureSensitive: pressureSensitive
        }
        surface.appendStrokePending = true
        surface.strokes = surface.strokes.concat([surface.currentStroke])
    }

    function appendStrokePoint(pointX, pointY, rawPressure, pressureSensitive) {
        if (!surface.currentStroke) {
            return false
        }
        var points = surface.currentStroke.points
        var nextPoint = surface.createStrokePoint(pointX, pointY, rawPressure, pressureSensitive)
        if (!points.length) {
            points.push(nextPoint)
            return true
        }
        var lastPoint = points[points.length - 1]
        if (!BrushEngine.shouldAppendPoint(
                    lastPoint.x,
                    lastPoint.y,
                    surface.strokePointSize(lastPoint, surface.currentStroke.size),
                    surface.strokePointOpacity(lastPoint),
                    nextPoint.x,
                    nextPoint.y,
                    nextPoint.size,
                    nextPoint.opacity,
                    surface.currentStroke.size)) {
            return false
        }
        points.push(nextPoint)
        return true
    }

    function endStroke(pointX, pointY, rawPressure, pressureSensitive) {
        if (!surface.currentStroke) {
            return
        }
        if (surface.appendStrokePoint(pointX, pointY, rawPressure, pressureSensitive)) {
            surface.schedulePaint(false)
        }
        surface.currentStroke = null
    }

    function drawStamp(ctx, pointX, pointY, diameter, opacity) {
        if (diameter <= 0 || opacity <= 0) {
            return
        }
        ctx.globalAlpha = opacity
        ctx.beginPath()
        ctx.arc(pointX, pointY, diameter / 2, 0, Math.PI * 2)
        ctx.fill()
    }

    function drawStroke(ctx, stroke, startIndex) {
        if (!stroke || !stroke.points || stroke.points.length === 0) {
            return
        }

        var points = stroke.points
        var pointCount = points.length
        var start = startIndex !== undefined ? startIndex : 0
        if (start < 0) {
            start = 0
        }
        if (start >= pointCount) {
            return
        }

        ctx.save()
        ctx.globalCompositeOperation = stroke.erase ? "destination-out" : "source-over"
        ctx.fillStyle = stroke.color

        if (pointCount === 1 && start === 0) {
            var singlePoint = points[0]
            surface.drawStamp(
                        ctx,
                        singlePoint.x,
                        singlePoint.y,
                        surface.strokePointSize(singlePoint, stroke.size),
                        surface.strokePointOpacity(singlePoint))
            ctx.restore()
            return
        }

        if (start === 0) {
            surface.drawStamp(
                        ctx,
                        points[0].x,
                        points[0].y,
                        surface.strokePointSize(points[0], stroke.size),
                        surface.strokePointOpacity(points[0]))
        }

        var beginIndex = Math.max(1, start)
        for (var i = beginIndex; i < pointCount; ++i) {
            var previousPoint = points[i - 1]
            var currentPoint = points[i]
            var previousSize = surface.strokePointSize(previousPoint, stroke.size)
            var currentSize = surface.strokePointSize(currentPoint, stroke.size)
            var previousOpacity = surface.strokePointOpacity(previousPoint)
            var currentOpacity = surface.strokePointOpacity(currentPoint)
            var stamps = BrushEngine.stampCount(
                        previousPoint.x,
                        previousPoint.y,
                        previousSize,
                        currentPoint.x,
                        currentPoint.y,
                        currentSize)
            for (var step = 1; step <= stamps; ++step) {
                var t = step / stamps
                var stampX = previousPoint.x + (currentPoint.x - previousPoint.x) * t
                var stampY = previousPoint.y + (currentPoint.y - previousPoint.y) * t
                var stampSize = previousSize + (currentSize - previousSize) * t
                var stampOpacity = previousOpacity + (currentOpacity - previousOpacity) * t
                surface.drawStamp(ctx, stampX, stampY, stampSize, stampOpacity)
            }
        }

        ctx.restore()
    }

    onDocumentViewModelChanged: {
        surface.canvasBackend.clearHistory()
        surface.syncStateFromViewModel()
    }

    onToolModeChanged: {
        if (surface.currentStroke) {
            surface.currentStroke = null
        }
    }

    Component.onCompleted: {
        surface.canvasBackend.clearHistory()
        surface.syncStateFromViewModel()
        surface.updateCanvasSizeFromViewport()
        Qt.callLater(surface.updateCanvasSizeFromViewport)
        surface.forceActiveFocus()
        surface.schedulePaint(true)
    }

    function newCanvas() {
        if (!surface.canMutateDocument()) {
            return
        }
        surface.pushUndoState()
        surface.updateCanvasSizeFromViewport()
        surface.clearDocumentState()
    }

    function clearCanvas() {
        if (!surface.canMutateDocument()) {
            return
        }
        surface.pushUndoState()
        surface.clearDocumentState()
    }

    function openRaster(fileUrl) {
        if (!surface.canMutateDocument()) {
            return
        }

        var openResult = surface.rasterDocumentIO.loadRasterDocument(fileUrl ? fileUrl.toString() : "")
        if (!openResult.ok) {
            console.warn("Raster open failed:", openResult.error)
            return
        }

        surface.pushUndoState()
        surface.currentStroke = null

        var fit = surface.canvasBackend.documentFitTransform(
                    surface.canvasWidth,
                    surface.canvasHeight,
                    openResult.width !== undefined ? openResult.width : surface.canvasWidth,
                    openResult.height !== undefined ? openResult.height : surface.canvasHeight)

        surface.backgroundSource = openResult.source
        surface.backgroundPlacement = {
            x: fit.offsetX,
            y: fit.offsetY,
            width: (openResult.width !== undefined ? openResult.width : surface.canvasWidth) * fit.scale,
            height: (openResult.height !== undefined ? openResult.height : surface.canvasHeight) * fit.scale
        }
        paintCanvas.loadImage(surface.backgroundSource)
        surface.schedulePaint(true)
    }

    function saveToFile(fileUrl) {
        var path = toLocalPath(fileUrl)
        if (!path) {
            return false
        }

        return canvasContainer.grabToImage(function(result) {
            result.saveToFile(path)
        })
    }

    Rectangle {
        id: canvasContainer
        anchors.centerIn: parent
        width: surface.canvasWidth
        height: surface.canvasHeight
        radius: 6
        color: "white"
        border.color: "#d0d0d0"
        border.width: 1
        clip: true
    }

    Canvas {
        id: paintCanvas
        parent: canvasContainer
        anchors.fill: parent
        renderTarget: Canvas.Image
        z: 10
        enabled: false

        onImageLoaded: surface.schedulePaint(true)

        onPaint: {
            var ctx = getContext("2d")
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (surface.fullRedrawPending) {
                ctx.clearRect(0, 0, width, height)

                if (surface.backgroundSource.length) {
                    if (!paintCanvas.isImageLoaded(surface.backgroundSource)) {
                        paintCanvas.loadImage(surface.backgroundSource)
                        return
                    }

                    ctx.drawImage(surface.backgroundSource,
                                  surface.backgroundPlacement.x,
                                  surface.backgroundPlacement.y,
                                  surface.backgroundPlacement.width,
                                  surface.backgroundPlacement.height)
                }

                for (var redrawIndex = 0; redrawIndex < surface.strokes.length; ++redrawIndex) {
                    surface.drawStroke(ctx, surface.strokes[redrawIndex], 0)
                }

                surface.fullRedrawPending = false
                if (surface.strokes.length > 0) {
                    surface.lastPaintedStrokeIndex = surface.strokes.length - 1
                    var lastStroke = surface.strokes[surface.lastPaintedStrokeIndex]
                    surface.lastPaintedPointCount = lastStroke ? lastStroke.points.length : 0
                } else {
                    surface.lastPaintedStrokeIndex = -1
                    surface.lastPaintedPointCount = 0
                }
                return
            }

            var strokeCount = surface.strokes.length
            if (!strokeCount) {
                return
            }

            if (strokeCount - 1 > surface.lastPaintedStrokeIndex) {
                for (var strokeIndex = surface.lastPaintedStrokeIndex + 1; strokeIndex < strokeCount; ++strokeIndex) {
                    surface.drawStroke(ctx, surface.strokes[strokeIndex], 0)
                }
                surface.lastPaintedStrokeIndex = strokeCount - 1
                var appendedStroke = surface.strokes[surface.lastPaintedStrokeIndex]
                surface.lastPaintedPointCount = appendedStroke ? appendedStroke.points.length : 0
                return
            }

            if (surface.lastPaintedStrokeIndex >= 0) {
                var currentStroke = surface.strokes[surface.lastPaintedStrokeIndex]
                if (currentStroke && currentStroke.points
                        && currentStroke.points.length > surface.lastPaintedPointCount) {
                    surface.drawStroke(ctx, currentStroke, surface.lastPaintedPointCount)
                    surface.lastPaintedPointCount = currentStroke.points.length
                }
            }
        }
    }

    onStrokesChanged: {
        if (surface.appendStrokePending) {
            surface.appendStrokePending = false
            surface.schedulePaint(false)
            return
        }
        surface.schedulePaint(true)
    }

    PointHandler {
        id: stylusStrokeHandler
        parent: canvasContainer
        acceptedPointerTypes: PointerDevice.Pen | PointerDevice.Eraser
        enabled: surface.toolMode === "brush" || surface.toolMode === "eraser"
        target: null

        onActiveChanged: {
            if (active) {
                surface.forceActiveFocus()
                surface.beginStroke(point.position.x, point.position.y, point.pressure, true)
            } else {
                surface.endStroke(point.position.x, point.position.y, point.pressure, true)
            }
        }

        onPointChanged: {
            if (!active) {
                return
            }
            if (!surface.currentStroke) {
                surface.beginStroke(point.position.x, point.position.y, point.pressure, true)
                return
            }
            if (surface.appendStrokePoint(point.position.x, point.position.y, point.pressure, true)) {
                surface.schedulePaint(false)
            }
        }
    }

    PointHandler {
        id: mouseStrokeHandler
        parent: canvasContainer
        acceptedDevices: PointerDevice.Mouse
        acceptedButtons: Qt.LeftButton
        enabled: surface.toolMode === "brush" || surface.toolMode === "eraser"
        target: null

        onActiveChanged: {
            if (active) {
                surface.forceActiveFocus()
                surface.beginStroke(point.position.x, point.position.y, 1.0, false)
            } else {
                surface.endStroke(point.position.x, point.position.y, 1.0, false)
            }
        }

        onPointChanged: {
            if (!active) {
                return
            }
            if (!surface.currentStroke) {
                surface.beginStroke(point.position.x, point.position.y, 1.0, false)
                return
            }
            if (surface.appendStrokePoint(point.position.x, point.position.y, 1.0, false)) {
                surface.schedulePaint(false)
            }
        }
    }

    MouseArea {
        parent: canvasContainer
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
        onActivated: surface.undo()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.Redo
        onActivated: surface.redo()
    }

    function toLocalPath(fileUrl) {
        if (!fileUrl) {
            return ""
        }
        var path = fileUrl.toString()
        if (path.startsWith("file://")) {
            path = decodeURIComponent(path.substring(7))
            if (Qt.platform.os === "windows" && path.startsWith("/")) {
                path = path.substring(1)
            }
            return path
        }
        if (path.indexOf("://") !== -1) {
            return ""
        }
        return path
    }
}
