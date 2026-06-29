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
    readonly property int toolbarTopMargin: topChromeReservedHeight + spacingSmall
    readonly property int fallbackNewCanvasWidth: 1024
    readonly property int fallbackNewCanvasHeight: 768
    readonly property int layerPanelWidth: Math.min(260, Math.max(220, Math.round(width * 0.22)))
    readonly property int layerPanelTopMargin: toolbarTopMargin + canvasToolBar.height + spacingSmall

    property int topChromeReservedHeight: 0
    property var vm: null

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
                anchors.leftMargin: painterPage.spacingSmall
                anchors.bottomMargin: painterPage.spacingSmall
                width: painterPage.layerPanelWidth
                z: 4
                minimumPanelWidth: painterPage.layerPanelWidth
                minimumPanelHeight: 160
                panelColor: LV.Theme.panelBackground05
                editable: true
                model: drawingSurface.layerHierarchyRows
                footerVisible: false

                onListItemActivated: function (item) {
                    if (item && item.itemKey !== undefined) {
                        drawingSurface.activateLayerByKey(item.itemKey);
                    }
                }

                onListItemMoved: function () {
                    drawingSurface.applyLayerHierarchyOrder(layerHierarchyPanel.model);
                    Qt.callLater(painterPage.syncLayerHierarchySelection);
                }

                onToolbarButtonTriggered: function (button, buttonId) {
                    if (buttonId === "delete") {
                        drawingSurface.deleteSelectedDrawableObject();
                        Qt.callLater(painterPage.syncLayerHierarchySelection);
                    }
                }

                LV.ToolbarButton {
                    buttonId: "layers"
                    iconName: "projectStructure"
                    enabled: false
                }

                LV.ToolbarButton {
                    buttonId: "delete"
                    iconName: "delete"
                    enabled: drawingSurface.hasSelectedDrawableObject()
                }
            }

            Painting.DrawingSurface {
                id: drawingSurface
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.left: layerHierarchyPanel.right
                anchors.leftMargin: painterPage.spacingSmall
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
        }

        BrushUi.CanvasToolBar {
            id: canvasToolBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: painterPage.toolbarTopMargin
            anchors.leftMargin: painterPage.spacingSmall
            anchors.rightMargin: painterPage.spacingSmall
            z: 10
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
