#include <QImage>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "rasterdocumentio.h"

class tst_RasterDocumentIO : public QObject
{
    Q_OBJECT

private slots:
    void supportsOnlyFlatRasterFormats();
    void loadsRasterMetadataFromLocalFile();
    void rejectsUnsupportedOrMissingInputs();
};

void tst_RasterDocumentIO::supportsOnlyFlatRasterFormats()
{
    RasterDocumentIO documentIO;

    QVERIFY(documentIO.supportsRasterFile(QStringLiteral("/tmp/example.png")));
    QVERIFY(documentIO.supportsRasterFile(QStringLiteral("/tmp/example.JPG")));
    QVERIFY(!documentIO.supportsRasterFile(QStringLiteral("/tmp/example.psd")));
    QVERIFY(!documentIO.supportsRasterFile(QStringLiteral("https://example.com/example.png")));
}

void tst_RasterDocumentIO::loadsRasterMetadataFromLocalFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QImage image(7, 5, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#26c6da")));

    const QString imagePath = directory.filePath(QStringLiteral("sample.PNG"));
    QVERIFY(image.save(imagePath));

    RasterDocumentIO documentIO;
    const QVariantMap result = documentIO.loadRasterDocument(QUrl::fromLocalFile(imagePath).toString());

    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    QCOMPARE(result.value(QStringLiteral("source")).toString(), QUrl::fromLocalFile(imagePath).toString());
    QCOMPARE(result.value(QStringLiteral("width")).toInt(), 7);
    QCOMPARE(result.value(QStringLiteral("height")).toInt(), 5);
}

void tst_RasterDocumentIO::rejectsUnsupportedOrMissingInputs()
{
    RasterDocumentIO documentIO;

    const QVariantMap remoteResult = documentIO.loadRasterDocument(QStringLiteral("https://example.com/example.png"));
    QVERIFY(!remoteResult.value(QStringLiteral("ok")).toBool());

    const QVariantMap unsupportedResult = documentIO.loadRasterDocument(QStringLiteral("/tmp/example.psd"));
    QVERIFY(!unsupportedResult.value(QStringLiteral("ok")).toBool());

    const QVariantMap missingResult = documentIO.loadRasterDocument(QStringLiteral("/tmp/missing-file.png"));
    QVERIFY(!missingResult.value(QStringLiteral("ok")).toBool());
}

QTEST_APPLESS_MAIN(tst_RasterDocumentIO)

#include "tst_rasterdocumentio.moc"
