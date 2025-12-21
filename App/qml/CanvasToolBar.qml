import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts

Controls.ToolBar {
    id: toolbar
    background: Item {}
    layer.enabled: true
    layer.smooth: true

    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    readonly property int iconSizeMedium: 24

    property real brushSize: 2
    property color currentColor: "#1a1a1a"
    property var palette: []
    property string currentTool: "brush"
    readonly property color accentColor: (palette && palette.highlight !== undefined) ? palette.highlight : "#2d89ef"

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
        property int iconSize: toolbar.iconSizeMedium
        property int iconBoxSize: toolbar.iconSizeMedium
        property string tooltipText: ""

        hoverEnabled: true
        padding: toolbar.spacingSmall
        implicitWidth: iconBoxSize + padding * 2
        implicitHeight: iconBoxSize + padding * 2

        contentItem: Item {
            implicitWidth: control.iconBoxSize
            implicitHeight: control.iconBoxSize

            Image {
                anchors.centerIn: parent
                width: control.iconSize
                height: control.iconSize
                source: control.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
        }

        background: Rectangle {
            anchors.fill: parent
            radius: toolbar.spacingLarge
            color: control.checked || control.pressed
                ? Qt.rgba(0, 0, 0, 0.15)
                : (control.hovered ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(255, 255, 255, 0.05))
            border.width: control.checked ? 1 : 0
            border.color: toolbar.accentColor
        }

        Controls.ToolTip.visible: control.hovered && Controls.ToolTip.text.length > 0
        Controls.ToolTip.delay: 300
        Controls.ToolTip.text: control.tooltipText.length ? control.tooltipText : control.text
    }

    component ToolbarDivider: Rectangle {
        width: 1
        Layout.preferredHeight: 32
        Layout.alignment: Qt.AlignVCenter
        color: Qt.rgba(255, 255, 255, 0.12)
    }

    component ColorSwatch: Rectangle {
        property color swatchColor: "#ffffff"
        property string swatchLabel: ""

        width: 28
        height: 28
        radius: 4
        color: swatchColor
        border.width: toolbar.currentColor === swatchColor ? 2 : 1
        border.color: toolbar.currentColor === swatchColor ? toolbar.accentColor : "#e0e0e0"

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

    contentItem: Item {
        id: toolbarContent
        anchors.fill: parent
        implicitHeight: toolbarLayout.implicitHeight + toolbar.spacingSmall * 4
        implicitWidth: toolbarLayout.implicitWidth + toolbar.spacingSmall * 4

        Rectangle {
            id: floatingBackground
            anchors.fill: parent
            anchors.leftMargin: toolbar.spacingSmall
            anchors.rightMargin: toolbar.spacingSmall
            anchors.topMargin: toolbar.spacingSmall
            anchors.bottomMargin: toolbar.spacingSmall
            radius: toolbar.spacingLarge * 1.5
            color: Qt.rgba(26 / 255, 26 / 255, 26 / 255, 1.0)
            border.width: 1
            border.color: Qt.rgba(255, 255, 255, 0.08)
        }

        MouseArea {
            id: toolbarEventBlocker
            anchors.fill: floatingBackground
            z: -1
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onPressed: mouse.accepted = true
            onPositionChanged: mouse.accepted = true
            onReleased: mouse.accepted = true
            onCanceled: mouse.accepted = true
            onWheel: wheel.accepted = true
        }

        Item {
            id: layoutWrapper
            anchors.fill: floatingBackground

            RowLayout {
                id: toolbarLayout
                z: 1
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: toolbar.spacingSmall
                anchors.topMargin: toolbar.spacingSmall
                anchors.bottomMargin: toolbar.spacingSmall
                anchors.right: paletteContainer.left
                anchors.rightMargin: toolbar.spacingMedium
                spacing: toolbar.spacingMedium

                RowLayout {
                    id: fileActionsRow
                    spacing: toolbar.spacingSmall
                    Layout.alignment: Qt.AlignVCenter

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/new.svg"
                        tooltipText: qsTr("New canvas")
                        Accessible.name: tooltipText
                        onClicked: toolbar.newCanvasRequested()
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/open.svg"
                        tooltipText: qsTr("Open image")
                        Accessible.name: tooltipText
                        onClicked: toolbar.openFileDialog()
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/save.svg"
                        tooltipText: qsTr("Save image")
                        Accessible.name: tooltipText
                        onClicked: toolbar.openSaveDialog()
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/clear.svg"
                        tooltipText: qsTr("Clear canvas")
                        Accessible.name: tooltipText
                        onClicked: toolbar.clearCanvasRequested()
                    }
                }

                ToolbarDivider { }

                RowLayout {
                    id: toolSelectionRow
                    spacing: toolbar.spacingSmall
                    Layout.alignment: Qt.AlignVCenter

                    ToolbarButton {
                        checkable: true
                        checked: toolbar.currentTool === "brush"
                        iconSource: "qrc:/../resources/icons/brush.svg"
                        tooltipText: qsTr("Brush tool")
                        Accessible.name: tooltipText
                        onClicked: toolbar.toolSelected("brush")
                    }

                    ToolbarButton {
                        checkable: true
                        checked: toolbar.currentTool === "eraser"
                        iconSource: "qrc:/../resources/icons/eraser.svg"
                        tooltipText: qsTr("Eraser tool")
                        Accessible.name: tooltipText
                        onClicked: toolbar.toolSelected("eraser")
                    }

                    ToolbarButton {
                        checkable: true
                        checked: toolbar.currentTool === "grab"
                        iconSource: "qrc:/../resources/icons/grab.svg"
                        tooltipText: qsTr("Grab tool")
                        Accessible.name: tooltipText
                        onClicked: toolbar.toolSelected("grab")
                    }
                }

                ToolbarDivider { }

                Rectangle {
                    id: brushPreview
                    width: 44
                    height: 44
                    radius: 22
                    color: Qt.rgba(255, 255, 255, 0.04)
                    border.width: 1
                    border.color: Qt.rgba(255, 255, 255, 0.15)
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        readonly property real normalized: (toolbar.brushSize - 1) / 47
                        width: 8 + normalized * 24
                        height: width
                        radius: width / 2
                        color: toolbar.currentColor
                        anchors.centerIn: parent
                        border.width: toolbar.currentColor === "#ffffff" ? 1 : 0
                        border.color: Qt.rgba(0, 0, 0, 0.25)
                    }
                }

                RowLayout {
                    id: brushControlsRow
                    spacing: toolbar.spacingSmall
                    Layout.alignment: Qt.AlignVCenter

                    Controls.Slider {
                        id: sizeSlider
                        from: 1
                        to: 48
                        Layout.preferredWidth: 160
                        value: toolbar.brushSize
                        hoverEnabled: true
                        Accessible.name: qsTr("Brush size")
                        onMoved: toolbar.brushSizeChangeRequested(value)
                        onValueChanged: {
                            if (pressed || activeFocus) {
                                toolbar.brushSizeChangeRequested(value);
                            }
                        }
                        Controls.ToolTip.visible: hovered || pressed
                        Controls.ToolTip.text: qsTr("%1 px").arg(Math.round(value))
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/zoom-in.svg"
                        tooltipText: qsTr("Increase brush size")
                        Accessible.name: tooltipText
                        onClicked: toolbar.brushSizeChangeRequested(Math.min(48, toolbar.brushSize + 1))
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/zoom-out.svg"
                        tooltipText: qsTr("Decrease brush size")
                        Accessible.name: tooltipText
                        onClicked: toolbar.brushSizeChangeRequested(Math.max(1, toolbar.brushSize - 1))
                    }
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Item {
                id: paletteContainer
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                anchors.topMargin: toolbar.spacingSmall
                anchors.bottomMargin: toolbar.spacingSmall
                anchors.rightMargin: toolbar.spacingSmall
                width: Math.min(paletteRow.implicitWidth, Math.max(160, parent.width * 0.35))
                clip: true

                Row {
                    id: paletteRow
                    spacing: toolbar.spacingSmall
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right

                    Repeater {
                        model: toolbar.palette
                        delegate: ColorSwatch {
                            swatchColor: modelData.color
                            swatchLabel: modelData.name ?? ""
                        }
                    }
                }
            }
        }
    }
}
