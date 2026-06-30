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

    menuBar: Controls.MenuBar {
        Controls.Menu {
            title: qsTr("File")

            Controls.MenuItem {
                text: qsTr("New Canvas...")
                enabled: window.canvasPage !== null
                onTriggered: window.requestNewCanvas()
            }

            Controls.MenuItem {
                text: qsTr("Open Image...")
                enabled: window.canvasPage !== null
                onTriggered: window.requestOpenImage()
            }

            Controls.MenuItem {
                text: qsTr("Save Image As...")
                enabled: window.canvasPage !== null
                onTriggered: window.requestSaveImage()
            }

            Controls.MenuItem {
                text: qsTr("Clear Canvas")
                enabled: window.canvasPage !== null
                onTriggered: window.requestClearCanvas()
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Quit Vincent")
                onTriggered: Qt.quit()
            }
        }

        Controls.Menu {
            title: qsTr("Edit")

            Controls.MenuItem {
                text: qsTr("Undo")
                enabled: window.canvasPage !== null
                onTriggered: window.requestUndo()
            }

            Controls.MenuItem {
                text: qsTr("Redo")
                enabled: window.canvasPage !== null
                onTriggered: window.requestRedo()
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Add Layer")
                enabled: window.canvasPage !== null
                onTriggered: window.requestAddLayer()
            }

            Controls.MenuItem {
                text: qsTr("Delete Current Layer")
                enabled: window.canvasPage !== null && window.canvasPage.canDeleteCurrentLayer()
                onTriggered: window.requestDeleteCurrentLayer()
            }

            Controls.MenuSeparator {}

            Controls.Menu {
                title: qsTr("Tools")

                Controls.MenuItem {
                    text: qsTr("Brush")
                    checkable: true
                    checked: window.currentToolMode === "brush"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("brush")
                }

                Controls.MenuItem {
                    text: qsTr("Eraser")
                    checkable: true
                    checked: window.currentToolMode === "eraser"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("eraser")
                }

                Controls.MenuItem {
                    text: qsTr("Hand Pan")
                    checkable: true
                    checked: window.currentToolMode === "pan"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("pan")
                }

                Controls.MenuItem {
                    text: qsTr("Move")
                    checkable: true
                    checked: window.currentToolMode === "move"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("move")
                }

                Controls.MenuItem {
                    text: qsTr("Zoom")
                    checkable: true
                    checked: window.currentToolMode === "zoom"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("zoom")
                }

                Controls.MenuItem {
                    text: qsTr("Shape")
                    checkable: true
                    checked: window.currentToolMode === "shape"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("shape")
                }

                Controls.MenuItem {
                    text: qsTr("Fill")
                    checkable: true
                    checked: window.currentToolMode === "fill"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestToolMode("fill")
                }

                Controls.MenuItem {
                    text: qsTr("Text")
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
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "rectangle"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("rectangle")
                }

                Controls.MenuItem {
                    text: qsTr("Ellipse")
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "ellipse"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("ellipse")
                }

                Controls.MenuItem {
                    text: qsTr("Triangle")
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "triangle"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("triangle")
                }

                Controls.MenuItem {
                    text: qsTr("Diamond")
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "diamond"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("diamond")
                }

                Controls.MenuItem {
                    text: qsTr("Star")
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "star"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("star")
                }

                Controls.MenuItem {
                    text: qsTr("Rectangle Bubble")
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "rectanglebubble"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("rectanglebubble")
                }

                Controls.MenuItem {
                    text: qsTr("Ellipse Bubble")
                    checkable: true
                    checked: window.currentToolMode === "shape" && window.currentShapeKind === "ellipsebubble"
                    enabled: window.canvasPage !== null
                    onTriggered: window.requestShapeTool("ellipsebubble")
                }
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Decrease Brush Size")
                enabled: window.canvasPage !== null
                onTriggered: window.requestBrushSizeDelta(-1)
            }

            Controls.MenuItem {
                text: qsTr("Increase Brush Size")
                enabled: window.canvasPage !== null
                onTriggered: window.requestBrushSizeDelta(1)
            }
        }

        Controls.Menu {
            title: qsTr("Window")

            Controls.MenuItem {
                text: qsTr("Fit Canvas to Window")
                enabled: window.canvasPage !== null
                onTriggered: window.requestFitCanvasToWindow()
            }

            Controls.MenuItem {
                text: qsTr("Reset Canvas View")
                enabled: window.canvasPage !== null
                onTriggered: window.requestResetCanvasView()
            }

            Controls.MenuSeparator {}

            Controls.MenuItem {
                text: qsTr("Minimize")
                onTriggered: window.requestMinimizeWindow()
            }

            Controls.MenuItem {
                text: window.visibility === QtQuickWindow.Window.FullScreen ? qsTr("Exit Full Screen") : qsTr("Enter Full Screen")
                onTriggered: window.requestToggleFullScreen()
            }
        }

        Controls.Menu {
            title: qsTr("Help")

            Controls.Menu {
                title: qsTr("Keyboard Shortcuts")

                Controls.MenuItem {
                    text: qsTr("B - Brush")
                    enabled: false
                }

                Controls.MenuItem {
                    text: qsTr("E - Eraser")
                    enabled: false
                }

                Controls.MenuItem {
                    text: qsTr("H - Hand Pan")
                    enabled: false
                }

                Controls.MenuItem {
                    text: qsTr("V - Move")
                    enabled: false
                }

                Controls.MenuItem {
                    text: qsTr("Z - Zoom")
                    enabled: false
                }

                Controls.MenuItem {
                    text: qsTr("U - Shape")
                    enabled: false
                }

                Controls.MenuItem {
                    text: qsTr("G - Fill")
                    enabled: false
                }

                Controls.MenuItem {
                    text: qsTr("T - Text")
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
