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
    readonly property bool dialogActive: openDialog.visible || saveDialog.visible
    readonly property string modifierKeyLabel: Qt.platform.os === "osx" ? "Cmd" : "Ctrl"
    readonly property string shortcutNew: modifierKeyLabel + "+N"
    readonly property string shortcutOpen: modifierKeyLabel + "+O"
    readonly property string shortcutSave: modifierKeyLabel + "+S"
    readonly property string shortcutClear: modifierKeyLabel + "+Shift+K"
    readonly property color accentColor: (palette && palette.highlight !== undefined) ? palette.highlight : "#2d89ef"

    signal newCanvasRequested
    signal clearCanvasRequested
    signal openRequested(string fileUrl)
    signal saveRequested(string fileUrl)
    signal brushSizeChangeRequested(real size)
    signal colorPicked(color swatchColor)
    signal toolSelected(string tool)
    signal freeTransformRequested

    function openFileDialog() {
        openDialog.open();
    }
    function openSaveDialog() {
        saveDialog.open();
    }

    function selectedDialogFileUrl(dialog) {
        const selected = dialog.selectedFile;
        return selected ? selected.toString() : "";
    }

    function hasPathExtension(urlString) {
        const pathOnly = urlString.split("?")[0].split("#")[0];
        const lastSlashIndex = pathOnly.lastIndexOf("/");
        const fileName = lastSlashIndex >= 0 ? pathOnly.substring(lastSlashIndex + 1) : pathOnly;
        return fileName.lastIndexOf(".") > 0;
    }

    function defaultSaveExtension(nameFilter) {
        const suffix = (nameFilter || "").toLowerCase();
        if (suffix.indexOf("jpeg") !== -1 || suffix.indexOf("jpg") !== -1) {
            return ".jpg";
        }
        if (suffix.indexOf("bmp") !== -1) {
            return ".bmp";
        }
        return ".png";
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.New
        enabled: !toolbar.dialogActive
        onActivated: toolbar.newCanvasRequested()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.Open
        enabled: !toolbar.dialogActive
        onActivated: toolbar.openFileDialog()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequence: StandardKey.Save
        enabled: !toolbar.dialogActive
        onActivated: toolbar.openSaveDialog()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [Qt.platform.os === "osx" ? "Meta+Shift+K" : "Ctrl+Shift+K"]
        enabled: !toolbar.dialogActive
        onActivated: toolbar.clearCanvasRequested()
    }

    component ToolbarButton: Controls.ToolButton {
        id: control
        property url iconSource
        property int iconSize: toolbar.iconSizeMedium
        property int iconBoxSize: toolbar.iconSizeMedium
        property string tooltipText: ""
        property string shortcutText: ""

        hoverEnabled: true
        padding: Math.max(2, Math.round(toolbar.spacingSmall / 2))
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
            radius: 6
            color: control.checked || control.pressed
                ? Qt.rgba(0, 0, 0, 0.15)
                : (control.hovered ? Qt.rgba(0, 0, 0, 0.08) : Qt.rgba(255, 255, 255, 0.05))
            border.width: control.checked ? 1 : 0
            border.color: toolbar.accentColor
        }

        Controls.ToolTip.visible: control.hovered && Controls.ToolTip.text.length > 0
        Controls.ToolTip.delay: 300
        Controls.ToolTip.text: {
            const baseText = control.tooltipText.length ? control.tooltipText : control.text;
            if (!baseText.length) {
                return "";
            }
            return control.shortcutText.length
                ? qsTr("%1 (%2)").arg(baseText).arg(control.shortcutText)
                : baseText;
        }
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

        width: 20
        height: 20
        radius: 6
        color: swatchColor
        border.width: toolbar.currentColor === swatchColor ? 2 : 1
        border.color: toolbar.currentColor === swatchColor ? toolbar.accentColor : "#e0e0e0"

        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 8
            height: parent.height - 8
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
            const urlString = toolbar.selectedDialogFileUrl(openDialog);
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
            var urlString = toolbar.selectedDialogFileUrl(saveDialog);
            if (!urlString.length) {
                return;
            }

            if (!toolbar.hasPathExtension(urlString)) {
                if (urlString.endsWith('/')) {
                    urlString += 'canvas';
                }
                urlString += toolbar.defaultSaveExtension(saveDialog.selectedNameFilter);
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
            radius: 12
            color: Qt.rgba(26 / 255, 26 / 255, 26 / 255, 1.0)
            border.width: 1
            border.color: Qt.rgba(255, 255, 255, 0.08)
        }

        MouseArea {
            id: toolbarEventBlocker
            anchors.fill: floatingBackground
            z: 0
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            onPressed: function (mouse) { mouse.accepted = true; }
            onPositionChanged: function (mouse) { mouse.accepted = true; }
            onReleased: function (mouse) { mouse.accepted = true; }
            onWheel: function (wheel) { wheel.accepted = true; }
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
                        shortcutText: toolbar.shortcutNew
                        Accessible.name: tooltipText
                        onClicked: toolbar.newCanvasRequested()
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/open.svg"
                        tooltipText: qsTr("Open image")
                        shortcutText: toolbar.shortcutOpen
                        Accessible.name: tooltipText
                        onClicked: toolbar.openFileDialog()
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/save.svg"
                        tooltipText: qsTr("Save image")
                        shortcutText: toolbar.shortcutSave
                        Accessible.name: tooltipText
                        onClicked: toolbar.openSaveDialog()
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/clear.svg"
                        tooltipText: qsTr("Clear canvas")
                        shortcutText: toolbar.shortcutClear
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
                        shortcutText: "B"
                        Accessible.name: tooltipText
                        onClicked: toolbar.toolSelected("brush")
                    }

                    ToolbarButton {
                        checkable: true
                        checked: toolbar.currentTool === "eraser"
                        iconSource: "qrc:/../resources/icons/eraser.svg"
                        tooltipText: qsTr("Eraser tool")
                        shortcutText: "E"
                        Accessible.name: tooltipText
                        onClicked: toolbar.toolSelected("eraser")
                    }

                    ToolbarButton {
                        checkable: true
                        checked: toolbar.currentTool === "grab"
                        iconSource: "qrc:/../resources/icons/grab.svg"
                        tooltipText: qsTr("Grab tool")
                        shortcutText: "V"
                        Accessible.name: tooltipText
                        onClicked: toolbar.toolSelected("grab")
                    }

                    ToolbarButton {
                        checkable: true
                        checked: toolbar.currentTool === "text"
                        iconSource: "qrc:/../resources/icons/text.svg"
                        tooltipText: qsTr("Text tool")
                        shortcutText: "T"
                        Accessible.name: tooltipText
                        onClicked: toolbar.toolSelected("text")
                    }
                }

                ToolbarDivider { }

                Rectangle {
                    id: brushPreview
                    implicitWidth: 36
                    implicitHeight: 36
                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: implicitHeight
                    radius: 18
                    color: Qt.rgba(255, 255, 255, 0.04)
                    border.width: 1
                    border.color: Qt.rgba(255, 255, 255, 0.15)
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        readonly property real normalized: (toolbar.brushSize - 1) / 47
                        width: 6 + normalized * 20
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
                        shortcutText: "]"
                        Accessible.name: tooltipText
                        onClicked: toolbar.brushSizeChangeRequested(Math.min(48, toolbar.brushSize + 1))
                    }

                    ToolbarButton {
                        iconSource: "qrc:/../resources/icons/zoom-out.svg"
                        tooltipText: qsTr("Decrease brush size")
                        shortcutText: "["
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
                width: paletteRow.implicitWidth
                clip: false

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
