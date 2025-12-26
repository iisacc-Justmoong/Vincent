import QtQuick
import QtQuick.Controls as Controls
import Vincent 1.0
import "."

Controls.Page {
    id: painterPage

    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    padding: 0

    readonly property var primaryPalette: [
        {
            name: qsTr("Ink Black"),
            color: "#1a1a1a"
        },
        {
            name: qsTr("Signal Red"),
            color: "#e53935"
        },
        {
            name: qsTr("Amber"),
            color: "#fb8c00"
        },
        {
            name: qsTr("Sun Yellow"),
            color: "#fdd835"
        },
        {
            name: qsTr("Leaf Green"),
            color: "#43a047"
        },
        {
            name: qsTr("Sky Blue"),
            color: "#1e88e5"
        },
        {
            name: qsTr("Violet"),
            color: "#5e35b1"
        },
        {
            name: qsTr("Clay"),
            color: "#8d6e63"
        },
        {
            name: qsTr("Pure White"),
            color: "#ffffff"
        },
        {
            name: qsTr("Pitch Black"),
            color: "#000000"
        }
    ]

    readonly property var extendedPalette: [
        {
            name: qsTr("Coral"),
            color: "#ff7043"
        },
        {
            name: qsTr("Rose"),
            color: "#f06292"
        },
        {
            name: qsTr("Lilac"),
            color: "#ba68c8"
        },
        {
            name: qsTr("Cerulean"),
            color: "#0091ea"
        },
        {
            name: qsTr("Seafoam"),
            color: "#26c6da"
        },
        {
            name: qsTr("Forest"),
            color: "#2e7d32"
        },
        {
            name: qsTr("Olive"),
            color: "#827717"
        },
        {
            name: qsTr("Burnt Sienna"),
            color: "#d84315"
        },
        {
            name: qsTr("Slate"),
            color: "#546e7a"
        }
    ]

    readonly property var defaultPalette: PaletteUtils.buildDefaultPalette(primaryPalette, extendedPalette)

    property color brushColor: defaultPalette[0].color
    property real brushSize: 2
    property var colorPalette: defaultPalette
    property string toolMode: "brush"

    signal pageReady

    Component.onCompleted: pageReady()

    function newCanvas() {
        drawingSurface.newCanvas();
    }

    function clearCanvas() {
        drawingSurface.newCanvas();
    }

    function setBrushColor(colorValue) {
        brushColor = colorValue;
    }

    function adjustBrush(delta) {
        brushSize = Math.max(1, Math.min(48, brushSize + delta));
    }

    function toggleFreeTransformMode() {
        drawingSurface.toggleFreeTransformMode();
    }

    function saveCanvasAs(fileUrl) {
        drawingSurface.saveToFile(fileUrl);
    }

    function openImage(fileUrl) {
        drawingSurface.loadImage(fileUrl);
    }

    Item {
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: painterPage.palette.window

            DrawingSurface {
                id: drawingSurface
                anchors.fill: parent
                brushColor: painterPage.brushColor
                brushSize: painterPage.brushSize
                toolMode: painterPage.toolMode
                onBrushDeltaRequested: painterPage.adjustBrush(delta)
            }
        }

        CanvasToolBar {
            id: canvasToolBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: painterPage.spacingSmall
            anchors.leftMargin: painterPage.spacingSmall
            anchors.rightMargin: painterPage.spacingSmall
            z: 10
            brushSize: painterPage.brushSize
            currentColor: painterPage.brushColor
            currentTool: painterPage.toolMode
            palette: painterPage.colorPalette
            onNewCanvasRequested: painterPage.newCanvas()
            onClearCanvasRequested: painterPage.clearCanvas()
            onBrushSizeChangeRequested: function (size) {
                painterPage.brushSize = size;
            }
            onColorPicked: function (swatchColor) {
                painterPage.setBrushColor(swatchColor);
            }
            onToolSelected: function (tool) {
                painterPage.toolMode = tool;
            }
            onSaveRequested: function (fileUrl) {
                painterPage.saveCanvasAs(fileUrl);
            }
            onOpenRequested: function (fileUrl) {
                painterPage.openImage(fileUrl);
            }
            onFreeTransformRequested: painterPage.toggleFreeTransformMode()
        }
    }

}
