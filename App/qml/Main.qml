import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Window as QtQuickWindow
import LVRS 1.0 as LV
import "./canvas" as CanvasViews

LV.ApplicationWindow {
    id: window
    readonly property int initialWidth: 1400
    readonly property int initialHeight: 880
    width: initialWidth
    height: initialHeight
    minimumWidth: initialWidth
    minimumHeight: initialHeight
    visible: true
    windowColor: LV.Theme.window
    solidChrome: true
    windowDragHandleEnabled: true
    navigationEnabled: false
    autoAttachRuntimeEvents: true

    property var canvasPage: null
    readonly property string currentToolMode: canvasPage && canvasPage.vm ? canvasPage.vm.toolMode : ""
    readonly property string currentShapeKind: canvasPage && canvasPage.vm ? canvasPage.vm.shapeKind : ""
    readonly property string menuCommandModifier: Qt.platform.os === "osx" ? "Meta" : "Ctrl"
    readonly property string shortcutNewCanvas: menuCommandModifier + "+N"
    readonly property string shortcutOpenImage: menuCommandModifier + "+O"
    readonly property string shortcutSaveImageAs: menuCommandModifier + "+S"
    readonly property string shortcutClearCanvas: menuCommandModifier + "+Shift+K"
    readonly property string shortcutQuit: menuCommandModifier + "+Q"
    readonly property string shortcutUndo: menuCommandModifier + "+Z"
    readonly property string shortcutRedo: Qt.platform.os === "osx" ? "Meta+Shift+Z" : "Ctrl+Y"
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

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutQuit]
        onActivated: Qt.quit()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutAddLayer]
        enabled: window.canvasPage !== null
        onActivated: window.requestAddLayer()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutDeleteCurrentLayer]
        enabled: window.canvasPage !== null && window.canvasPage.canDeleteCurrentLayer()
        onActivated: window.requestDeleteCurrentLayer()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutRectangleShape]
        enabled: window.canvasPage !== null
        onActivated: window.requestShapeTool("rectangle")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutEllipseShape]
        enabled: window.canvasPage !== null
        onActivated: window.requestShapeTool("ellipse")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutTriangleShape]
        enabled: window.canvasPage !== null
        onActivated: window.requestShapeTool("triangle")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutDiamondShape]
        enabled: window.canvasPage !== null
        onActivated: window.requestShapeTool("diamond")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutStarShape]
        enabled: window.canvasPage !== null
        onActivated: window.requestShapeTool("star")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutRectangleBubbleShape]
        enabled: window.canvasPage !== null
        onActivated: window.requestShapeTool("rectanglebubble")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutEllipseBubbleShape]
        enabled: window.canvasPage !== null
        onActivated: window.requestShapeTool("ellipsebubble")
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutFitCanvasToWindow]
        enabled: window.canvasPage !== null
        onActivated: window.requestFitCanvasToWindow()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutResetCanvasView]
        enabled: window.canvasPage !== null
        onActivated: window.requestResetCanvasView()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutMinimizeWindow]
        onActivated: window.requestMinimizeWindow()
    }

    Shortcut {
        context: Qt.ApplicationShortcut
        sequences: [window.shortcutToggleFullScreen]
        onActivated: window.requestToggleFullScreen()
    }

    menuBar: Controls.MenuBar {
        Controls.Menu {
            title: qsTr("File")

            Controls.MenuItem {
                text: qsTr("New Canvas...")
                shortcut: window.shortcutNewCanvas
                enabled: window.canvasPage !== null
                onTriggered: window.requestNewCanvas()
            }

            Controls.MenuItem {
                text: qsTr("Open Image...")
                shortcut: window.shortcutOpenImage
                enabled: window.canvasPage !== null
                onTriggered: window.requestOpenImage()
            }

            Controls.MenuItem {
                text: qsTr("Save Image As...")
                shortcut: window.shortcutSaveImageAs
                enabled: window.canvasPage !== null
                onTriggered: window.requestSaveImage()
            }

            Controls.MenuItem {
                text: qsTr("Clear Canvas")
                shortcut: window.shortcutClearCanvas
                enabled: window.canvasPage !== null
                onTriggered: window.requestClearCanvas()
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Quit Vincent")
                shortcut: window.shortcutQuit
                onTriggered: Qt.quit()
            }
        }

        Controls.Menu {
            title: qsTr("Edit")

            Controls.MenuItem {
                text: qsTr("Undo")
                shortcut: window.shortcutUndo
                enabled: window.canvasPage !== null
                onTriggered: window.requestUndo()
            }

            Controls.MenuItem {
                text: qsTr("Redo")
                shortcut: window.shortcutRedo
                enabled: window.canvasPage !== null
                onTriggered: window.requestRedo()
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Add Layer")
                shortcut: window.shortcutAddLayer
                enabled: window.canvasPage !== null
                onTriggered: window.requestAddLayer()
            }

            Controls.MenuItem {
                text: qsTr("Delete Current Layer")
                shortcut: window.shortcutDeleteCurrentLayer
                enabled: window.canvasPage !== null && window.canvasPage.canDeleteCurrentLayer()
                onTriggered: window.requestDeleteCurrentLayer()
            }

            Controls.MenuSeparator {}

            Controls.Menu {
                title: qsTr("Tools")

                Controls.MenuItem {
                    text: qsTr("Brush")
                    shortcut: window.shortcutBrushTool
                    checkable: true
                    checked: window.currentToolMode === "brush"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("brush")
                }

                Controls.MenuItem {
                    text: qsTr("Eraser")
                    shortcut: window.shortcutEraserTool
                    checkable: true
                    checked: window.currentToolMode === "eraser"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("eraser")
                }

                Controls.MenuItem {
                    text: qsTr("Hand Pan")
                    shortcut: window.shortcutHandPanTool
                    checkable: true
                    checked: window.currentToolMode === "pan"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("pan")
                }

                Controls.MenuItem {
                    text: qsTr("Move")
                    shortcut: window.shortcutMoveTool
                    checkable: true
                    checked: window.currentToolMode === "move"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("move")
                }

                Controls.MenuItem {
                    text: qsTr("Zoom")
                    shortcut: window.shortcutZoomTool
                    checkable: true
                    checked: window.currentToolMode === "zoom"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("zoom")
                }

                Controls.MenuItem {
                    text: qsTr("Shape")
                    shortcut: window.shortcutShapeTool
                    checkable: true
                    checked: window.currentToolMode === "shape"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("shape")
                }

                Controls.MenuItem {
                    text: qsTr("Fill")
                    shortcut: window.shortcutFillTool
                    checkable: true
                    checked: window.currentToolMode === "fill"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("fill")
                }

                Controls.MenuItem {
                    text: qsTr("Text")
                    shortcut: window.shortcutTextTool
                    checkable: true
                    checked: window.currentToolMode === "text"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("text")
                }
            }

            Controls.Menu {
                title: qsTr("Shape Kind")

                Controls.MenuItem {
                    text: qsTr("Rectangle")
                    shortcut: window.shortcutRectangleShape
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "rectangle"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("rectangle")
                }

                Controls.MenuItem {
                    text: qsTr("Ellipse")
                    shortcut: window.shortcutEllipseShape
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "ellipse"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("ellipse")
                }

                Controls.MenuItem {
                    text: qsTr("Triangle")
                    shortcut: window.shortcutTriangleShape
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "triangle"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("triangle")
                }

                Controls.MenuItem {
                    text: qsTr("Diamond")
                    shortcut: window.shortcutDiamondShape
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "diamond"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("diamond")
                }

                Controls.MenuItem {
                    text: qsTr("Star")
                    shortcut: window.shortcutStarShape
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "star"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("star")
                }

                Controls.MenuItem {
                    text: qsTr("Rectangle Bubble")
                    shortcut: window.shortcutRectangleBubbleShape
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "rectanglebubble"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("rectanglebubble")
                }

                Controls.MenuItem {
                    text: qsTr("Ellipse Bubble")
                    shortcut: window.shortcutEllipseBubbleShape
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "ellipsebubble"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("ellipsebubble")
                }
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Decrease Brush Size")
                shortcut: window.shortcutDecreaseBrushSize
                enabled: window.canvasPage !== null
                onTriggered: window.requestBrushSizeDelta(-1)
            }

            Controls.MenuItem {
                text: qsTr("Increase Brush Size")
                shortcut: window.shortcutIncreaseBrushSize
                enabled: window.canvasPage !== null
                onTriggered: window.requestBrushSizeDelta(1)
            }
        }

        Controls.Menu {
            title: qsTr("Window")

            Controls.MenuItem {
                text: qsTr("Fit Canvas to Window")
                shortcut: window.shortcutFitCanvasToWindow
                enabled: window.canvasPage !== null
                onTriggered: window.requestFitCanvasToWindow()
            }

            Controls.MenuItem {
                text: qsTr("Reset Canvas View")
                shortcut: window.shortcutResetCanvasView
                enabled: window.canvasPage !== null
                onTriggered: window.requestResetCanvasView()
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Minimize")
                shortcut: window.shortcutMinimizeWindow
                onTriggered: window.requestMinimizeWindow()
            }

            Controls.MenuItem {
                text: window.visibility === QtQuickWindow.Window.FullScreen ? qsTr("Exit Full Screen") : qsTr("Enter Full Screen")
                shortcut: window.shortcutToggleFullScreen
                onTriggered: window.requestToggleFullScreen()
            }
        }

        Controls.Menu {
            title: qsTr("Help")

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

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Vincent 2.2.1")
                enabled: false
            }
        }
    }

    CanvasViews.PainterCanvasPage {
        id: painterPage
        anchors.fill: parent
        topChromeReservedHeight: window.windowDragHandleEnabled ? window.windowDragHandleHeight : 0
        onPageReady: window.canvasPage = painterPage
    }
}
