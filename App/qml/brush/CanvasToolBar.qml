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
    property real brushFlow: 1
    property real brushOpacity: 1
    property real brushHardness: 1
    property real brushSpacing: 0
    property real brushSpacingRatio: 0
    property real pressureCurveMinimum: 0
    property real pressureCurveCenter: 0.5
    property real pressureCurveMaximum: 1
    property real stabilizerStrength: 0
    property color currentColor: "#1a1a1a"
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
    signal brushPropertyChangeRequested(string propertyName, real value)
    signal colorPicked(color swatchColor)
    signal toolSelected(string tool)

    function openFileDialog() {
        openDialog.open();
    }

    function openSaveDialog() {
        saveDialog.open();
    }

    function openBrushSettingsMenu(triggerItem) {
        const mappedPosition = triggerItem.mapToItem(toolbar, 0, triggerItem.height + toolbar.spacingSmall);
        brushSettingsMenu.x = Math.max(toolbar.spacingSmall, Math.min(mappedPosition.x, toolbar.width - brushSettingsMenu.width - toolbar.spacingSmall));
        brushSettingsMenu.y = mappedPosition.y;
        brushSettingsMenu.open();
    }

    function openColorPickerMenu(triggerItem) {
        const mappedPosition = triggerItem.mapToItem(toolbar, 0, triggerItem.height + toolbar.spacingSmall);
        colorPickerMenu.x = Math.max(toolbar.spacingSmall, Math.min(mappedPosition.x, toolbar.width - colorPickerMenu.width - toolbar.spacingSmall));
        colorPickerMenu.y = mappedPosition.y;
        hslTriangleColorPicker.selectedColor = toolbar.currentColor;
        colorPickerMenu.open();
    }

    function activateBrushTool(triggerItem) {
        if (toolbar.currentTool === "brush") {
            toolbar.openBrushSettingsMenu(triggerItem);
            return;
        }
        toolbar.toolSelected("brush");
    }

    function requestBrushPropertyChange(propertyName, value) {
        if (propertyName === "brushSize") {
            brushSizeChangeRequested(value);
            return;
        }
        brushPropertyChangeRequested(propertyName, value);
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

    component ToolbarDivider: Rectangle {
        width: 1
        Layout.preferredHeight: 32
        Layout.alignment: Qt.AlignVCenter
        color: Qt.rgba(255, 255, 255, 0.12)
    }

    component BrushPropertySlider: Item {
        id: brushPropertySlider

        property string label: ""
        property string propertyName: ""
        property real value: 0
        property real from: 0
        property real to: 1
        property real stepSize: 0.01
        property int decimals: 0
        property real displayScale: 1
        property string suffix: ""
        property real lastRequestedValue: -999999

        Layout.fillWidth: true
        implicitWidth: 300
        implicitHeight: sliderColumn.implicitHeight

        function formattedValue(rawValue) {
            const scaledValue = rawValue * displayScale;
            const textValue = decimals === 0 ? Math.round(scaledValue).toString() : scaledValue.toFixed(decimals);
            return textValue + suffix;
        }

        function requestValue(rawValue) {
            if (Math.abs(lastRequestedValue - rawValue) < 0.0001) {
                return;
            }
            lastRequestedValue = rawValue;
            toolbar.requestBrushPropertyChange(propertyName, rawValue);
        }

        ColumnLayout {
            id: sliderColumn
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: toolbar.spacingSmall

            RowLayout {
                Layout.fillWidth: true
                spacing: toolbar.spacingSmall

                Controls.Label {
                    text: brushPropertySlider.label
                    color: "#f2f4f7"
                    font.pixelSize: 12
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Controls.Label {
                    text: brushPropertySlider.formattedValue(propertySlider.value)
                    color: "#d7dde6"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: 52
                }
            }

            Controls.Slider {
                id: propertySlider
                from: brushPropertySlider.from
                to: brushPropertySlider.to
                stepSize: brushPropertySlider.stepSize
                value: brushPropertySlider.value
                hoverEnabled: true
                Layout.fillWidth: true
                Accessible.name: brushPropertySlider.label
                onMoved: brushPropertySlider.requestValue(value)
                onValueChanged: {
                    if (pressed || activeFocus) {
                        brushPropertySlider.requestValue(value);
                    }
                }
                onPressedChanged: {
                    if (!pressed) {
                        brushPropertySlider.lastRequestedValue = -999999;
                    }
                }

                background: Rectangle {
                    x: propertySlider.leftPadding
                    y: propertySlider.topPadding + (propertySlider.availableHeight - height) / 2
                    width: propertySlider.availableWidth
                    height: 4
                    radius: 2
                    color: Qt.rgba(255, 255, 255, 0.18)

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, Math.min(parent.width, propertySlider.position * parent.width))
                        height: parent.height
                        radius: parent.radius
                        color: LV.Theme.primary
                    }
                }

                handle: Rectangle {
                    x: propertySlider.leftPadding + (propertySlider.availableWidth - width) * propertySlider.position
                    y: propertySlider.topPadding + (propertySlider.availableHeight - height) / 2
                    implicitWidth: 12
                    implicitHeight: 12
                    radius: 6
                    color: propertySlider.pressed ? LV.Theme.primary : "#f2f4f7"
                    border.width: 1
                    border.color: propertySlider.pressed ? Qt.rgba(255, 255, 255, 0.28) : Qt.rgba(0, 0, 0, 0.38)
                }
            }
        }
    }

    Dialogs.FileDialog {
        id: openDialog
        title: qsTr("Open Image")
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff)")]
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
                if (urlString.endsWith("/")) {
                    urlString += "canvas";
                }
                urlString += toolbar.defaultSaveExtension(saveDialog.selectedNameFilter);
            }

            toolbar.saveRequested(urlString);
        }
    }

    Controls.Popup {
        id: colorPickerMenu
        width: 320
        padding: toolbar.spacingMedium
        modal: false
        focus: true
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside | Controls.Popup.CloseOnReleaseOutside

        background: Rectangle {
            radius: LV.Theme.radiusLg
            color: LV.Theme.panelBackground06
            border.width: 1
            border.color: Qt.rgba(255, 255, 255, 0.12)
        }

        contentItem: HslTriangleColorPicker {
            id: hslTriangleColorPicker
            selectedColor: toolbar.currentColor
            onColorSelected: selectedColor => toolbar.colorPicked(selectedColor)
        }
    }

    Controls.Popup {
        id: brushSettingsMenu
        width: 340
        z: 20
        padding: toolbar.spacingMedium
        modal: false
        focus: true
        closePolicy: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutside | Controls.Popup.CloseOnReleaseOutside

        background: Rectangle {
            radius: LV.Theme.radiusLg
            color: LV.Theme.panelBackground06
            border.width: 1
            border.color: Qt.rgba(255, 255, 255, 0.12)
        }

        contentItem: ColumnLayout {
            spacing: toolbar.spacingMedium

            Controls.Label {
                text: qsTr("Brush")
                color: "#ffffff"
                font.pixelSize: 13
                font.bold: true
                Layout.fillWidth: true
            }

            BrushPropertySlider {
                label: qsTr("Size")
                propertyName: "brushSize"
                value: toolbar.brushSize
                from: 1
                to: 48
                stepSize: 1
                suffix: qsTr(" px")
            }

            BrushPropertySlider {
                label: qsTr("Flow")
                propertyName: "brushFlow"
                value: toolbar.brushFlow
                displayScale: 100
                suffix: qsTr("%")
            }

            BrushPropertySlider {
                label: qsTr("Opacity")
                propertyName: "brushOpacity"
                value: toolbar.brushOpacity
                displayScale: 100
                suffix: qsTr("%")
            }

            BrushPropertySlider {
                label: qsTr("Hardness")
                propertyName: "brushHardness"
                value: toolbar.brushHardness
                from: 0.01
                displayScale: 100
                suffix: qsTr("%")
            }

            BrushPropertySlider {
                label: qsTr("Spacing")
                propertyName: "brushSpacing"
                value: toolbar.brushSpacing
                from: 0
                to: 64
                stepSize: 0.5
                decimals: 1
                suffix: qsTr(" px")
            }

            BrushPropertySlider {
                label: qsTr("Spacing Ratio")
                propertyName: "brushSpacingRatio"
                value: toolbar.brushSpacingRatio
                displayScale: 100
                suffix: qsTr("%")
            }

            BrushPropertySlider {
                label: qsTr("Pressure Minimum")
                propertyName: "pressureCurveMinimum"
                value: toolbar.pressureCurveMinimum
                displayScale: 100
                suffix: qsTr("%")
            }

            BrushPropertySlider {
                label: qsTr("Pressure Center")
                propertyName: "pressureCurveCenter"
                value: toolbar.pressureCurveCenter
                displayScale: 100
                suffix: qsTr("%")
            }

            BrushPropertySlider {
                label: qsTr("Pressure Maximum")
                propertyName: "pressureCurveMaximum"
                value: toolbar.pressureCurveMaximum
                displayScale: 100
                suffix: qsTr("%")
            }

            BrushPropertySlider {
                label: qsTr("Stabilizer")
                propertyName: "stabilizerStrength"
                value: toolbar.stabilizerStrength
                displayScale: 100
                suffix: qsTr("%")
            }
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
            mouse.accepted = true;
        }
        onPositionChanged: function (mouse) {
            mouse.accepted = true;
        }
        onReleased: function (mouse) {
            mouse.accepted = true;
        }
        onWheel: function (wheel) {
            wheel.accepted = true;
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

            LV.IconButton {
                iconSize: 20
                tone: LV.AbstractButton.Borderless
                iconName: "addFile"
                Accessible.name: qsTr("New canvas")
                onClicked: toolbar.newCanvasRequested()
            }

            LV.IconButton {
                iconSize: 20
                tone: LV.AbstractButton.Borderless
                iconName: "generalopen"
                Accessible.name: qsTr("Open raster image")
                onClicked: toolbar.openFileDialog()
            }

            LV.IconButton {
                iconSize: 20
                tone: LV.AbstractButton.Borderless
                iconName: "generalsave"
                Accessible.name: qsTr("Save image")
                onClicked: toolbar.openSaveDialog()
            }

            LV.IconButton {
                iconSize: 20
                tone: LV.AbstractButton.Borderless
                iconName: "generaldelete"
                Accessible.name: qsTr("Clear canvas")
                onClicked: toolbar.clearCanvasRequested()
            }
        }

        ToolbarDivider {}

        LV.HStack {
            id: toolSelectionRow
            spacing: toolbar.spacingSmall
            Layout.alignment: Qt.AlignVCenter

            LV.IconButton {
                id: brushToolButton
                iconSize: 20
                tone: toolbar.currentTool === "brush" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless
                iconName: "generaledit"
                Accessible.name: qsTr("Brush tool")
                onClicked: toolbar.activateBrushTool(brushToolButton)
            }

            LV.IconButton {
                iconSize: 20
                tone: toolbar.currentTool === "eraser" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless
                iconName: "eraser"
                Accessible.name: qsTr("Eraser tool")
                onClicked: toolbar.toolSelected("eraser")
            }
        }

        ToolbarDivider {}

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

            LV.IconButton {
                iconSize: 20
                tone: LV.AbstractButton.Borderless
                iconName: "imagezoomOut"
                Accessible.name: qsTr("Decrease brush size")
                onClicked: toolbar.brushSizeChangeRequested(Math.max(1, toolbar.brushSize - 1))
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
                        toolbar.brushSizeChangeRequested(value);
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
                    border.color: sizeSlider.pressed ? Qt.rgba(255, 255, 255, 0.28) : Qt.rgba(0, 0, 0, 0.38)
                }
            }

            LV.IconButton {
                iconSize: 20
                tone: LV.AbstractButton.Borderless
                iconName: "imagezoomIn"
                Accessible.name: qsTr("Increase brush size")
                onClicked: toolbar.brushSizeChangeRequested(Math.min(48, toolbar.brushSize + 1))
            }
        }

        LV.Spacer {}

        Rectangle {
            id: currentColorButton
            implicitWidth: 36
            implicitHeight: 36
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            radius: 18
            color: colorButtonMouseArea.containsMouse || colorPickerMenu.opened ? LV.Theme.panelBackground10 : Qt.rgba(255, 255, 255, 0.04)
            border.width: colorPickerMenu.opened ? 2 : 1
            border.color: colorPickerMenu.opened ? toolbar.accentColor : Qt.rgba(255, 255, 255, 0.15)

            Canvas {
                id: colorPickerBall
                anchors.centerIn: parent
                width: 24
                height: 24
                renderTarget: Canvas.Image

                function paintRgbRainbowBall() {
                    const context = getContext("2d");
                    context.clearRect(0, 0, width, height);

                    const centerX = width / 2;
                    const centerY = height / 2;
                    const radius = Math.min(width, height) / 2 - 1;
                    for (let y = 0; y < height; ++y) {
                        for (let x = 0; x < width; ++x) {
                            const dx = x + 0.5 - centerX;
                            const dy = y + 0.5 - centerY;
                            const distance = Math.sqrt(dx * dx + dy * dy);
                            if (distance > radius) {
                                continue;
                            }

                            const hue = (Math.atan2(dy, dx) / (Math.PI * 2) + 1) % 1;
                            const saturation = Math.min(1, distance / radius);
                            const lightness = 0.58 - saturation * 0.08;
                            context.fillStyle = Qt.hsla(hue, saturation, lightness, 1).toString();
                            context.fillRect(x, y, 1, 1);
                        }
                    }

                    context.save();
                    context.beginPath();
                    context.arc(centerX, centerY, radius, 0, Math.PI * 2);
                    context.clip();

                    var highlight = context.createRadialGradient(centerX - radius * 0.4, centerY - radius * 0.45, 0, centerX - radius * 0.35, centerY - radius * 0.35, radius * 0.75);
                    highlight.addColorStop(0, "rgba(255, 255, 255, 0.62)");
                    highlight.addColorStop(0.45, "rgba(255, 255, 255, 0.16)");
                    highlight.addColorStop(1, "rgba(255, 255, 255, 0)");
                    context.fillStyle = highlight;
                    context.fillRect(0, 0, width, height);

                    var shade = context.createRadialGradient(centerX + radius * 0.42, centerY + radius * 0.48, 0, centerX + radius * 0.2, centerY + radius * 0.2, radius * 1.2);
                    shade.addColorStop(0, "rgba(0, 0, 0, 0.18)");
                    shade.addColorStop(1, "rgba(0, 0, 0, 0)");
                    context.fillStyle = shade;
                    context.fillRect(0, 0, width, height);

                    context.restore();
                    context.beginPath();
                    context.arc(centerX, centerY, radius, 0, Math.PI * 2);
                    context.lineWidth = 1;
                    context.strokeStyle = "rgba(255, 255, 255, 0.55)";
                    context.stroke();
                }

                onPaint: colorPickerBall.paintRgbRainbowBall()
            }

            Rectangle {
                anchors.centerIn: parent
                width: 28
                height: 28
                radius: 14
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(255, 255, 255, 0.22)
            }

            MouseArea {
                id: colorButtonMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: toolbar.openColorPickerMenu(currentColorButton)
                Accessible.name: qsTr("Brush color")
            }

            Controls.ToolTip.visible: colorButtonMouseArea.containsMouse
            Controls.ToolTip.text: qsTr("Brush color")
        }
    }
}
