import QtQuick

Rectangle {
    id: surface
    color: "white"
    radius: 6
    border.color: "#d0d0d0"
    border.width: 1

    property color brushColor: "#1a1a1a"
    property real brushSize: 2
    property var strokes: []
    property var currentStroke: null
    property string toolMode: "brush"
    property string importedImageSource: ""
    property bool importedImageReady: false
    readonly property bool hasImportedImage: importedImageReady

    signal brushDeltaRequested(int delta)

    function newCanvas() {
        strokes = []
        currentStroke = null
        surface.clearImportedImage()
        paintCanvas.requestPaint()
    }

    function loadImage(fileUrl) {
        var sourceUrl = normalizeUrl(fileUrl)
        if (!sourceUrl) {
            return
        }
        importedImageReady = false
        importedImageNode.originalWidth = 0
        importedImageNode.originalHeight = 0
        importedImageNode.scaleFactor = 1.0
        importedImageNode.x = 0
        importedImageNode.y = 0
        importedImageSource = sourceUrl
        currentStroke = null
        paintCanvas.requestPaint()
    }

    function saveToFile(fileUrl) {
        var path = toLocalPath(fileUrl)
        if (!path) {
            return false
        }
        return paintCanvas.save(path)
    }

    function clearImportedImage() {
        importedImageSource = ""
        importedImageReady = false
        importedImageNode.originalWidth = 0
        importedImageNode.originalHeight = 0
        importedImageNode.scaleFactor = 1.0
        importedImageNode.x = 0
        importedImageNode.y = 0
        paintCanvas.requestPaint()
    }

    function resetImportedImagePlacement() {
        if (importedImageNode.originalWidth <= 0 || importedImageNode.originalHeight <= 0) {
            return
        }
        const fitScale = Math.min(
                    surface.width / importedImageNode.originalWidth,
                    surface.height / importedImageNode.originalHeight,
                    1)
        importedImageNode.scaleFactor = fitScale
        importedImageNode.x = (surface.width - importedImageNode.width) / 2
        importedImageNode.y = (surface.height - importedImageNode.height) / 2
        importedImageReady = true
        paintCanvas.requestPaint()
    }

    Canvas {
        id: paintCanvas
        anchors.fill: parent
        renderTarget: Canvas.Image

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#ffffff"
            ctx.fillRect(0, 0, width, height)

            if (surface.hasImportedImage && importedImage.status === Image.Ready) {
                ctx.drawImage(importedImage, importedImageNode.x, importedImageNode.y, importedImageNode.width, importedImageNode.height)
            }

            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            for (var i = 0; i < surface.strokes.length; ++i) {
                var stroke = surface.strokes[i]
                if (!stroke || stroke.points.length === 0) {
                    continue
                }

                if (stroke.points.length === 1) {
                    var point = stroke.points[0]
                    ctx.beginPath()
                    ctx.fillStyle = stroke.color
                    ctx.arc(point.x, point.y, stroke.size / 2, 0, Math.PI * 2)
                    ctx.fill()
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
            }
        }
    }

    Item {
        id: importedImageLayer
        anchors.fill: parent
        visible: surface.hasImportedImage
        enabled: surface.toolMode === "grab"
        z: 2

        HoverHandler {
            acceptedDevices: PointerDevice.Mouse
            cursorShape: surface.toolMode === "grab"
                ? (imageDragHandler.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor)
                : Qt.ArrowCursor
        }

        Item {
            id: importedImageNode
            property real originalWidth: 0
            property real originalHeight: 0
            property real scaleFactor: 1.0
            width: originalWidth > 0 ? originalWidth * scaleFactor : 0
            height: originalHeight > 0 ? originalHeight * scaleFactor : 0
            visible: originalWidth > 0 && originalHeight > 0

            Image {
                id: importedImage
                anchors.fill: parent
                source: surface.importedImageSource
                asynchronous: true
                smooth: true
                fillMode: Image.Stretch
                opacity: surface.toolMode === "grab" ? 0.25 : 0
                visible: status === Image.Ready
                onStatusChanged: {
                    if (status === Image.Ready) {
                        importedImageNode.originalWidth = implicitWidth
                        importedImageNode.originalHeight = implicitHeight
                        surface.resetImportedImagePlacement()
                    } else {
                        surface.importedImageReady = false
                    }
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: surface.toolMode === "grab"
                color: "transparent"
                border.color: Qt.rgba(255, 255, 255, 0.35)
                border.width: 1
            }

            DragHandler {
                id: imageDragHandler
                target: importedImageNode
                acceptedButtons: Qt.LeftButton
                enabled: surface.hasImportedImage && surface.toolMode === "grab"
                onTranslationChanged: paintCanvas.requestPaint()
                onActiveChanged: {
                    if (!active) {
                        paintCanvas.requestPaint()
                    }
                }
            }

            WheelHandler {
                acceptedModifiers: Qt.ControlModifier
                enabled: surface.hasImportedImage && surface.toolMode === "grab"
                onWheel: {
                    if (importedImageNode.originalWidth <= 0) {
                        return
                    }
                    const factor = wheel.angleDelta.y > 0 ? 1.1 : 0.9
                    const newScale = Math.max(0.1, Math.min(5, importedImageNode.scaleFactor * factor))
                    if (newScale === importedImageNode.scaleFactor) {
                        return
                    }
                    const centerX = importedImageNode.x + importedImageNode.width / 2
                    const centerY = importedImageNode.y + importedImageNode.height / 2
                    importedImageNode.scaleFactor = newScale
                    importedImageNode.x = centerX - importedImageNode.width / 2
                    importedImageNode.y = centerY - importedImageNode.height / 2
                    paintCanvas.requestPaint()
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
        cursorShape: surface.toolMode === "eraser" ? Qt.PointingHandCursor : Qt.CrossCursor

        onPressed: function(mouse) {
            if (surface.toolMode === "grab") {
                mouse.accepted = false
                return
            }
            if (mouse.button !== Qt.LeftButton) {
                mouse.accepted = false
                return
            }
            var colorValue
            if (surface.toolMode === "eraser") {
                colorValue = "#ffffff"
            } else {
                colorValue = typeof surface.brushColor === "string" ? surface.brushColor : surface.brushColor.toString()
            }
            surface.currentStroke = {
                color: colorValue,
                size: surface.brushSize,
                points: [ { x: mouse.x, y: mouse.y } ]
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
