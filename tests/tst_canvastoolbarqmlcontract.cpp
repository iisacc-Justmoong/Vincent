#include <QFile>
#include <QString>
#include <QtTest>

class tst_CanvasToolBarQmlContract : public QObject
{
    Q_OBJECT

private slots:
    void brushReselectionOpensBrushSettingsMenu();
    void colorSelectionUsesHslTrianglePicker();
    void leftToolbarMatchesFigmaDesignContract();
    void drawingSurfaceProvidesPaintStyleTextToolEditor();
    void shapeToolProvidesSplitMenuAndDragInsertion();
    void brushSizeControlsFlowFromDecreaseToSliderToIncrease();
    void toolbarUsesFullRoundSolidCylinderBackground();
    void toolbarIsOffsetBelowApplicationWindowTopChrome();
    void pressureCurveControlsUseThreePointGraphAtBottom();
};

void tst_CanvasToolBarQmlContract::brushReselectionOpensBrushSettingsMenu()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(source.contains(QStringLiteral("function activateBrushTool(triggerItem)")));
    QVERIFY(source.contains(QStringLiteral("if (toolbar.currentTool === \"brush\")")));
    QVERIFY(source.contains(QStringLiteral("toolbar.openBrushSettingsMenu(triggerItem);")));
    QVERIFY(source.contains(QStringLiteral("toolbar.toolSelected(\"brush\");")));
    QVERIFY(source.contains(QStringLiteral("onClicked: toolbar.activateBrushTool(brushToolButton)")));
    QVERIFY(!source.contains(QStringLiteral("acceptedButtons: Qt.RightButton")));
}

void tst_CanvasToolBarQmlContract::colorSelectionUsesHslTrianglePicker()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());
    const qsizetype colorPickerIndex = toolbarSource.indexOf(QStringLiteral("id: colorPickerMenu"));
    const qsizetype shapeMenuIndex = toolbarSource.indexOf(QStringLiteral("id: shapeMenu"));
    const qsizetype brushSettingsIndex = toolbarSource.indexOf(QStringLiteral("id: brushSettingsMenu"));
    QVERIFY(colorPickerIndex >= 0);
    QVERIFY(shapeMenuIndex > colorPickerIndex);
    QVERIFY(brushSettingsIndex > colorPickerIndex);
    const QString colorPickerSource = toolbarSource.mid(colorPickerIndex, shapeMenuIndex - colorPickerIndex);

    const QString pickerQmlPath = QFINDTESTDATA("../App/qml/brush/HslTriangleColorPicker.qml");
    QVERIFY2(!pickerQmlPath.isEmpty(), "HslTriangleColorPicker.qml test data was not found");

    QFile pickerQml(pickerQmlPath);
    QVERIFY(pickerQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pickerSource = QString::fromUtf8(pickerQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("Controls.Popup")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: colorPickerMenu")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function openColorPickerMenu(triggerItem)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("HslTriangleColorPicker")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onColorSelected: selectedColor => toolbar.colorPicked(selectedColor)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Brush color\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: colorPickerBall")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function paintRgbRainbowBall()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onPaint: colorPickerBall.paintRgbRainbowBall()")));
    QVERIFY(!colorPickerSource.contains(QStringLiteral("Repeater")));
    QVERIFY(!colorPickerSource.contains(QStringLiteral("model: toolbar.palette")));
    QVERIFY(!colorPickerSource.contains(QStringLiteral("component ColorSwatch")));

    QVERIFY(pickerSource.contains(QStringLiteral("signal colorSelected(color selectedColor)")));
    QVERIFY(pickerSource.contains(QStringLiteral("function colorFromHue(hueValue)")));
    QVERIFY(pickerSource.contains(QStringLiteral("function colorFromTrianglePoint(point)")));
    QVERIFY(pickerSource.contains(QStringLiteral("Qt.hsla")));
    QVERIFY(pickerSource.contains(QStringLiteral("Canvas")));
    QVERIFY(pickerSource.contains(QStringLiteral("hueWheelCanvas")));
    QVERIFY(pickerSource.contains(QStringLiteral("triangleCanvas")));
    QVERIFY(pickerSource.contains(QStringLiteral("context.fillRect(x, y, 1, 1)")));
    QVERIFY(pickerSource.contains(QStringLiteral("picker.colorFromWeights(picker.selectedHue")));
    QVERIFY(!pickerSource.contains(QStringLiteral("createImageData")));
    QVERIFY(!pickerSource.contains(QStringLiteral("putImageData")));
}

