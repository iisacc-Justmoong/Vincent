#include <QtTest>

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>

#include "imageexport.h"
#include "imageimport.h"

namespace {

QString writePngFile(const QString &path, const QImage &image)
{
    return image.save(path) ? path : QString();
}

QString dataUrlForImage(const QImage &image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        return {};
    }
    return QStringLiteral("data:image/png;base64,%1").arg(QString::fromLatin1(bytes.toBase64()));
}

QVariantMap layerEntry(int imageId,
                       const QString &source,
                       int x,
                       int y,
                       int originalWidth,
                       int originalHeight,
                       qreal scaleX,
                       qreal scaleY,
                       const QString &layerName,
                       bool layerVisible,
                       qreal layerOpacity,
                       const QString &blendModeKey)
{
    return {
        {QStringLiteral("imageId"), imageId},
        {QStringLiteral("source"), source},
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
        {QStringLiteral("originalWidth"), originalWidth},
        {QStringLiteral("originalHeight"), originalHeight},
        {QStringLiteral("scaleX"), scaleX},
        {QStringLiteral("scaleY"), scaleY},
        {QStringLiteral("ready"), true},
        {QStringLiteral("layerName"), layerName},
        {QStringLiteral("layerVisible"), layerVisible},
        {QStringLiteral("layerOpacity"), layerOpacity},
        {QStringLiteral("blendModeKey"), blendModeKey}
    };
}

} // namespace

class tst_ImageExport : public QObject
{
    Q_OBJECT

private slots:
    void exportsTransparentCanvasAsPsd();
    void exportsLayeredPsdWithStrokeLayer();
};

void tst_ImageExport::exportsTransparentCanvasAsPsd()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString psdPath = directory.filePath(QStringLiteral("empty.psd"));

    ImageExport exporter;
    const QVariantMap exportResult = exporter.saveDocumentAsPsd(psdPath, 3, 2, {}, QString());
    QVERIFY(exportResult.value(QStringLiteral("ok")).toBool());
    QVERIFY(QFile::exists(psdPath));

    ImageImport importer;
    importer.setCacheDirectory(directory.filePath(QStringLiteral("cache")));
    const QVariantMap importResult = importer.prepareImageImport(psdPath);
    QVERIFY(importResult.value(QStringLiteral("ok")).toBool());
    QVERIFY(importResult.value(QStringLiteral("layers")).toList().isEmpty());

    const QVariantMap metadata = importResult.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("kind")).toString(), QStringLiteral("psd"));
    const QVariantMap psdMetadata = metadata.value(QStringLiteral("psd")).toMap();
    QCOMPARE(psdMetadata.value(QStringLiteral("width")).toInt(), 3);
    QCOMPARE(psdMetadata.value(QStringLiteral("height")).toInt(), 2);
    QCOMPARE(psdMetadata.value(QStringLiteral("layerCount")).toInt(), 0);
    QCOMPARE(psdMetadata.value(QStringLiteral("importedLayerCount")).toInt(), 0);
    QCOMPARE(psdMetadata.value(QStringLiteral("compression")).toString(), QStringLiteral("raw"));

    const QImage composite(QUrl(importResult.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!composite.isNull());
    QCOMPARE(composite.size(), QSize(3, 2));
    QCOMPARE(composite.pixelColor(0, 0), QColor(0, 0, 0, 0));
}

