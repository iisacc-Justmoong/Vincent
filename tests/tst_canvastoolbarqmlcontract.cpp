#include <QFile>
#include <QString>
#include <QtTest>

class tst_CanvasToolBarQmlContract : public QObject
{
    Q_OBJECT

private slots:
    void brushReselectionOpensBrushSettingsMenu();
    void colorSelectionUsesHslTrianglePicker();
    void usesSelectedToolbarIconsExceptClearCanvas();
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
    QVERIFY(!toolbarSource.contains(QStringLiteral("Repeater")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("model: toolbar.palette")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("component ColorSwatch")));

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

void tst_CanvasToolBarQmlContract::usesSelectedToolbarIconsExceptClearCanvas()
{
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");

    QFile toolbarQml(toolbarQmlPath);
    QVERIFY(toolbarQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString toolbarSource = QString::fromUtf8(toolbarQml.readAll());

    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"addFile\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"generalopen\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"generalsave\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"generaldelete\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"generaledit\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"eraser\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"imagezoomOut\"")));
    QVERIFY(toolbarSource.contains(QStringLiteral("iconName: \"imagezoomIn\"")));

    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"virtualFolder\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"imageClassification\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"rendererKit\"")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("iconName: \"clearOutputs\"")));
}

QTEST_APPLESS_MAIN(tst_CanvasToolBarQmlContract)

#include "tst_canvastoolbarqmlcontract.moc"
