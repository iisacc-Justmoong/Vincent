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
    readonly property var imageModel: documentViewModel ? documentViewModel.layerListModel : null
    readonly property int importedLayerCount: documentViewModel
        ? documentViewModel.layerCount
        : (imageModel ? imageModel.count : 0)
    property int selectedImageId: -1
    readonly property bool hasImportedImage: importedLayerCount > 0
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
    readonly property int layerPanelWidth: 220
    readonly property int layerPanelTopMargin: 96

    function updateCanvasSizeFromViewport() {
        surface.canvasWidth = Math.max(1, Math.round(surface.width))
        surface.canvasHeight = Math.max(1, Math.round(surface.height))
        surface.updateDocumentProperty("canvasWidth", surface.canvasWidth)
        surface.updateDocumentProperty("canvasHeight", surface.canvasHeight)
    }

    function clearDocumentState() {
        surface.cancelTextEntry()
        surface.strokes = []
        surface.currentStroke = null
        if (documentViewModel) {
            documentViewModel.resetDocument()
        } else if (imageModel) {
            imageModel.clear()
        }
        surface.selectedImageId = documentViewModel ? documentViewModel.selectedLayerId : -1
        surface.imageElementRegistry = ({})
        surface.freeTransformActive = false
        surface.freeTransformSnapshot = {}
        surface.transformUndoCaptured = false
        surface.updateSelectedImageItem()
        surface.notifySelectionOverlay()
        surface.schedulePaint(true)
    }

    function updateDocumentProperty(propertyName, value) {
        if (!documentViewModel) {
            return false
        }
        if (viewId.length && LV.ViewModels && LV.ViewModels.updateProperty(viewId, propertyName, value)) {
            return true
        }
        documentViewModel[propertyName] = value
        return true
    }

    function updateLayerById(imageId, changes) {
        if (!documentViewModel || imageId === undefined || imageId === null || imageId === -1 || !changes) {
            return false
        }
        return documentViewModel.updateLayerById(imageId, changes)
    }

    function isSupportedImageUrl(fileUrl) {
        return !!fileUrl && ImageImport.supportsImageFile(fileUrl.toString())
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
        if (imageId === -1 || !imageModel) {
            return -1;
        }
        return imageModel.indexOfImageId(imageId)
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
        if (documentViewModel && documentViewModel.selectedLayerId === surface.selectedImageId) {
            var selectedLayerData = documentViewModel.selectedLayerData
            if (selectedLayerData) {
                return surface.cloneMetadata(selectedLayerData)
            }
        }
        var index = surface.selectedImageIndex();
        if (index < 0) {
            return null;
        }
        var entry = imageModel.get(index);
        if (!entry) {
            return null;
        }
        var result = {};
        for (var key in entry) {
            result[key] = entry[key];
        }
        return result;
    }

    function defaultImportedLayerName(metadata, fallbackIndex) {
        if (metadata && metadata.psdLayer && metadata.psdLayer.name) {
            return metadata.psdLayer.name
        }
        if (metadata && metadata.fileName) {
            return metadata.fileName
        }
        return qsTr("Layer %1").arg(fallbackIndex)
    }

    function documentFitTransform(documentWidth, documentHeight) {
        var safeWidth = Math.max(1, documentWidth !== undefined ? documentWidth : surface.canvasWidth)
        var safeHeight = Math.max(1, documentHeight !== undefined ? documentHeight : surface.canvasHeight)
        var scale = Math.min(surface.canvasWidth / safeWidth, surface.canvasHeight / safeHeight, 1)
        return {
            scale: scale,
            offsetX: (surface.canvasWidth - safeWidth * scale) / 2,
            offsetY: (surface.canvasHeight - safeHeight * scale) / 2
        }
    }

    function appendImportedImageEntry(entryData) {
        if (!documentViewModel) {
            return -1
        }
        return documentViewModel.appendLayer({
            source: entryData.source !== undefined ? entryData.source : "",
            x: entryData.x !== undefined ? entryData.x : 0,
            y: entryData.y !== undefined ? entryData.y : 0,
            originalWidth: entryData.originalWidth !== undefined ? entryData.originalWidth : 0,
            originalHeight: entryData.originalHeight !== undefined ? entryData.originalHeight : 0,
            scaleX: entryData.scaleX !== undefined ? entryData.scaleX : 1.0,
            scaleY: entryData.scaleY !== undefined ? entryData.scaleY : 1.0,
            ready: entryData.ready === true,
            layerName: entryData.layerName !== undefined
                ? entryData.layerName
                : surface.defaultImportedLayerName(entryData.importMetadata, surface.importedLayerCount + 1),
            layerVisible: entryData.layerVisible !== false,
            layerOpacity: entryData.layerOpacity !== undefined ? entryData.layerOpacity : 1.0,
            blendModeKey: entryData.blendModeKey !== undefined ? entryData.blendModeKey : "",
            importMetadata: entryData.importMetadata !== undefined ? entryData.importMetadata : ({})
        })
    }

    function setLayerVisibility(imageId, layerVisible) {
        if (!documentViewModel) {
            return
        }
        documentViewModel.setLayerVisibility(imageId, layerVisible)
        surface.notifySelectionOverlay()
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
            surface.updateDocumentProperty("selectedLayerId", -1)
            surface.freeTransformActive = false;
            surface.freeTransformSnapshot = {};
            surface.updateSelectedImageItem();
            surface.notifySelectionOverlay();
            return;
        }
        surface.selectedImageId = imageId;
        surface.updateDocumentProperty("selectedLayerId", imageId)
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
                pressureSensitive: stroke.pressureSensitive === true,
                points: []
            };
            for (var j = 0; j < stroke.points.length; ++j) {
                var pt = stroke.points[j];
                clonedStroke.points.push({
                    x: pt.x,
                    y: pt.y,
                    pressure: pt.pressure !== undefined ? pt.pressure : 1.0,
                    size: pt.size !== undefined ? pt.size : stroke.size,
                    opacity: pt.opacity !== undefined ? pt.opacity : (pt.pressure !== undefined ? pt.pressure : 1.0)
                });
            }
            result.push(clonedStroke);
        }
        return result;
    }

    function cloneImages() {
        if (!imageModel) {
            return [];
        }
        var result = [];
        for (var i = 0; i < surface.importedLayerCount; ++i) {
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
                ready: entry.ready,
                layerName: entry.layerName,
                layerVisible: entry.layerVisible,
                layerOpacity: entry.layerOpacity,
                blendModeKey: entry.blendModeKey,
                importMetadata: entry.importMetadata !== undefined ? entry.importMetadata : ({})
            });
        }
        return result;
    }

    function cloneMetadata(value) {
        if (value === undefined || value === null) {
            return ({})
        }
        return JSON.parse(JSON.stringify(value))
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
            surface.updateDocumentProperty("canvasWidth", surface.canvasWidth)
            surface.updateDocumentProperty("canvasHeight", surface.canvasHeight)
        }
        var restoredStrokes = [];
        for (var i = 0; i < snapshot.strokes.length; ++i) {
            restoredStrokes.push(snapshot.strokes[i]);
        }
        surface.strokes = restoredStrokes;

        if (documentViewModel) {
            documentViewModel.importLayers(snapshot.images !== undefined ? snapshot.images : [])
        } else if (imageModel) {
            imageModel.clear()
            for (var j = 0; j < snapshot.images.length; ++j) {
                imageModel.append(snapshot.images[j]);
            }
        }

        surface.selectedImageId = snapshot.selectedImageId !== undefined ? snapshot.selectedImageId : -1;
        surface.updateDocumentProperty("selectedLayerId", surface.selectedImageId)
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

    Connections {
        target: surface.documentViewModel
        ignoreUnknownSignals: true

        function onSelectedLayerIdChanged() {
            surface.selectedImageId = surface.documentViewModel ? surface.documentViewModel.selectedLayerId : -1
            surface.updateSelectedImageItem()
        }

        function onCanvasWidthChanged() {
            if (surface.documentViewModel && surface.canvasWidth !== surface.documentViewModel.canvasWidth) {
                surface.canvasWidth = surface.documentViewModel.canvasWidth
            }
        }

        function onCanvasHeightChanged() {
            if (surface.documentViewModel && surface.canvasHeight !== surface.documentViewModel.canvasHeight) {
                surface.canvasHeight = surface.documentViewModel.canvasHeight
            }
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
            var point = points[0]
            surface.drawStamp(
                        ctx,
                        point.x,
                        point.y,
                        surface.strokePointSize(point, stroke.size),
                        surface.strokePointOpacity(point))
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

    Component.onCompleted: {
        surface.selectedImageId = documentViewModel ? documentViewModel.selectedLayerId : -1
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
        var preparedImport = ImageImport.prepareImageImport(fileUrl ? fileUrl.toString() : "")
        if (!preparedImport.ok) {
            console.warn("Image import failed:", preparedImport.error)
            return
        }
        var sourceUrl = preparedImport.source
        var importMetadata = preparedImport.metadata !== undefined ? preparedImport.metadata : ({})
        var importedLayers = preparedImport.layers !== undefined ? preparedImport.layers : []
        var shouldSkipUndo = options && options.skipUndo === true
        if (!shouldSkipUndo) {
            surface.pushUndoState()
        }
        surface.cancelTextEntry()
        currentStroke = null

        if (importedLayers.length) {
            var psdMetadata = importMetadata.psd !== undefined ? importMetadata.psd : ({})
            var fit = surface.documentFitTransform(psdMetadata.width, psdMetadata.height)
            var selectedLayerId = -1
            for (var layerIndex = importedLayers.length - 1; layerIndex >= 0; --layerIndex) {
                var layer = importedLayers[layerIndex]
                var layerWidth = layer.width !== undefined ? layer.width : 0
                var layerHeight = layer.height !== undefined ? layer.height : 0
                var layerMetadata = layer.metadata !== undefined ? layer.metadata : importMetadata
                selectedLayerId = surface.appendImportedImageEntry({
                    source: layer.source,
                    x: fit.offsetX + (layer.x !== undefined ? layer.x : 0) * fit.scale,
                    y: fit.offsetY + (layer.y !== undefined ? layer.y : 0) * fit.scale,
                    originalWidth: layerWidth,
                    originalHeight: layerHeight,
                    scaleX: fit.scale,
                    scaleY: fit.scale,
                    ready: layerWidth > 0 && layerHeight > 0,
                    layerName: layer.name !== undefined ? layer.name : surface.defaultImportedLayerName(layerMetadata, layerIndex + 1),
                    layerVisible: layer.visible !== false,
                    layerOpacity: layer.opacity !== undefined ? layer.opacity : 1.0,
                    blendModeKey: layer.blendModeKey !== undefined ? layer.blendModeKey : "",
                    importMetadata: layerMetadata
                })
            }
            if (selectedLayerId !== -1) {
                surface.selectImage(selectedLayerId)
            }
            surface.freeTransformActive = false
            surface.freeTransformSnapshot = {}
            return
        }

        var newId = surface.appendImportedImageEntry({
            source: sourceUrl,
            x: 0,
            y: 0,
            originalWidth: 0,
            originalHeight: 0,
            scaleX: 1.0,
            scaleY: 1.0,
            ready: false,
            layerName: surface.defaultImportedLayerName(importMetadata, surface.importedLayerCount + 1),
            layerVisible: true,
            layerOpacity: 1.0,
            importMetadata: importMetadata
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
            if (documentViewModel) {
                documentViewModel.removeLayerById(entry.imageId)
            } else {
                imageModel.remove(index)
            }
        }
        surface.selectedImageId = documentViewModel
            ? documentViewModel.selectedLayerId
            : (surface.importedLayerCount > 0 ? imageModel.get(surface.importedLayerCount - 1).imageId : -1)
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
        if (!surface.updateLayerById(imageId, {
                scaleX: fitScale,
                scaleY: fitScale,
                x: (surface.canvasWidth - width) / 2,
                y: (surface.canvasHeight - height) / 2,
                ready: true
            })) {
            imageModel.setProperty(index, "scaleX", fitScale)
            imageModel.setProperty(index, "scaleY", fitScale)
            imageModel.setProperty(index, "x", (surface.canvasWidth - width) / 2)
            imageModel.setProperty(index, "y", (surface.canvasHeight - height) / 2)
            imageModel.setProperty(index, "ready", true)
        }
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
            var newId = surface.appendImportedImageEntry({
                source: dataUrl,
                x: finalX,
                y: finalY,
                originalWidth: renderedWidth,
                originalHeight: renderedHeight,
                scaleX: 1.0,
                scaleY: 1.0,
                ready: true,
                layerName: qsTr("Text"),
                layerVisible: true,
                layerOpacity: 1.0
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
        if (surface.currentStroke) {
            surface.currentStroke = null
        }
        if (toolMode !== "text" && textInputOverlay.visible) {
            surface.cancelTextEntry()
        }
        if (toolMode === "grab") {
            surface.startFreeTransform()
        } else if (surface.freeTransformActive) {
            surface.commitFreeTransform()
        }
    }

    onFreeTransformActiveChanged: {
        surface.updateConstrainAspectState()
        surface.updateDocumentProperty("freeTransformActive", surface.freeTransformActive)
    }

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
            if (!surface.updateLayerById(surface.selectedImageId, {
                    x: snapshot.x !== undefined ? snapshot.x : imageModel.get(index).x,
                    y: snapshot.y !== undefined ? snapshot.y : imageModel.get(index).y,
                    scaleX: snapshot.scaleX !== undefined ? snapshot.scaleX : imageModel.get(index).scaleX,
                    scaleY: snapshot.scaleY !== undefined ? snapshot.scaleY : imageModel.get(index).scaleY
                })) {
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

    Rectangle {
        id: layerPanel
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: surface.layerPanelTopMargin
        anchors.rightMargin: 16
        width: surface.layerPanelWidth
        height: Math.min(parent.height - surface.layerPanelTopMargin - 16, layerPanelContent.implicitHeight + 20)
        radius: 12
        color: LV.Theme.panelBackground03
        border.width: 1
        border.color: Qt.rgba(255, 255, 255, 0.10)
        visible: surface.hasImportedImage
        z: 40

        Column {
            id: layerPanelContent
            anchors.fill: parent
            anchors.margins: 10
            spacing: 10

            Text {
                text: qsTr("Layers")
                color: "#f5f7fa"
                font.pixelSize: 14
                font.bold: true
            }

            Flickable {
                id: layerFlickable
                width: parent.width
                height: Math.max(48, layerPanel.height - 56)
                contentWidth: width
                contentHeight: layerListColumn.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Column {
                    id: layerListColumn
                    width: layerFlickable.width
                    spacing: 6

                    Repeater {
                        model: surface.importedLayerCount

                        delegate: Rectangle {
                            id: layerRow
                            required property int index
                            readonly property int modelIndex: surface.importedLayerCount - 1 - index
                            readonly property var entry: modelIndex >= 0 && surface.imageModel
                                ? surface.imageModel.get(modelIndex)
                                : null
                            readonly property bool selected: entry && entry.imageId === surface.selectedImageId
                            width: layerListColumn.width
                            height: 44
                            radius: 10
                            color: selected ? Qt.rgba(45 / 255, 137 / 255, 239 / 255, 0.22) : Qt.rgba(255, 255, 255, 0.04)
                            border.width: 1
                            border.color: selected ? "#2d89ef" : Qt.rgba(255, 255, 255, 0.08)
                            opacity: entry && entry.layerVisible === false ? 0.6 : 1.0

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (layerRow.entry) {
                                        surface.selectImage(layerRow.entry.imageId)
                                    }
                                }
                            }

                            Rectangle {
                                id: visibilityChip
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                width: 28
                                height: 28
                                radius: 8
                                color: layerRow.entry && layerRow.entry.layerVisible === false
                                    ? Qt.rgba(255, 255, 255, 0.08)
                                    : Qt.rgba(45 / 255, 137 / 255, 239 / 255, 0.20)
                                border.width: 1
                                border.color: layerRow.entry && layerRow.entry.layerVisible === false
                                    ? Qt.rgba(255, 255, 255, 0.10)
                                    : "#2d89ef"

                                Text {
                                    anchors.centerIn: parent
                                    text: layerRow.entry && layerRow.entry.layerVisible === false ? qsTr("Off") : qsTr("On")
                                    color: "#f5f7fa"
                                    font.pixelSize: 10
                                    font.bold: true
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (layerRow.entry) {
                                            surface.setLayerVisibility(layerRow.entry.imageId, layerRow.entry.layerVisible === false)
                                        }
                                        mouse.accepted = true
                                    }
                                }
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 46
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                Text {
                                    width: parent.width
                                    text: layerRow.entry ? layerRow.entry.layerName : ""
                                    color: "#f5f7fa"
                                    font.pixelSize: 12
                                    font.bold: layerRow.selected
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: layerRow.entry
                                        ? (layerRow.entry.blendModeKey && layerRow.entry.blendModeKey.length
                                            ? layerRow.entry.blendModeKey
                                            : qsTr("Normal"))
                                        : ""
                                    color: Qt.rgba(245 / 255, 247 / 255, 250 / 255, 0.70)
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
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
            model: surface.imageModel ? surface.imageModel : null

            delegate: Image {
                id: imageDisplay
                required property int index
                readonly property var entry: surface.imageModel ? surface.imageModel.get(index) : null
                property int delegateImageId: entry ? entry.imageId : -1
                property int registeredImageId: -1
                x: entry ? entry.x : 0
                y: entry ? entry.y : 0
                width: entry && entry.originalWidth > 0 ? entry.originalWidth * entry.scaleX : 0
                height: entry && entry.originalHeight > 0 ? entry.originalHeight * entry.scaleY : 0
                visible: entry && entry.ready && (entry.layerVisible !== false)
                smooth: true
                asynchronous: true
                fillMode: Image.Stretch
                opacity: entry && entry.layerOpacity !== undefined ? entry.layerOpacity : 1
                source: entry ? entry.source : ""

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
                    var currentEntry = surface.imageModel ? surface.imageModel.get(modelIndex) : null
                    if (!currentEntry || currentEntry.ready) {
                        return
                    }
                    if (!surface.updateLayerById(delegateImageId, {
                            originalWidth: implicitWidth,
                            originalHeight: implicitHeight
                        })) {
                        surface.imageModel.setProperty(modelIndex, "originalWidth", implicitWidth)
                        surface.imageModel.setProperty(modelIndex, "originalHeight", implicitHeight)
                    }
                    surface.resetImagePlacement(delegateImageId)
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    enabled: surface.toolMode === "grab" && imageDisplay.visible
                    onTapped: {
                        surface.selectImage(imageDisplay.delegateImageId)
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
                currentEntry = selectedIndex >= 0 && surface.imageModel ? surface.imageModel.get(selectedIndex) : null
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
                target: surface.imageModel
                function onDataChanged() {
                    selectionOverlay.refreshSelectionState()
                }
                function onCountChanged() {
                    selectionOverlay.refreshSelectionState()
                }
            }

            visible: selectedItem
                && currentEntry
                && currentEntry.ready
                && currentEntry.layerVisible !== false
                && (surface.toolMode === "grab" || surface.freeTransformActive)
            x: selectedItem ? selectedItem.x : 0
            y: selectedItem ? selectedItem.y : 0
            width: selectedItem ? selectedItem.width : 0
            height: selectedItem ? selectedItem.height : 0

            function updateGeometry(newX, newY, newWidth, newHeight) {
                if (!currentEntry || selectedIndex < 0 || currentEntry.originalWidth <= 0 || currentEntry.originalHeight <= 0) {
                    return
                }
                if (!surface.updateLayerById(currentEntry.imageId, {
                        x: newX,
                        y: newY,
                        scaleX: newWidth / currentEntry.originalWidth,
                        scaleY: newHeight / currentEntry.originalHeight
                    })) {
                    imageModel.setProperty(selectedIndex, "x", newX)
                    imageModel.setProperty(selectedIndex, "y", newY)
                    imageModel.setProperty(selectedIndex, "scaleX", newWidth / currentEntry.originalWidth)
                    imageModel.setProperty(selectedIndex, "scaleY", newHeight / currentEntry.originalHeight)
                }
            }

            function moveSelection(newX, newY) {
                if (!currentEntry || selectedIndex < 0) {
                    return
                }
                if (!surface.updateLayerById(currentEntry.imageId, {
                        x: newX,
                        y: newY
                    })) {
                    imageModel.setProperty(selectedIndex, "x", newX)
                    imageModel.setProperty(selectedIndex, "y", newY)
                }
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
                        id: transformHandle
                        required property var modelData
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
                            cursorShape: transformHandle.modelData.cursor
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
                                            transformHandle.modelData.role,
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
        onVisibleChanged: surface.updateDocumentProperty("textEntryActive", visible)
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
        acceptedButtons: surface.toolMode === "text" ? Qt.LeftButton : Qt.NoButton
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
            mouse.accepted = false
        }

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
