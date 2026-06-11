#include <QFile>
#include <QString>
#include <QtTest>

class tst_MacOSBuildWorkflowContract : public QObject
{
    Q_OBJECT

private slots:
    void buildGuideSeparatesLocalAndDistributionSigning();
};

void tst_MacOSBuildWorkflowContract::buildGuideSeparatesLocalAndDistributionSigning()
{
    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");

    QFile buildGuide(buildGuidePath);
    QVERIFY(buildGuide.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(buildGuide.readAll());

    QVERIFY(source.contains(QStringLiteral("./build.sh local")));
    QVERIFY(source.contains(QStringLiteral("ctest --test-dir build --output-on-failure")));
    QVERIFY(source.contains(QStringLiteral("Apple Development")));
    QVERIFY(source.contains(QStringLiteral("VINCENT_BUILD_MODE=devid ./build.sh")));
    QVERIFY(source.contains(QStringLiteral("VINCENT_BUILD_MODE=mas ./build.sh")));
    QVERIFY(source.contains(QStringLiteral("Developer ID Application")));
    QVERIFY(source.contains(QStringLiteral("Apple Distribution")));
    QVERIFY(source.contains(QStringLiteral("NOTARY_APP_PASSWORD")));
}

QTEST_APPLESS_MAIN(tst_MacOSBuildWorkflowContract)

#include "tst_macosbuildworkflowcontract.moc"