void tst_CanvasToolBarQmlContract::leftToolbarMatchesFigmaDesignContract()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("readonly property int figmaToolbarButtonSize: 20")));
    QVERIFY(toolbarSource.contains(QStringLiteral("readonly property int figmaToolbarIconSize: 16")));
    QVERIFY(toolbarSource.contains(QStringLiteral("readonly property url translateObjectIconSource: \"qrc:/Vincent/resources/icons/translateObject.svg\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("readonly property url typeAliasIconSource: \"qrc:/Vincent/resources/icons/typeAlias.svg\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: leftToolbarActions")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: floatingBackground")));
    QVERIFY(toolbarSource.contains(QStringLiteral("anchors.fill: floatingBackground")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("readonly property int figmaLeftToolbarWidth")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("readonly property int figmaLeftToolbarHeight")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("readonly property int figmaToolbarOuterHorizontalPadding")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("readonly property int figmaToolbarOuterVerticalPadding")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("readonly property color figmaToolbarBackground")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("id: figmaLeftToolbar")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("Layout.preferredWidth: toolbar.figmaLeftToolbarWidth")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("Layout.preferredHeight: toolbar.figmaLeftToolbarHeight")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("color: toolbar.figmaToolbarBackground")));

    const qsizetype addIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"addFile\""));
    const qsizetype deleteIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"generaldelete\""));
    const qsizetype translateIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"translateObject\""));
    const qsizetype brushIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"showCode\""));
    const qsizetype eraserIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"eraser\""));
    const qsizetype shapeIndex = toolbarSource.indexOf(QStringLiteral("id: shapeToolButton"));
    const qsizetype typeIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"typeAlias\""));

    QVERIFY(addIndex >= 0);
    QVERIFY(deleteIndex > addIndex);
    QVERIFY(translateIndex > deleteIndex);
    QVERIFY(brushIndex > translateIndex);
    QVERIFY(eraserIndex > brushIndex);
    QVERIFY(shapeIndex > eraserIndex);
    QVERIFY(typeIndex > shapeIndex);

    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"New canvas\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.newCanvasRequested()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Clear canvas\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.clearCanvasRequested()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconSource: toolbar.translateObjectIconSource")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Brush tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.activateBrushTool(brushToolButton)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Eraser tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"eraser\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("accessibleName: qsTr(\"Shape tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("menuAccessibleName: qsTr(\"Open shape menu\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("tone: toolbar.currentTool === \"shape\" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: toolbar.selectedShapeIconName(toolbar.currentShape)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onBodyClicked: toolbar.toolSelected(\"shape\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onMenuClicked: toolbar.openShapeMenu(shapeToolButton)")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("onClicked: toolbar.openColorPickerMenu(shapeToolButton)")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("onClicked: toolbar.openColorPickerMenu(colorMenuButton)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("tone: toolbar.currentTool === \"text\" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconSource: toolbar.typeAliasIconSource")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"text\")")));

    const QString qrcPath = QFINDTESTDATA("../App/resources.qrc");
    QVERIFY2(!qrcPath.isEmpty(), "resources.qrc test data was not found");
    QFile qrcFile(qrcPath);
    QVERIFY(qrcFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString qrcSource = QString::fromUtf8(qrcFile.readAll());
    QVERIFY(qrcSource.contains(QStringLiteral("resources/icons/typeAlias.svg")));

    const QString typeAliasSvgPath = QFINDTESTDATA("../App/resources/icons/typeAlias.svg");
    QVERIFY2(!typeAliasSvgPath.isEmpty(), "typeAlias.svg test data was not found");
    QFile typeAliasSvg(typeAliasSvgPath);
    QVERIFY(typeAliasSvg.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString typeAliasSvgSource = QString::fromUtf8(typeAliasSvg.readAll());
    QVERIFY(typeAliasSvgSource.contains(QStringLiteral("rect x=\"2.5\" y=\"2.5\" width=\"11\" height=\"11\" rx=\"1.5\"")));
    QVERIFY(typeAliasSvgSource.contains(QStringLiteral("fill=\"#2F2936\"")));
    QVERIFY(typeAliasSvgSource.contains(QStringLiteral("stroke=\"#A571E6\"")));
    QVERIFY(typeAliasSvgSource.contains(QStringLiteral("M7.5 5.5H5V4.5H11V5.5H8.5V11.5H7.5V5.5Z")));

    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"virtualFolder\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"imageClassification\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"rendererKit\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"clearOutputs\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"generalopen\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"generalsave\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"generaledit\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("toolbar.toolSelected(\"translate\")")));
}

