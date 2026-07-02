#include <QFile>
#include <QString>
#include <QStringList>
#include <QtTest>

class tst_MainQmlContract : public QObject
{
    Q_OBJECT

private slots:
    void applicationWindowKeepsNativeControlsWhileUsingSolidVisualChrome();
    void applicationWindowKeepsEmptyLogicalTopDragHandle();
    void applicationWindowPassesTopDragHandleHeightToCanvasPage();
    void applicationWindowProvidesApplicationMenuBar();
    void applicationMenuAssignsShortcutContracts();
};

void tst_MainQmlContract::applicationWindowKeepsNativeControlsWhileUsingSolidVisualChrome()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("LV.ApplicationWindow")));
    QVERIFY(mainSource.contains(QStringLiteral("solidChrome: true")));
    QVERIFY(!mainSource.contains(QStringLiteral("flags:")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.CustomizeWindowHint")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.FramelessWindowHint")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.WindowTitleHint")));
}

void tst_MainQmlContract::applicationWindowKeepsEmptyLogicalTopDragHandle()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("windowDragHandleEnabled: true")));
    QVERIFY(!mainSource.contains(QStringLiteral("windowDragHandleHeight: 0")));
    QVERIFY(!mainSource.contains(QStringLiteral("onTitlebarDragRequested: window.requestWindowMove()")));
    QVERIFY(!mainSource.contains(QStringLiteral("MouseArea")));
    QVERIFY(!mainSource.contains(QStringLiteral("DragHandler")));
}

void tst_MainQmlContract::applicationWindowPassesTopDragHandleHeightToCanvasPage()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("topChromeReservedHeight: window.windowDragHandleEnabled ? window.windowDragHandleHeight : 0")));
}

void tst_MainQmlContract::applicationWindowProvidesApplicationMenuBar()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("import QtQuick.Controls as Controls")));
    QVERIFY(mainSource.contains(QStringLiteral("import QtQuick.Window as QtQuickWindow")));
    QVERIFY(mainSource.contains(QStringLiteral("menuBar: Controls.MenuBar")));
    const qsizetype fileMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"File\")"));
    const qsizetype editMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Edit\")"));
    const qsizetype windowMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Window\")"));
    const qsizetype helpMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Help\")"));
    QVERIFY(fileMenuIndex >= 0);
    QVERIFY(editMenuIndex > fileMenuIndex);
    QVERIFY(windowMenuIndex > editMenuIndex);
    QVERIFY(helpMenuIndex > windowMenuIndex);
    QVERIFY(mainSource.contains(QStringLiteral("function requestNewCanvas()")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openNewCanvasDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openFileDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openSaveDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.undoActiveRasterSurface();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.redoActiveRasterSurface();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.addLayer();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.deleteCurrentLayer();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.setToolMode(toolMode);")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.selectShapeTool(shapeKind);")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.fitCanvasToWindow();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.resetCanvasView();")));
    QVERIFY(mainSource.contains(QStringLiteral("QtQuickWindow.Window.FullScreen")));
    QVERIFY(mainSource.contains(QStringLiteral("Qt.quit()")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"New Canvas...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Open Image...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Save Image As...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Clear Canvas\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Undo\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Redo\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Add Layer\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Delete Current Layer\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Tools\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Shape Kind\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Fit Canvas to Window\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Reset Canvas View\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Keyboard Shortcuts\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Vincent 4.0\")")));
}

void tst_MainQmlContract::applicationMenuAssignsShortcutContracts()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("readonly property string menuCommandModifier: Qt.platform.os === \"osx\" ? \"Meta\" : \"Ctrl\"")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property string shortcutSaveImageAs: menuCommandModifier + \"+S\"")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property string shortcutRedo: Qt.platform.os === \"osx\" ? \"Meta+Shift+Z\" : \"Ctrl+Y\"")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property string shortcutToggleFullScreen: Qt.platform.os === \"osx\" ? \"Ctrl+Meta+F\" : \"F11\"")));
    QVERIFY(mainSource.contains(QStringLiteral("function shortcutReference(commandName, shortcutText)")));

    const QStringList actionShortcutContracts = {
            QStringLiteral("shortcutNewCanvas|newCanvasAction"),
            QStringLiteral("shortcutOpenImage|openImageAction"),
            QStringLiteral("shortcutSaveImageAs|saveImageAsAction"),
            QStringLiteral("shortcutClearCanvas|clearCanvasAction"),
            QStringLiteral("shortcutQuit|quitAction"),
            QStringLiteral("shortcutUndo|undoAction"),
            QStringLiteral("shortcutRedo|redoAction"),
            QStringLiteral("shortcutAddLayer|addLayerAction"),
            QStringLiteral("shortcutDeleteCurrentLayer|deleteCurrentLayerAction"),
            QStringLiteral("shortcutBrushTool|brushToolAction"),
            QStringLiteral("shortcutEraserTool|eraserToolAction"),
            QStringLiteral("shortcutHandPanTool|handPanToolAction"),
            QStringLiteral("shortcutMoveTool|moveToolAction"),
            QStringLiteral("shortcutZoomTool|zoomToolAction"),
            QStringLiteral("shortcutShapeTool|shapeToolAction"),
            QStringLiteral("shortcutFillTool|fillToolAction"),
            QStringLiteral("shortcutTextTool|textToolAction"),
            QStringLiteral("shortcutRectangleShape|rectangleShapeAction"),
            QStringLiteral("shortcutEllipseShape|ellipseShapeAction"),
            QStringLiteral("shortcutTriangleShape|triangleShapeAction"),
            QStringLiteral("shortcutDiamondShape|diamondShapeAction"),
            QStringLiteral("shortcutStarShape|starShapeAction"),
            QStringLiteral("shortcutRectangleBubbleShape|rectangleBubbleShapeAction"),
            QStringLiteral("shortcutEllipseBubbleShape|ellipseBubbleShapeAction"),
            QStringLiteral("shortcutDecreaseBrushSize|decreaseBrushSizeAction"),
            QStringLiteral("shortcutIncreaseBrushSize|increaseBrushSizeAction"),
            QStringLiteral("shortcutFitCanvasToWindow|fitCanvasToWindowAction"),
            QStringLiteral("shortcutResetCanvasView|resetCanvasViewAction"),
            QStringLiteral("shortcutMinimizeWindow|minimizeWindowAction"),
            QStringLiteral("shortcutToggleFullScreen|toggleFullScreenAction"),
    };

    QVERIFY(mainSource.contains(QStringLiteral("Controls.Action {")));
    QVERIFY(!mainSource.contains(QStringLiteral("Shortcut {")));

    for (const QString &actionShortcutContract : actionShortcutContracts) {
        const QStringList parts = actionShortcutContract.split(QLatin1Char('|'));
        QCOMPARE(parts.size(), 2);
        const QString shortcutContract = parts.at(0);
        const QString actionId = parts.at(1);

        QVERIFY2(mainSource.contains(QStringLiteral("readonly property string ") + shortcutContract),
                 qPrintable(shortcutContract + QStringLiteral(" property is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("id: ") + actionId),
                 qPrintable(actionId + QStringLiteral(" action is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("shortcut: window.") + shortcutContract),
                 qPrintable(shortcutContract + QStringLiteral(" action shortcut is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("action: ") + actionId),
                 qPrintable(actionId + QStringLiteral(" menu binding is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral(", window.") + shortcutContract + QStringLiteral(")")),
                 qPrintable(shortcutContract + QStringLiteral(" help reference is missing")));
    }
}

QTEST_APPLESS_MAIN(tst_MainQmlContract)

#include "tst_mainqmlcontract.moc"
