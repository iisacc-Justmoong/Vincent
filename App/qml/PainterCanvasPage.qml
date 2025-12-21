import QtQuick
import QtQuick.Controls as Controls
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

    readonly property var defaultPalette: (function () {
        var merged = [];
        var seen = {};

        function appendEntry(entry) {
            if (!entry || !entry.color) {
                return;
            }
            var key = entry.color.toString().toLowerCase();
            if (seen[key]) {
                return;
            }
            seen[key] = true;
            merged.push(entry);
        }

        function hexToRgb(value) {
            var hex = value ? value.toString() : "#000000";
            if (hex.indexOf("#") === 0) {
                hex = hex.substring(1);
            }
            if (hex.length === 3) {
                var expanded = "";
                for (var k = 0; k < hex.length; ++k) {
                    expanded += hex[k] + hex[k];
                }
                hex = expanded;
            }
            while (hex.length < 6) {
                hex += "0";
            }
            var r = parseInt(hex.substring(0, 2), 16);
            var g = parseInt(hex.substring(2, 4), 16);
            var b = parseInt(hex.substring(4, 6), 16);
            return {
                r: isNaN(r) ? 0 : r,
                g: isNaN(g) ? 0 : g,
                b: isNaN(b) ? 0 : b
            };
        }

        function rgbToHsl(rgb) {
            var r = rgb.r / 255;
            var g = rgb.g / 255;
            var b = rgb.b / 255;
            var max = Math.max(r, g, b);
            var min = Math.min(r, g, b);
            var l = (max + min) / 2;
            var h = 0;
            var s = 0;

            if (max !== min) {
                var d = max - min;
                s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
                switch (max) {
                case r:
                    h = (g - b) / d + (g < b ? 6 : 0);
                    break;
                case g:
                    h = (b - r) / d + 2;
                    break;
                default:
                    h = (r - g) / d + 4;
                    break;
                }
                h /= 6;
            }

            return {
                h: Math.round(h * 360),
                s: Math.round(s * 100),
                l: Math.round(l * 100)
            };
        }

        for (var i = 0; i < primaryPalette.length; ++i) {
            appendEntry(primaryPalette[i]);
        }
        for (var j = 0; j < extendedPalette.length; ++j) {
            appendEntry(extendedPalette[j]);
        }

        var colored = [];
        var neutrals = [];

        for (var idx = 0; idx < merged.length; ++idx) {
            var entry = merged[idx];
            var hsl = rgbToHsl(hexToRgb(entry.color));
            var bucket = {
                entry: entry,
                hue: hsl.h,
                saturation: hsl.s,
                lightness: hsl.l
            };
            if (bucket.saturation < 15) {
                neutrals.push(bucket);
            } else {
                colored.push(bucket);
            }
        }

        neutrals.sort(function (a, b) {
            return a.lightness - b.lightness;
        });
        colored.sort(function (a, b) {
            if (a.hue === b.hue) {
                return a.lightness - b.lightness;
            }
            return a.hue - b.hue;
        });

        var result = [];
        for (var n = 0; n < neutrals.length; ++n) {
            if (neutrals[n].lightness < 50) {
                result.push(neutrals[n].entry);
            }
        }
        for (var c = 0; c < colored.length; ++c) {
            result.push(colored[c].entry);
        }
        for (var m = 0; m < neutrals.length; ++m) {
            if (neutrals[m].lightness >= 50) {
                result.push(neutrals[m].entry);
            }
        }

        return result;
    })()

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
