pragma ComponentBehavior: Bound

import QtQuick
import LVRS 1.0 as LV
import "../brush" as BrushUi
import "../painting" as Painting

Item {
    id: painterPage

    readonly property int spacingSmall: LV.Theme.gap8
    readonly property int spacingMedium: LV.Theme.gap12
    readonly property int spacingLarge: LV.Theme.gap16
    readonly property string viewId: "PainterCanvasPage"
    readonly property real maximumAntialiasingBrushHardness: 1
    readonly property color toolbarBackgroundColor: LV.Theme.panelBackground03
    readonly property int toolbarTopMargin: topChromeReservedHeight
    readonly property int fallbackNewCanvasWidth: 1024
    readonly property int fallbackNewCanvasHeight: 768
    readonly property int layerPanelWidth: Math.min(260, Math.max(220, Math.round(width * 0.22)))
    readonly property int layerPanelTopMargin: toolbarTopMargin + canvasToolBar.height
    readonly property int layerRenameActivationWindowMs: 500
    readonly property int layerRenameRepeatActivationThreshold: 3

    property int topChromeReservedHeight: 0
    property var vm: null
    property bool layerRenameActive: false
    property string layerRenameKey: ""
    property string layerRenameActivationKey: ""
    property int layerRenameActivationCount: 0
    property real layerRenameLastActivationMs: 0
    property real layerRenameEditorX: 0
    property real layerRenameEditorY: 0
    property real layerRenameEditorHeight: 20

    signal pageReady

    function refreshViewModel() {
        painterPage.vm = LV.ViewModels.getForView(painterPage.viewId);
    }

    function bindViewModel() {
        if (!LV.ViewModels.bindView(painterPage.viewId, "CanvasDocument", true)) {
            console.warn(LV.ViewModels.lastError);
        }
        painterPage.refreshViewModel();
    }

    function updateDocumentProperty(propertyName, value) {
        if (!LV.ViewModels.updateProperty(painterPage.viewId, propertyName, value)) {
            console.warn(LV.ViewModels.lastError);
        }
    }

    Component.onCompleted: {
        painterPage.bindViewModel();
        pageReady();
    }

    Component.onDestruction: {
        LV.ViewModels.unbindView(painterPage.viewId);
    }

    Connections {
        target: LV.ViewModels

        function onViewsChanged() {
            painterPage.refreshViewModel();
        }

        function onBindingsChanged() {
            painterPage.refreshViewModel();
        }
    }

    function newCanvas(canvasWidth, canvasHeight) {
        drawingSurface.newCanvas(canvasWidth, canvasHeight);
    }

    function clearCanvas() {
        drawingSurface.clearCanvas();
    }

    function setBrushColor(colorValue) {
        painterPage.updateDocumentProperty("brushColor", colorValue);
    }

    function adjustBrush(delta) {
        const baseSize = painterPage.vm ? painterPage.vm.brushSize : 2;
        painterPage.updateDocumentProperty("brushSize", Math.max(1, Math.min(48, baseSize + delta)));
    }

    function setBrushSize(size) {
        painterPage.updateDocumentProperty("brushSize", size);
    }

    function setBrushProperty(propertyName, value) {
        painterPage.updateDocumentProperty(propertyName, value);
    }

    function setToolMode(tool) {
        painterPage.updateDocumentProperty("toolMode", tool);
    }

    function setShapeKind(shapeKind) {
        painterPage.updateDocumentProperty("shapeKind", shapeKind);
    }

    function saveCanvasAs(fileUrl) {
        drawingSurface.saveToFile(fileUrl);
    }

    function openRaster(fileUrl) {
        drawingSurface.openRaster(fileUrl);
    }

    function syncLayerHierarchySelection() {
        if (!layerHierarchyPanel.visible) {
            return;
        }
        layerHierarchyPanel.activateListItemByKey(drawingSurface.currentLayerKey());
    }

    function resetLayerRenameActivation() {
        painterPage.layerRenameActivationKey = "";
        painterPage.layerRenameActivationCount = 0;
        painterPage.layerRenameLastActivationMs = 0;
    }

    function handleLayerHierarchyItemActivated(item) {
        if (!item || item.itemKey === undefined) {
            return;
        }

        const itemKey = String(item.itemKey);
        drawingSurface.activateLayerByKey(itemKey);
        if (!drawingSurface.canRenameLayerByKey(itemKey)) {
            painterPage.resetLayerRenameActivation();
            return;
        }

        const now = Date.now();
        const sameActivationRun = painterPage.layerRenameActivationKey === itemKey && now - painterPage.layerRenameLastActivationMs <= painterPage.layerRenameActivationWindowMs;
        painterPage.layerRenameActivationKey = itemKey;
        painterPage.layerRenameActivationCount = sameActivationRun ? painterPage.layerRenameActivationCount + 1 : 1;
        painterPage.layerRenameLastActivationMs = now;

        if (painterPage.layerRenameActivationCount >= painterPage.layerRenameRepeatActivationThreshold) {
            painterPage.resetLayerRenameActivation();
            painterPage.beginLayerRename(item);
        }
    }

    function beginLayerRename(item) {
        if (!item || item.itemKey === undefined) {
            return false;
        }

        const itemKey = String(item.itemKey);
        if (!drawingSurface.canRenameLayerByKey(itemKey)) {
            return false;
        }

        const panelPoint = item.mapToItem(layerHierarchyPanel, 0, 0);
        const labelX = Number(item.leftPadding || 0) + Number(item.iconSize || 16) + Number(item.leadingSpacing || 4);
        painterPage.layerRenameKey = itemKey;
        painterPage.layerRenameEditorX = Math.max(painterPage.spacingSmall, labelX);
        painterPage.layerRenameEditorY = Math.max(0, panelPoint.y);
        painterPage.layerRenameEditorHeight = Math.max(20, item.height || 20);
        layerRenameField.text = drawingSurface.layerNameByKey(itemKey);
        painterPage.layerRenameActive = true;
        Qt.callLater(function () {
            layerRenameField.forceActiveFocus();
            layerRenameField.selectAll();
        });
        return true;
    }

    function commitLayerRename() {
        if (!painterPage.layerRenameActive) {
            return false;
        }

        const itemKey = painterPage.layerRenameKey;
        const nextName = layerRenameField.text;
        painterPage.layerRenameActive = false;
        painterPage.layerRenameKey = "";
        const renamed = drawingSurface.renameLayerByKey(itemKey, nextName);
        Qt.callLater(painterPage.syncLayerHierarchySelection);
        return renamed;
    }

    function cancelLayerRename() {
        painterPage.layerRenameActive = false;
        painterPage.layerRenameKey = "";
        painterPage.resetLayerRenameActivation();
    }

    Item {
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: LV.Theme.window

            LV.Hierarchy {
                id: layerHierarchyPanel
                objectName: "layerHierarchyPanel"
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: painterPage.layerPanelTopMargin
                anchors.leftMargin: 0
                anchors.bottomMargin: 0
                width: painterPage.layerPanelWidth
                z: 4
                minimumPanelWidth: painterPage.layerPanelWidth
                minimumPanelHeight: 160
                panelColor: LV.Theme.panelBackground05
                editable: true
                model: drawingSurface.layerHierarchyRows
                footerVisible: true
                footerButton1: ({
                        type: "icon",
                        iconName: "add",
                        enabled: true
                    })
                footerButton2: ({
                        type: "icon",
                        iconName: "remove",
                        enabled: drawingSurface.selectedDrawableObjectId >= 0
                    })
                footerButton3: ({
                        type: "icon",
                        visible: false,
                        enabled: false
                    })

                onListItemActivated: function (item) {
                    painterPage.handleLayerHierarchyItemActivated(item);
                }

                onListItemMoved: function () {
                    drawingSurface.applyLayerHierarchyOrder(layerHierarchyPanel.model);
                    Qt.callLater(painterPage.syncLayerHierarchySelection);
                }

                onFooterButtonTriggered: function (index) {
                    if (index === 0) {
                        drawingSurface.addEmptyLayer();
                        Qt.callLater(painterPage.syncLayerHierarchySelection);
                    } else if (index === 1) {
                        drawingSurface.deleteSelectedDrawableObject();
                        Qt.callLater(painterPage.syncLayerHierarchySelection);
                    }
                }

                LV.ToolbarButton {
                    buttonId: "layers"
                    iconName: "projectStructure"
                    enabled: false
                }
            }

            Painting.DrawingSurface {
                id: drawingSurface
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.left: layerHierarchyPanel.right
                anchors.leftMargin: 0
                workspaceColor: LV.Theme.window
                documentViewModel: painterPage.vm
                viewId: painterPage.viewId
                brushColor: painterPage.vm ? painterPage.vm.brushColor : "#1a1a1a"
                brushSize: painterPage.vm ? painterPage.vm.brushSize : 2
                brushFlow: painterPage.vm ? painterPage.vm.brushFlow : 1
                brushOpacity: painterPage.vm ? painterPage.vm.brushOpacity : 1
                brushHardness: painterPage.vm ? painterPage.vm.brushHardness : painterPage.maximumAntialiasingBrushHardness
                brushSpacing: painterPage.vm ? painterPage.vm.brushSpacing : 0
                brushSpacingRatio: painterPage.vm ? painterPage.vm.brushSpacingRatio : 0
                pressureCurveMinimum: painterPage.vm ? painterPage.vm.pressureCurveMinimum : 0
                pressureCurveCenter: painterPage.vm ? painterPage.vm.pressureCurveCenter : 0.5
                pressureCurveMaximum: painterPage.vm ? painterPage.vm.pressureCurveMaximum : 1
                stabilizerStrength: painterPage.vm ? painterPage.vm.stabilizerStrength : 0
                toolMode: painterPage.vm ? painterPage.vm.toolMode : "brush"
                shapeKind: painterPage.vm ? painterPage.vm.shapeKind : "rectangle"
                textToolAccentColor: LV.Theme.primary
                textToolFramePadding: painterPage.spacingSmall
                canvasWidth: painterPage.vm ? painterPage.vm.canvasWidth : 1
                canvasHeight: painterPage.vm ? painterPage.vm.canvasHeight : 1
                onBrushDeltaRequested: delta => painterPage.adjustBrush(delta)
                onToolShortcutRequested: tool => painterPage.setToolMode(tool)
            }

            Connections {
                target: drawingSurface

                function onLayerHierarchyRowsChanged() {
                    Qt.callLater(painterPage.syncLayerHierarchySelection);
                }

                function onSelectedDrawableObjectIdChanged() {
                    Qt.callLater(painterPage.syncLayerHierarchySelection);
                }
            }

            Rectangle {
                id: layerRenameEditorFrame
                parent: layerHierarchyPanel
                visible: painterPage.layerRenameActive
                x: painterPage.layerRenameEditorX
                y: painterPage.layerRenameEditorY
                width: Math.max(96, layerHierarchyPanel.width - x - painterPage.spacingSmall)
                height: painterPage.layerRenameEditorHeight
                z: 40
                radius: 3
                color: LV.Theme.panelBackground12
                border.width: 1
                border.color: LV.Theme.primary

                TextInput {
                    id: layerRenameField
                    anchors.fill: parent
                    anchors.leftMargin: painterPage.spacingSmall
                    anchors.rightMargin: painterPage.spacingSmall
                    color: LV.Theme.bodyColor
                    selectionColor: LV.Theme.primary
                    selectedTextColor: "#ffffff"
                    verticalAlignment: TextInput.AlignVCenter
                    selectByMouse: true
                    clip: true

                    onAccepted: painterPage.commitLayerRename()
                    onActiveFocusChanged: {
                        if (!activeFocus && painterPage.layerRenameActive) {
                            painterPage.commitLayerRename();
                        }
                    }

                    Keys.onEscapePressed: function (event) {
                        event.accepted = true;
                        painterPage.cancelLayerRename();
                    }
                }
            }
        }

        Rectangle {
            id: toolbarChromeBackground
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: painterPage.toolbarTopMargin + canvasToolBar.height
            color: painterPage.toolbarBackgroundColor
            z: 9
        }

        BrushUi.CanvasToolBar {
            id: canvasToolBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: painterPage.toolbarTopMargin
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            z: 10
            backgroundColor: painterPage.toolbarBackgroundColor
            brushSize: painterPage.vm ? painterPage.vm.brushSize : 2
            brushFlow: painterPage.vm ? painterPage.vm.brushFlow : 1
            brushOpacity: painterPage.vm ? painterPage.vm.brushOpacity : 1
            brushHardness: painterPage.vm ? painterPage.vm.brushHardness : painterPage.maximumAntialiasingBrushHardness
            brushSpacing: painterPage.vm ? painterPage.vm.brushSpacing : 0
            brushSpacingRatio: painterPage.vm ? painterPage.vm.brushSpacingRatio : 0
            pressureCurveMinimum: painterPage.vm ? painterPage.vm.pressureCurveMinimum : 0
            pressureCurveCenter: painterPage.vm ? painterPage.vm.pressureCurveCenter : 0.5
            pressureCurveMaximum: painterPage.vm ? painterPage.vm.pressureCurveMaximum : 1
            stabilizerStrength: painterPage.vm ? painterPage.vm.stabilizerStrength : 0
            currentColor: painterPage.vm ? painterPage.vm.brushColor : "#1a1a1a"
            currentTool: painterPage.vm ? painterPage.vm.toolMode : "brush"
            currentShape: painterPage.vm ? painterPage.vm.shapeKind : "rectangle"
            canvasWidth: painterPage.vm ? painterPage.vm.canvasWidth : painterPage.fallbackNewCanvasWidth
            canvasHeight: painterPage.vm ? painterPage.vm.canvasHeight : painterPage.fallbackNewCanvasHeight
            onNewCanvasRequested: (canvasWidth, canvasHeight) => painterPage.newCanvas(canvasWidth, canvasHeight)
            onClearCanvasRequested: painterPage.clearCanvas()
            onBrushSizeChangeRequested: size => painterPage.setBrushSize(size)
            onBrushPropertyChangeRequested: (propertyName, value) => painterPage.setBrushProperty(propertyName, value)
            onColorPicked: swatchColor => painterPage.setBrushColor(swatchColor)
            onToolSelected: tool => painterPage.setToolMode(tool)
            onShapeSelected: shapeKind => painterPage.setShapeKind(shapeKind)
            onSaveRequested: fileUrl => painterPage.saveCanvasAs(fileUrl)
            onOpenRequested: fileUrl => painterPage.openRaster(fileUrl)
        }
    }
}
