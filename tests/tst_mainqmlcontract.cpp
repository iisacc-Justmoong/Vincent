#include <QFile>
#include <QString>
#include <QtTest>

class tst_MainQmlContract : public QObject
{
    Q_OBJECT

private slots:
    void applicationWindowKeepsNativeControlsWhileUsingSolidVisualChrome();
    void applicationWindowKeepsEmptyLogicalTopDragHandle();
    void applicationWindowPassesTopDragHandleHeightToCanvasPage();
    void applicationWindowProvidesApplicationMenuBar();
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
}

QTEST_APPLESS_MAIN(tst_MainQmlContract)

#include "tst_mainqmlcontract.moc"
