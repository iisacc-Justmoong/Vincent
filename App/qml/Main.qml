import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Window as QtQuickWindow
import LVRS 1.0 as LV
import "./canvas" as CanvasViews
import "./license" as LicenseViews

LV.ApplicationWindow {
    id: window
    readonly property int initialWidth: 1400
    readonly property int initialHeight: 880
    readonly property int minimumWindowWidth: 640
    readonly property int minimumWindowHeight: 400
    width: initialWidth
    height: initialHeight
    minimumWidth: minimumWindowWidth
    minimumHeight: minimumWindowHeight
    visible: false
    windowColor: LV.Theme.window
    solidChrome: true
    windowDragHandleEnabled: Qt.platform.os === "osx" && visibility !== QtQuickWindow.Window.FullScreen
    navigationEnabled: false

    property var canvasPage: null
    property bool canvasIncubationRequested: false
    property string clipboardPasteFailureMessage: ""
    readonly property bool licenseGranted: VincentLicenseManager.licensed
    readonly property bool canvasCommandsEnabled: canvasPage !== null && !canvasPage.dialogActive
    readonly property bool canvasEditingCommandsEnabled: canvasCommandsEnabled && !canvasPage.textEditingActive
    readonly property string currentToolMode: canvasPage && canvasPage.vm ? canvasPage.vm.toolMode : ""
    readonly property string currentShapeKind: canvasPage && canvasPage.vm ? canvasPage.vm.shapeKind : ""
    readonly property string menuCommandModifier: Qt.platform.os === "osx" ? "Meta" : "Ctrl"
    readonly property string shortcutNewCanvas: menuCommandModifier + "+N"
    readonly property string shortcutOpenImage: menuCommandModifier + "+O"
    readonly property string shortcutSaveImageAs: menuCommandModifier + "+S"
    readonly property string shortcutClearCanvas: menuCommandModifier + "+Shift+K"
    readonly property string shortcutQuit: menuCommandModifier + "+Q"
    readonly property string shortcutUndo: menuCommandModifier + "+Z"
    readonly property string shortcutRedo: Qt.platform.os === "osx" ? "Meta+Shift+Z" : (Qt.platform.os === "windows" ? "Ctrl+Y" : "Ctrl+Shift+Z")
    readonly property string shortcutPasteImage: menuCommandModifier + "+V"
    readonly property string shortcutAddLayer: menuCommandModifier + "+Shift+N"
    readonly property string shortcutDeleteCurrentLayer: menuCommandModifier + "+Shift+Delete"
    readonly property string shortcutBrushTool: "B"
    readonly property string shortcutEraserTool: "E"
    readonly property string shortcutHandPanTool: "H"
    readonly property string shortcutMoveTool: "V"
    readonly property string shortcutZoomTool: "Z"
    readonly property string shortcutShapeTool: "U"
    readonly property string shortcutFillTool: "G"
    readonly property string shortcutTextTool: "T"
    readonly property string shortcutRectangleShape: menuCommandModifier + "+Alt+1"
    readonly property string shortcutEllipseShape: menuCommandModifier + "+Alt+2"
    readonly property string shortcutTriangleShape: menuCommandModifier + "+Alt+3"
    readonly property string shortcutDiamondShape: menuCommandModifier + "+Alt+4"
    readonly property string shortcutStarShape: menuCommandModifier + "+Alt+5"
    readonly property string shortcutRectangleBubbleShape: menuCommandModifier + "+Alt+6"
    readonly property string shortcutEllipseBubbleShape: menuCommandModifier + "+Alt+7"
    readonly property string shortcutDecreaseBrushSize: "["
    readonly property string shortcutIncreaseBrushSize: "]"
    readonly property string shortcutFitCanvasToWindow: menuCommandModifier + "+0"
    readonly property string shortcutResetCanvasView: menuCommandModifier + "+1"
    readonly property string shortcutMinimizeWindow: menuCommandModifier + "+M"
    readonly property string shortcutToggleFullScreen: Qt.platform.os === "osx" ? "Ctrl+Meta+F" : "F11"

    Component.onCompleted: Qt.callLater(function () {
        window.canvasIncubationRequested = true;
    })

    function requestNewCanvas() {
        if (window.canvasPage) {
            window.canvasPage.openNewCanvasDialog();
        }
    }

    function requestOpenImage() {
        if (window.canvasPage) {
            window.canvasPage.openFileDialog();
        }
    }

    function requestSaveImage() {
        if (window.canvasPage) {
            window.canvasPage.openSaveDialog();
        }
    }

    function requestClearCanvas() {
        if (window.canvasPage) {
            window.canvasPage.clearCanvas();
        }
    }

    function requestUndo() {
        if (window.canvasPage) {
            window.canvasPage.undoActiveRasterSurface();
        }
    }

    function requestRedo() {
        if (window.canvasPage) {
            window.canvasPage.redoActiveRasterSurface();
        }
    }

    function requestPasteImage() {
        if (window.canvasPage) {
            const pasted = window.canvasPage.pasteClipboardImage();
            if (pasted) {
                window.clipboardPasteFailureMessage = "";
                clipboardPasteFailureTimer.stop();
            }
            return pasted;
        }
        return false;
    }

    function clipboardPasteFailureText(errorCode) {
        switch (String(errorCode || "")) {
        case "clipboard-unavailable":
            return qsTr("The system clipboard is unavailable.");
        case "canvas-unavailable":
            return qsTr("The canvas is still loading. Try pasting again in a moment.");
        case "no-image":
            return qsTr("Copy an image or a local image file, then paste again.");
        case "decode-failed":
            return qsTr("Vincent could not read the image. Try another image or copy it and paste again.");
        case "image-too-large":
            return qsTr("The image is too large. Use an image up to 32,768 pixels per side and 64 megapixels.");
        case "cache-write-failed":
            return qsTr("Vincent could not store the image. Check available disk space and cache-folder permissions.");
        case "download-failed":
            return qsTr("Vincent could not download the dropped web image. Check the connection, or save the image locally and drop it again.");
        case "download-too-large":
            return qsTr("The dropped web image download exceeds the 64 MB safety limit. Save or resize it locally, then drop it again.");
        default:
            return qsTr("Vincent could not paste the clipboard image.");
        }
    }

    function showClipboardPasteFailure(errorCode) {
        window.clipboardPasteFailureMessage = window.clipboardPasteFailureText(errorCode);
        clipboardPasteFailureTimer.restart();
    }

    function requestAddLayer() {
        if (window.canvasPage) {
            window.canvasPage.addLayer();
        }
    }

    function requestDeleteCurrentLayer() {
        if (window.canvasPage) {
            window.canvasPage.deleteCurrentLayer();
        }
    }

    function requestToolMode(toolMode) {
        if (window.canvasPage) {
            window.canvasPage.setToolMode(toolMode);
        }
    }

    function requestShapeTool(shapeKind) {
        if (window.canvasPage) {
            window.canvasPage.selectShapeTool(shapeKind);
        }
    }

    function requestBrushSizeDelta(delta) {
        if (window.canvasPage) {
            window.canvasPage.adjustBrush(delta);
        }
    }

    function requestFitCanvasToWindow() {
        if (window.canvasPage) {
            window.canvasPage.fitCanvasToWindow();
        }
    }

    function requestResetCanvasView() {
        if (window.canvasPage) {
            window.canvasPage.resetCanvasView();
        }
    }

    function requestMinimizeWindow() {
        window.showMinimized();
    }

    function requestToggleFullScreen() {
        if (window.visibility === QtQuickWindow.Window.FullScreen) {
            window.showNormal();
            return;
        }
        window.showFullScreen();
    }

    function shortcutReference(commandName, shortcutText) {
        return commandName + " - " + shortcutText;
    }

    Controls.Action {
        id: newCanvasAction
        text: qsTr("New Canvas...")
        shortcut: window.shortcutNewCanvas
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestNewCanvas()
    }

    Controls.Action {
        id: openImageAction
        text: qsTr("Open Image...")
        shortcut: window.shortcutOpenImage
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestOpenImage()
    }

    Controls.Action {
        id: saveImageAsAction
        text: qsTr("Save Image As...")
        shortcut: window.shortcutSaveImageAs
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestSaveImage()
    }

    Controls.Action {
        id: clearCanvasAction
        text: qsTr("Clear Canvas")
        shortcut: window.shortcutClearCanvas
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestClearCanvas()
    }

    Controls.Action {
        id: quitAction
        text: qsTr("Quit Vincent")
        shortcut: window.shortcutQuit
        onTriggered: Qt.quit()
    }

    Controls.Action {
        id: checkForUpdatesAction
        text: qsTr("Check for Updates…")
        enabled: VincentUpdateManager.selfUpdateSupported && !VincentUpdateManager.busy
        onTriggered: {
            updateModal.open = true;
            VincentUpdateManager.checkForUpdates();
        }
    }

    Controls.Action {
        id: undoAction
        text: qsTr("Undo")
        shortcut: window.shortcutUndo
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestUndo()
    }

    Controls.Action {
        id: redoAction
        text: qsTr("Redo")
        shortcut: window.shortcutRedo
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestRedo()
    }

    Controls.Action {
        id: pasteImageAction
        text: qsTr("Paste Image")
        shortcut: window.shortcutPasteImage
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestPasteImage()
    }

    Controls.Action {
        id: addLayerAction
        text: qsTr("Add Layer")
        shortcut: window.shortcutAddLayer
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestAddLayer()
    }

    Controls.Action {
        id: deleteCurrentLayerAction
        text: qsTr("Delete Current Layer")
        shortcut: window.shortcutDeleteCurrentLayer
        enabled: window.canvasEditingCommandsEnabled && window.canvasPage.canDeleteCurrentLayer()
        onTriggered: window.requestDeleteCurrentLayer()
    }

    Controls.Action {
        id: brushToolAction
        text: qsTr("Brush")
        shortcut: window.shortcutBrushTool
        checkable: true
        checked: window.currentToolMode === "brush"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("brush")
    }

    Controls.Action {
        id: eraserToolAction
        text: qsTr("Eraser")
        shortcut: window.shortcutEraserTool
        checkable: true
        checked: window.currentToolMode === "eraser"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("eraser")
    }

    Controls.Action {
        id: handPanToolAction
        text: qsTr("Hand Pan")
        shortcut: window.shortcutHandPanTool
        checkable: true
        checked: window.currentToolMode === "pan"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("pan")
    }

    Controls.Action {
        id: moveToolAction
        text: qsTr("Move")
        shortcut: window.shortcutMoveTool
        checkable: true
        checked: window.currentToolMode === "move"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("move")
    }

    Controls.Action {
        id: zoomToolAction
        text: qsTr("Zoom")
        shortcut: window.shortcutZoomTool
        checkable: true
        checked: window.currentToolMode === "zoom"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("zoom")
    }

    Controls.Action {
        id: shapeToolAction
        text: qsTr("Shape")
        shortcut: window.shortcutShapeTool
        checkable: true
        checked: window.currentToolMode === "shape"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("shape")
    }

    Controls.Action {
        id: fillToolAction
        text: qsTr("Fill")
        shortcut: window.shortcutFillTool
        checkable: true
        checked: window.currentToolMode === "fill"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("fill")
    }

    Controls.Action {
        id: textToolAction
        text: qsTr("Text")
        shortcut: window.shortcutTextTool
        checkable: true
        checked: window.currentToolMode === "text"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestToolMode("text")
    }

    Controls.Action {
        id: rectangleShapeAction
        text: qsTr("Rectangle")
        shortcut: window.shortcutRectangleShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "rectangle"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("rectangle")
    }

    Controls.Action {
        id: ellipseShapeAction
        text: qsTr("Ellipse")
        shortcut: window.shortcutEllipseShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "ellipse"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("ellipse")
    }

    Controls.Action {
        id: triangleShapeAction
        text: qsTr("Triangle")
        shortcut: window.shortcutTriangleShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "triangle"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("triangle")
    }

    Controls.Action {
        id: diamondShapeAction
        text: qsTr("Diamond")
        shortcut: window.shortcutDiamondShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "diamond"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("diamond")
    }

    Controls.Action {
        id: starShapeAction
        text: qsTr("Star")
        shortcut: window.shortcutStarShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "star"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("star")
    }

    Controls.Action {
        id: rectangleBubbleShapeAction
        text: qsTr("Rectangle Bubble")
        shortcut: window.shortcutRectangleBubbleShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "rectanglebubble"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("rectanglebubble")
    }

    Controls.Action {
        id: ellipseBubbleShapeAction
        text: qsTr("Ellipse Bubble")
        shortcut: window.shortcutEllipseBubbleShape
        checkable: true
        checked: window.currentToolMode === "shape" && window.currentShapeKind === "ellipsebubble"
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestShapeTool("ellipsebubble")
    }

    Controls.Action {
        id: decreaseBrushSizeAction
        text: qsTr("Decrease Brush Size")
        shortcut: window.shortcutDecreaseBrushSize
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestBrushSizeDelta(-1)
    }

    Controls.Action {
        id: increaseBrushSizeAction
        text: qsTr("Increase Brush Size")
        shortcut: window.shortcutIncreaseBrushSize
        enabled: window.canvasEditingCommandsEnabled
        onTriggered: window.requestBrushSizeDelta(1)
    }

    Controls.Action {
        id: fitCanvasToWindowAction
        text: qsTr("Fit Canvas to Window")
        shortcut: window.shortcutFitCanvasToWindow
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestFitCanvasToWindow()
    }

    Controls.Action {
        id: resetCanvasViewAction
        text: qsTr("Reset Canvas View")
        shortcut: window.shortcutResetCanvasView
        enabled: window.canvasCommandsEnabled
        onTriggered: window.requestResetCanvasView()
    }

    Controls.Action {
        id: minimizeWindowAction
        text: qsTr("Minimize")
        shortcut: window.shortcutMinimizeWindow
        onTriggered: window.requestMinimizeWindow()
    }

    Controls.Action {
        id: toggleFullScreenAction
        text: window.visibility === QtQuickWindow.Window.FullScreen ? qsTr("Exit Full Screen") : qsTr("Enter Full Screen")
        shortcut: window.shortcutToggleFullScreen
        onTriggered: window.requestToggleFullScreen()
    }

    menuBar: Controls.MenuBar {
        id: applicationMenuBar
        objectName: "applicationMenuBar"
        implicitHeight: LV.Theme.controlHeightSm
        padding: LV.Theme.gapNone
        spacing: LV.Theme.gapNone
        palette.button: window.windowColor
        palette.buttonText: LV.Theme.textPrimary
        palette.mid: LV.Theme.surfaceAlt

        delegate: Controls.MenuBarItem {
            id: applicationMenuBarItem
            implicitHeight: applicationMenuBar.implicitHeight
            topPadding: LV.Theme.gap2
            bottomPadding: LV.Theme.gap2
            leftPadding: LV.Theme.gap8
            rightPadding: LV.Theme.gap8

            contentItem: LV.Label {
                text: applicationMenuBarItem.text
                style: body
                color: applicationMenuBarItem.enabled ? LV.Theme.textPrimary : LV.Theme.textOctonary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                implicitHeight: applicationMenuBar.implicitHeight
                color: applicationMenuBarItem.down || applicationMenuBarItem.highlighted ? LV.Theme.surfaceAlt : "transparent"
            }
        }

        background: Rectangle {
            implicitHeight: LV.Theme.controlHeightSm
            color: window.windowColor
        }

        Controls.Menu {
            title: qsTr("File")

            Controls.MenuItem {
                action: newCanvasAction
            }

            Controls.MenuItem {
                action: openImageAction
            }

            Controls.MenuItem {
                action: saveImageAsAction
            }

            Controls.MenuItem {
                action: clearCanvasAction
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                action: quitAction
            }
        }

        Controls.Menu {
            title: qsTr("Edit")

            Controls.MenuItem {
                action: undoAction
            }

            Controls.MenuItem {
                action: redoAction
            }

            Controls.MenuItem {
                action: pasteImageAction
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                action: addLayerAction
            }

            Controls.MenuItem {
                action: deleteCurrentLayerAction
            }

            Controls.MenuSeparator {}

            Controls.Menu {
                title: qsTr("Tools")

                Controls.MenuItem {
                    action: brushToolAction
                }

                Controls.MenuItem {
                    action: eraserToolAction
                }

                Controls.MenuItem {
                    action: handPanToolAction
                }

                Controls.MenuItem {
                    action: moveToolAction
                }

                Controls.MenuItem {
                    action: zoomToolAction
                }

                Controls.MenuItem {
                    action: shapeToolAction
                }

                Controls.MenuItem {
                    action: fillToolAction
                }

                Controls.MenuItem {
                    action: textToolAction
                }
            }

            Controls.Menu {
                title: qsTr("Shape Kind")

                Controls.MenuItem {
                    action: rectangleShapeAction
                }

                Controls.MenuItem {
                    action: ellipseShapeAction
                }

                Controls.MenuItem {
                    action: triangleShapeAction
                }

                Controls.MenuItem {
                    action: diamondShapeAction
                }

                Controls.MenuItem {
                    action: starShapeAction
                }

                Controls.MenuItem {
                    action: rectangleBubbleShapeAction
                }

                Controls.MenuItem {
                    action: ellipseBubbleShapeAction
                }
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                action: decreaseBrushSizeAction
            }

            Controls.MenuItem {
                action: increaseBrushSizeAction
            }
        }

        Controls.Menu {
            title: qsTr("Window")

            Controls.MenuItem {
                action: fitCanvasToWindowAction
            }

            Controls.MenuItem {
                action: resetCanvasViewAction
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                action: minimizeWindowAction
            }

            Controls.MenuItem {
                action: toggleFullScreenAction
            }
        }

        Controls.Menu {
            title: qsTr("Help")

            Controls.MenuItem {
                action: checkForUpdatesAction
                visible: VincentUpdateManager.selfUpdateSupported
            }

            Controls.MenuSeparator {
                visible: VincentUpdateManager.selfUpdateSupported
            }

            Controls.Menu {
                title: qsTr("Keyboard Shortcuts")

                Controls.MenuItem {
                    text: qsTr("File")
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("New Canvas"), window.shortcutNewCanvas)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Open Image"), window.shortcutOpenImage)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Save Image As"), window.shortcutSaveImageAs)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Clear Canvas"), window.shortcutClearCanvas)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Quit Vincent"), window.shortcutQuit)
                    enabled: false
                }

                Controls.MenuSeparator {}

                Controls.MenuItem {
                    text: qsTr("Edit")
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Undo"), window.shortcutUndo)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Redo"), window.shortcutRedo)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Paste Image"), window.shortcutPasteImage)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Add Layer"), window.shortcutAddLayer)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Delete Current Layer"), window.shortcutDeleteCurrentLayer)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Decrease Brush Size"), window.shortcutDecreaseBrushSize)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Increase Brush Size"), window.shortcutIncreaseBrushSize)
                    enabled: false
                }

                Controls.MenuSeparator {}

                Controls.MenuItem {
                    text: qsTr("Tools")
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Brush"), window.shortcutBrushTool)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Eraser"), window.shortcutEraserTool)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Hand Pan"), window.shortcutHandPanTool)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Move"), window.shortcutMoveTool)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Zoom"), window.shortcutZoomTool)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Shape"), window.shortcutShapeTool)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Fill"), window.shortcutFillTool)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Text"), window.shortcutTextTool)
                    enabled: false
                }

                Controls.MenuSeparator {}

                Controls.MenuItem {
                    text: qsTr("Shape Kind")
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Rectangle"), window.shortcutRectangleShape)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Ellipse"), window.shortcutEllipseShape)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Triangle"), window.shortcutTriangleShape)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Diamond"), window.shortcutDiamondShape)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Star"), window.shortcutStarShape)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Rectangle Bubble"), window.shortcutRectangleBubbleShape)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Ellipse Bubble"), window.shortcutEllipseBubbleShape)
                    enabled: false
                }

                Controls.MenuSeparator {}

                Controls.MenuItem {
                    text: qsTr("Window")
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Fit Canvas to Window"), window.shortcutFitCanvasToWindow)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Reset Canvas View"), window.shortcutResetCanvasView)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Minimize"), window.shortcutMinimizeWindow)
                    enabled: false
                }

                Controls.MenuItem {
                    text: window.shortcutReference(qsTr("Enter Full Screen"), window.shortcutToggleFullScreen)
                    enabled: false
                }
            }

            Controls.MenuItem {
                text: qsTr("Vincent %1").arg(Qt.application.version)
                enabled: false
            }
        }
    }

    Loader {
        id: painterPageLoader
        anchors.fill: parent
        active: window.canvasIncubationRequested && window.licenseGranted
        asynchronous: true
        sourceComponent: CanvasViews.PainterCanvasPage {
            id: painterPage
            topChromeReservedHeight: window.windowDragHandleEnabled ? window.windowDragHandleHeight : 0
            onPageReady: window.canvasPage = painterPage
            onClipboardImagePasteFailed: errorCode => window.showClipboardPasteFailure(errorCode)
            onImageDropSucceeded: {
                window.clipboardPasteFailureMessage = "";
                clipboardPasteFailureTimer.stop();
            }
            onImageDropFailed: errorCode => window.showClipboardPasteFailure(errorCode)
        }
    }

    LicenseViews.LicenseActivationPage {
        anchors.fill: parent
        visible: !window.licenseGranted
    }

    LV.AppCard {
        objectName: "licensePersistenceWarning"
        anchors.top: parent.top
        anchors.topMargin: LV.Theme.gap24
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(560, Math.max(320, parent.width - LV.Theme.gap24 * 2))
        z: 1000
        visible: window.licenseGranted && VincentLicenseManager.resultCode === "secure_storage_unavailable"
        title: qsTr("Vincent could not remember this license")
        subtitle: qsTr("The canvas is unlocked for this session. Enter the license again the next time Vincent starts.")
    }

    LV.AppCard {
        id: clipboardPasteFailureCard
        objectName: "clipboardPasteFailureCard"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: LV.Theme.gap24
        width: Math.min(620, Math.max(320, parent.width - LV.Theme.gap24 * 2))
        z: 1100
        visible: window.clipboardPasteFailureMessage.length > 0
        title: qsTr("Image not pasted")
        subtitle: window.clipboardPasteFailureMessage
        Accessible.name: title + ": " + subtitle
        Accessible.role: Accessible.AlertMessage
    }

    Timer {
        id: clipboardPasteFailureTimer
        interval: 5000
        repeat: false
        onTriggered: window.clipboardPasteFailureMessage = ""
    }

    LV.Label {
        anchors.centerIn: parent
        visible: window.licenseGranted && painterPageLoader.status !== Loader.Ready
        text: qsTr("Loading canvas…")
    }

    LV.Modal {
        id: updateModal
        objectName: "vincentUpdateModal"
        open: false
        dismissOnBackground: !VincentUpdateManager.busy
        showIcon: false
        title: VincentUpdateManager.title
        description: VincentUpdateManager.message + (VincentUpdateManager.busy && VincentUpdateManager.progress > 0 ? qsTr("\n\nProgress: %1%").arg(Math.round(VincentUpdateManager.progress * 100)) : "")
        buttonCount: VincentUpdateManager.canUpdate ? 2 : 1
        primaryText: VincentUpdateManager.canCancel ? qsTr("Cancel") : VincentUpdateManager.canUpdate ? qsTr("Update now") : qsTr("Close")
        secondaryText: qsTr("Not now")
        primaryEnabled: VincentUpdateManager.canUpdate || VincentUpdateManager.canCancel || !VincentUpdateManager.busy

        onPrimaryClicked: {
            if (VincentUpdateManager.canUpdate) {
                VincentUpdateManager.updateNow();
            } else if (VincentUpdateManager.canCancel) {
                VincentUpdateManager.cancelUpdate();
            } else {
                updateModal.open = false;
            }
        }
        onSecondaryClicked: updateModal.open = false
        onCanceled: {
            if (VincentUpdateManager.canCancel)
                VincentUpdateManager.cancelUpdate();
        }
    }
}
