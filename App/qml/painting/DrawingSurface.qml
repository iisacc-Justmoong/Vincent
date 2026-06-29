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
    property bool shapeAspectLocked: false
    property real shapeStartX: 0
    property real shapeStartY: 0
    property real shapeCurrentX: 0
    property real shapeCurrentY: 0
    property real canvasZoomScale: 1
    property bool zoomDraggingActive: false
    property real zoomDragStartX: 0
    property real zoomDragStartScale: 1
    property real canvasPanOffsetX: 0
    property real canvasPanOffsetY: 0
    property bool panDraggingActive: false
    property real panDragStartX: 0
    property real panDragStartY: 0
    property real panDragStartOffsetX: 0
    property real panDragStartOffsetY: 0
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
    readonly property int minimumCanvasDimension: 1
    readonly property int maximumCanvasDimension: 8192
    readonly property int minimumTextToolFontPixelSize: 8
    readonly property int textToolFontPixelSize: Math.max(surface.minimumTextToolFontPixelSize, Math.round(surface.brushSize))
    readonly property int textToolMinimumWidth: 96
    readonly property int shapeToolMinimumDragDistance: 2
    readonly property int speechBubbleTailMinimumHeight: 4
    readonly property real speechBubbleTailHeightRatio: 0.24
    readonly property real speechBubbleTailMaximumHeightRatio: 0.35
    readonly property real speechBubbleTailLeftBaseXRatio: 0.26
    readonly property real speechBubbleTailTipXRatio: 0.18
    readonly property real speechBubbleTailRightBaseXRatio: 0.44
    readonly property real ellipseBubbleTailLeftAngle: 2.15
    readonly property real ellipseBubbleTailRightAngle: 1.70
    readonly property int ellipseBubbleArcSegmentCount: 32
    readonly property int drawableObjectMinimumDimension: 8
    readonly property int drawableObjectHandleSize: 8
    readonly property int drawableObjectHandleHitSize: 16
    readonly property real defaultCanvasZoomScale: 1
    readonly property real minimumCanvasZoomScale: 0.01
    readonly property real maximumCanvasZoomScale: 8
    readonly property real zoomDragPixelsPerDoubling: 180
    readonly property var drawableObjectHandles: [
        {
            mode: "resize-nw",
            xRatio: 0,
            yRatio: 0
        },
        {
            mode: "resize-ne",
            xRatio: 1,
            yRatio: 0
        },
        {
            mode: "resize-se",
            xRatio: 1,
            yRatio: 1
        },
        {
            mode: "resize-sw",
            xRatio: 0,
            yRatio: 1
        }
    ]
    property var drawableObjects: []
    property int nextDrawableObjectId: 1
    property int nextEmptyLayerNumber: 1
    property int selectedDrawableObjectId: -1
    property var layerHierarchyRows: []
    readonly property int layerHierarchyThumbnailSize: 32
    readonly property int layerHierarchyThumbnailRefreshDelayMs: 1000
    readonly property int brushLivePreviewFrameIntervalMs: 16
    property bool layerHierarchyRowsRebuildScheduled: false
    property bool backgroundLayerThumbnailRefreshPending: false
    property var pendingRasterLayerThumbnailRefreshes: ({})
    property string backgroundLayerThumbnailSource: ""
    property var drawableObjectThumbnailSources: ({})
    property var rasterLayerItems: ({})
    property var rasterLayerSnapshotSources: ({})
    property var rasterLayerThumbnailSources: ({})
    property bool persistRasterLayerSnapshots: true
    property bool drawableObjectTransformActive: false
    property string drawableObjectTransformMode: ""
    property real drawableObjectTransformStartX: 0
    property real drawableObjectTransformStartY: 0
    property var drawableObjectTransformOriginal: null

    signal brushDeltaRequested(int delta)
    signal toolShortcutRequested(string tool)

    onSelectedDrawableObjectIdChanged: rebuildLayerHierarchyRows()

    function syncCanvasItemSizeToWorkspace() {
        if (!canvasItemReady || surface.width <= 0 || surface.height <= 0) {
            return;
        }
        canvasSurface.resizeCanvasSurface(workspaceCanvasWidth, workspaceCanvasHeight);
        resizeRasterLayerItems(workspaceCanvasWidth, workspaceCanvasHeight);
        canvasSizeCreated = true;
        fitCanvasZoomToCurrentCanvas();
    }

    function normalizedCanvasDimension(value, fallbackValue) {
        const parsedValue = Math.round(Number(value));
        if (!isFinite(parsedValue)) {
            return fallbackValue;
        }
        return Math.max(surface.minimumCanvasDimension, Math.min(surface.maximumCanvasDimension, parsedValue));
    }

    function resizeCanvasItemToDimensions(canvasWidth, canvasHeight) {
        if (!canvasItemReady) {
            return;
        }
        const nextCanvasWidth = normalizedCanvasDimension(canvasWidth, workspaceCanvasWidth);
        const nextCanvasHeight = normalizedCanvasDimension(canvasHeight, workspaceCanvasHeight);
        canvasSurface.resizeCanvasSurface(nextCanvasWidth, nextCanvasHeight);
        resizeRasterLayerItems(nextCanvasWidth, nextCanvasHeight);
        canvasSizeCreated = true;
        fitCanvasZoomToCurrentCanvas();
    }

    function newCanvas(canvasWidth, canvasHeight) {
        cancelActiveText();
        cancelActiveShape();
        resetCanvasPan();
        clearDrawableObjects();
        if (arguments.length >= 2) {
            resizeCanvasItemToDimensions(canvasWidth, canvasHeight);
        } else {
            syncCanvasItemSizeToWorkspace();
        }
        canvasSurface.newCanvas();
    }

    function clearCanvas() {
        cancelActiveText();
        cancelActiveShape();
        resetCanvasPan();
        clearDrawableObjects();
        syncCanvasItemSizeToWorkspace();
        canvasSurface.clearCanvas();
    }

    function openRaster(fileUrl) {
        cancelActiveText();
        cancelActiveShape();
        resetCanvasPan();
        const sourceUrl = fileUrl ? fileUrl.toString() : "";
        const imageObject = canvasSurface.imageObjectForFile(sourceUrl, surface.workspaceCanvasWidth, surface.workspaceCanvasHeight);
        if (!imageObject.source || imageObject.width <= 0 || imageObject.height <= 0) {
            return false;
        }

        clearDrawableObjects();
        syncCanvasItemSizeToWorkspace();
        canvasSurface.clearCanvas();
        appendDrawableObject({
            id: surface.nextDrawableObjectId++,
            type: "image",
            x: Math.max(0, Math.round((canvasSurface.width - imageObject.width) / 2)),
            y: Math.max(0, Math.round((canvasSurface.height - imageObject.height) / 2)),
            width: imageObject.width,
            height: imageObject.height,
            source: imageObject.source,
            originalWidth: imageObject.originalWidth,
            originalHeight: imageObject.originalHeight
        });
        return true;
    }

    function saveToFile(fileUrl) {
        commitActiveText();
        commitActiveShape();
        return canvasSurface.saveToFileWithObjectsAndRasterLayers(fileUrl ? fileUrl.toString() : "", surface.drawableObjects, rasterLayerDescriptors());
    }

    function psdCompatibilityManifest() {
        commitActiveText();
        commitActiveShape();
        return canvasSurface.psdCompatibilityManifest(surface.drawableObjects);
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
            appendDrawableObject({
                id: surface.nextDrawableObjectId++,
                type: "text",
                x: textToolEditorFrame.x,
                y: textToolEditorFrame.y,
                width: textToolEditorFrame.width,
                height: textToolEditorFrame.height,
                text: committedText,
                fontPixelSize: surface.textToolFontPixelSize,
                color: surface.brushColor.toString()
            });
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

    function shapeAspectLockedFromMouse(mouse) {
        return (mouse.modifiers & Qt.ShiftModifier) !== 0;
    }

    function shapeDragAxisDirection(delta, negativeLimit, positiveLimit) {
        if (delta < 0) {
            return -1;
        }
        if (delta > 0) {
            return 1;
        }
        return positiveLimit >= negativeLimit ? 1 : -1;
    }

    function constrainedShapeDragPoint(pointX, pointY) {
        const clampedX = clampedShapePointX(pointX);
        const clampedY = clampedShapePointY(pointY);
        const deltaX = clampedX - surface.shapeStartX;
        const deltaY = clampedY - surface.shapeStartY;
        const leftLimit = surface.shapeStartX;
        const rightLimit = canvasSurface.width - surface.shapeStartX;
        const topLimit = surface.shapeStartY;
        const bottomLimit = canvasSurface.height - surface.shapeStartY;
        const horizontalDirection = shapeDragAxisDirection(deltaX, leftLimit, rightLimit);
        const verticalDirection = shapeDragAxisDirection(deltaY, topLimit, bottomLimit);
        const horizontalLimit = horizontalDirection < 0 ? leftLimit : rightLimit;
        const verticalLimit = verticalDirection < 0 ? topLimit : bottomLimit;
        const side = Math.min(Math.max(Math.abs(deltaX), Math.abs(deltaY)), horizontalLimit, verticalLimit);
        return {
            x: surface.shapeStartX + horizontalDirection * side,
            y: surface.shapeStartY + verticalDirection * side
        };
    }

    function shapeDragPoint(pointX, pointY, aspectLocked) {
        if (aspectLocked) {
            return constrainedShapeDragPoint(pointX, pointY);
        }
        return {
            x: clampedShapePointX(pointX),
            y: clampedShapePointY(pointY)
        };
    }

    function applyShapeDragPoint(pointX, pointY, aspectLocked) {
        surface.shapeAspectLocked = aspectLocked === true;
        const dragPoint = shapeDragPoint(pointX, pointY, surface.shapeAspectLocked);
        surface.shapeCurrentX = dragPoint.x;
        surface.shapeCurrentY = dragPoint.y;
        requestShapePreviewPaint();
    }

    function requestShapePreviewPaint() {
        if (shapePreviewCanvas.visible) {
            shapePreviewCanvas.requestPaint();
        }
    }

    function beginShapeDrag(pointX, pointY, aspectLocked) {
        if (surface.toolMode !== "shape") {
            return;
        }

        commitActiveText();
        surface.shapeStartX = clampedShapePointX(pointX);
        surface.shapeStartY = clampedShapePointY(pointY);
        surface.shapeCurrentX = surface.shapeStartX;
        surface.shapeCurrentY = surface.shapeStartY;
        surface.shapeAspectLocked = aspectLocked === true;
        surface.shapeDraggingActive = true;
        requestShapePreviewPaint();
    }

    function updateShapeDrag(pointX, pointY, aspectLocked) {
        if (!surface.shapeDraggingActive) {
            return;
        }

        applyShapeDragPoint(pointX, pointY, aspectLocked);
    }

    function commitActiveShape() {
        if (!surface.shapeDraggingActive) {
            return;
        }

        const bounds = shapePreviewRect();
        surface.shapeDraggingActive = false;
        surface.shapeAspectLocked = false;
        if (bounds.width >= surface.shapeToolMinimumDragDistance && bounds.height >= surface.shapeToolMinimumDragDistance) {
            appendDrawableObject({
                id: surface.nextDrawableObjectId++,
                type: "shape",
                x: bounds.x,
                y: bounds.y,
                width: bounds.width,
                height: bounds.height,
                shapeKind: surface.shapeKind,
                color: surface.brushColor.toString()
            });
        }
        requestShapePreviewPaint();
    }

    function clearDrawableObjects() {
        surface.persistRasterLayerSnapshots = false;
        drawableObjectVisualModel.clear();
        surface.drawableObjects = [];
        surface.rasterLayerItems = {};
        surface.rasterLayerSnapshotSources = {};
        surface.rasterLayerThumbnailSources = {};
        surface.drawableObjectThumbnailSources = {};
        surface.pendingRasterLayerThumbnailRefreshes = {};
        surface.backgroundLayerThumbnailRefreshPending = false;
        layerThumbnailRefreshTimer.stop();
        surface.persistRasterLayerSnapshots = true;
        surface.nextEmptyLayerNumber = 1;
        surface.selectedDrawableObjectId = -1;
        resetDrawableObjectTransform();
        rebuildLayerHierarchyRows();
    }

    function rasterLayerObjectSelected() {
        const drawableObject = selectedDrawableObject();
        return drawableObject !== null && drawableObject.type === "layer";
    }

    function rasterLayerItemById(objectId) {
        return surface.rasterLayerItems[String(objectId)] || null;
    }

    function rasterLayerSnapshotSource(objectId) {
        return surface.rasterLayerSnapshotSources[String(objectId)] || "";
    }

    function copyStringMap(mapValue) {
        const copied = {};
        if (!mapValue) {
            return copied;
        }
        for (const key in mapValue) {
            copied[key] = mapValue[key];
        }
        return copied;
    }

    function scheduleLayerHierarchyRowsRebuild() {
        if (surface.layerHierarchyRowsRebuildScheduled) {
            return;
        }
        surface.layerHierarchyRowsRebuildScheduled = true;
        Qt.callLater(function () {
            surface.layerHierarchyRowsRebuildScheduled = false;
            surface.rebuildLayerHierarchyRows();
        });
    }

    function scheduleLayerThumbnailRefreshFlush() {
        layerThumbnailRefreshTimer.restart();
    }

    function requestBackgroundLayerThumbnailRefresh() {
        surface.backgroundLayerThumbnailRefreshPending = true;
        scheduleLayerThumbnailRefreshFlush();
    }

    function requestRasterLayerThumbnailRefresh(objectId) {
        const nextPending = copyStringMap(surface.pendingRasterLayerThumbnailRefreshes);
        nextPending[String(objectId)] = true;
        surface.pendingRasterLayerThumbnailRefreshes = nextPending;
        scheduleLayerThumbnailRefreshFlush();
    }

    function clearPendingRasterLayerThumbnailRefresh(objectId) {
        const key = String(objectId);
        if (surface.pendingRasterLayerThumbnailRefreshes[key] === undefined) {
            return;
        }
        const nextPending = copyStringMap(surface.pendingRasterLayerThumbnailRefreshes);
        delete nextPending[key];
        surface.pendingRasterLayerThumbnailRefreshes = nextPending;
    }

    function flushPendingLayerThumbnailRefreshes() {
        layerThumbnailRefreshTimer.stop();
        if (surface.backgroundLayerThumbnailRefreshPending) {
            surface.backgroundLayerThumbnailRefreshPending = false;
            refreshBackgroundLayerThumbnailSource();
        }

        const pendingLayerIds = Object.keys(surface.pendingRasterLayerThumbnailRefreshes);
        if (pendingLayerIds.length === 0) {
            return;
        }
        surface.pendingRasterLayerThumbnailRefreshes = {};
        for (let index = 0; index < pendingLayerIds.length; ++index) {
            refreshRasterLayerThumbnailSource(Number(pendingLayerIds[index]));
        }
    }

    function setDrawableObjectThumbnailSource(objectId, source) {
        const key = String(objectId);
        if ((surface.drawableObjectThumbnailSources[key] || "") === source) {
            return;
        }
        const nextSources = copyStringMap(surface.drawableObjectThumbnailSources);
        if (source.length > 0) {
            nextSources[key] = source;
        } else {
            delete nextSources[key];
        }
        surface.drawableObjectThumbnailSources = nextSources;
    }

    function setRasterLayerThumbnailSource(objectId, source) {
        const key = String(objectId);
        if ((surface.rasterLayerThumbnailSources[key] || "") === source) {
            return;
        }
        const nextSources = copyStringMap(surface.rasterLayerThumbnailSources);
        if (source.length > 0) {
            nextSources[key] = source;
        } else {
            delete nextSources[key];
        }
        surface.rasterLayerThumbnailSources = nextSources;
    }

    function clearRasterLayerThumbnailState(objectId) {
        clearPendingRasterLayerThumbnailRefresh(objectId);
        setRasterLayerThumbnailSource(objectId, "");
    }

    function fallbackRasterThumbnailSource(surfaceItem) {
        if (!surfaceItem || !surfaceItem.cacheRasterThumbnailSource) {
            return "";
        }
        return surfaceItem.cacheRasterThumbnailSource(surface.layerHierarchyThumbnailSize, surface.layerHierarchyThumbnailSize) || "";
    }

    function refreshRasterSurfaceThumbnailSource(surfaceItem, applySource) {
        if (!surfaceItem) {
            return "";
        }

        if (surfaceItem.grabToImage) {
            const accepted = surfaceItem.grabToImage(function (result) {
                const grabbedSource = result && surfaceItem.cacheGrabbedThumbnailSource ? surfaceItem.cacheGrabbedThumbnailSource(result) : "";
                if (grabbedSource.length > 0) {
                    applySource(grabbedSource);
                    scheduleLayerHierarchyRowsRebuild();
                    return;
                }

                applySource(fallbackRasterThumbnailSource(surfaceItem));
                scheduleLayerHierarchyRowsRebuild();
            }, Qt.size(surface.layerHierarchyThumbnailSize, surface.layerHierarchyThumbnailSize));
            if (accepted) {
                return "";
            }
        }

        const source = fallbackRasterThumbnailSource(surfaceItem);
        applySource(source);
        scheduleLayerHierarchyRowsRebuild();
        return source;
    }

    function invalidateDrawableObjectThumbnailSource(objectId) {
        setDrawableObjectThumbnailSource(objectId, "");
    }

    function refreshDrawableObjectThumbnailSource(drawableObject) {
        if (!drawableObject || drawableObject.type === "layer") {
            return "";
        }
        const source = canvasSurface.cacheDrawableObjectThumbnailSource(drawableObject, surface.layerHierarchyThumbnailSize, surface.layerHierarchyThumbnailSize);
        setDrawableObjectThumbnailSource(drawableObject.id, source || "");
        return source || "";
    }

    function refreshRasterLayerThumbnailSource(objectId) {
        const item = rasterLayerItemById(objectId);
        if (!item) {
            return "";
        }
        clearPendingRasterLayerThumbnailRefresh(objectId);
        return refreshRasterSurfaceThumbnailSource(item, function (source) {
            if (rasterLayerItemById(objectId) !== item) {
                return;
            }
            setRasterLayerThumbnailSource(objectId, source || "");
        });
    }

    function refreshBackgroundLayerThumbnailSource() {
        surface.backgroundLayerThumbnailRefreshPending = false;
        return refreshRasterSurfaceThumbnailSource(canvasSurface, function (source) {
            surface.backgroundLayerThumbnailSource = source || "";
        });
    }

    function layerIconSourceForDrawableObject(drawableObject) {
        if (!drawableObject) {
            return "";
        }
        if (drawableObject.type === "layer") {
            return surface.rasterLayerThumbnailSources[String(drawableObject.id)] || "";
        }

        const key = String(drawableObject.id);
        const cachedSource = surface.drawableObjectThumbnailSources[key] || "";
        return cachedSource.length > 0 ? cachedSource : refreshDrawableObjectThumbnailSource(drawableObject);
    }

    function registerRasterLayerItem(objectId, surfaceItem) {
        if (!surfaceItem) {
            return;
        }

        surface.rasterLayerItems[String(objectId)] = surfaceItem;

        surfaceItem.resizeCanvasSurface(canvasSurface.width, canvasSurface.height);
        const snapshotSource = rasterLayerSnapshotSource(objectId);
        if (snapshotSource.length > 0) {
            surfaceItem.restoreRasterSnapshot(snapshotSource);
        } else {
            surfaceItem.newCanvas();
        }
        refreshRasterLayerThumbnailSource(objectId);
    }

    function unregisterRasterLayerItem(objectId, surfaceItem) {
        if (surface.persistRasterLayerSnapshots && surfaceItem) {
            const snapshotSource = surfaceItem.cacheRasterSnapshotSource();
            if (snapshotSource.length > 0) {
                surface.rasterLayerSnapshotSources[String(objectId)] = snapshotSource;
            }
        }

        delete surface.rasterLayerItems[String(objectId)];
        clearRasterLayerThumbnailState(objectId);
    }

    function resizeRasterLayerItems(canvasWidth, canvasHeight) {
        for (const key in surface.rasterLayerItems) {
            const item = surface.rasterLayerItems[key];
            if (item) {
                item.resizeCanvasSurface(canvasWidth, canvasHeight);
            }
        }
    }

    function activeRasterSurface() {
        if (rasterLayerObjectSelected()) {
            const item = rasterLayerItemById(surface.selectedDrawableObjectId);
            if (item) {
                return item;
            }
        }
        return canvasSurface;
    }

    function rasterLayerDescriptors() {
        const layers = [];
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (!drawableObject || drawableObject.type !== "layer") {
                continue;
            }
            layers.push({
                objectId: drawableObject.id,
                item: rasterLayerItemById(drawableObject.id),
                snapshotSource: rasterLayerSnapshotSource(drawableObject.id)
            });
        }
        return layers;
    }

    function drawableObjectVisualModelEntry(drawableObject) {
        return {
            objectId: drawableObject.id,
            objectType: drawableObject.type || "",
            objectX: Number(drawableObject.x || 0),
            objectY: Number(drawableObject.y || 0),
            objectWidth: Math.max(1, Number(drawableObject.width || 1)),
            objectHeight: Math.max(1, Number(drawableObject.height || 1)),
            objectSource: drawableObject.source || "",
            objectShapeKind: drawableObject.shapeKind || "",
            objectColor: drawableObject.color || "",
            objectText: drawableObject.text || "",
            objectFontPixelSize: Math.max(1, Number(drawableObject.fontPixelSize || 1))
        };
    }

    function drawableObjectVisualModelIndexForObjectId(objectId) {
        for (let index = 0; index < drawableObjectVisualModel.count; ++index) {
            if (drawableObjectVisualModel.get(index).objectId === objectId) {
                return index;
            }
        }
        return -1;
    }

    function drawableObjectVisualModelCount() {
        return drawableObjectVisualModel.count;
    }

    function setDrawableObjectVisualModelEntry(drawableObject) {
        const index = drawableObjectVisualModelIndexForObjectId(drawableObject.id);
        if (index >= 0) {
            drawableObjectVisualModel.set(index, drawableObjectVisualModelEntry(drawableObject));
        }
    }

    function reorderDrawableObjectVisualModel(orderedObjects) {
        for (let targetIndex = 0; targetIndex < orderedObjects.length; ++targetIndex) {
            const currentIndex = drawableObjectVisualModelIndexForObjectId(orderedObjects[targetIndex].id);
            if (currentIndex >= 0 && currentIndex !== targetIndex) {
                drawableObjectVisualModel.move(currentIndex, targetIndex, 1);
            }
        }
    }

    function cloneDrawableObject(drawableObject) {
        const copy = {};
        for (const key in drawableObject) {
            copy[key] = drawableObject[key];
        }
        return copy;
    }

    function explicitLayerName(drawableObject) {
        if (!drawableObject) {
            return "";
        }
        const name = drawableObject.name === undefined || drawableObject.name === null ? "" : String(drawableObject.name).trim();
        return name;
    }

    function titleCaseLayerLabel(value, fallbackValue) {
        const textValue = value === undefined || value === null ? "" : String(value).trim();
        if (textValue.length === 0) {
            return fallbackValue;
        }
        return textValue.charAt(0).toUpperCase() + textValue.slice(1);
    }

    function fileNameFromLayerSource(sourceValue) {
        const sourceText = sourceValue === undefined || sourceValue === null ? "" : String(sourceValue);
        const pathText = sourceText.split("?")[0].split("#")[0];
        const lastSlashIndex = pathText.lastIndexOf("/");
        const fileName = lastSlashIndex >= 0 ? pathText.substring(lastSlashIndex + 1) : pathText;
        try {
            return decodeURIComponent(fileName);
        } catch (error) {
            return fileName;
        }
    }

    function firstLayerTextLine(textValue) {
        const text = textValue === undefined || textValue === null ? "" : String(textValue).trim();
        if (text.length === 0) {
            return qsTr("Text");
        }
        const firstLine = text.split(/\r?\n/)[0].trim();
        return (firstLine.length > 0 ? firstLine : qsTr("Text")).slice(0, 32);
    }

    function layerLabelForDrawableObject(drawableObject) {
        if (!drawableObject) {
            return qsTr("Layer");
        }

        const explicitName = explicitLayerName(drawableObject);
        if (explicitName.length > 0) {
            return explicitName;
        }
        if (drawableObject.type === "image") {
            const fileName = fileNameFromLayerSource(drawableObject.originalSource || drawableObject.source);
            return fileName.length > 0 ? fileName : qsTr("Image");
        }
        if (drawableObject.type === "text") {
            return firstLayerTextLine(drawableObject.text);
        }
        if (drawableObject.type === "shape") {
            return titleCaseLayerLabel(drawableObject.shapeKind, qsTr("Shape"));
        }
        if (drawableObject.type === "layer") {
            return qsTr("Layer %1").arg(drawableObject.id);
        }
        return qsTr("Layer %1").arg(drawableObject.id);
    }

    function layerKeyForDrawableObjectId(objectId) {
        return "object-" + objectId;
    }

    function drawableObjectForLayerKey(layerKey) {
        const normalizedKey = layerKey === undefined || layerKey === null ? "" : String(layerKey);
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (layerKeyForDrawableObjectId(drawableObject.id) === normalizedKey) {
                return drawableObject;
            }
        }
        return null;
    }

    function currentLayerKey() {
        return surface.selectedDrawableObjectId >= 0 ? layerKeyForDrawableObjectId(surface.selectedDrawableObjectId) : "raster-canvas";
    }

    function rebuildLayerHierarchyRows() {
        const rows = [];
        for (let index = surface.drawableObjects.length - 1; index >= 0; --index) {
            const drawableObject = surface.drawableObjects[index];
            rows.push({
                key: layerKeyForDrawableObjectId(drawableObject.id),
                itemId: drawableObject.id,
                objectId: drawableObject.id,
                layerKind: "object",
                depth: 0,
                parentKey: "",
                parentItemKey: "",
                label: layerLabelForDrawableObject(drawableObject),
                iconSource: layerIconSourceForDrawableObject(drawableObject),
                iconGlyph: "",
                selected: drawableObject.id === surface.selectedDrawableObjectId,
                enabled: true,
                activatable: true,
                draggable: true,
                showChevron: false
            });
        }
        rows.push({
            key: "raster-canvas",
            itemId: 0,
            objectId: -1,
            layerKind: "raster",
            depth: 0,
            parentKey: "",
            parentItemKey: "",
            label: qsTr("Background"),
            iconSource: surface.backgroundLayerThumbnailSource,
            iconGlyph: "",
            selected: surface.selectedDrawableObjectId < 0,
            enabled: true,
            activatable: true,
            draggable: false,
            showChevron: false
        });
        surface.layerHierarchyRows = rows;
    }

    function appendDrawableObject(drawableObject) {
        const nextObjects = surface.drawableObjects.slice();
        nextObjects.push(drawableObject);
        surface.drawableObjects = nextObjects;
        refreshDrawableObjectThumbnailSource(drawableObject);
        drawableObjectVisualModel.append(drawableObjectVisualModelEntry(drawableObject));
        surface.selectedDrawableObjectId = drawableObject.id;
    }

    function nextEmptyLayerName() {
        return qsTr("Layer %1").arg(surface.nextEmptyLayerNumber++);
    }

    function addEmptyLayer() {
        commitActiveText();
        cancelActiveShape();
        resetDrawableObjectTransform();

        const layerId = surface.nextDrawableObjectId++;
        appendDrawableObject({
            id: layerId,
            type: "layer",
            name: nextEmptyLayerName(),
            x: 0,
            y: 0,
            width: Math.max(1, canvasSurface.width),
            height: Math.max(1, canvasSurface.height),
            opacity: 1,
            visible: true
        });
        return layerId;
    }

    function replaceDrawableObjectById(objectId, drawableObject) {
        const nextObjects = surface.drawableObjects.slice();
        for (let index = 0; index < nextObjects.length; ++index) {
            if (nextObjects[index].id === objectId) {
                nextObjects[index] = drawableObject;
                surface.drawableObjects = nextObjects;
                invalidateDrawableObjectThumbnailSource(objectId);
                setDrawableObjectVisualModelEntry(drawableObject);
                return true;
            }
        }
        return false;
    }

    function activateLayerByKey(layerKey) {
        const normalizedKey = layerKey === undefined || layerKey === null ? "" : String(layerKey);
        if (normalizedKey === "raster-canvas") {
            surface.selectedDrawableObjectId = -1;
            resetDrawableObjectTransform();
            return true;
        }

        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (layerKeyForDrawableObjectId(drawableObject.id) === normalizedKey) {
                surface.selectedDrawableObjectId = drawableObject.id;
                resetDrawableObjectTransform();
                return true;
            }
        }
        return false;
    }

    function deleteLayerByKey(layerKey) {
        if (!activateLayerByKey(layerKey) || surface.selectedDrawableObjectId < 0) {
            return false;
        }
        return deleteSelectedDrawableObject();
    }

    function canRenameLayerByKey(layerKey) {
        return drawableObjectForLayerKey(layerKey) !== null;
    }

    function layerNameByKey(layerKey) {
        const drawableObject = drawableObjectForLayerKey(layerKey);
        return drawableObject ? layerLabelForDrawableObject(drawableObject) : "";
    }

    function renameLayerByKey(layerKey, layerName) {
        const normalizedName = layerName === undefined || layerName === null ? "" : String(layerName).trim();
        if (normalizedName.length === 0) {
            return false;
        }

        const normalizedKey = layerKey === undefined || layerKey === null ? "" : String(layerKey);
        const nextObjects = surface.drawableObjects.slice();
        for (let index = 0; index < nextObjects.length; ++index) {
            const drawableObject = nextObjects[index];
            if (layerKeyForDrawableObjectId(drawableObject.id) === normalizedKey) {
                const renamedObject = cloneDrawableObject(drawableObject);
                renamedObject.name = normalizedName;
                nextObjects[index] = renamedObject;
                surface.drawableObjects = nextObjects;
                setDrawableObjectVisualModelEntry(renamedObject);
                rebuildLayerHierarchyRows();
                return true;
            }
        }
        return false;
    }

    function applyLayerHierarchyOrder(layerRows) {
        const rows = Array.isArray(layerRows) ? layerRows : surface.layerHierarchyRows;
        const objectById = {};
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            objectById[String(drawableObject.id)] = drawableObject;
        }

        const topToBottomObjects = [];
        for (let rowIndex = 0; rowIndex < rows.length; ++rowIndex) {
            const row = rows[rowIndex];
            if (!row || row.layerKind !== "object") {
                continue;
            }
            const objectId = String(row.objectId);
            if (objectById[objectId] !== undefined) {
                topToBottomObjects.push(objectById[objectId]);
            }
        }
        if (topToBottomObjects.length !== surface.drawableObjects.length) {
            rebuildLayerHierarchyRows();
            return false;
        }

        const nextObjects = [];
        for (let index = topToBottomObjects.length - 1; index >= 0; --index) {
            nextObjects.push(topToBottomObjects[index]);
        }
        surface.drawableObjects = nextObjects;
        reorderDrawableObjectVisualModel(nextObjects);
        if (surface.selectedDrawableObjectId >= 0 && objectById[String(surface.selectedDrawableObjectId)] === undefined) {
            surface.selectedDrawableObjectId = -1;
        }
        rebuildLayerHierarchyRows();
        return true;
    }

    function selectedDrawableObject() {
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            if (surface.drawableObjects[index].id === surface.selectedDrawableObjectId) {
                return surface.drawableObjects[index];
            }
        }
        return null;
    }

    function hasSelectedDrawableObject() {
        return selectedDrawableObject() !== null;
    }

    function drawableObjectIsTransformable(drawableObject) {
        if (!drawableObject) {
            return false;
        }
        return drawableObject.type === "image" || drawableObject.type === "text" || drawableObject.type === "shape";
    }

    function hasTransformableSelectedDrawableObject() {
        return drawableObjectIsTransformable(selectedDrawableObject());
    }

    function deleteSelectedDrawableObject() {
        const selectedObjectId = surface.selectedDrawableObjectId;
        if (selectedObjectId < 0) {
            return false;
        }

        const nextObjects = [];
        var removed = false;
        var removedIndex = -1;
        var removedObject = null;
        for (let index = 0; index < surface.drawableObjects.length; ++index) {
            const drawableObject = surface.drawableObjects[index];
            if (drawableObject.id === selectedObjectId) {
                removed = true;
                removedIndex = index;
                removedObject = drawableObject;
                continue;
            }
            nextObjects.push(drawableObject);
        }

        if (!removed) {
            surface.selectedDrawableObjectId = -1;
            resetDrawableObjectTransform();
            return false;
        }

        surface.drawableObjects = nextObjects;
        if (removedIndex >= 0) {
            const previousPersistRasterLayerSnapshots = surface.persistRasterLayerSnapshots;
            if (removedObject && removedObject.type === "layer") {
                surface.persistRasterLayerSnapshots = false;
                delete surface.rasterLayerSnapshotSources[String(selectedObjectId)];
                clearRasterLayerThumbnailState(selectedObjectId);
            }
            invalidateDrawableObjectThumbnailSource(selectedObjectId);
            drawableObjectVisualModel.remove(removedIndex);
            surface.persistRasterLayerSnapshots = previousPersistRasterLayerSnapshots;
        }
        surface.selectedDrawableObjectId = -1;
        resetDrawableObjectTransform();
        return true;
    }

    function selectedDrawableObjectProperty(propertyName, fallbackValue) {
        const drawableObject = selectedDrawableObject();
        return drawableObject ? drawableObject[propertyName] : fallbackValue;
    }

    function drawableObjectIndexAt(pointX, pointY) {
        for (let index = surface.drawableObjects.length - 1; index >= 0; --index) {
            const drawableObject = surface.drawableObjects[index];
            if (!drawableObjectIsTransformable(drawableObject)) {
                continue;
            }
            if (pointX >= drawableObject.x && pointX <= drawableObject.x + drawableObject.width && pointY >= drawableObject.y && pointY <= drawableObject.y + drawableObject.height) {
                return index;
            }
        }
        return -1;
    }

    function drawableObjectHandleAt(pointX, pointY) {
        const drawableObject = selectedDrawableObject();
        if (!drawableObjectIsTransformable(drawableObject)) {
            return "";
        }

        const halfHitSize = surface.drawableObjectHandleHitSize / 2;
        for (let index = 0; index < surface.drawableObjectHandles.length; ++index) {
            const handle = surface.drawableObjectHandles[index];
            const handleX = drawableObject.x + drawableObject.width * handle.xRatio;
            const handleY = drawableObject.y + drawableObject.height * handle.yRatio;
            if (pointX >= handleX - halfHitSize && pointX <= handleX + halfHitSize && pointY >= handleY - halfHitSize && pointY <= handleY + halfHitSize) {
                return handle.mode;
            }
        }
        return "";
    }

    function resetDrawableObjectTransform() {
        surface.drawableObjectTransformActive = false;
        surface.drawableObjectTransformMode = "";
        surface.drawableObjectTransformOriginal = null;
    }

    function cancelActiveDrawableObjectTransform() {
        if (surface.drawableObjectTransformActive && surface.drawableObjectTransformOriginal) {
            replaceDrawableObjectById(surface.drawableObjectTransformOriginal.id, surface.drawableObjectTransformOriginal);
        }
        resetDrawableObjectTransform();
    }

    function beginDrawableObjectTransform(pointX, pointY) {
        if (surface.toolMode !== "move") {
            return false;
        }

        commitActiveText();
        cancelActiveShape();
        const handleMode = drawableObjectHandleAt(pointX, pointY);
        if (handleMode.length > 0) {
            surface.drawableObjectTransformMode = handleMode;
        } else {
            const objectIndex = drawableObjectIndexAt(pointX, pointY);
            if (objectIndex < 0) {
                surface.selectedDrawableObjectId = -1;
                resetDrawableObjectTransform();
                return false;
            }
            surface.selectedDrawableObjectId = surface.drawableObjects[objectIndex].id;
            surface.drawableObjectTransformMode = "move";
        }

        const drawableObject = selectedDrawableObject();
        if (!drawableObjectIsTransformable(drawableObject)) {
            resetDrawableObjectTransform();
            return false;
        }

        surface.drawableObjectTransformStartX = pointX;
        surface.drawableObjectTransformStartY = pointY;
        surface.drawableObjectTransformOriginal = cloneDrawableObject(drawableObject);
        surface.drawableObjectTransformActive = true;
        return true;
    }

    function movedDrawableObject(originalObject, pointX, pointY) {
        const deltaX = pointX - surface.drawableObjectTransformStartX;
        const deltaY = pointY - surface.drawableObjectTransformStartY;
        const movedObject = cloneDrawableObject(originalObject);
        const maxX = Math.max(0, canvasSurface.width - movedObject.width);
        const maxY = Math.max(0, canvasSurface.height - movedObject.height);
        movedObject.x = Math.max(0, Math.min(maxX, originalObject.x + deltaX));
        movedObject.y = Math.max(0, Math.min(maxY, originalObject.y + deltaY));
        return movedObject;
    }

    function resizedDrawableObject(originalObject, pointX, pointY) {
        const deltaX = pointX - surface.drawableObjectTransformStartX;
        const deltaY = pointY - surface.drawableObjectTransformStartY;
        var left = originalObject.x;
        var top = originalObject.y;
        var right = originalObject.x + originalObject.width;
        var bottom = originalObject.y + originalObject.height;

        if (surface.drawableObjectTransformMode.indexOf("w") >= 0) {
            left = Math.max(0, Math.min(right - surface.drawableObjectMinimumDimension, originalObject.x + deltaX));
        }
        if (surface.drawableObjectTransformMode.indexOf("e") >= 0) {
            right = Math.min(canvasSurface.width, Math.max(left + surface.drawableObjectMinimumDimension, originalObject.x + originalObject.width + deltaX));
        }
        if (surface.drawableObjectTransformMode.indexOf("n") >= 0) {
            top = Math.max(0, Math.min(bottom - surface.drawableObjectMinimumDimension, originalObject.y + deltaY));
        }
        if (surface.drawableObjectTransformMode.indexOf("s") >= 0) {
            bottom = Math.min(canvasSurface.height, Math.max(top + surface.drawableObjectMinimumDimension, originalObject.y + originalObject.height + deltaY));
        }

        const resizedObject = cloneDrawableObject(originalObject);
        resizedObject.x = left;
        resizedObject.y = top;
        resizedObject.width = right - left;
        resizedObject.height = bottom - top;
        return resizedObject;
    }

    function updateDrawableObjectTransform(pointX, pointY) {
        if (!surface.drawableObjectTransformActive || !surface.drawableObjectTransformOriginal) {
            return;
        }

        const nextObject = surface.drawableObjectTransformMode === "move" ? movedDrawableObject(surface.drawableObjectTransformOriginal, pointX, pointY) : resizedDrawableObject(surface.drawableObjectTransformOriginal, pointX, pointY);
        replaceDrawableObjectById(nextObject.id, nextObject);
    }

    function commitDrawableObjectTransform() {
        const drawableObject = selectedDrawableObject();
        if (drawableObject) {
            refreshDrawableObjectThumbnailSource(drawableObject);
            rebuildLayerHierarchyRows();
        }
        resetDrawableObjectTransform();
    }

    function fillAt(pointX, pointY) {
        if (surface.toolMode !== "fill") {
            return;
        }

        commitActiveText();
        cancelActiveShape();
        activeRasterSurface().fillAt(pointX, pointY, surface.brushColor);
    }

    function resetCanvasPan() {
        surface.canvasPanOffsetX = 0;
        surface.canvasPanOffsetY = 0;
        surface.panDraggingActive = false;
    }

    function beginPanDrag(pointX, pointY) {
        if (surface.toolMode !== "pan") {
            return;
        }

        commitActiveText();
        cancelActiveShape();
        resetDrawableObjectTransform();
        commitZoomDrag();
        surface.panDragStartX = pointX;
        surface.panDragStartY = pointY;
        surface.panDragStartOffsetX = surface.canvasPanOffsetX;
        surface.panDragStartOffsetY = surface.canvasPanOffsetY;
        surface.panDraggingActive = true;
    }

    function updatePanDrag(pointX, pointY) {
        if (!surface.panDraggingActive) {
            return;
        }

        surface.canvasPanOffsetX = surface.panDragStartOffsetX + pointX - surface.panDragStartX;
        surface.canvasPanOffsetY = surface.panDragStartOffsetY + pointY - surface.panDragStartY;
    }

    function commitPanDrag() {
        surface.panDraggingActive = false;
    }

    function cancelPanDrag() {
        if (!surface.panDraggingActive) {
            return;
        }
        surface.canvasPanOffsetX = surface.panDragStartOffsetX;
        surface.canvasPanOffsetY = surface.panDragStartOffsetY;
        surface.panDraggingActive = false;
    }

    function boundedCanvasZoomScale(scaleValue) {
        const parsedScale = Number(scaleValue);
        if (!isFinite(parsedScale)) {
            return surface.defaultCanvasZoomScale;
        }
        return Math.max(surface.minimumCanvasZoomScale, Math.min(surface.maximumCanvasZoomScale, parsedScale));
    }

    function fittedCanvasZoomScale(canvasWidth, canvasHeight) {
        const normalizedWidth = Math.max(surface.minimumCanvasDimension, Number(canvasWidth));
        const normalizedHeight = Math.max(surface.minimumCanvasDimension, Number(canvasHeight));
        if (!isFinite(normalizedWidth) || !isFinite(normalizedHeight)) {
            return surface.defaultCanvasZoomScale;
        }

        return boundedCanvasZoomScale(Math.min(surface.defaultCanvasZoomScale, surface.workspaceCanvasWidth / normalizedWidth, surface.workspaceCanvasHeight / normalizedHeight));
    }

    function fitCanvasZoomToCurrentCanvas() {
        surface.zoomDraggingActive = false;
        surface.canvasZoomScale = fittedCanvasZoomScale(canvasSurface.width, canvasSurface.height);
    }

    function beginZoomDrag(pointX) {
        if (surface.toolMode !== "zoom") {
            return;
        }

        commitActiveText();
        cancelActiveShape();
        resetDrawableObjectTransform();
        surface.zoomDragStartX = pointX;
        surface.zoomDragStartScale = surface.canvasZoomScale;
        surface.zoomDraggingActive = true;
    }

    function updateZoomDrag(pointX) {
        if (!surface.zoomDraggingActive) {
            return;
        }

        const deltaX = pointX - surface.zoomDragStartX;
        surface.canvasZoomScale = boundedCanvasZoomScale(surface.zoomDragStartScale * Math.pow(2, deltaX / surface.zoomDragPixelsPerDoubling));
    }

    function commitZoomDrag() {
        surface.zoomDraggingActive = false;
    }

    function cancelZoomDrag() {
        if (!surface.zoomDraggingActive) {
            return;
        }
        surface.canvasZoomScale = surface.zoomDragStartScale;
        surface.zoomDraggingActive = false;
    }

    function canvasMouseAcceptedButtons() {
        if (surface.toolMode === "shape" || surface.toolMode === "move" || surface.toolMode === "zoom" || surface.toolMode === "fill" || surface.toolMode === "text") {
            return Qt.LeftButton;
        }
        return Qt.NoButton;
    }

    function canvasCursorShape() {
        if (surface.toolMode === "shape") {
            return Qt.CrossCursor;
        }
        if (surface.toolMode === "text") {
            return Qt.IBeamCursor;
        }
        if (surface.toolMode === "pan") {
            return surface.panDraggingActive ? Qt.ClosedHandCursor : Qt.OpenHandCursor;
        }
        if (surface.toolMode === "move") {
            return Qt.SizeAllCursor;
        }
        if (surface.toolMode === "zoom") {
            return Qt.SizeHorCursor;
        }
        if (surface.toolMode === "fill" || surface.toolMode === "eraser") {
            return Qt.PointingHandCursor;
        }
        return Qt.CrossCursor;
    }

    function cancelActiveShape() {
        if (!surface.shapeDraggingActive) {
            return;
        }

        surface.shapeDraggingActive = false;
        surface.shapeAspectLocked = false;
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

    function speechBubbleTailHeight(heightValue) {
        return Math.min(Math.max(surface.speechBubbleTailMinimumHeight, heightValue * surface.speechBubbleTailHeightRatio), heightValue * surface.speechBubbleTailMaximumHeightRatio);
    }

    function speechBubbleBodyHeight(heightValue) {
        return Math.max(surface.shapeToolMinimumDragDistance, heightValue - speechBubbleTailHeight(heightValue));
    }

    function ellipsePoint(x, y, widthValue, heightValue, angle) {
        return {
            x: x + widthValue / 2 + Math.cos(angle) * widthValue / 2,
            y: y + heightValue / 2 + Math.sin(angle) * heightValue / 2
        };
    }

    function traceEllipseArcPath(context, x, y, widthValue, heightValue, startAngle, endAngle) {
        for (let index = 1; index <= surface.ellipseBubbleArcSegmentCount; ++index) {
            const angle = startAngle + (endAngle - startAngle) * index / surface.ellipseBubbleArcSegmentCount;
            const point = ellipsePoint(x, y, widthValue, heightValue, angle);
            context.lineTo(point.x, point.y);
        }
    }

    function traceRectangleBubblePath(context, x, y, widthValue, heightValue) {
        const bodyHeight = speechBubbleBodyHeight(heightValue);
        const bodyBottom = y + bodyHeight;
        const tailLeftX = x + widthValue * surface.speechBubbleTailLeftBaseXRatio;
        const tailTipX = x + widthValue * surface.speechBubbleTailTipXRatio;
        const tailRightX = x + widthValue * surface.speechBubbleTailRightBaseXRatio;

        context.moveTo(x, y);
        context.lineTo(x + widthValue, y);
        context.lineTo(x + widthValue, bodyBottom);
        context.lineTo(tailRightX, bodyBottom);
        context.lineTo(tailTipX, y + heightValue);
        context.lineTo(tailLeftX, bodyBottom);
        context.lineTo(x, bodyBottom);
        context.closePath();
    }

    function traceEllipseBubblePath(context, x, y, widthValue, heightValue) {
        const bodyHeight = speechBubbleBodyHeight(heightValue);
        const tailLeftPoint = ellipsePoint(x, y, widthValue, bodyHeight, surface.ellipseBubbleTailLeftAngle);
        const tailTipX = x + widthValue * surface.speechBubbleTailTipXRatio;

        context.moveTo(tailLeftPoint.x, tailLeftPoint.y);
        traceEllipseArcPath(context, x, y, widthValue, bodyHeight, surface.ellipseBubbleTailLeftAngle, surface.ellipseBubbleTailRightAngle + Math.PI * 2);
        context.lineTo(tailTipX, y + heightValue);
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
        if (shape === "rectanglebubble") {
            traceRectangleBubblePath(context, x, y, widthValue, heightValue);
            return;
        }
        if (shape === "ellipsebubble") {
            traceEllipseBubblePath(context, x, y, widthValue, heightValue);
            return;
        }

        context.rect(x, y, widthValue, heightValue);
    }

    function paintShapePreview(context, previewWidth, previewHeight) {
        context.clearRect(0, 0, previewWidth, previewHeight);
        if (!surface.shapeDraggingActive) {
            return;
        }

        context.beginPath();
        traceShapePath(context, surface.shapeKind, 0, 0, previewWidth, previewHeight);
        context.fillStyle = surface.brushColor.toString();
        context.fill();
    }

    onToolModeChanged: {
        if (toolMode !== "text") {
            commitActiveText();
        }
        if (toolMode !== "shape") {
            cancelActiveShape();
        }
        if (toolMode !== "move") {
            resetDrawableObjectTransform();
        }
        if (toolMode !== "pan") {
            commitPanDrag();
        }
        if (toolMode !== "zoom") {
            commitZoomDrag();
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
            x: Math.round((parent.width - width) / 2 + surface.canvasPanOffsetX)
            y: Math.round((parent.height - height) / 2 + surface.canvasPanOffsetY)
            width: canvasSurface.width
            height: canvasSurface.height
            transformOrigin: Item.Center
            scale: surface.canvasZoomScale
            color: surface.canvasColor
            border.color: "#b8bcc4"
            border.width: canvasPaper.width < surface.width || canvasPaper.height < surface.height ? 1 : 0
        }

        DrawingSurfaceItem {
            id: canvasSurface
            x: Math.round((parent.width - width) / 2 + surface.canvasPanOffsetX)
            y: Math.round((parent.height - height) / 2 + surface.canvasPanOffsetY)
            z: 1
            width: 1
            height: 1
            transformOrigin: Item.Center
            scale: surface.canvasZoomScale
            brushColor: surface.brushColor
            brushSize: surface.brushSize
            brushFlow: surface.brushFlow
            brushOpacity: surface.brushOpacity
            brushHardness: surface.brushHardness
            brushSpacing: surface.brushSpacing
            brushSpacingRatio: surface.brushSpacingRatio
            livePreviewFrameIntervalMs: surface.brushLivePreviewFrameIntervalMs
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
                surface.refreshBackgroundLayerThumbnailSource();
                surface.rebuildLayerHierarchyRows();
            }

            onRasterContentChanged: surface.requestBackgroundLayerThumbnailRefresh()
        }

        Timer {
            id: layerThumbnailRefreshTimer
            interval: surface.layerHierarchyThumbnailRefreshDelayMs
            repeat: false
            onTriggered: surface.flushPendingLayerThumbnailRefreshes()
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

        ListModel {
            id: drawableObjectVisualModel
        }

        Repeater {
            parent: canvasSurface
            model: drawableObjectVisualModel

            delegate: Item {
                id: drawableObjectDelegate
                required property int objectId
                required property string objectType
                required property real objectX
                required property real objectY
                required property real objectWidth
                required property real objectHeight
                required property string objectSource
                required property string objectShapeKind
                required property string objectColor
                required property string objectText
                required property real objectFontPixelSize
                readonly property bool isRasterLayer: objectType === "layer"
                readonly property int rasterLayerObjectId: objectId

                z: isRasterLayer && surface.selectedDrawableObjectId === rasterLayerObjectId && (surface.toolMode === "brush" || surface.toolMode === "eraser") ? 4 : 2
                x: objectX
                y: objectY
                width: Math.max(1, objectWidth)
                height: Math.max(1, objectHeight)

                Loader {
                    id: rasterLayerLoader
                    anchors.fill: parent
                    active: drawableObjectDelegate.isRasterLayer
                    sourceComponent: rasterLayerSurfaceComponent

                    onLoaded: {
                        if (item) {
                            surface.registerRasterLayerItem(drawableObjectDelegate.rasterLayerObjectId, item);
                        }
                    }

                    Component.onDestruction: {
                        if (item) {
                            surface.unregisterRasterLayerItem(drawableObjectDelegate.rasterLayerObjectId, item);
                        }
                    }
                }

                Component {
                    id: rasterLayerSurfaceComponent

                    DrawingSurfaceItem {
                        anchors.fill: parent
                        enabled: surface.selectedDrawableObjectId === drawableObjectDelegate.rasterLayerObjectId && (surface.toolMode === "brush" || surface.toolMode === "eraser")
                        objectName: "rasterLayerSurface-" + drawableObjectDelegate.rasterLayerObjectId
                        brushColor: surface.brushColor
                        brushSize: surface.brushSize
                        brushFlow: surface.brushFlow
                        brushOpacity: surface.brushOpacity
                        brushHardness: surface.brushHardness
                        brushSpacing: surface.brushSpacing
                        brushSpacingRatio: surface.brushSpacingRatio
                        livePreviewFrameIntervalMs: surface.brushLivePreviewFrameIntervalMs
                        pressureCurveMinimum: surface.pressureCurveMinimum
                        pressureCurveCenter: surface.pressureCurveCenter
                        pressureCurveMaximum: surface.pressureCurveMaximum
                        stabilizerStrength: surface.stabilizerStrength
                        toolMode: surface.toolMode
                        documentViewModel: surface.documentViewModel
                        viewId: surface.viewId

                        onRasterContentChanged: surface.requestRasterLayerThumbnailRefresh(drawableObjectDelegate.rasterLayerObjectId)
                    }
                }

                Image {
                    anchors.fill: parent
                    visible: drawableObjectDelegate.objectType === "image"
                    source: drawableObjectDelegate.objectType === "image" ? drawableObjectDelegate.objectSource : ""
                    fillMode: Image.Stretch
                    smooth: true
                }

                Canvas {
                    anchors.fill: parent
                    visible: drawableObjectDelegate.objectType === "shape"
                    renderTarget: Canvas.Image

                    onPaint: {
                        const context = getContext("2d");
                        context.clearRect(0, 0, width, height);
                        context.beginPath();
                        surface.traceShapePath(context, drawableObjectDelegate.objectShapeKind, 0, 0, width, height);
                        context.fillStyle = drawableObjectDelegate.objectColor;
                        context.fill();
                    }
                    onVisibleChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    Component.onCompleted: requestPaint()
                }

                Text {
                    anchors.fill: parent
                    visible: drawableObjectDelegate.objectType === "text"
                    text: drawableObjectDelegate.objectText
                    color: drawableObjectDelegate.objectColor
                    font.pixelSize: drawableObjectDelegate.objectFontPixelSize
                    wrapMode: Text.Wrap
                    clip: true
                }
            }
        }

        Rectangle {
            id: drawableObjectSelectionFrame
            parent: canvasSurface
            visible: surface.toolMode === "move" && surface.hasTransformableSelectedDrawableObject()
            z: 5
            x: surface.selectedDrawableObjectProperty("x", 0)
            y: surface.selectedDrawableObjectProperty("y", 0)
            width: Math.max(1, surface.selectedDrawableObjectProperty("width", 1))
            height: Math.max(1, surface.selectedDrawableObjectProperty("height", 1))
            color: "transparent"
            border.width: 1
            border.color: surface.textToolAccentColor

            Repeater {
                model: surface.drawableObjectHandles

                delegate: Rectangle {
                    required property var modelData

                    width: surface.drawableObjectHandleSize
                    height: surface.drawableObjectHandleSize
                    x: drawableObjectSelectionFrame.width * modelData.xRatio - width / 2
                    y: drawableObjectSelectionFrame.height * modelData.yRatio - height / 2
                    color: surface.canvasColor
                    border.width: 1
                    border.color: surface.textToolAccentColor
                }
            }
        }

        MouseArea {
            id: canvasPanMouseArea
            anchors.fill: parent
            z: 6
            enabled: surface.toolMode === "pan"
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            cursorShape: surface.panDraggingActive ? Qt.ClosedHandCursor : Qt.OpenHandCursor

            onPressed: function (mouse) {
                surface.beginPanDrag(mouse.x, mouse.y);
                mouse.accepted = true;
            }

            onPositionChanged: function (mouse) {
                if (surface.panDraggingActive) {
                    surface.updatePanDrag(mouse.x, mouse.y);
                    mouse.accepted = true;
                }
            }

            onReleased: function (mouse) {
                surface.updatePanDrag(mouse.x, mouse.y);
                surface.commitPanDrag();
                mouse.accepted = true;
            }

            onCanceled: surface.cancelPanDrag()
        }
    }

    MouseArea {
        parent: canvasSurface
        anchors.fill: parent
        z: 3
        hoverEnabled: true
        acceptedButtons: surface.canvasMouseAcceptedButtons()
        cursorShape: surface.canvasCursorShape()

        onPressed: function (mouse) {
            if (surface.toolMode === "zoom") {
                surface.beginZoomDrag(mouse.x);
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "move") {
                surface.beginDrawableObjectTransform(mouse.x, mouse.y);
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "shape") {
                const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);
                surface.beginShapeDrag(mouse.x, mouse.y, aspectLocked);
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "fill") {
                surface.fillAt(mouse.x, mouse.y);
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "text") {
                surface.beginTextPlacement(mouse.x, mouse.y);
                mouse.accepted = true;
            }
        }

        onPositionChanged: function (mouse) {
            if (surface.toolMode === "zoom" && surface.zoomDraggingActive) {
                surface.updateZoomDrag(mouse.x);
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "move" && surface.drawableObjectTransformActive) {
                surface.updateDrawableObjectTransform(mouse.x, mouse.y);
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "shape" && surface.shapeDraggingActive) {
                const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);
                surface.updateShapeDrag(mouse.x, mouse.y, aspectLocked);
                mouse.accepted = true;
            }
        }

        onReleased: function (mouse) {
            if (surface.toolMode === "zoom") {
                surface.updateZoomDrag(mouse.x);
                surface.commitZoomDrag();
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "move") {
                surface.updateDrawableObjectTransform(mouse.x, mouse.y);
                surface.commitDrawableObjectTransform();
                mouse.accepted = true;
                return;
            }

            if (surface.toolMode === "shape") {
                const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);
                surface.updateShapeDrag(mouse.x, mouse.y, aspectLocked);
                surface.commitActiveShape();
                mouse.accepted = true;
            }
        }

        onCanceled: {
            surface.cancelActiveShape();
            surface.cancelActiveDrawableObjectTransform();
            surface.cancelPanDrag();
            surface.cancelZoomDrag();
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
        sequences: ["H", "ㅗ"]
        enabled: !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("pan")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["V", "ㅍ"]
        enabled: !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("move")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["Z", "ㅋ"]
        enabled: !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("zoom")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["U", "ㅕ"]
        enabled: !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("shape")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["G", "ㅎ"]
        enabled: !surface.textEditingActive
        onActivated: surface.toolShortcutRequested("fill")
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
        onActivated: surface.activeRasterSurface().undo()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.Redo
        enabled: !surface.textEditingActive
        onActivated: surface.activeRasterSurface().redo()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: ["Delete", "Backspace"]
        enabled: !surface.textEditingActive && !surface.shapeDraggingActive && !surface.drawableObjectTransformActive && surface.hasSelectedDrawableObject()
        onActivated: surface.deleteSelectedDrawableObject()
    }
}
