#include <QFile>
#include <QString>
#include <QtTest>

class tst_CanvasToolBarQmlContract : public QObject
{
    Q_OBJECT

private slots:
    void brushReselectionOpensBrushSettingsMenu();
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

QTEST_APPLESS_MAIN(tst_CanvasToolBarQmlContract)

#include "tst_canvastoolbarqmlcontract.moc"