void tst_ImageExport::exportsLayeredPsdWithStrokeLayer()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QImage baseImage(4, 4, QImage::Format_ARGB32);
    baseImage.fill(QColor(0, 0, 255, 255));
    const QString basePath = directory.filePath(QStringLiteral("base.png"));
    QVERIFY(!writePngFile(basePath, baseImage).isEmpty());

    QImage accentImage(1, 1, QImage::Format_ARGB32);
    accentImage.fill(QColor(255, 0, 0, 255));
    const QString accentPath = directory.filePath(QStringLiteral("accent.png"));
    QVERIFY(!writePngFile(accentPath, accentImage).isEmpty());

    QImage strokeImage(4, 4, QImage::Format_ARGB32);
    strokeImage.fill(Qt::transparent);
    strokeImage.setPixelColor(3, 3, QColor(0, 255, 0, 255));
    const QString strokeDataUrl = dataUrlForImage(strokeImage);
    QVERIFY(!strokeDataUrl.isEmpty());

    const QVariantList layers = {
        layerEntry(1,
                   QUrl::fromLocalFile(basePath).toString(),
                   0,
                   0,
                   4,
                   4,
                   1.0,
                   1.0,
                   QStringLiteral("기본"),
                   true,
                   1.0,
                   QStringLiteral("norm")),
        layerEntry(2,
                   QUrl::fromLocalFile(accentPath).toString(),
                   1,
                   0,
                   1,
                   1,
                   2.0,
                   2.0,
                   QStringLiteral("Accent"),
                   true,
                   0.5,
                   QStringLiteral("mul "))
    };

    const QString psdPath = directory.filePath(QStringLiteral("layered.psd"));

    ImageExport exporter;
    const QVariantMap exportResult = exporter.saveDocumentAsPsd(psdPath, 4, 4, layers, strokeDataUrl);
    QVERIFY(exportResult.value(QStringLiteral("ok")).toBool());
    QVERIFY(QFile::exists(psdPath));

    ImageImport importer;
    importer.setCacheDirectory(directory.filePath(QStringLiteral("cache")));
    const QVariantMap importResult = importer.prepareImageImport(QUrl::fromLocalFile(psdPath).toString());
    QVERIFY(importResult.value(QStringLiteral("ok")).toBool());
    QVERIFY(importResult.value(QStringLiteral("source")).toString().isEmpty());

    const QVariantMap metadata = importResult.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("kind")).toString(), QStringLiteral("psd"));
    const QVariantMap psdMetadata = metadata.value(QStringLiteral("psd")).toMap();
    QCOMPARE(psdMetadata.value(QStringLiteral("width")).toInt(), 4);
    QCOMPARE(psdMetadata.value(QStringLiteral("height")).toInt(), 4);
    QCOMPARE(psdMetadata.value(QStringLiteral("layerCount")).toInt(), 3);
    QCOMPARE(psdMetadata.value(QStringLiteral("importedLayerCount")).toInt(), 3);
    QCOMPARE(psdMetadata.value(QStringLiteral("hasAlpha")).toBool(), true);

    const QVariantList importedLayers = importResult.value(QStringLiteral("layers")).toList();
    QCOMPARE(importedLayers.size(), 3);

    const QVariantMap strokeLayer = importedLayers.at(0).toMap();
    QCOMPARE(strokeLayer.value(QStringLiteral("name")).toString(), QStringLiteral("Paint"));
    QCOMPARE(strokeLayer.value(QStringLiteral("x")).toInt(), 3);
    QCOMPARE(strokeLayer.value(QStringLiteral("y")).toInt(), 3);
    QCOMPARE(strokeLayer.value(QStringLiteral("width")).toInt(), 1);
    QCOMPARE(strokeLayer.value(QStringLiteral("height")).toInt(), 1);
    QCOMPARE(strokeLayer.value(QStringLiteral("opacity")).toDouble(), 1.0);
    const QImage decodedStroke(QUrl(strokeLayer.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!decodedStroke.isNull());
    QCOMPARE(decodedStroke.pixelColor(0, 0), QColor(0, 255, 0, 255));

    const QVariantMap accentLayer = importedLayers.at(1).toMap();
    QCOMPARE(accentLayer.value(QStringLiteral("name")).toString(), QStringLiteral("Accent"));
    QCOMPARE(accentLayer.value(QStringLiteral("x")).toInt(), 1);
    QCOMPARE(accentLayer.value(QStringLiteral("y")).toInt(), 0);
    QCOMPARE(accentLayer.value(QStringLiteral("width")).toInt(), 2);
    QCOMPARE(accentLayer.value(QStringLiteral("height")).toInt(), 2);
    QCOMPARE(accentLayer.value(QStringLiteral("blendModeKey")).toString(), QStringLiteral("mul "));
    QCOMPARE(accentLayer.value(QStringLiteral("opacity")).toDouble(), 128.0 / 255.0);
    QCOMPARE(accentLayer.value(QStringLiteral("metadata")).toMap()
                 .value(QStringLiteral("psdLayer")).toMap()
                 .value(QStringLiteral("blendMode")).toString(),
             QStringLiteral("Multiply"));
    const QImage decodedAccent(QUrl(accentLayer.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!decodedAccent.isNull());
    QCOMPARE(decodedAccent.size(), QSize(2, 2));
    QCOMPARE(decodedAccent.pixelColor(0, 0), QColor(255, 0, 0, 255));

    const QVariantMap baseLayer = importedLayers.at(2).toMap();
    QCOMPARE(baseLayer.value(QStringLiteral("name")).toString(), QStringLiteral("기본"));
    QCOMPARE(baseLayer.value(QStringLiteral("x")).toInt(), 0);
    QCOMPARE(baseLayer.value(QStringLiteral("y")).toInt(), 0);
    QCOMPARE(baseLayer.value(QStringLiteral("width")).toInt(), 4);
    QCOMPARE(baseLayer.value(QStringLiteral("height")).toInt(), 4);
    const QImage decodedBase(QUrl(baseLayer.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!decodedBase.isNull());
    QCOMPARE(decodedBase.pixelColor(0, 0), QColor(0, 0, 255, 255));
}

QTEST_APPLESS_MAIN(tst_ImageExport)

#include "tst_imageexport.moc"
