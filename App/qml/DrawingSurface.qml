import QtQuick

Rectangle {
    id: surface
    color: "transparent"
    focus: true
    clip: true

    property color brushColor: "#1a1a1a"
    property real brushSize: 2
    property int canvasWidth: 1
    property int canvasHeight: 1
    property var strokes: []
    property var currentStroke: null
    property bool fullRedrawPending: true
    property bool appendStrokePending: false
    property bool paintScheduled: false
    property int lastPaintedStrokeIndex: -1
    property int lastPaintedPointCount: 0
    readonly property real minPointDistance: Math.max(0.5, brushSize * 0.05)
    property string toolMode: "brush"
    ListModel {
        id: imageModel
    }

    property int imageIdCounter: 0
    property int selectedImageId: -1
    readonly property bool hasImportedImage: imageModel.count > 0
    property bool freeTransformActive: false
    property var freeTransformSnapshot: ({})
    property bool constrainAspect: false
    property int shiftHoldCount: 0
    property var imageElementRegistry: ({})
    property var selectedImageItem: null
    property var undoStack: []
    property var redoStack: []
    readonly property int maxUndoSteps: 64
    property bool transformUndoCaptured: false
    readonly property bool textEntryActive: textInputOverlay.visible
    property bool externalDragHasSupportedImage: false

    function updateCanvasSizeFromViewport() {
        surface.canvasWidth = Math.max(1, Math.round(surface.width))
        surface.canvasHeight = Math.max(1, Math.round(surface.height))
    }

    function clearDocumentState() {
        surface.cancelTextEntry()
        surface.strokes = []
        surface.currentStroke = null
        imageModel.clear()
        surface.selectedImageId = -1
        surface.imageElementRegistry = ({})
        surface.freeTransformActive = false
        surface.freeTransformSnapshot = {}
        surface.transformUndoCaptured = false
        surface.updateSelectedImageItem()
        surface.notifySelectionOverlay()
        surface.schedulePaint(true)
    }

    function isSupportedImageUrl(fileUrl) {
        if (!fileUrl) {
            return false
        }
        var urlString = fileUrl.toString().toLowerCase()
        var pathOnly = urlString.split("?")[0].split("#")[0]
        return pathOnly.endsWith(".png")
            || pathOnly.endsWith(".jpg")
            || pathOnly.endsWith(".jpeg")
            || pathOnly.endsWith(".bmp")
            || pathOnly.endsWith(".gif")
            || pathOnly.endsWith(".webp")
            || pathOnly.endsWith(".tif")
            || pathOnly.endsWith(".tiff")
    }

    function extractImageUrlsFromDrop(event) {
        var result = []
        function appendIfSupported(candidate) {
            if (!candidate) {
                return
            }
            var normalized = normalizeUrl(candidate)
            if (!normalized || !surface.isSupportedImageUrl(normalized)) {
                return
            }
            for (var i = 0; i < result.length; ++i) {
                if (result[i] === normalized) {
                    return
                }
            }
            result.push(normalized)
        }

        if (event && event.hasUrls && event.urls) {
            for (var index = 0; index < event.urls.length; ++index) {
                appendIfSupported(event.urls[index].toString())
            }
        }

        if (!result.length && event && event.hasText && event.text) {
            var textLines = event.text.split(/\r?\n/)
            for (var lineIndex = 0; lineIndex < textLines.length; ++lineIndex) {
                var line = textLines[lineIndex].trim()
                if (!line.length || line.startsWith("#")) {
                    continue
                }
                appendIfSupported(line)
            }
        }

        return result
    }

    function importDroppedImages(urls) {
        if (!urls || !urls.length) {
            return false
        }
        surface.pushUndoState()
        for (var i = 0; i < urls.length; ++i) {
            surface.loadImage(urls[i], {
                skipUndo: true
            })
        }
        return true
    }

    function findImageIndexById(imageId) {
        if (imageId === -1) {
            return -1;
        }
        for (var i = 0; i < imageModel.count; ++i) {
            var entry = imageModel.get(i);
            if (entry && entry.imageId === imageId) {
                return i;
            }
        }
        return -1;
    }

    function notifySelectionOverlay() {
        if (selectionOverlay) {
            selectionOverlay.refreshSelectionState();
        }
    }

    function selectedImageIndex() {
        return findImageIndexById(surface.selectedImageId);
    }

    function selectedImageData() {
        var index = surface.selectedImageIndex();
        return index >= 0 ? imageModel.get(index) : null;
    }

    function findImageElementById(imageId) {
        if (imageId === -1 || !imageRepeater || imageRepeater.count <= 0) {
            return null
        }
        for (var i = 0; i < imageRepeater.count; ++i) {
            var item = imageRepeater.itemAt(i)
            if (item && item.delegateImageId === imageId) {
                return item
            }
        }
        return null
    }

    function updateSelectedImageItem() {
        var item = surface.imageElementRegistry[surface.selectedImageId]
        if (item && item.delegateImageId !== surface.selectedImageId) {
            item = null
        }
        if (!item) {
            item = surface.findImageElementById(surface.selectedImageId)
            if (item) {
                surface.imageElementRegistry[surface.selectedImageId] = item
            }
        }
        surface.selectedImageItem = item ? item : null
        surface.notifySelectionOverlay()
    }

    function selectImage(imageId) {
        var idx = findImageIndexById(imageId);
        if (idx === -1) {
            surface.selectedImageId = -1;
            surface.freeTransformActive = false;
            surface.freeTransformSnapshot = {};
            surface.updateSelectedImageItem();
            surface.notifySelectionOverlay();
            return;
        }
        if (idx !== imageModel.count - 1) {
            imageModel.move(idx, imageModel.count - 1, 1);
            idx = imageModel.count - 1;
        }
        surface.selectedImageId = imageId;
        surface.updateSelectedImageItem();
        Qt.callLater(surface.updateSelectedImageItem);
        surface.freeTransformActive = false;
        surface.freeTransformSnapshot = {};
        if (surface.toolMode === "grab") {
            surface.startFreeTransform();
        }
        surface.notifySelectionOverlay();
    }

    function registerImageElement(imageId, element) {
        if (!element) {
            return;
        }
        surface.imageElementRegistry[imageId] = element;
        if (imageId === surface.selectedImageId) {
            surface.updateSelectedImageItem();
        }
        surface.notifySelectionOverlay();
    }

    function unregisterImageElement(imageId, element) {
        if (imageId === undefined || imageId === null) {
            return;
        }
        var current = surface.imageElementRegistry[imageId]
        if (!current) {
            return;
        }
        if (element && current !== element) {
            return;
        }
        delete surface.imageElementRegistry[imageId];
        surface.updateSelectedImageItem();
    }

    function cloneStrokes(src) {
        var result = [];
        for (var i = 0; i < src.length; ++i) {
            var stroke = src[i];
            if (!stroke) {
                continue;
            }
            var clonedStroke = {
                color: stroke.color,
                size: stroke.size,
                erase: stroke.erase === true,
                points: []
            };
            for (var j = 0; j < stroke.points.length; ++j) {
                var pt = stroke.points[j];
                clonedStroke.points.push({ x: pt.x, y: pt.y });
            }
            result.push(clonedStroke);
        }
        return result;
    }

    function cloneImages() {
        var result = [];
        for (var i = 0; i < imageModel.count; ++i) {
            var entry = imageModel.get(i);
            result.push({
                imageId: entry.imageId,
                source: entry.source,
                x: entry.x,
                y: entry.y,
                originalWidth: entry.originalWidth,
                originalHeight: entry.originalHeight,
                scaleX: entry.scaleX,
                scaleY: entry.scaleY,
                ready: entry.ready
            });
        }
        return result;
    }

    function captureSnapshot() {
        return {
            canvasWidth: surface.canvasWidth,
            canvasHeight: surface.canvasHeight,
            strokes: cloneStrokes(surface.strokes),
            images: cloneImages(),
            selectedImageId: surface.selectedImageId
        };
    }

    function pushUndoState() {
        var snapshot = surface.captureSnapshot();
        surface.undoStack.push(snapshot);
        if (surface.undoStack.length > surface.maxUndoSteps) {
            surface.undoStack.shift();
        }
        surface.redoStack = [];
    }

    function applySnapshot(snapshot) {
        if (snapshot.canvasWidth !== undefined && snapshot.canvasHeight !== undefined) {
            surface.canvasWidth = Math.max(1, Math.round(snapshot.canvasWidth))
            surface.canvasHeight = Math.max(1, Math.round(snapshot.canvasHeight))
        }
        var restoredStrokes = [];
        for (var i = 0; i < snapshot.strokes.length; ++i) {
            restoredStrokes.push(snapshot.strokes[i]);
        }
        surface.strokes = restoredStrokes;

        imageModel.clear();
        for (var j = 0; j < snapshot.images.length; ++j) {
            imageModel.append(snapshot.images[j]);
        }

        surface.selectedImageId = snapshot.selectedImageId !== undefined ? snapshot.selectedImageId : -1;
        surface.updateSelectedImageItem();
        surface.freeTransformActive = false;
        surface.freeTransformSnapshot = {};
        surface.transformUndoCaptured = false;
        surface.notifySelectionOverlay();
        surface.cancelTextEntry();
    }

    function undo() {
        if (!surface.undoStack.length) {
            return;
        }
        var currentSnapshot = surface.captureSnapshot();
        var snapshot = surface.undoStack.pop();
        surface.redoStack.push(currentSnapshot);
        if (surface.redoStack.length > surface.maxUndoSteps) {
            surface.redoStack.shift();
        }
        surface.applySnapshot(snapshot);
    }

    function redo() {
        if (!surface.redoStack.length) {
            return;
        }
        var currentSnapshot = surface.captureSnapshot();
        var snapshot = surface.redoStack.pop();
        surface.undoStack.push(currentSnapshot);
        if (surface.undoStack.length > surface.maxUndoSteps) {
            surface.undoStack.shift();
        }
        surface.applySnapshot(snapshot);
    }

    function beginTransformUndoCapture() {
        if (!surface.transformUndoCaptured) {
            surface.pushUndoState();
            surface.transformUndoCaptured = true;
        }
    }

    signal brushDeltaRequested(int delta)
    signal toolShortcutRequested(string tool)
    signal freeTransformShortcutRequested

    function updateConstrainAspectState() {
        surface.constrainAspect = surface.freeTransformActive && surface.shiftHoldCount > 0
    }

    function isShiftModifierActive(modifiers) {
        return (modifiers & Qt.ShiftModifier) !== 0
    }

    function schedulePaint(fullRedraw) {
        if (fullRedraw) {
            surface.fullRedrawPending = true
        }
        if (surface.paintScheduled) {
            return
        }
        surface.paintScheduled = true
        Qt.callLater(function () {
            surface.paintScheduled = false
            paintCanvas.requestPaint()
        })
    }

    function appendStrokePoint(pointX, pointY) {
        if (!surface.currentStroke) {
            return false
        }
        var points = surface.currentStroke.points
        if (!points.length) {
            points.push({ x: pointX, y: pointY })
            return true
        }
        var lastPoint = points[points.length - 1]
        var dx = pointX - lastPoint.x
        var dy = pointY - lastPoint.y
        var minDistance = surface.minPointDistance
        if (dx * dx + dy * dy < minDistance * minDistance) {
            return false
        }
        points.push({ x: pointX, y: pointY })
        return true
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

        ctx.globalCompositeOperation = stroke.erase ? "destination-out" : "source-over"

        if (pointCount === 1 && start === 0) {
            var point = points[0]
            ctx.beginPath()
            ctx.fillStyle = stroke.color
            ctx.arc(point.x, point.y, stroke.size / 2, 0, Math.PI * 2)
            ctx.fill()
            return
        }

        ctx.beginPath()
        ctx.strokeStyle = stroke.color
        ctx.lineWidth = stroke.size
        var moveIndex = start > 0 ? start - 1 : 0
        ctx.moveTo(points[moveIndex].x, points[moveIndex].y)
        for (var i = Math.max(1, start); i < pointCount; ++i) {
            ctx.lineTo(points[i].x, points[i].y)
        }
        ctx.stroke()
    }

    Component.onCompleted: {
        surface.updateCanvasSizeFromViewport()
        Qt.callLater(surface.updateCanvasSizeFromViewport)
        surface.forceActiveFocus()
        surface.schedulePaint(true)
    }

    function newCanvas() {
        surface.pushUndoState()
        surface.updateCanvasSizeFromViewport()
        surface.clearDocumentState()
    }

    function clearCanvas() {
        surface.pushUndoState()
        surface.clearDocumentState()
    }

    function loadImage(fileUrl, options) {
        var sourceUrl = normalizeUrl(fileUrl)
        if (!sourceUrl) {
            return
        }
        var shouldSkipUndo = options && options.skipUndo === true
        if (!shouldSkipUndo) {
            surface.pushUndoState()
        }
        surface.cancelTextEntry()
        currentStroke = null
        var newId = ++surface.imageIdCounter
        imageModel.append({
            imageId: newId,
            source: sourceUrl,
            x: 0,
            y: 0,
            originalWidth: 0,
            originalHeight: 0,
            scaleX: 1.0,
            scaleY: 1.0,
            ready: false
        })
        surface.selectImage(newId)
        surface.freeTransformActive = false
        surface.freeTransformSnapshot = {}
    }

    function saveToFile(fileUrl) {
        var path = toLocalPath(fileUrl)
        if (!path) {
            return false
        }

        var overlayWasVisible = selectionOverlay.visible
        var textOverlayWasVisible = textInputOverlay.visible
        selectionOverlay.visible = false
        textInputOverlay.visible = false
        var grabResult = canvasContainer.grabToImage(function(result) {
            result.saveToFile(path)
            selectionOverlay.visible = overlayWasVisible
            textInputOverlay.visible = textOverlayWasVisible
        })

        if (!grabResult) {
            selectionOverlay.visible = overlayWasVisible
            textInputOverlay.visible = textOverlayWasVisible
        }

        return grabResult
    }

    function clearImportedImage() {
        var index = surface.selectedImageIndex()
        if (index === -1) {
            return
        }
        surface.pushUndoState()
        var entry = imageModel.get(index)
        if (entry) {
            surface.unregisterImageElement(entry.imageId)
        }
        imageModel.remove(index)
        surface.selectedImageId = imageModel.count > 0 ? imageModel.get(imageModel.count - 1).imageId : -1
        surface.updateSelectedImageItem()
        surface.freeTransformActive = false
        surface.freeTransformSnapshot = {}
        if (surface.selectedImageId !== -1 && surface.toolMode === "grab") {
            surface.startFreeTransform()
        }
        surface.notifySelectionOverlay()
    }

    function resetImagePlacement(imageId) {
        var index = surface.findImageIndexById(imageId)
        if (index === -1) {
            return
        }
        var entry = imageModel.get(index)
        if (!entry || entry.originalWidth <= 0 || entry.originalHeight <= 0) {
            return
        }
        const fitScale = Math.min(
                    surface.canvasWidth / entry.originalWidth,
                    surface.canvasHeight / entry.originalHeight,
                    1)
        var width = entry.originalWidth * fitScale
        var height = entry.originalHeight * fitScale
        imageModel.setProperty(index, "scaleX", fitScale)
        imageModel.setProperty(index, "scaleY", fitScale)
        imageModel.setProperty(index, "x", (surface.canvasWidth - width) / 2)
        imageModel.setProperty(index, "y", (surface.canvasHeight - height) / 2)
        imageModel.setProperty(index, "ready", true)
        surface.freeTransformActive = false
        if (surface.toolMode === "grab" && surface.selectedImageId === imageId) {
            surface.startFreeTransform()
        }
        surface.notifySelectionOverlay()
    }

    function insertText(textValue, posX, posY, fontPixelSize) {
        if (!textValue || !textValue.length) {
            return
        }
        surface.pushUndoState()
        var fontPx = fontPixelSize !== undefined ? fontPixelSize : Math.max(12, surface.brushSize * 2)
        textMetrics.font.pixelSize = fontPx
        textMetrics.font.family = textRasterizer.fontFamily
        var lines = textValue.split("\n")
        var maxAdvance = 0
        for (var i = 0; i < lines.length; ++i) {
            var lineText = lines[i].length ? lines[i] : " "
            textMetrics.text = lineText
            maxAdvance = Math.max(maxAdvance, Math.ceil(textMetrics.advanceWidth))
        }
        var lineSpacing = Math.ceil(fontPx * textRasterizer.lineSpacingFactor)
        var width = Math.max(1, maxAdvance + textRasterizer.padding)
        var height = Math.max(1, lineSpacing * lines.length + textRasterizer.padding)
        textRasterizer.width = width
        textRasterizer.height = height
        textRasterizer.fontSize = fontPx
        textRasterizer.textColor = surface.brushColor
        var targetX = posX !== undefined ? posX : (surface.canvasWidth - width) / 2
        var targetY = posY !== undefined ? posY : (surface.canvasHeight - height) / 2
        targetX = Math.max(0, Math.min(surface.canvasWidth - width, targetX))
        targetY = Math.max(0, Math.min(surface.canvasHeight - height, targetY))
        textRasterizer.targetX = targetX
        textRasterizer.targetY = targetY
        textRasterizer.completion = function (dataUrl, renderedWidth, renderedHeight, finalX, finalY) {
            var newId = ++surface.imageIdCounter
            imageModel.append({
                imageId: newId,
                source: dataUrl,
                x: finalX,
                y: finalY,
                originalWidth: renderedWidth,
                originalHeight: renderedHeight,
                scaleX: 1.0,
                scaleY: 1.0,
                ready: true
            })
            surface.selectImage(newId)
        }
        textRasterizer.textValue = textValue
        textRasterizer.requestPaint()
    }

    function startTextEntry(x, y) {
        textInputOverlay.fontPixelSize = Math.max(12, surface.brushSize * 2)
        var width = Math.min(320, surface.canvasWidth)
        var height = Math.min(140, surface.canvasHeight)
        textInputOverlay.overlayWidth = width
        textInputOverlay.overlayHeight = height
        textInputOverlay.targetX = Math.max(0, Math.min(surface.canvasWidth - width, x))
        textInputOverlay.targetY = Math.max(0, Math.min(surface.canvasHeight - height, y))
        textEntryEdit.text = ""
        textInputOverlay.visible = true
        textEntryEdit.forceActiveFocus()
    }

    function commitTextEntry() {
        if (!textInputOverlay.visible) {
            return
        }
        var content = textEntryEdit.text.trim()
        textInputOverlay.visible = false
        if (content.length) {
            surface.insertText(content, textInputOverlay.x, textInputOverlay.y, textInputOverlay.fontPixelSize)
        }
        textEntryEdit.text = ""
    }

    function cancelTextEntry() {
        if (!textInputOverlay.visible) {
            return
        }
        textInputOverlay.visible = false
        textEntryEdit.text = ""
    }

    onToolModeChanged: {
        if (toolMode !== "text" && textInputOverlay.visible) {
            surface.cancelTextEntry()
        }
        if (toolMode === "grab") {
            surface.startFreeTransform()
        } else if (surface.freeTransformActive) {
            surface.commitFreeTransform()
        }
    }

    onFreeTransformActiveChanged: surface.updateConstrainAspectState()

    function startFreeTransform() {
        var index = surface.selectedImageIndex()
        if (index === -1 || surface.freeTransformActive) {
            return
        }
        var entry = imageModel.get(index)
        if (!entry || !entry.ready) {
            return
        }
        surface.freeTransformSnapshot = {
            imageId: entry.imageId,
            x: entry.x,
            y: entry.y,
            scaleX: entry.scaleX,
            scaleY: entry.scaleY
        }
        surface.freeTransformActive = true
        surface.updateConstrainAspectState()
    }

    function commitFreeTransform() {
        if (!surface.freeTransformActive) {
            return
        }
        surface.freeTransformActive = false
        surface.freeTransformSnapshot = {}
        surface.transformUndoCaptured = false
        surface.updateConstrainAspectState()
    }

    function cancelFreeTransform() {
        if (!surface.freeTransformActive) {
            return
        }
        var index = surface.selectedImageIndex()
        if (index === -1) {
            surface.freeTransformActive = false
            surface.freeTransformSnapshot = {}
            surface.updateConstrainAspectState()
            return
        }
        var snapshot = surface.freeTransformSnapshot
        if (snapshot && snapshot.imageId === surface.selectedImageId) {
            if (snapshot.x !== undefined) {
                imageModel.setProperty(index, "x", snapshot.x)
            }
            if (snapshot.y !== undefined) {
                imageModel.setProperty(index, "y", snapshot.y)
            }
            if (snapshot.scaleX !== undefined) {
                imageModel.setProperty(index, "scaleX", snapshot.scaleX)
            }
            if (snapshot.scaleY !== undefined) {
                imageModel.setProperty(index, "scaleY", snapshot.scaleY)
            }
        }
        surface.freeTransformActive = false
        surface.freeTransformSnapshot = {}
        surface.transformUndoCaptured = false
        surface.updateConstrainAspectState()
    }

    function toggleFreeTransformMode() {
        if (!surface.hasImportedImage || surface.selectedImageIndex() === -1) {
            return
        }
        if (!surface.freeTransformActive) {
            surface.startFreeTransform()
        } else {
            surface.commitFreeTransform()
        }
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

    Rectangle {
        parent: canvasContainer
        anchors.fill: parent
        z: 25
        visible: externalDropArea.containsDrag
        color: surface.externalDragHasSupportedImage
            ? Qt.rgba(45 / 255, 137 / 255, 239 / 255, 0.16)
            : Qt.rgba(220 / 255, 60 / 255, 60 / 255, 0.16)
        border.width: 2
        border.color: surface.externalDragHasSupportedImage ? "#2d89ef" : "#dc3c3c"

        Text {
            anchors.centerIn: parent
            text: surface.externalDragHasSupportedImage
                ? qsTr("Drop image to import")
                : qsTr("Unsupported file type")
            color: "#1a1a1a"
            font.pixelSize: 18
            font.bold: true
        }
    }

    DropArea {
        id: externalDropArea
        parent: canvasContainer
        anchors.fill: parent
        z: 30
        onEntered: function (drag) {
            var urls = surface.extractImageUrlsFromDrop(drag)
            surface.externalDragHasSupportedImage = urls.length > 0
            drag.accepted = surface.externalDragHasSupportedImage
        }
        onPositionChanged: function (drag) {
            if (!surface.externalDragHasSupportedImage) {
                var urls = surface.extractImageUrlsFromDrop(drag)
                surface.externalDragHasSupportedImage = urls.length > 0
            }
            drag.accepted = surface.externalDragHasSupportedImage
        }
        onDropped: function (drop) {
            var urls = surface.extractImageUrlsFromDrop(drop)
            var imported = surface.importDroppedImages(urls)
            drop.accepted = imported
            surface.externalDragHasSupportedImage = false
        }
        onExited: {
            surface.externalDragHasSupportedImage = false
        }
    }

    Canvas {
        id: paintCanvas
        parent: canvasContainer
        anchors.fill: parent
        renderTarget: Canvas.Image
        z: 10
        enabled: false

        onPaint: {
            var ctx = getContext("2d")
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (surface.fullRedrawPending) {
                ctx.clearRect(0, 0, width, height)
                for (var i = 0; i < surface.strokes.length; ++i) {
                    surface.drawStroke(ctx, surface.strokes[i], 0)
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
                for (var s = surface.lastPaintedStrokeIndex + 1; s < strokeCount; ++s) {
                    surface.drawStroke(ctx, surface.strokes[s], 0)
                }
                surface.lastPaintedStrokeIndex = strokeCount - 1
                var appendedStroke = surface.strokes[surface.lastPaintedStrokeIndex]
                surface.lastPaintedPointCount = appendedStroke ? appendedStroke.points.length : 0
                return
            }

            if (surface.lastPaintedStrokeIndex >= 0) {
                var stroke = surface.strokes[surface.lastPaintedStrokeIndex]
                if (stroke && stroke.points && stroke.points.length > surface.lastPaintedPointCount) {
                    surface.drawStroke(ctx, stroke, surface.lastPaintedPointCount)
                    surface.lastPaintedPointCount = stroke.points.length
                }
            }
        }
    }

    TextMetrics {
        id: textMetrics
        text: ""
    }

    Canvas {
        id: textRasterizer
        visible: false
        property string textValue: ""
        property color textColor: "#ffffff"
        property string fontFamily: "Helvetica"
        property int fontSize: 32
        property int padding: 16
        property var completion: null
        property real targetX: 0
        property real targetY: 0
        property real lineSpacingFactor: 1.25
        onPaint: {
            var ctx = getContext("2d")
            ctx.save()
            ctx.clearRect(0, 0, width, height)
            if (!textRasterizer.textValue.length) {
                ctx.restore()
                return
            }
            ctx.fillStyle = textColor
            ctx.font = fontSize + "px " + fontFamily
            ctx.textBaseline = "top"
            var lines = textValue.split("\n")
            var lineSpacing = Math.ceil(fontSize * lineSpacingFactor)
            for (var i = 0; i < lines.length; ++i) {
                var lineText = lines[i]
                ctx.fillText(lineText, padding / 2, padding / 2 + i * lineSpacing)
            }
            ctx.restore()
            if (completion) {
                var dataUrl = textRasterizer.toDataURL("image/png")
                var callback = completion
                var finalX = textRasterizer.targetX
                var finalY = textRasterizer.targetY
                completion = null
                callback(dataUrl, width, height, finalX, finalY)
            }
        }
    }

    Item {
        id: imageLayer
        parent: canvasContainer
        anchors.fill: parent
        visible: surface.hasImportedImage
        clip: true
        z: 5

        Repeater {
            id: imageRepeater
            model: imageModel

            delegate: Image {
                id: imageDisplay
                property int delegateImageId: model.imageId
                property int registeredImageId: -1
                x: model.x
                y: model.y
                width: model.originalWidth > 0 ? model.originalWidth * model.scaleX : 0
                height: model.originalHeight > 0 ? model.originalHeight * model.scaleY : 0
                visible: model.ready
                smooth: true
                asynchronous: true
                fillMode: Image.Stretch
                opacity: 1
                source: model.source

                Component.onCompleted: {
                    registeredImageId = delegateImageId
                    surface.registerImageElement(registeredImageId, imageDisplay)
                }

                onDelegateImageIdChanged: {
                    if (registeredImageId === delegateImageId) {
                        return
                    }
                    if (registeredImageId !== -1) {
                        surface.unregisterImageElement(registeredImageId, imageDisplay)
                    }
                    registeredImageId = delegateImageId
                    surface.registerImageElement(registeredImageId, imageDisplay)
                }

                Component.onDestruction: surface.unregisterImageElement(registeredImageId, imageDisplay)

                onStatusChanged: {
                    if (status !== Image.Ready) {
                        return
                    }
                    var modelIndex = surface.findImageIndexById(delegateImageId)
                    if (modelIndex === -1) {
                        return
                    }
                    var entry = imageModel.get(modelIndex)
                    if (!entry || entry.ready) {
                        return
                    }
                    imageModel.setProperty(modelIndex, "originalWidth", implicitWidth)
                    imageModel.setProperty(modelIndex, "originalHeight", implicitHeight)
                    surface.resetImagePlacement(delegateImageId)
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    enabled: surface.toolMode === "grab"
                    onTapped: {
                        surface.selectImage(delegateImageId)
                    }
                }
            }
        }

        Item {
            id: selectionOverlay
            z: 10
            property int selectedIndex: -1
            property var currentEntry: null
            property var selectedItem: null
            property real minSize: 24

            function refreshSelectionState() {
                selectedIndex = surface.findImageIndexById(surface.selectedImageId)
                currentEntry = selectedIndex >= 0 ? imageModel.get(selectedIndex) : null
                selectedItem = surface.selectedImageItem
            }

            Component.onCompleted: refreshSelectionState()

            Connections {
                target: surface
                function onSelectedImageIdChanged() {
                    selectionOverlay.refreshSelectionState()
                }
                function onSelectedImageItemChanged() {
                    selectionOverlay.refreshSelectionState()
                }
            }

            Connections {
                target: imageModel
                function onDataChanged() {
                    selectionOverlay.refreshSelectionState()
                }
                function onCountChanged() {
                    selectionOverlay.refreshSelectionState()
                }
            }

            visible: selectedItem && currentEntry && currentEntry.ready && (surface.toolMode === "grab" || surface.freeTransformActive)
            x: selectedItem ? selectedItem.x : 0
            y: selectedItem ? selectedItem.y : 0
            width: selectedItem ? selectedItem.width : 0
            height: selectedItem ? selectedItem.height : 0

            function updateGeometry(newX, newY, newWidth, newHeight) {
                if (!currentEntry || selectedIndex < 0 || currentEntry.originalWidth <= 0 || currentEntry.originalHeight <= 0) {
                    return
                }
                imageModel.setProperty(selectedIndex, "x", newX)
                imageModel.setProperty(selectedIndex, "y", newY)
                imageModel.setProperty(selectedIndex, "scaleX", newWidth / currentEntry.originalWidth)
                imageModel.setProperty(selectedIndex, "scaleY", newHeight / currentEntry.originalHeight)
            }

            function moveSelection(newX, newY) {
                if (!currentEntry || selectedIndex < 0) {
                    return
                }
                imageModel.setProperty(selectedIndex, "x", newX)
                imageModel.setProperty(selectedIndex, "y", newY)
            }

            function handleTransform(role, dx, dy, startRect, forceConstrainAspect) {
                if (!currentEntry || selectedIndex < 0) {
                    return
                }
                var startLeft = startRect.x
                var startTop = startRect.y
                var startRight = startRect.x + startRect.w
                var startBottom = startRect.y + startRect.h

                var newLeft = startLeft
                var newTop = startTop
                var newRight = startRight
                var newBottom = startBottom

                switch (role) {
                case "topLeft":
                    newLeft = startLeft + dx
                    newTop = startTop + dy
                    break
                case "top":
                    newTop = startTop + dy
                    break
                case "topRight":
                    newRight = startRight + dx
                    newTop = startTop + dy
                    break
                case "right":
                    newRight = startRight + dx
                    break
                case "bottomRight":
                    newRight = startRight + dx
                    newBottom = startBottom + dy
                    break
                case "bottom":
                    newBottom = startBottom + dy
                    break
                case "bottomLeft":
                    newLeft = startLeft + dx
                    newBottom = startBottom + dy
                    break
                case "left":
                    newLeft = startLeft + dx
                    break
                default:
                    break
                }

                var minWidth = Math.max(minSize, 8)
                var minHeight = Math.max(minSize, 8)
                var maxWidth = surface.canvasWidth * 4
                var maxHeight = surface.canvasHeight * 4

                var width = newRight - newLeft
                if (width < minWidth) {
                    if (role === "left" || role === "topLeft" || role === "bottomLeft") {
                        newLeft = newRight - minWidth
                    } else {
                        newRight = newLeft + minWidth
                    }
                } else if (width > maxWidth) {
                    if (role === "left" || role === "topLeft" || role === "bottomLeft") {
                        newLeft = newRight - maxWidth
                    } else {
                        newRight = newLeft + maxWidth
                    }
                }

                var height = newBottom - newTop
                if (height < minHeight) {
                    if (role === "top" || role === "topLeft" || role === "topRight") {
                        newTop = newBottom - minHeight
                    } else {
                        newBottom = newTop + minHeight
                    }
                } else if (height > maxHeight) {
                    if (role === "top" || role === "topLeft" || role === "topRight") {
                        newTop = newBottom - maxHeight
                    } else {
                        newBottom = newTop + maxHeight
                    }
                }

                var startWidth = startRect.w
                var startHeight = startRect.h

                var constrainAspectNow = forceConstrainAspect === undefined
                    ? surface.constrainAspect
                    : forceConstrainAspect

                if (constrainAspectNow && startWidth > 0 && startHeight > 0) {
                    var centerX = startLeft + startWidth / 2
                    var centerY = startTop + startHeight / 2
                    var minScale = Math.max(minWidth / startWidth, minHeight / startHeight)
                    var maxScale = Math.min(maxWidth / startWidth, maxHeight / startHeight)
                    var scaleCandidate = 1

                    if (role === "left" || role === "right") {
                        scaleCandidate = (newRight - newLeft) / startWidth
                    } else if (role === "top" || role === "bottom") {
                        scaleCandidate = (newBottom - newTop) / startHeight
                    } else {
                        var scaleX = (newRight - newLeft) / startWidth
                        var scaleY = (newBottom - newTop) / startHeight
                        scaleCandidate = Math.max(Math.abs(scaleX), Math.abs(scaleY))
                    }

                    scaleCandidate = Math.max(minScale, Math.min(maxScale, Math.abs(scaleCandidate)))
                    var constrainedWidth = startWidth * scaleCandidate
                    var constrainedHeight = startHeight * scaleCandidate

                    switch (role) {
                    case "topLeft":
                        newRight = startRight
                        newBottom = startBottom
                        newLeft = newRight - constrainedWidth
                        newTop = newBottom - constrainedHeight
                        break
                    case "topRight":
                        newLeft = startLeft
                        newBottom = startBottom
                        newRight = newLeft + constrainedWidth
                        newTop = newBottom - constrainedHeight
                        break
                    case "bottomRight":
                        newLeft = startLeft
                        newTop = startTop
                        newRight = newLeft + constrainedWidth
                        newBottom = newTop + constrainedHeight
                        break
                    case "bottomLeft":
                        newRight = startRight
                        newTop = startTop
                        newLeft = newRight - constrainedWidth
                        newBottom = newTop + constrainedHeight
                        break
                    case "left":
                        newRight = startRight
                        newLeft = newRight - constrainedWidth
                        newTop = centerY - constrainedHeight / 2
                        newBottom = centerY + constrainedHeight / 2
                        break
                    case "right":
                        newLeft = startLeft
                        newRight = newLeft + constrainedWidth
                        newTop = centerY - constrainedHeight / 2
                        newBottom = centerY + constrainedHeight / 2
                        break
                    case "top":
                        newBottom = startBottom
                        newTop = newBottom - constrainedHeight
                        newLeft = centerX - constrainedWidth / 2
                        newRight = centerX + constrainedWidth / 2
                        break
                    case "bottom":
                        newTop = startTop
                        newBottom = newTop + constrainedHeight
                        newLeft = centerX - constrainedWidth / 2
                        newRight = centerX + constrainedWidth / 2
                        break
                    default:
                        break
                    }
                }

                var finalWidth = newRight - newLeft
                var finalHeight = newBottom - newTop
                updateGeometry(newLeft, newTop, finalWidth, finalHeight)
            }

            Rectangle {
                anchors.fill: parent
                visible: selectionOverlay.visible
                color: "transparent"
                border.color: surface.freeTransformActive ? Qt.rgba(88 / 255, 161 / 255, 234 / 255, 0.9) : Qt.rgba(255, 255, 255, 0.35)
                border.width: surface.freeTransformActive ? 2 : 1
            }

            HoverHandler {
                acceptedDevices: PointerDevice.Mouse
                cursorShape: surface.toolMode === "grab"
                    ? (selectionDrag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
                    : Qt.ArrowCursor
            }

            Item {
                id: overlayHandles
                anchors.fill: parent
                visible: surface.freeTransformActive
                readonly property real handleSize: 12

                Repeater {
                    model: [
                        { role: "topLeft", xFactor: 0, yFactor: 0, cursor: Qt.SizeFDiagCursor },
                        { role: "top", xFactor: 0.5, yFactor: 0, cursor: Qt.SizeVerCursor },
                        { role: "topRight", xFactor: 1, yFactor: 0, cursor: Qt.SizeBDiagCursor },
                        { role: "right", xFactor: 1, yFactor: 0.5, cursor: Qt.SizeHorCursor },
                        { role: "bottomRight", xFactor: 1, yFactor: 1, cursor: Qt.SizeFDiagCursor },
                        { role: "bottom", xFactor: 0.5, yFactor: 1, cursor: Qt.SizeVerCursor },
                        { role: "bottomLeft", xFactor: 0, yFactor: 1, cursor: Qt.SizeBDiagCursor },
                        { role: "left", xFactor: 0, yFactor: 0.5, cursor: Qt.SizeHorCursor }
                    ]

                    delegate: Rectangle {
                        width: overlayHandles.handleSize
                        height: overlayHandles.handleSize
                        radius: 2
                        color: "#ffffff"
                        border.color: "#1a1a1a"
                        antialiasing: true
                        visible: surface.freeTransformActive
                        x: modelData.xFactor * parent.width - width / 2
                        y: modelData.yFactor * parent.height - height / 2

                        HoverHandler {
                            cursorShape: modelData.cursor
                        }

                        DragHandler {
                            target: null
                            enabled: surface.freeTransformActive
                            acceptedButtons: Qt.LeftButton
                            property real startX: 0
                            property real startY: 0
                            property real startWidth: 0
                            property real startHeight: 0
                            property real startPointerSceneX: 0
                            property real startPointerSceneY: 0
                            onActiveChanged: {
                                if (active) {
                                    surface.forceActiveFocus()
                                    surface.beginTransformUndoCapture()
                                    startX = selectionOverlay.x
                                    startY = selectionOverlay.y
                                    startWidth = selectionOverlay.width
                                    startHeight = selectionOverlay.height
                                    startPointerSceneX = centroid.scenePosition.x
                                    startPointerSceneY = centroid.scenePosition.y
                                }
                            }
                            onTranslationChanged: {
                                var deltaX = centroid.scenePosition.x - startPointerSceneX
                                var deltaY = centroid.scenePosition.y - startPointerSceneY
                                var keepAspect = surface.isShiftModifierActive(centroid.modifiers) || surface.constrainAspect
                                selectionOverlay.handleTransform(
                                            modelData.role,
                                            deltaX,
                                            deltaY,
                                            { x: startX, y: startY, w: startWidth, h: startHeight },
                                            keepAspect)
                            }
                        }
                    }
                }
            }

            DragHandler {
                id: selectionDrag
                target: null
                enabled: selectionOverlay.visible && surface.toolMode === "grab"
                acceptedButtons: Qt.LeftButton
                property real startX: 0
                property real startY: 0
                property real startPointerSceneX: 0
                property real startPointerSceneY: 0

                onActiveChanged: {
                    if (active) {
                        surface.forceActiveFocus()
                        surface.beginTransformUndoCapture()
                        startX = selectionOverlay.selectedItem ? selectionOverlay.selectedItem.x : selectionOverlay.x
                        startY = selectionOverlay.selectedItem ? selectionOverlay.selectedItem.y : selectionOverlay.y
                        startPointerSceneX = centroid.scenePosition.x
                        startPointerSceneY = centroid.scenePosition.y
                        if (!surface.freeTransformActive) {
                            surface.startFreeTransform()
                        }
                    } else if (surface.freeTransformActive) {
                        surface.commitFreeTransform()
                    }
                }

                onTranslationChanged: {
                    var deltaX = centroid.scenePosition.x - startPointerSceneX
                    var deltaY = centroid.scenePosition.y - startPointerSceneY
                    selectionOverlay.moveSelection(startX + deltaX, startY + deltaY)
                }
            }

            WheelHandler {
                acceptedModifiers: Qt.ControlModifier
                enabled: selectionOverlay.visible
                onWheel: {
                    if (!selectionOverlay.currentEntry || selectionOverlay.currentEntry.originalWidth <= 0) {
                        return
                    }
                    if (!surface.freeTransformActive) {
                        surface.startFreeTransform()
                    }
                    surface.beginTransformUndoCapture()
                    const factor = wheel.angleDelta.y > 0 ? 1.1 : 0.9
                    const newWidth = selectionOverlay.width * factor
                    const newHeight = selectionOverlay.height * factor
                    const centerX = selectionOverlay.x + selectionOverlay.width / 2
                    const centerY = selectionOverlay.y + selectionOverlay.height / 2
                    selectionOverlay.updateGeometry(centerX - newWidth / 2, centerY - newHeight / 2, newWidth, newHeight)
                }
            }
        }
    }

    Item {
        id: textInputOverlay
        parent: canvasContainer
        property real targetX: 0
        property real targetY: 0
        property real fontPixelSize: 24
        property real overlayWidth: 320
        property real overlayHeight: 140
        z: 15
        visible: false
        x: targetX
        y: targetY
        width: overlayWidth
        height: overlayHeight

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.7)
            border.color: Qt.rgba(255, 255, 255, 0.4)
            border.width: 1
            radius: 6
        }

        Text {
            id: textEntryHint
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 8
            text: qsTr("Press Enter to place text (Shift+Enter for newline)")
            color: Qt.rgba(1, 1, 1, 0.7)
            wrapMode: Text.WordWrap
            font.pixelSize: 12
        }

        TextEdit {
            id: textEntryEdit
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: textEntryHint.bottom
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.bottomMargin: 8
            anchors.topMargin: 4
            wrapMode: TextEdit.NoWrap
            color: surface.brushColor
            font.pixelSize: textInputOverlay.fontPixelSize
            focus: textInputOverlay.visible
            cursorVisible: textInputOverlay.visible
            Keys.onPressed: function (event) {
                if (!textInputOverlay.visible) {
                    return
                }
                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier)) {
                    event.accepted = true
                    surface.commitTextEntry()
                } else if (event.key === Qt.Key_Escape) {
                    event.accepted = true
                    surface.cancelTextEntry()
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

    MouseArea {
        parent: canvasContainer
        anchors.fill: parent
        z: 3
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        visible: surface.toolMode !== "grab"
        enabled: visible
        cursorShape: surface.toolMode === "eraser"
            ? Qt.PointingHandCursor
            : (surface.toolMode === "text" ? Qt.IBeamCursor : Qt.CrossCursor)

        onPressed: function(mouse) {
            if (surface.toolMode === "grab") {
                mouse.accepted = false
                return
            }
            if (surface.toolMode === "text") {
                if (mouse.button === Qt.LeftButton) {
                    if (textInputOverlay.visible && textEntryEdit.text.length) {
                        surface.commitTextEntry()
                    }
                    surface.startTextEntry(mouse.x, mouse.y)
                    mouse.accepted = true
                } else {
                    mouse.accepted = false
                }
                return
            }
            if (mouse.button !== Qt.LeftButton) {
                mouse.accepted = false
                return
            }
            var colorValue
            surface.pushUndoState()
            var isEraser = surface.toolMode === "eraser"
            if (isEraser) {
                colorValue = "#000000"
            } else {
                colorValue = typeof surface.brushColor === "string" ? surface.brushColor : surface.brushColor.toString()
            }
            surface.currentStroke = {
                color: colorValue,
                size: surface.brushSize,
                points: [ { x: mouse.x, y: mouse.y } ],
                erase: isEraser
            }
            surface.appendStrokePending = true
            surface.strokes = surface.strokes.concat([surface.currentStroke])
        }

        onPositionChanged: function(mouse) {
            if (surface.toolMode === "grab") {
                mouse.accepted = false
                return
            }
            if (surface.toolMode === "text") {
                mouse.accepted = false
                return
            }
            if (!surface.currentStroke) {
                return
            }
            if (surface.appendStrokePoint(mouse.x, mouse.y)) {
                surface.schedulePaint(false)
            }
        }

        onReleased: function(mouse) {
            if (surface.toolMode === "grab") {
                mouse.accepted = false
                return
            }
            if (surface.toolMode === "text") {
                mouse.accepted = false
                return
            }
            if (mouse.button !== Qt.LeftButton) {
                mouse.accepted = false
                return
            }
            if (!surface.currentStroke) {
                return
            }
            if (surface.appendStrokePoint(mouse.x, mouse.y)) {
                surface.schedulePaint(false)
            }
            surface.currentStroke = null
        }

        onCanceled: surface.currentStroke = null

        onWheel: function(wheel) {
            if (wheel.modifiers === Qt.ControlModifier) {
                wheel.accepted = false
                return
            }
            surface.brushDeltaRequested(wheel.angleDelta.y > 0 ? 1 : -1)
        }
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Shift && !event.isAutoRepeat) {
            surface.shiftHoldCount = surface.shiftHoldCount + 1
            surface.updateConstrainAspectState()
            event.accepted = true
            return
        }
        event.accepted = false
    }

    Keys.onReleased: function (event) {
        if (event.key === Qt.Key_Shift && !event.isAutoRepeat) {
            surface.shiftHoldCount = Math.max(0, surface.shiftHoldCount - 1)
            surface.updateConstrainAspectState()
            event.accepted = true
            return
        }
        event.accepted = false
    }

    onActiveFocusChanged: {
        if (!activeFocus && surface.shiftHoldCount > 0) {
            surface.shiftHoldCount = 0
            surface.updateConstrainAspectState()
        }
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: !textInputOverlay.visible
        sequences: [ "B", "ㅠ" ]
        onActivated: surface.toolShortcutRequested("brush")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: !textInputOverlay.visible
        sequences: [ "E", "ㄷ" ]
        onActivated: surface.toolShortcutRequested("eraser")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: !textInputOverlay.visible
        sequences: [ "V", "ㅍ" ]
        onActivated: surface.toolShortcutRequested("grab")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: !textInputOverlay.visible
        sequences: [ "T", "ㅅ" ]
        onActivated: surface.toolShortcutRequested("text")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: !textInputOverlay.visible
        sequences: [ "[" ]
        onActivated: surface.brushDeltaRequested(-1)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: !textInputOverlay.visible
        sequences: [ "]" ]
        onActivated: surface.brushDeltaRequested(1)
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: !textInputOverlay.visible
        sequences: [
            Qt.platform.os === "osx" ? "Meta+T" : "Ctrl+T",
            Qt.platform.os === "osx" ? "Meta+ㅅ" : "Ctrl+ㅅ"
        ]
        onActivated: surface.freeTransformShortcutRequested()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: surface.freeTransformActive
        sequences: [ "Return", "Enter" ]
        onActivated: surface.commitFreeTransform()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: surface.freeTransformActive
        sequences: [ "Esc" ]
        onActivated: surface.cancelFreeTransform()
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

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: surface.hasImportedImage
        sequence: StandardKey.Delete
        onActivated: surface.clearImportedImage()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: surface.hasImportedImage
        sequence: "Backspace"
        onActivated: surface.clearImportedImage()
    }

    function normalizeUrl(fileUrl) {
        if (!fileUrl) {
            return ""
        }
        var url = fileUrl.toString()
        if (url.startsWith("file://")) {
            return url
        }
        if (url.indexOf("://") !== -1) {
            return url
        }
        if (url.startsWith("/")) {
            return "file://" + url
        }
        return Qt.resolvedUrl(url)
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
