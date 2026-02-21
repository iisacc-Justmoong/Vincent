import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import LVRS 1.0 as LV

Item {
    id: toolbar
    layer.enabled: true
    layer.smooth: true
    clip: false

    readonly property int spacingSmall: LV.Theme.gap8
    readonly property int spacingMedium: LV.Theme.gap12
    readonly property int spacingLarge: LV.Theme.gap16

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
    readonly property color accentColor: LV.Theme.primary

    implicitHeight: toolbarLayout.implicitHeight + spacingSmall * 4
    implicitWidth: toolbarLayout.implicitWidth + spacingSmall * 4
    height: implicitHeight

    signal newCanvasRequested
    signal clearCanvasRequested
    signal openRequested(string fileUrl)
    signal saveRequested(string fileUrl)
    signal brushSizeChangeRequested(real size)
    signal colorPicked(color swatchColor)
    signal toolSelected(string tool)
    signal freeTransformRequested

    function openFileDialog() {
        openDialog.open()
    }

    function openSaveDialog() {
        saveDialog.open()
    }

    function selectedDialogFileUrl(dialog) {
        const selected = dialog.selectedFile
        return selected ? selected.toString() : ""
    }

    function hasPathExtension(urlString) {
        const pathOnly = urlString.split("?")[0].split("#")[0]
        const lastSlashIndex = pathOnly.lastIndexOf("/")
        const fileName = lastSlashIndex >= 0 ? pathOnly.substring(lastSlashIndex + 1) : pathOnly
        return fileName.lastIndexOf(".") > 0
    }

    function defaultSaveExtension(nameFilter) {
        const suffix = (nameFilter || "").toLowerCase()
        if (suffix.indexOf("jpeg") !== -1 || suffix.indexOf("jpg") !== -1) {
            return ".jpg"
        }
        if (suffix.indexOf("bmp") !== -1) {
            return ".bmp"
        }
        return ".png"
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

    component ToolbarIconButton: LV.IconButton {
        id: control
        property string tooltipText: ""
        property string shortcutText: ""
        property bool active: false

        iconSize: 20
        tone: active ? LV.AbstractButton.Default : LV.AbstractButton.Borderless
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
            const urlString = toolbar.selectedDialogFileUrl(openDialog)
            if (urlString.length) {
                toolbar.openRequested(urlString)
            }
        }
    }

    Dialogs.FileDialog {
        id: saveDialog
        title: qsTr("Save Image As")
        fileMode: Dialogs.FileDialog.SaveFile
        nameFilters: [qsTr("PNG Image (*.png)"), qsTr("JPEG Image (*.jpg *.jpeg)"), qsTr("Bitmap Image (*.bmp)")]
        onAccepted: {
            var urlString = toolbar.selectedDialogFileUrl(saveDialog)
            if (!urlString.length) {
                return
            }

            if (!toolbar.hasPathExtension(urlString)) {
                if (urlString.endsWith("/")) {
                    urlString += "canvas"
                }
                urlString += toolbar.defaultSaveExtension(saveDialog.selectedNameFilter)
            }

            toolbar.saveRequested(urlString)
        }
    }

    Rectangle {
        id: floatingBackground
        anchors.fill: parent
        anchors.leftMargin: toolbar.spacingSmall
        anchors.rightMargin: toolbar.spacingSmall
        anchors.topMargin: toolbar.spacingSmall
        anchors.bottomMargin: toolbar.spacingSmall
        radius: LV.Theme.radiusLg
        color: LV.Theme.panelBackground03
        border.width: 1
        border.color: Qt.rgba(255, 255, 255, 0.08)
    }

    MouseArea {
        id: toolbarEventBlocker
        anchors.fill: floatingBackground
        z: 0
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onPressed: function (mouse) {
            mouse.accepted = true
        }
        onPositionChanged: function (mouse) {
            mouse.accepted = true
        }
        onReleased: function (mouse) {
            mouse.accepted = true
        }
        onWheel: function (wheel) {
            wheel.accepted = true
        }
    }

    LV.HStack {
        id: toolbarLayout
        z: 1
        anchors.fill: floatingBackground
        anchors.leftMargin: toolbar.spacingSmall
        anchors.rightMargin: toolbar.spacingSmall
        anchors.topMargin: toolbar.spacingSmall
        anchors.bottomMargin: toolbar.spacingSmall
        spacing: toolbar.spacingMedium
        alignmentName: "center"

        LV.HStack {
            id: fileActionsRow
            spacing: toolbar.spacingSmall
            Layout.alignment: Qt.AlignVCenter

            ToolbarIconButton {
                iconName: "addFile"
                tooltipText: qsTr("New canvas")
                shortcutText: toolbar.shortcutNew
                Accessible.name: tooltipText
                onClicked: toolbar.newCanvasRequested()
            }

            ToolbarIconButton {
                iconName: "open"
                tooltipText: qsTr("Open image")
                shortcutText: toolbar.shortcutOpen
                Accessible.name: tooltipText
                onClicked: toolbar.openFileDialog()
            }

            ToolbarIconButton {
                iconName: "save"
                tooltipText: qsTr("Save image")
                shortcutText: toolbar.shortcutSave
                Accessible.name: tooltipText
                onClicked: toolbar.openSaveDialog()
            }

            ToolbarIconButton {
                iconName: "delete"
                tooltipText: qsTr("Clear canvas")
                shortcutText: toolbar.shortcutClear
                Accessible.name: tooltipText
                onClicked: toolbar.clearCanvasRequested()
            }
        }

        ToolbarDivider { }

        LV.HStack {
            id: toolSelectionRow
            spacing: toolbar.spacingSmall
            Layout.alignment: Qt.AlignVCenter

            ToolbarIconButton {
                active: toolbar.currentTool === "brush"
                iconName: "edit"
                tooltipText: qsTr("Brush tool")
                shortcutText: "B"
                Accessible.name: tooltipText
                onClicked: toolbar.toolSelected("brush")
            }

            ToolbarIconButton {
                active: toolbar.currentTool === "eraser"
                iconName: "clearOutputs"
                tooltipText: qsTr("Eraser tool")
                shortcutText: "E"
                Accessible.name: tooltipText
                onClicked: toolbar.toolSelected("eraser")
            }

            ToolbarIconButton {
                active: toolbar.currentTool === "grab"
                iconName: "selectAll"
                tooltipText: qsTr("Grab tool")
                shortcutText: "V"
                Accessible.name: tooltipText
                onClicked: toolbar.toolSelected("grab")
            }

            ToolbarIconButton {
                active: toolbar.currentTool === "text"
                iconName: "text"
                tooltipText: qsTr("Text tool")
                shortcutText: "T"
                Accessible.name: tooltipText
                onClicked: toolbar.toolSelected("text")
            }
        }

        ToolbarDivider { }

        LV.HStack {
            id: brushControlsRow
            spacing: toolbar.spacingSmall
            Layout.alignment: Qt.AlignVCenter

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

            ToolbarIconButton {
                iconName: "zoomOut"
                tooltipText: qsTr("Decrease brush size")
                shortcutText: "["
                Accessible.name: tooltipText
                onClicked: toolbar.brushSizeChangeRequested(Math.max(1, toolbar.brushSize - 1))
            }

            ToolbarIconButton {
                iconName: "zoomIn"
                tooltipText: qsTr("Increase brush size")
                shortcutText: "]"
                Accessible.name: tooltipText
                onClicked: toolbar.brushSizeChangeRequested(Math.min(48, toolbar.brushSize + 1))
            }

            Controls.Slider {
                id: sizeSlider
                from: 1
                to: 48
                Layout.preferredWidth: 160
                orientation: Qt.Horizontal
                value: toolbar.brushSize
                hoverEnabled: true
                Accessible.name: qsTr("Brush size")
                onMoved: toolbar.brushSizeChangeRequested(value)
                onValueChanged: {
                    if (pressed || activeFocus) {
                        toolbar.brushSizeChangeRequested(value)
                    }
                }
                Controls.ToolTip.visible: hovered || pressed
                Controls.ToolTip.text: qsTr("%1 px").arg(Math.round(value))

                background: Rectangle {
                    x: sizeSlider.leftPadding
                    y: sizeSlider.topPadding + (sizeSlider.availableHeight - height) / 2
                    width: sizeSlider.availableWidth
                    height: 4
                    radius: 2
                    color: Qt.rgba(255, 255, 255, 0.20)

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, Math.min(parent.width, sizeSlider.position * parent.width))
                        height: parent.height
                        radius: parent.radius
                        color: LV.Theme.primary
                    }
                }

                handle: Rectangle {
                    x: sizeSlider.leftPadding + (sizeSlider.availableWidth - width) * sizeSlider.position
                    y: sizeSlider.topPadding + (sizeSlider.availableHeight - height) / 2
                    implicitWidth: 12
                    implicitHeight: 12
                    radius: 6
                    color: sizeSlider.pressed ? LV.Theme.primary : "#f2f4f7"
                    border.width: 1
                    border.color: sizeSlider.pressed
                        ? Qt.rgba(255, 255, 255, 0.28)
                        : Qt.rgba(0, 0, 0, 0.38)
                }
            }
        }

        LV.Spacer { }

        Item {
            id: paletteContainer
            implicitWidth: paletteRow.implicitWidth
            implicitHeight: paletteRow.implicitHeight
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight

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
