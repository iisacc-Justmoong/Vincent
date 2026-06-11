#include <QFile>
#include <QString>
#include <QtTest>

class tst_MainQmlContract : public QObject
{
    Q_OBJECT

private slots:
    void applicationWindowKeepsNativeControlsWhileUsingSolidVisualChrome();
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

QTEST_APPLESS_MAIN(tst_MainQmlContract)

#include "tst_mainqmlcontract.moc"
