import QtQuick

Rectangle {
    id: surface
    color: "white"
    radius: 6
    border.color: "#d0d0d0"
    border.width: 1
    focus: true

    property color brushColor: "#1a1a1a"
    property real brushSize: 2
    property var strokes: []
    property var currentStroke: null
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
    readonly property int maxUndoSteps: 64
    property bool transformUndoCaptured: false

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

    function updateSelectedImageItem() {
        var item = surface.imageElementRegistry[surface.selectedImageId]
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

    function unregisterImageElement(imageId) {
        if (surface.imageElementRegistry[imageId]) {
            delete surface.imageElementRegistry[imageId];
            surface.updateSelectedImageItem();
        }
        surface.notifySelectionOverlay();
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

    function pushUndoState() {
        var snapshot = {
            strokes: cloneStrokes(surface.strokes),
            images: cloneImages(),
            selectedImageId: surface.selectedImageId
        };
        surface.undoStack.push(snapshot);
        if (surface.undoStack.length > surface.maxUndoSteps) {
            surface.undoStack.shift();
        }
    }

    function applySnapshot(snapshot) {
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
        paintCanvas.requestPaint();
    }

    function undo() {
        if (!surface.undoStack.length) {
            return;
        }
        var snapshot = surface.undoStack.pop();
        surface.applySnapshot(snapshot);
    }

    function beginTransformUndoCapture() {
        if (!surface.transformUndoCaptured) {
            surface.pushUndoState();
            surface.transformUndoCaptured = true;
        }
    }

    signal brushDeltaRequested(int delta)

    function updateConstrainAspectState() {
        surface.constrainAspect = surface.freeTransformActive && surface.shiftHoldCount > 0
    }

    Component.onCompleted: surface.forceActiveFocus()

    function newCanvas() {
        surface.pushUndoState()
        surface.cancelTextEntry()
        strokes = []
        currentStroke = null
        imageModel.clear()
        surface.selectedImageId = -1
        surface.imageElementRegistry = ({})
        surface.freeTransformActive = false
        surface.freeTransformSnapshot = {}
        paintCanvas.requestPaint()
    }

    function loadImage(fileUrl) {
        var sourceUrl = normalizeUrl(fileUrl)
        if (!sourceUrl) {
            return
        }
        surface.pushUndoState()
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
        paintCanvas.requestPaint()
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
        var grabResult = surface.grabToImage(function(result) {
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
        paintCanvas.requestPaint()
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
                    surface.width / entry.originalWidth,
                    surface.height / entry.originalHeight,
                    1)
        var width = entry.originalWidth * fitScale
        var height = entry.originalHeight * fitScale
        imageModel.setProperty(index, "scaleX", fitScale)
        imageModel.setProperty(index, "scaleY", fitScale)
        imageModel.setProperty(index, "x", (surface.width - width) / 2)
        imageModel.setProperty(index, "y", (surface.height - height) / 2)
        imageModel.setProperty(index, "ready", true)
        surface.freeTransformActive = false
        paintCanvas.requestPaint()
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
        var targetX = posX !== undefined ? posX : (surface.width - width) / 2
        var targetY = posY !== undefined ? posY : (surface.height - height) / 2
        targetX = Math.max(0, Math.min(surface.width - width, targetX))
        targetY = Math.max(0, Math.min(surface.height - height, targetY))
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
            paintCanvas.requestPaint()
        }
        textRasterizer.textValue = textValue
        textRasterizer.requestPaint()
    }

    function startTextEntry(x, y) {
        textInputOverlay.fontPixelSize = Math.max(12, surface.brushSize * 2)
        var width = Math.min(320, surface.width)
        var height = Math.min(140, surface.height)
        textInputOverlay.overlayWidth = width
        textInputOverlay.overlayHeight = height
        textInputOverlay.targetX = Math.max(0, Math.min(surface.width - width, x))
        textInputOverlay.targetY = Math.max(0, Math.min(surface.height - height, y))
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
        paintCanvas.requestPaint()
    }

    function commitFreeTransform() {
        if (!surface.freeTransformActive) {
            return
        }
        surface.freeTransformActive = false
        surface.freeTransformSnapshot = {}
        surface.transformUndoCaptured = false
        surface.updateConstrainAspectState()
        paintCanvas.requestPaint()
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
        paintCanvas.requestPaint()
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

    Canvas {
        id: paintCanvas
        anchors.fill: parent
        renderTarget: Canvas.Image
        z: 10
        enabled: false

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            for (var i = 0; i < surface.strokes.length; ++i) {
                var stroke = surface.strokes[i]
                if (!stroke || stroke.points.length === 0) {
                    continue
                }
                ctx.save()
                if (stroke.erase) {
                    ctx.globalCompositeOperation = "destination-out"
                } else {
                    ctx.globalCompositeOperation = "source-over"
                }

                if (stroke.points.length === 1) {
                    var point = stroke.points[0]
                    ctx.beginPath()
                    ctx.fillStyle = stroke.color
                    ctx.arc(point.x, point.y, stroke.size / 2, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.restore()
                    continue
                }

                ctx.beginPath()
                ctx.strokeStyle = stroke.color
                ctx.lineWidth = stroke.size
                ctx.moveTo(stroke.points[0].x, stroke.points[0].y)
                for (var j = 1; j < stroke.points.length; ++j) {
                    ctx.lineTo(stroke.points[j].x, stroke.points[j].y)
                }
                ctx.stroke()
                ctx.restore()
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
        anchors.fill: parent
        visible: surface.hasImportedImage
        z: 5

        Repeater {
            id: imageRepeater
            model: imageModel

            delegate: Image {
                id: imageDisplay
                property int modelIndex: index
                property int delegateImageId: model.imageId
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

                Component.onCompleted: surface.registerImageElement(delegateImageId, imageDisplay)
                Component.onDestruction: surface.unregisterImageElement(delegateImageId)

                onStatusChanged: {
                    if (status === Image.Ready && !model.ready) {
                        imageModel.setProperty(modelIndex, "originalWidth", implicitWidth)
                        imageModel.setProperty(modelIndex, "originalHeight", implicitHeight)
                        surface.resetImagePlacement(delegateImageId)
                    }
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
                paintCanvas.requestPaint()
            }

            function moveSelection(newX, newY) {
                if (!currentEntry || selectedIndex < 0) {
                    return
                }
                imageModel.setProperty(selectedIndex, "x", newX)
                imageModel.setProperty(selectedIndex, "y", newY)
                paintCanvas.requestPaint()
            }

            function handleTransform(role, dx, dy, startRect) {
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
                var maxWidth = surface.width * 4
                var maxHeight = surface.height * 4

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

                if (surface.constrainAspect && startWidth > 0 && startHeight > 0) {
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

                        property real startX: 0
                        property real startY: 0
                        property real startWidth: 0
                        property real startHeight: 0

                        HoverHandler {
                            cursorShape: modelData.cursor
                        }

                        DragHandler {
                            target: null
                            enabled: surface.freeTransformActive
                            acceptedButtons: Qt.LeftButton
                            onActiveChanged: {
                                if (active) {
                                    surface.beginTransformUndoCapture()
                                    startX = selectionOverlay.x
                                    startY = selectionOverlay.y
                                    startWidth = selectionOverlay.width
                                    startHeight = selectionOverlay.height
                                }
                            }
                            onTranslationChanged: {
                                selectionOverlay.handleTransform(
                                            modelData.role,
                                            translation.x,
                                            translation.y,
                                            { x: startX, y: startY, w: startWidth, h: startHeight })
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

                onActiveChanged: {
                    if (active) {
                        surface.beginTransformUndoCapture()
                        startX = selectionOverlay.selectedItem ? selectionOverlay.selectedItem.x : selectionOverlay.x
                        startY = selectionOverlay.selectedItem ? selectionOverlay.selectedItem.y : selectionOverlay.y
                        if (!surface.freeTransformActive) {
                            surface.startFreeTransform()
                        }
                    } else if (surface.freeTransformActive) {
                        surface.commitFreeTransform()
                    }
                }

                onTranslationChanged: {
                    selectionOverlay.moveSelection(startX + translation.x, startY + translation.y)
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


    onStrokesChanged: paintCanvas.requestPaint()

    MouseArea {
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
            var updated = surface.strokes.slice()
            updated.push(surface.currentStroke)
            surface.strokes = updated
            paintCanvas.requestPaint()
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
            surface.currentStroke.points.push({ x: mouse.x, y: mouse.y })
            paintCanvas.requestPaint()
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
            surface.currentStroke.points.push({ x: mouse.x, y: mouse.y })
            surface.currentStroke = null
            paintCanvas.requestPaint()
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

    Keys.onPressed: {
        if (event.key === Qt.Key_Shift && !event.isAutoRepeat) {
            surface.shiftHoldCount = surface.shiftHoldCount + 1
            surface.updateConstrainAspectState()
            event.accepted = true
            return
        }
        event.accepted = false
    }

    Keys.onReleased: {
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