void tst_CanvasToolBarQmlContract::drawingSurfaceProvidesPaintStyleTextToolEditor()
{
    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile drawingSurfaceQml(drawingSurfaceQmlPath);
    QVERIFY(drawingSurfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString surfaceSource = QString::fromUtf8(drawingSurfaceQml.readAll());

    QVERIFY(surfaceSource.contains(QStringLiteral("property color textToolAccentColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property int textToolFramePadding")));
    QVERIFY(surfaceSource.contains(QStringLiteral("readonly property int minimumTextToolFontPixelSize")));
    QVERIFY(surfaceSource.contains(QStringLiteral("readonly property int textToolFontPixelSize: Math.max(surface.minimumTextToolFontPixelSize, Math.round(surface.brushSize))")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("readonly property int textToolFontPixelSize: 28")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function longestTextLine(textValue)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function textToolAvailableWidth()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function textToolMeasuredWidth()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function textToolResponsiveWidth()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginTextPlacement(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function commitActiveText()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.commitText(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.brushColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: textToolEditor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("visible: surface.textEditingActive")));
    QVERIFY(surfaceSource.contains(QStringLiteral("color: surface.brushColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("acceptedButtons: surface.toolMode === \"shape\" ? Qt.LeftButton : surface.toolMode === \"text\" ? Qt.LeftButton : Qt.NoButton")));
    QVERIFY(surfaceSource.contains(QStringLiteral("cursorShape: surface.toolMode === \"shape\" ? Qt.CrossCursor : surface.toolMode === \"text\" ? Qt.IBeamCursor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("border.color: surface.textToolAccentColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("selectionColor: surface.textToolAccentColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("TextMetrics")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: textToolLineMetrics")));
    QVERIFY(surfaceSource.contains(QStringLiteral("text: surface.longestTextLine(textToolEditor.text)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("width: surface.textToolResponsiveWidth()")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("width: surface.textToolDefaultWidth")));
    QVERIFY(surfaceSource.contains(QStringLiteral("enabled: !surface.textEditingActive")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"text\")")));

    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile painterPageQml(painterPageQmlPath);
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString painterPageSource = QString::fromUtf8(painterPageQml.readAll());

    QVERIFY(painterPageSource.contains(QStringLiteral("textToolAccentColor: LV.Theme.primary")));
    QVERIFY(painterPageSource.contains(QStringLiteral("textToolFramePadding: painterPage.spacingSmall")));
}

void tst_CanvasToolBarQmlContract::shapeToolProvidesSplitMenuAndDragInsertion()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("property string currentShape: \"rectangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("signal shapeSelected(string shapeKind)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function openShapeMenu(triggerItem)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function selectedShapeIconName(shapeKind)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function selectShape(shapeKind)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("LV.ContextMenu")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: shapeMenu")));
    QVERIFY(toolbarSource.contains(QStringLiteral("items: toolbar.shapeMenuEntries")));
    QVERIFY(toolbarSource.contains(QStringLiteral("showIconSlot: true")));
    QVERIFY(toolbarSource.contains(QStringLiteral("selectedIndex: toolbar.shapeMenuEntries.findIndex(entry => entry.shape === toolbar.currentShape)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onItemTriggered: function (index, item)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolbar.selectShape(item.shape);")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"rectangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"rectangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"ellipse\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"ellipse\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"triangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"triangle\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"diamond\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"diamond\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"star\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"star\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"rectanglebubble\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"rectanglebubble\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shape: \"ellipsebubble\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"ellipsebubble\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("shapeSelected(shapeKind);")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolSelected(\"shape\");")));
    QVERIFY(toolbarSource.contains(QStringLiteral("return entry && entry.iconName ? entry.iconName : \"rectangle\";")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("id: shapeMenuRepeater")));

    const QString drawingSurfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile drawingSurfaceQml(drawingSurfaceQmlPath);
    QVERIFY(drawingSurfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString surfaceSource = QString::fromUtf8(drawingSurfaceQml.readAll());

    QVERIFY(surfaceSource.contains(QStringLiteral("property string shapeKind: \"rectangle\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property bool shapeDraggingActive: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property bool shapeAspectLocked: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("readonly property int shapeToolStrokeWidth: Math.max(1, Math.round(surface.brushSize))")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function shapeAspectLockedFromMouse(mouse)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return (mouse.modifiers & Qt.ShiftModifier) !== 0;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function constrainedShapeDragPoint(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("const side = Math.min(Math.max(Math.abs(deltaX), Math.abs(deltaY)), horizontalLimit, verticalLimit);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function shapeDragPoint(pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function applyShapeDragPoint(pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginShapeDrag(pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function updateShapeDrag(pointX, pointY, aspectLocked)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function commitActiveShape()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function paintShapePreview(context, previewWidth, previewHeight)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: shapePreviewCanvas")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.commitShape(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.shapeKind")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.brushColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.shapeToolStrokeWidth")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"shape\" ? Qt.LeftButton")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"shape\" ? Qt.CrossCursor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.beginShapeDrag(mouse.x, mouse.y, aspectLocked);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.updateShapeDrag(mouse.x, mouse.y, aspectLocked);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.commitActiveShape();")));

    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile painterPageQml(painterPageQmlPath);
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString painterPageSource = QString::fromUtf8(painterPageQml.readAll());

    QVERIFY(painterPageSource.contains(QStringLiteral("function setShapeKind(shapeKind)")));
    QVERIFY(painterPageSource.contains(QStringLiteral("painterPage.updateDocumentProperty(\"shapeKind\", shapeKind);")));
    QVERIFY(painterPageSource.contains(QStringLiteral("shapeKind: painterPage.vm ? painterPage.vm.shapeKind : \"rectangle\"")));
    QVERIFY(painterPageSource.contains(QStringLiteral("currentShape: painterPage.vm ? painterPage.vm.shapeKind : \"rectangle\"")));
    QVERIFY(painterPageSource.contains(QStringLiteral("onShapeSelected: shapeKind => painterPage.setShapeKind(shapeKind)")));
}

void tst_CanvasToolBarQmlContract::brushSizeControlsFlowFromDecreaseToSliderToIncrease()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    const qsizetype decreaseIndex = toolbarSource.indexOf(QStringLiteral("Accessible.name: qsTr(\"Decrease brush size\")"));
    const qsizetype sliderIndex = toolbarSource.indexOf(QStringLiteral("id: sizeSlider"));
    const qsizetype increaseIndex = toolbarSource.indexOf(QStringLiteral("Accessible.name: qsTr(\"Increase brush size\")"));

    QVERIFY(decreaseIndex >= 0);
    QVERIFY(sliderIndex >= 0);
    QVERIFY(increaseIndex >= 0);
    QVERIFY(decreaseIndex < sliderIndex);
    QVERIFY(sliderIndex < increaseIndex);
}

void tst_CanvasToolBarQmlContract::toolbarUsesFullRoundSolidCylinderBackground()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    const qsizetype backgroundIndex = toolbarSource.indexOf(QStringLiteral("id: floatingBackground"));
    const qsizetype layoutIndex = toolbarSource.indexOf(QStringLiteral("id: toolbarLayout"));
    QVERIFY(backgroundIndex >= 0);
    QVERIFY(layoutIndex > backgroundIndex);

    const QString backgroundBlock = toolbarSource.mid(backgroundIndex, layoutIndex - backgroundIndex);
    QVERIFY(backgroundBlock.contains(QStringLiteral("radius: height / 2")));
    QVERIFY(backgroundBlock.contains(QStringLiteral("color: LV.Theme.panelBackground03")));
    QVERIFY(!backgroundBlock.contains(QStringLiteral("gradient: Gradient")));
    QVERIFY(!backgroundBlock.contains(QStringLiteral("GradientStop")));
    QVERIFY(!backgroundBlock.contains(QStringLiteral("anchors.top: parent.top")));
    QVERIFY(!backgroundBlock.contains(QStringLiteral("radius: LV.Theme.radiusLg")));
}

void tst_CanvasToolBarQmlContract::toolbarIsOffsetBelowApplicationWindowTopChrome()
{
    const QString pageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!pageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile pageQml(pageQmlPath);
    QVERIFY(pageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pageSource = QString::fromUtf8(pageQml.readAll());

    QVERIFY(pageSource.contains(QStringLiteral("property int topChromeReservedHeight: 0")));
    QVERIFY(pageSource.contains(QStringLiteral("readonly property int toolbarTopMargin: topChromeReservedHeight + spacingSmall")));
    QVERIFY(pageSource.contains(QStringLiteral("anchors.topMargin: painterPage.toolbarTopMargin")));
    QVERIFY(!pageSource.contains(QStringLiteral("anchors.topMargin: painterPage.spacingSmall")));
}

void tst_CanvasToolBarQmlContract::pressureCurveControlsUseThreePointGraphAtBottom()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("component PressureCurveGraph: Item")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: pressureCurveGraph")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: pressureCurveCanvas")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: pressurePointRepeater")));
    QVERIFY(toolbarSource.contains(QStringLiteral("model: pressureCurveGraph.pointModel")));
    QVERIFY(toolbarSource.contains(QStringLiteral("propertyName: \"pressureCurveMinimum\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("propertyName: \"pressureCurveCenter\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("propertyName: \"pressureCurveMaximum\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolbar.requestBrushPropertyChange(point.propertyName, nextValue);")));

    const qsizetype stabilizerIndex = toolbarSource.indexOf(QStringLiteral("propertyName: \"stabilizerStrength\""));
    const qsizetype graphIndex = toolbarSource.lastIndexOf(QStringLiteral("id: pressureCurveGraph"));
    QVERIFY(stabilizerIndex >= 0);
    QVERIFY(graphIndex > stabilizerIndex);

    const QString sliderComponent = toolbarSource.mid(toolbarSource.indexOf(QStringLiteral("component BrushPropertySlider")),
                                                     toolbarSource.indexOf(QStringLiteral("component PressureCurveGraph")) - toolbarSource.indexOf(QStringLiteral("component BrushPropertySlider")));
    QVERIFY(!sliderComponent.contains(QStringLiteral("pressureCurveMinimum")));
    QVERIFY(!sliderComponent.contains(QStringLiteral("pressureCurveCenter")));
    QVERIFY(!sliderComponent.contains(QStringLiteral("pressureCurveMaximum")));
}

QTEST_APPLESS_MAIN(tst_CanvasToolBarQmlContract)

#include "tst_canvastoolbarqmlcontract.moc"
