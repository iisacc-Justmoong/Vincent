import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts

Controls.ToolBar {
    id: toolbar

    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    readonly property int iconSizeMedium: 24

    property real brushSize: 2
    property color currentColor: "#1a1a1a"
    property var palette: []
    property string currentTool: "brush"

    signal newCanvasRequested
    signal clearCanvasRequested
    signal openRequested(string fileUrl)
    signal saveRequested(string fileUrl)
    signal brushSizeChangeRequested(real size)
    signal colorPicked(color swatchColor)
    signal toolSelected(string tool)

    function openFileDialog() {
        openDialog.open();
    }
    function openSaveDialog() {
        saveDialog.open();
    }

    component ToolbarButton: Controls.ToolButton {
        id: control
        property url iconSource
        property bool showText: true
        property int iconSize: toolbar.iconSizeMedium
        property int iconBoxSize: toolbar.iconSizeMedium
        property int contentSpacing: toolbar.spacingSmall

        padding: toolbar.spacingSmall

        contentItem: RowLayout {
            spacing: control.showText ? control.contentSpacing : 0
            Layout.alignment: Qt.AlignVCenter

            Item {
                width: control.iconBoxSize
                height: control.iconBoxSize
                Layout.alignment: Qt.AlignVCenter

                Image {
                    anchors.centerIn: parent
                    width: control.iconSize
                    height: control.iconSize
                    source: control.iconSource
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }
            }

            Controls.Label {
                text: control.text
                Layout.alignment: Qt.AlignVCenter
                visible: control.showText
                Layout.preferredWidth: control.showText ? implicitWidth : 0
                Layout.preferredHeight: control.showText ? implicitHeight : 0
            }
        }
    }

    Dialogs.FileDialog {
        id: openDialog
        title: qsTr("Open Image")
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.gif)")]
        onAccepted: {
            const selected = openDialog.selectedFile || openDialog.fileUrl;
            const urlString = selected ? selected.toString() : "";
            if (urlString.length) {
                toolbar.openRequested(urlString);
            }
        }
    }

    Dialogs.FileDialog {
        id: saveDialog
        title: qsTr("Save Image As")
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: [qsTr("PNG Image (*.png)"), qsTr("JPEG Image (*.jpg *.jpeg)"), qsTr("Bitmap Image (*.bmp)")]
        onAccepted: {
            const selected = saveDialog.selectedFile || saveDialog.fileUrl;
            var urlString = selected ? selected.toString() : "";
            if (!urlString.length) {
                return;
            }

            if (!urlString.includes('.')) {
                if (urlString.endsWith('/')) {
                    urlString += 'canvas';
                }
                const suffix = saveDialog.selectedNameFilter.toLowerCase();
                if (suffix.indexOf('jpeg') !== -1 || suffix.indexOf('jpg') !== -1) {
                    urlString += '.jpg';
                } else if (suffix.indexOf('bmp') !== -1) {
                    urlString += '.bmp';
                } else {
                    urlString += '.png';
                }
            }

            toolbar.saveRequested(urlString);
        }
    }

    contentItem: RowLayout {
        spacing: toolbar.spacingMedium

        ToolbarButton {
            id: newButton

            text: qsTr("New")
            iconSource: "qrc:/../resources/icons/new.svg"
            Accessible.name: text
            onClicked: toolbar.newCanvasRequested()
        }

        ToolbarButton {
            id: openButton

            text: qsTr("Open")
            iconSource: "qrc:/../resources/icons/open.svg"
            Accessible.name: text
            onClicked: toolbar.openFileDialog()
        }

        ToolbarButton {
            id: saveButton

            text: qsTr("Save")
            iconSource: "qrc:/../resources/icons/save.svg"
            Accessible.name: text
            onClicked: toolbar.openSaveDialog()
        }

        ToolbarButton {
            id: clearButton

            text: qsTr("Clear")
            iconSource: "qrc:/../resources/icons/clear.svg"
            Accessible.name: text
            onClicked: toolbar.clearCanvasRequested()
        }

        Rectangle {
            visible: true
            Layout.fillHeight: true
            width: 1
            color: Qt.rgba(0, 0, 0, 0.2)
        }

        RowLayout {
            spacing: toolbar.spacingSmall

            ToolbarButton {
                checkable: true
                checked: toolbar.currentTool === "brush"
                iconSource: "qrc:/../resources/icons/brush.svg"
                showText: false
                Accessible.name: qsTr("Brush tool")
                onClicked: toolbar.toolSelected("brush")
            }

            ToolbarButton {
                checkable: true
                checked: toolbar.currentTool === "eraser"
                iconSource: "qrc:/../resources/icons/eraser.svg"
                showText: false
                Accessible.name: qsTr("Eraser tool")
                onClicked: toolbar.toolSelected("eraser")
            }
        }

        Rectangle {
            visible: true
            Layout.fillHeight: true
            width: 1
            color: Qt.rgba(0, 0, 0, 0.2)
        }

        Controls.Label {
            text: qsTr("Brush")
            font.bold: true
        }

        RowLayout {
            spacing: toolbar.spacingSmall

            Controls.Slider {
                id: sizeSlider
                from: 1
                to: 48
                Layout.preferredWidth: 160
                value: toolbar.brushSize
                onMoved: toolbar.brushSizeChangeRequested(value)
                onValueChanged: {
                    if (pressed || activeFocus) {
                        toolbar.brushSizeChangeRequested(value);
                    }
                }
            }

            ToolbarButton {
                iconSource: "qrc:/../resources/icons/zoom-in.svg"
                showText: false
                onClicked: toolbar.brushSizeChangeRequested(Math.min(48, toolbar.brushSize + 1))
                Accessible.name: qsTr("Increase brush size")
            }

            ToolbarButton {
                iconSource: "qrc:/../resources/icons/zoom-out.svg"
                showText: false
                onClicked: toolbar.brushSizeChangeRequested(Math.max(1, toolbar.brushSize - 1))
                Accessible.name: qsTr("Decrease brush size")
            }
        }

        Controls.Label {
            text: qsTr("%1 px").arg(Math.round(toolbar.brushSize))
            width: 120
        }

        Item {
            Layout.fillWidth: true
        } //For Fixed Layout

        Repeater {
            model: toolbar.palette
            delegate: Rectangle {
                readonly property color swatchColor: modelData.color
                readonly property string swatchLabel: modelData.name ?? ""
                width: 28
                height: 28
                radius: 4
                color: swatchColor
                border.width: toolbar.currentColor === swatchColor ? 2 : 1
                border.color: toolbar.currentColor === swatchColor
                    ? ((toolbar.palette && toolbar.palette.highlight !== undefined) ? toolbar.palette.highlight : "#2d89ef")
                    : "#e0e0e0"

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width - 12
                    height: parent.height - 12
                    visible: swatchColor === "#ffffff"
                    color: "transparent"
                    border.color: "#b0b0b0"
                    border.width: 1
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: toolbar.colorPicked(swatchColor)
                    Accessible.name: swatchLabel.length ? swatchLabel : qsTr("Brush color")
                }
            }
        }
    }
}
