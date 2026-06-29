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
    void painterPageUsesLvHierarchyLayerPanel();
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
    const qsizetype currentColorButtonIndex = toolbarSource.indexOf(QStringLiteral("id: currentColorButton"));
    const qsizetype colorButtonMouseAreaIndex = toolbarSource.indexOf(QStringLiteral("id: colorButtonMouseArea"));
    QVERIFY(colorPickerIndex >= 0);
    QVERIFY(shapeMenuIndex > colorPickerIndex);
    QVERIFY(brushSettingsIndex > colorPickerIndex);
    QVERIFY(currentColorButtonIndex > brushSettingsIndex);
    QVERIFY(colorButtonMouseAreaIndex > currentColorButtonIndex);
    const QString colorPickerSource = toolbarSource.mid(colorPickerIndex, shapeMenuIndex - colorPickerIndex);
    const QString currentColorButtonSource = toolbarSource.mid(currentColorButtonIndex, colorButtonMouseAreaIndex - currentColorButtonIndex);

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
    QVERIFY(currentColorButtonSource.contains(QStringLiteral("id: colorPickerBall")));
    QVERIFY(currentColorButtonSource.contains(QStringLiteral("color: toolbar.currentColor")));
    QVERIFY(!currentColorButtonSource.contains(QStringLiteral("Canvas")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("function paintRgbRainbowBall()")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("onPaint: colorPickerBall.paintRgbRainbowBall()")));
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
    QVERIFY(toolbarSource.contains(QStringLiteral("readonly property url panHandIconSource: \"qrc:/Vincent/resources/icons/panHand.svg\"")));
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
    const qsizetype openIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"generalopen\""));
    const qsizetype saveIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"generalsave\""));
    const qsizetype panIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"panHand\""));
    const qsizetype translateIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"translateObject\""));
    const qsizetype zoomIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"generalsearch\""));
    const qsizetype brushIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"showCode\""));
    const qsizetype eraserIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"eraser\""));
    const qsizetype shapeIndex = toolbarSource.indexOf(QStringLiteral("id: shapeToolButton"));
    const qsizetype fillIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"fillbucket\""));
    const qsizetype typeIndex = toolbarSource.indexOf(QStringLiteral("iconName: \"typeAlias\""));

    QVERIFY(addIndex >= 0);
    QVERIFY(openIndex > addIndex);
    QVERIFY(saveIndex > openIndex);
    QVERIFY(panIndex > saveIndex);
    QVERIFY(translateIndex > panIndex);
    QVERIFY(zoomIndex > translateIndex);
    QVERIFY(brushIndex > zoomIndex);
    QVERIFY(eraserIndex > brushIndex);
    QVERIFY(shapeIndex > eraserIndex);
    QVERIFY(fillIndex > shapeIndex);
    QVERIFY(typeIndex > fillIndex);

    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"New canvas\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("signal newCanvasRequested(int canvasWidth, int canvasHeight)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function openNewCanvasDialog()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: newCanvasDialog")));
    QVERIFY(toolbarSource.contains(QStringLiteral("modal: true")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: newCanvasWidthField")));
    QVERIFY(toolbarSource.contains(QStringLiteral("id: newCanvasHeightField")));
    QVERIFY(toolbarSource.contains(QStringLiteral("toolbar.newCanvasRequested(nextWidth, nextHeight);")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.openNewCanvasDialog()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onActivated: toolbar.openNewCanvasDialog()")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("onClicked: toolbar.newCanvasRequested()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Open image\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.openFileDialog()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff *.psd)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Save image\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.openSaveDialog()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("function defaultSaveExtension(nameFilter)")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"PNG Image (*.png)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"JPEG Image (*.jpg *.jpeg)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"Bitmap Image (*.bmp)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"WebP Image (*.webp)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"TIFF Image (*.tif *.tiff)\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("qsTr(\"Photoshop Document (*.psd)\")")));
    const qsizetype psdSaveFilterIndex = toolbarSource.indexOf(QStringLiteral("qsTr(\"Photoshop Document (*.psd)\")"));
    const qsizetype pngSaveFilterIndex = toolbarSource.indexOf(QStringLiteral("qsTr(\"PNG Image (*.png)\")"));
    QVERIFY(psdSaveFilterIndex >= 0);
    QVERIFY(pngSaveFilterIndex >= 0);
    QVERIFY(psdSaveFilterIndex < pngSaveFilterIndex);
    QVERIFY(toolbarSource.contains(QStringLiteral("return \".webp\";")));
    QVERIFY(toolbarSource.contains(QStringLiteral("return \".tif\";")));
    QVERIFY(toolbarSource.contains(QStringLiteral("return \".psd\";")));
    QVERIFY(toolbarSource.contains(QStringLiteral("if (suffix.indexOf(\"png\") !== -1)")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"generaldelete\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Clear canvas\")")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("onClicked: toolbar.clearCanvasRequested()")));
    QVERIFY(toolbarSource.contains(QStringLiteral("tone: toolbar.currentTool === \"pan\" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconSource: toolbar.panHandIconSource")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Pan tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"pan\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("tone: toolbar.currentTool === \"move\" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconSource: toolbar.translateObjectIconSource")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"move\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("tone: toolbar.currentTool === \"zoom\" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Zoom tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"zoom\")")));
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
    QVERIFY(toolbarSource.contains(QStringLiteral("tone: toolbar.currentTool === \"fill\" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless")));
    QVERIFY(toolbarSource.contains(QStringLiteral("Accessible.name: qsTr(\"Fill tool\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"fill\")")));
    QVERIFY(toolbarSource.contains(QStringLiteral("tone: toolbar.currentTool === \"text\" ? LV.AbstractButton.Default : LV.AbstractButton.Borderless")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconSource: toolbar.typeAliasIconSource")));
    QVERIFY(toolbarSource.contains(QStringLiteral("onClicked: toolbar.toolSelected(\"text\")")));

    const QString qrcPath = QFINDTESTDATA("../App/resources.qrc");
    QVERIFY2(!qrcPath.isEmpty(), "resources.qrc test data was not found");
    QFile qrcFile(qrcPath);
    QVERIFY(qrcFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString qrcSource = QString::fromUtf8(qrcFile.readAll());
    QVERIFY(qrcSource.contains(QStringLiteral("resources/icons/panHand.svg")));
    QVERIFY(qrcSource.contains(QStringLiteral("resources/icons/typeAlias.svg")));

    const QString panHandSvgPath = QFINDTESTDATA("../App/resources/icons/panHand.svg");
    QVERIFY2(!panHandSvgPath.isEmpty(), "panHand.svg test data was not found");
    QFile panHandSvg(panHandSvgPath);
    QVERIFY(panHandSvg.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString panHandSvgSource = QString::fromUtf8(panHandSvg.readAll());
    QVERIFY(panHandSvgSource.contains(QStringLiteral("stroke=\"#CED0D6\"")));
    QVERIFY(panHandSvgSource.contains(QStringLiteral("fill=\"#2F2936\"")));

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
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.imageObjectForFile(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("appendDrawableObject({")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"image\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"text\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.saveToFileWithObjectsAndRasterLayers(")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function rasterLayerDescriptors()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.brushColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: textToolEditor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("visible: surface.textEditingActive")));
    QVERIFY(surfaceSource.contains(QStringLiteral("color: surface.brushColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function canvasMouseAcceptedButtons()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"shape\" || surface.toolMode === \"move\" || surface.toolMode === \"zoom\" || surface.toolMode === \"fill\" || surface.toolMode === \"text\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("acceptedButtons: surface.canvasMouseAcceptedButtons()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function canvasCursorShape()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"pan\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return surface.panDraggingActive ? Qt.ClosedHandCursor : Qt.OpenHandCursor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"move\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.SizeAllCursor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"zoom\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.SizeHorCursor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"fill\" || surface.toolMode === \"eraser\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("cursorShape: surface.canvasCursorShape()")));
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
    QVERIFY(surfaceSource.contains(QStringLiteral("function traceRectangleBubblePath(context, x, y, widthValue, heightValue)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function traceEllipseBubblePath(context, x, y, widthValue, heightValue)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function traceEllipseArcPath(context, x, y, widthValue, heightValue, startAngle, endAngle)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: shapePreviewCanvas")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"shape\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.shapeKind")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.brushColor")));
    QVERIFY(surfaceSource.contains(QStringLiteral("context.fillStyle = surface.brushColor.toString();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("context.fillStyle = drawableObjectDelegate.objectColor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("context.fill();")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("strokeWidth")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("context.stroke();")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("traceEllipsePath(context, x, y, widthValue, bodyHeight);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"shape\" || surface.toolMode === \"move\" || surface.toolMode === \"zoom\" || surface.toolMode === \"fill\" || surface.toolMode === \"text\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"shape\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("return Qt.CrossCursor;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("const aspectLocked = surface.shapeAspectLockedFromMouse(mouse);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.beginShapeDrag(mouse.x, mouse.y, aspectLocked);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.updateShapeDrag(mouse.x, mouse.y, aspectLocked);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.commitActiveShape();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function fillAt(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode !== \"fill\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("activeRasterSurface().fillAt(pointX, pointY, surface.brushColor);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.fillAt(mouse.x, mouse.y);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property real canvasZoomScale: 1")));
    QVERIFY(surfaceSource.contains(QStringLiteral("readonly property real defaultCanvasZoomScale: 1")));
    QVERIFY(surfaceSource.contains(QStringLiteral("readonly property real minimumCanvasZoomScale: 0.01")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function fittedCanvasZoomScale(canvasWidth, canvasHeight)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function fitCanvasZoomToCurrentCanvas()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("Math.min(surface.defaultCanvasZoomScale, surface.workspaceCanvasWidth / normalizedWidth, surface.workspaceCanvasHeight / normalizedHeight)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginZoomDrag(pointX)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function updateZoomDrag(pointX)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function cancelZoomDrag()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("scale: surface.canvasZoomScale")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.beginZoomDrag(mouse.x);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.updateZoomDrag(mouse.x);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.commitZoomDrag();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property real canvasPanOffsetX: 0")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginPanDrag(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function updatePanDrag(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function resetCanvasPan()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: canvasPanMouseArea")));
    QVERIFY(surfaceSource.contains(QStringLiteral("enabled: surface.toolMode === \"pan\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("x: Math.round((parent.width - width) / 2 + surface.canvasPanOffsetX)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("y: Math.round((parent.height - height) / 2 + surface.canvasPanOffsetY)")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("anchors.horizontalCenterOffset: surface.canvasPanOffsetX")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("anchors.verticalCenterOffset: surface.canvasPanOffsetY")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.beginPanDrag(mouse.x, mouse.y);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.updatePanDrag(mouse.x, mouse.y);")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.commitPanDrag();")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"B\", \"ㅠ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"brush\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"E\", \"ㄷ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"eraser\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"H\", \"ㅗ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"pan\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"V\", \"ㅍ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"move\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"Z\", \"ㅋ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"zoom\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"U\", \"ㅕ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"shape\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"G\", \"ㅎ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"fill\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"T\", \"ㅅ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.toolShortcutRequested(\"text\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property var drawableObjects: []")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function psdCompatibilityManifest()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("canvasSurface.psdCompatibilityManifest(surface.drawableObjects)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("source: drawableObjectDelegate.objectType === \"image\" ? drawableObjectDelegate.objectSource : \"\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function beginDrawableObjectTransform(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function updateDrawableObjectTransform(pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function resizedDrawableObject(originalObject, pointX, pointY)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function deleteSelectedDrawableObject()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"Delete\", \"Backspace\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onActivated: surface.deleteSelectedDrawableObject()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: drawableObjectSelectionFrame")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.beginDrawableObjectTransform(mouse.x, mouse.y);")));

    const QString painterPageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!painterPageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile painterPageQml(painterPageQmlPath);
    QVERIFY(painterPageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString painterPageSource = QString::fromUtf8(painterPageQml.readAll());

    QVERIFY(painterPageSource.contains(QStringLiteral("function setShapeKind(shapeKind)")));
    QVERIFY(painterPageSource.contains(QStringLiteral("function newCanvas(canvasWidth, canvasHeight)")));
    QVERIFY(painterPageSource.contains(QStringLiteral("drawingSurface.newCanvas(canvasWidth, canvasHeight);")));
    QVERIFY(painterPageSource.contains(QStringLiteral("canvasWidth: painterPage.vm ? painterPage.vm.canvasWidth : painterPage.fallbackNewCanvasWidth")));
    QVERIFY(painterPageSource.contains(QStringLiteral("canvasHeight: painterPage.vm ? painterPage.vm.canvasHeight : painterPage.fallbackNewCanvasHeight")));
    QVERIFY(painterPageSource.contains(QStringLiteral("onNewCanvasRequested: (canvasWidth, canvasHeight) => painterPage.newCanvas(canvasWidth, canvasHeight)")));
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

void tst_CanvasToolBarQmlContract::painterPageUsesLvHierarchyLayerPanel()
{
    const QString pageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    QVERIFY2(!pageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");

    QFile pageQml(pageQmlPath);
    QVERIFY(pageQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString pageSource = QString::fromUtf8(pageQml.readAll());

    QVERIFY(pageSource.contains(QStringLiteral("readonly property int layerPanelWidth")));
    QVERIFY(pageSource.contains(QStringLiteral("readonly property int layerRenameActivationWindowMs")));
    QVERIFY(pageSource.contains(QStringLiteral("readonly property int layerRenameRepeatActivationThreshold")));
    QVERIFY(pageSource.contains(QStringLiteral("property bool layerRenameActive: false")));
    QVERIFY(pageSource.contains(QStringLiteral("id: layerHierarchyPanel")));
    QVERIFY(pageSource.contains(QStringLiteral("objectName: \"layerHierarchyPanel\"")));
    QVERIFY(pageSource.contains(QStringLiteral("LV.Hierarchy")));
    QVERIFY(pageSource.contains(QStringLiteral("anchors.left: parent.left")));
    QVERIFY(pageSource.contains(QStringLiteral("model: drawingSurface.layerHierarchyRows")));
    QVERIFY(pageSource.contains(QStringLiteral("editable: true")));
    QVERIFY(pageSource.contains(QStringLiteral("footerVisible: true")));
    QVERIFY(pageSource.contains(QStringLiteral("footerButton1")));
    QVERIFY(pageSource.contains(QStringLiteral("iconName: \"add\"")));
    QVERIFY(pageSource.contains(QStringLiteral("footerButton2")));
    QVERIFY(pageSource.contains(QStringLiteral("iconName: \"remove\"")));
    QVERIFY(!pageSource.contains(QStringLiteral("iconName: \"delete\"")));
    QVERIFY(pageSource.contains(QStringLiteral("footerButton3")));
    QVERIFY(pageSource.contains(QStringLiteral("visible: false")));
    QVERIFY(pageSource.contains(QStringLiteral("function handleLayerHierarchyItemActivated(item)")));
    QVERIFY(pageSource.contains(QStringLiteral("painterPage.layerRenameActivationCount >= painterPage.layerRenameRepeatActivationThreshold")));
    QVERIFY(pageSource.contains(QStringLiteral("painterPage.beginLayerRename(item);")));
    QVERIFY(pageSource.contains(QStringLiteral("function beginLayerRename(item)")));
    QVERIFY(pageSource.contains(QStringLiteral("function commitLayerRename()")));
    QVERIFY(pageSource.contains(QStringLiteral("function cancelLayerRename()")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.activateLayerByKey(itemKey);")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.canRenameLayerByKey(itemKey)")));
    QVERIFY(pageSource.contains(QStringLiteral("layerRenameField.text = drawingSurface.layerNameByKey(itemKey);")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.renameLayerByKey(itemKey, nextName);")));
    QVERIFY(pageSource.contains(QStringLiteral("painterPage.handleLayerHierarchyItemActivated(item);")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.applyLayerHierarchyOrder(layerHierarchyPanel.model);")));
    QVERIFY(pageSource.contains(QStringLiteral("onFooterButtonTriggered: function (index)")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.addEmptyLayer();")));
    QVERIFY(pageSource.contains(QStringLiteral("drawingSurface.deleteSelectedDrawableObject();")));
    QVERIFY(pageSource.contains(QStringLiteral("layerHierarchyPanel.activateListItemByKey(drawingSurface.currentLayerKey())")));
    QVERIFY(pageSource.contains(QStringLiteral("LV.ToolbarButton")));
    QVERIFY(!pageSource.contains(QStringLiteral("buttonId: \"delete\"")));
    QVERIFY(pageSource.contains(QStringLiteral("anchors.left: layerHierarchyPanel.right")));
    QVERIFY(pageSource.contains(QStringLiteral("id: layerRenameEditorFrame")));
    QVERIFY(pageSource.contains(QStringLiteral("parent: layerHierarchyPanel")));
    QVERIFY(pageSource.contains(QStringLiteral("TextInput")));
    QVERIFY(pageSource.contains(QStringLiteral("id: layerRenameField")));
    QVERIFY(pageSource.contains(QStringLiteral("onAccepted: painterPage.commitLayerRename()")));
    QVERIFY(pageSource.contains(QStringLiteral("Keys.onEscapePressed: function (event)")));

    const QString surfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!surfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    QFile surfaceQml(surfaceQmlPath);
    QVERIFY(surfaceQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString surfaceSource = QString::fromUtf8(surfaceQml.readAll());

    QVERIFY(surfaceSource.contains(QStringLiteral("property var layerHierarchyRows: []")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property string backgroundLayerThumbnailSource: \"\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property var drawableObjectThumbnailSources: ({})")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property var rasterLayerThumbnailSources: ({})")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function rebuildLayerHierarchyRows()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function layerIconSourceForDrawableObject(drawableObject)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("iconSource: layerIconSourceForDrawableObject(drawableObject)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("iconSource: surface.backgroundLayerThumbnailSource")));
    QVERIFY(surfaceSource.contains(QStringLiteral("iconGlyph: \"\"")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("function layerIconGlyphForDrawableObject(drawableObject)")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("iconGlyph: \"R\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function addEmptyLayer()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property int nextEmptyLayerNumber: 1")));
    QVERIFY(surfaceSource.contains(QStringLiteral("id: drawableObjectVisualModel")));
    QVERIFY(surfaceSource.contains(QStringLiteral("model: drawableObjectVisualModel")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("model: surface.drawableObjects")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function drawableObjectVisualModelEntry(drawableObject)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function reorderDrawableObjectVisualModel(orderedObjects)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function drawableObjectVisualModelCount()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("parent: canvasSurface")));
    QVERIFY(surfaceSource.contains(QStringLiteral("property var rasterLayerItems: ({})")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function registerRasterLayerItem(objectId, surfaceItem)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function unregisterRasterLayerItem(objectId, surfaceItem)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("cacheRasterThumbnailSource(surface.layerHierarchyThumbnailSize, surface.layerHierarchyThumbnailSize)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("cacheDrawableObjectThumbnailSource(drawableObject, surface.layerHierarchyThumbnailSize, surface.layerHierarchyThumbnailSize)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onRasterContentChanged: surface.refreshBackgroundLayerThumbnailSource()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("onRasterContentChanged: surface.refreshRasterLayerThumbnailSource(drawableObjectDelegate.rasterLayerObjectId)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function activeRasterSurface()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function nextEmptyLayerName()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function drawableObjectForLayerKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function activateLayerByKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function deleteLayerByKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function canRenameLayerByKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function layerNameByKey(layerKey)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function renameLayerByKey(layerKey, layerName)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("renamedObject.name = normalizedName;")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function applyLayerHierarchyOrder(layerRows)")));
    QVERIFY(surfaceSource.contains(QStringLiteral("function hasTransformableSelectedDrawableObject()")));
    QVERIFY(surfaceSource.contains(QStringLiteral("type: \"layer\"")));
    QVERIFY(surfaceSource.contains(QStringLiteral("objectName: \"rasterLayerSurface-\" + drawableObjectDelegate.rasterLayerObjectId")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.selectedDrawableObjectId === drawableObjectDelegate.rasterLayerObjectId && (surface.toolMode === \"brush\" || surface.toolMode === \"eraser\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("label: qsTr(\"Background\")")));
    QVERIFY(surfaceSource.contains(QStringLiteral("draggable: false")));
    QVERIFY(surfaceSource.contains(QStringLiteral("surface.toolMode === \"move\" && surface.hasTransformableSelectedDrawableObject()")));
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
