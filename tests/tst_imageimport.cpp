#include <QtTest>

#include <QFileInfo>
#include <QBuffer>
#include <QDataStream>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>

#include "imageimport.h"

namespace {

QByteArray encodeLiteralPackBits(const QByteArray &rawRow)
{
    QByteArray encoded;
    qsizetype offset = 0;
    while (offset < rawRow.size()) {
        const qsizetype chunkSize = std::min<qsizetype>(128, rawRow.size() - offset);
        encoded.append(static_cast<char>(chunkSize - 1));
        encoded.append(rawRow.constData() + offset, chunkSize);
        offset += chunkSize;
    }
    return encoded;
}

QByteArray buildLayerNameExtraData(const QString &name)
{
    QByteArray extraData;
    QBuffer buffer(&extraData);
    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << quint32(0);
    stream << quint32(0);

    const QByteArray nameBytes = name.left(255).toLatin1();
    buffer.putChar(static_cast<char>(nameBytes.size()));
    buffer.write(nameBytes);

    const int padding = (4 - ((1 + nameBytes.size()) % 4)) % 4;
    if (padding > 0) {
        buffer.write(QByteArray(padding, '\0'));
    }

    return extraData;
}

QByteArray buildChannelChunk(const QByteArray &rawChannel, quint16 width, quint16 height, quint16 compression)
{
    QByteArray chunk;
    QBuffer buffer(&chunk);
    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << compression;

    if (compression == 0) {
        buffer.write(rawChannel);
        return chunk;
    }

    QVector<QByteArray> encodedRows;
    encodedRows.reserve(height);
    for (quint16 row = 0; row < height; ++row) {
        encodedRows.push_back(encodeLiteralPackBits(rawChannel.mid(row * width, width)));
    }

    for (const QByteArray &row : encodedRows) {
        stream << quint16(row.size());
    }

    for (const QByteArray &row : encodedRows) {
        buffer.write(row);
    }

    return chunk;
}

struct TestPsdLayer
{
    QString name;
    qint32 top = 0;
    qint32 left = 0;
    qint32 bottom = 0;
    qint32 right = 0;
    QVector<QPair<qint16, QByteArray>> channels;
    quint16 compression = 0;
    QByteArray blendModeKey = QByteArrayLiteral("norm");
    quint8 opacity = 255;
    quint8 flags = 0;
};

QByteArray buildPsdDocument(quint16 width,
                            quint16 height,
                            quint16 colorMode,
                            const QVector<QByteArray> &channels,
                            quint16 compression)
{
    QByteArray document;
    QBuffer buffer(&document);
    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);

    buffer.write("8BPS", 4);
    stream << quint16(1);
    buffer.write(QByteArray(6, '\0'));
    stream << quint16(channels.size());
    stream << quint32(height);
    stream << quint32(width);
    stream << quint16(8);
    stream << colorMode;
    stream << quint32(0);
    stream << quint32(0);
    stream << quint32(0);
    stream << compression;

    if (compression == 0) {
        for (const QByteArray &channel : channels) {
            buffer.write(channel);
        }
        return document;
    }

    QVector<QByteArray> encodedRows;
    encodedRows.reserve(channels.size() * height);
    for (const QByteArray &channel : channels) {
        for (quint16 row = 0; row < height; ++row) {
            const QByteArray rawRow = channel.mid(row * width, width);
            encodedRows.push_back(encodeLiteralPackBits(rawRow));
        }
    }

    for (const QByteArray &row : encodedRows) {
        stream << quint16(row.size());
    }

    for (const QByteArray &row : encodedRows) {
        buffer.write(row);
    }

    return document;
}

QByteArray buildLayeredPsdDocument(quint16 width,
                                   quint16 height,
                                   quint16 colorMode,
                                   const QVector<TestPsdLayer> &layers,
                                   const QVector<QByteArray> &compositeChannels,
                                   quint16 compositeCompression)
{
    QByteArray layerInfoData;
    QBuffer layerInfoBuffer(&layerInfoData);
    layerInfoBuffer.open(QIODevice::WriteOnly);

    QDataStream layerInfoStream(&layerInfoBuffer);
    layerInfoStream.setByteOrder(QDataStream::BigEndian);
    layerInfoStream << qint16(layers.size());

    QVector<QVector<QByteArray>> layerChunks;
    layerChunks.reserve(layers.size());

    for (const TestPsdLayer &layer : layers) {
        QVector<QByteArray> channelChunks;
        channelChunks.reserve(layer.channels.size());
        const quint16 layerWidth = static_cast<quint16>(layer.right - layer.left);
        const quint16 layerHeight = static_cast<quint16>(layer.bottom - layer.top);
        for (const auto &channel : layer.channels) {
            channelChunks.push_back(buildChannelChunk(channel.second, layerWidth, layerHeight, layer.compression));
        }
        layerChunks.push_back(channelChunks);
    }

    for (int layerIndex = 0; layerIndex < layers.size(); ++layerIndex) {
        const TestPsdLayer &layer = layers.at(layerIndex);
        const QVector<QByteArray> &channelChunks = layerChunks.at(layerIndex);
        layerInfoStream << layer.top << layer.left << layer.bottom << layer.right;
        layerInfoStream << qint16(layer.channels.size());
        for (int channelIndex = 0; channelIndex < layer.channels.size(); ++channelIndex) {
            layerInfoStream << layer.channels.at(channelIndex).first;
            layerInfoStream << quint32(channelChunks.at(channelIndex).size());
        }

        layerInfoBuffer.write("8BIM", 4);
        QByteArray blendModeKey = layer.blendModeKey.leftJustified(4, ' ', true).left(4);
        layerInfoBuffer.write(blendModeKey.constData(), blendModeKey.size());
        layerInfoBuffer.putChar(static_cast<char>(layer.opacity));
        layerInfoBuffer.putChar('\0');
        layerInfoBuffer.putChar(static_cast<char>(layer.flags));
        layerInfoBuffer.putChar('\0');

        const QByteArray extraData = buildLayerNameExtraData(layer.name);
        layerInfoStream << quint32(extraData.size());
        layerInfoBuffer.write(extraData);
    }

    for (const QVector<QByteArray> &channelChunks : layerChunks) {
        for (const QByteArray &chunk : channelChunks) {
            layerInfoBuffer.write(chunk);
        }
    }

    QByteArray layerAndMaskSection;
    QBuffer layerAndMaskBuffer(&layerAndMaskSection);
    layerAndMaskBuffer.open(QIODevice::WriteOnly);

    QDataStream layerAndMaskStream(&layerAndMaskBuffer);
    layerAndMaskStream.setByteOrder(QDataStream::BigEndian);
    layerAndMaskStream << quint32(layerInfoData.size());
    layerAndMaskBuffer.write(layerInfoData);
    layerAndMaskStream << quint32(0);

    QByteArray document;
    QBuffer buffer(&document);
    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);

    buffer.write("8BPS", 4);
    stream << quint16(1);
    buffer.write(QByteArray(6, '\0'));
    stream << quint16(compositeChannels.size());
    stream << quint32(height);
    stream << quint32(width);
    stream << quint16(8);
    stream << colorMode;
    stream << quint32(0);
    stream << quint32(0);
    stream << quint32(layerAndMaskSection.size());
    buffer.write(layerAndMaskSection);
    stream << compositeCompression;

    if (compositeCompression == 0) {
        for (const QByteArray &channel : compositeChannels) {
            buffer.write(channel);
        }
        return document;
    }

    QVector<QByteArray> encodedRows;
    encodedRows.reserve(compositeChannels.size() * height);
    for (const QByteArray &channel : compositeChannels) {
        for (quint16 row = 0; row < height; ++row) {
            encodedRows.push_back(encodeLiteralPackBits(channel.mid(row * width, width)));
        }
    }

    for (const QByteArray &row : encodedRows) {
        stream << quint16(row.size());
    }

    for (const QByteArray &row : encodedRows) {
        buffer.write(row);
    }

    return document;
}

QString writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return {};
    }
    if (file.write(data) != data.size()) {
        return {};
    }
    return path;
}

} // namespace

class tst_ImageImport : public QObject
{
    Q_OBJECT

private slots:
    void supportsPsdAndRasterFormats();
    void passthroughsRegularRasterImage();
    void rasterizesRawRgbPsd();
    void rasterizesRleRgbaPsd();
    void extractsSeparatePsdLayers();
};

void tst_ImageImport::supportsPsdAndRasterFormats()
{
    ImageImport importer;

    QVERIFY(importer.supportsImageFile(QStringLiteral("/tmp/example.psd")));
    QVERIFY(importer.supportsImageFile(QStringLiteral("/tmp/example.png")));
    QVERIFY(!importer.supportsImageFile(QStringLiteral("/tmp/example.txt")));
}

void tst_ImageImport::passthroughsRegularRasterImage()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString pngPath = directory.filePath(QStringLiteral("sample.png"));
    QImage source(2, 2, QImage::Format_ARGB32);
    source.fill(QColor(12, 34, 56, 255));
    QVERIFY(source.save(pngPath));

    ImageImport importer;
    importer.setCacheDirectory(directory.filePath(QStringLiteral("cache")));

    const QVariantMap result = importer.prepareImageImport(QUrl::fromLocalFile(pngPath).toString());
    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    QCOMPARE(result.value(QStringLiteral("source")).toString(), QUrl::fromLocalFile(pngPath).toString());

    const QVariantMap metadata = result.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("kind")).toString(), QStringLiteral("raster"));
    QCOMPARE(metadata.value(QStringLiteral("originalSource")).toString(), QUrl::fromLocalFile(pngPath).toString());
    QCOMPARE(metadata.value(QStringLiteral("originalSuffix")).toString(), QStringLiteral("png"));
    QCOMPARE(metadata.value(QStringLiteral("fileName")).toString(), QStringLiteral("sample.png"));
    QCOMPARE(metadata.value(QStringLiteral("filePath")).toString(), pngPath);
    QCOMPARE(metadata.value(QStringLiteral("fileSize")).toLongLong(), QFileInfo(pngPath).size());
    QVERIFY(!metadata.value(QStringLiteral("importedAtUtc")).toString().isEmpty());
}

void tst_ImageImport::rasterizesRawRgbPsd()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QVector<QByteArray> channels = {
        QByteArray::fromRawData("\xFF\x00", 2),
        QByteArray::fromRawData("\x00\x00", 2),
        QByteArray::fromRawData("\x00\xFF", 2)
    };

    const QString psdPath = directory.filePath(QStringLiteral("raw.psd"));
    QVERIFY(!writeFile(psdPath, buildPsdDocument(2, 1, 3, channels, 0)).isEmpty());

    ImageImport importer;
    importer.setCacheDirectory(directory.filePath(QStringLiteral("cache")));

    const QVariantMap result = importer.prepareImageImport(QUrl::fromLocalFile(psdPath).toString());
    QVERIFY(result.value(QStringLiteral("ok")).toBool());

    const QVariantMap metadata = result.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("kind")).toString(), QStringLiteral("psd"));
    QCOMPARE(metadata.value(QStringLiteral("originalSource")).toString(), QUrl::fromLocalFile(psdPath).toString());
    QCOMPARE(metadata.value(QStringLiteral("originalSuffix")).toString(), QStringLiteral("psd"));
    QCOMPARE(metadata.value(QStringLiteral("fileName")).toString(), QStringLiteral("raw.psd"));
    const QVariantMap psdMetadata = metadata.value(QStringLiteral("psd")).toMap();
    QCOMPARE(psdMetadata.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(psdMetadata.value(QStringLiteral("width")).toInt(), 2);
    QCOMPARE(psdMetadata.value(QStringLiteral("height")).toInt(), 1);
    QCOMPARE(psdMetadata.value(QStringLiteral("channels")).toInt(), 3);
    QCOMPARE(psdMetadata.value(QStringLiteral("depth")).toInt(), 8);
    QCOMPARE(psdMetadata.value(QStringLiteral("colorMode")).toString(), QStringLiteral("RGB"));
    QCOMPARE(psdMetadata.value(QStringLiteral("compression")).toString(), QStringLiteral("raw"));
    QCOMPARE(psdMetadata.value(QStringLiteral("layerCount")).toInt(), 0);
    QCOMPARE(psdMetadata.value(QStringLiteral("importedLayerCount")).toInt(), 0);
    QCOMPARE(psdMetadata.value(QStringLiteral("hasAlpha")).toBool(), false);
    QCOMPARE(psdMetadata.value(QStringLiteral("colorModeDataBytes")).toLongLong(), 0);
    QCOMPARE(psdMetadata.value(QStringLiteral("imageResourcesBytes")).toLongLong(), 0);
    QCOMPARE(psdMetadata.value(QStringLiteral("layerMaskInfoBytes")).toLongLong(), 0);

    const QImage image(QUrl(result.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!image.isNull());
    QCOMPARE(image.size(), QSize(2, 1));
    QCOMPARE(image.pixelColor(0, 0), QColor(255, 0, 0, 255));
    QCOMPARE(image.pixelColor(1, 0), QColor(0, 0, 255, 255));
}

void tst_ImageImport::rasterizesRleRgbaPsd()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QVector<QByteArray> channels = {
        QByteArray::fromRawData("\x00\x00", 2),
        QByteArray::fromRawData("\x80\x80", 2),
        QByteArray::fromRawData("\x00\x00", 2),
        QByteArray::fromRawData("\xFF\x40", 2)
    };

    const QString psdPath = directory.filePath(QStringLiteral("rle.psd"));
    QVERIFY(!writeFile(psdPath, buildPsdDocument(2, 1, 3, channels, 1)).isEmpty());

    ImageImport importer;
    importer.setCacheDirectory(directory.filePath(QStringLiteral("cache")));

    const QVariantMap result = importer.prepareImageImport(psdPath);
    QVERIFY(result.value(QStringLiteral("ok")).toBool());

    const QVariantMap metadata = result.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("kind")).toString(), QStringLiteral("psd"));
    const QVariantMap psdMetadata = metadata.value(QStringLiteral("psd")).toMap();
    QCOMPARE(psdMetadata.value(QStringLiteral("compression")).toString(), QStringLiteral("rle"));
    QCOMPARE(psdMetadata.value(QStringLiteral("channels")).toInt(), 4);
    QCOMPARE(psdMetadata.value(QStringLiteral("layerCount")).toInt(), 0);
    QCOMPARE(psdMetadata.value(QStringLiteral("importedLayerCount")).toInt(), 0);
    QCOMPARE(psdMetadata.value(QStringLiteral("hasAlpha")).toBool(), true);

    const QImage image(QUrl(result.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!image.isNull());
    QCOMPARE(image.pixelColor(0, 0), QColor(0, 128, 0, 255));
    QCOMPARE(image.pixelColor(1, 0), QColor(0, 128, 0, 64));
}

void tst_ImageImport::extractsSeparatePsdLayers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    TestPsdLayer topLayer;
    topLayer.name = QStringLiteral("Highlight");
    topLayer.top = 1;
    topLayer.left = 1;
    topLayer.bottom = 3;
    topLayer.right = 3;
    topLayer.opacity = 128;
    topLayer.flags = 0;
    topLayer.channels = {
        qMakePair<qint16, QByteArray>(0, QByteArray::fromRawData("\xFF\xFF\xFF\xFF", 4)),
        qMakePair<qint16, QByteArray>(1, QByteArray::fromRawData("\x00\x00\x00\x00", 4)),
        qMakePair<qint16, QByteArray>(2, QByteArray::fromRawData("\x00\x00\x00\x00", 4))
    };

    TestPsdLayer bottomLayer;
    bottomLayer.name = QStringLiteral("Base");
    bottomLayer.top = 0;
    bottomLayer.left = 0;
    bottomLayer.bottom = 4;
    bottomLayer.right = 4;
    bottomLayer.flags = 0x02;
    bottomLayer.channels = {
        qMakePair<qint16, QByteArray>(0, QByteArray(16, '\x00')),
        qMakePair<qint16, QByteArray>(1, QByteArray(16, '\x00')),
        qMakePair<qint16, QByteArray>(2, QByteArray(16, '\xFF'))
    };

    const QVector<QByteArray> compositeChannels = {
        QByteArray(16, '\x00'),
        QByteArray(16, '\x00'),
        QByteArray(16, '\x00')
    };

    const QString psdPath = directory.filePath(QStringLiteral("layered.psd"));
    QVERIFY(!writeFile(psdPath,
                       buildLayeredPsdDocument(4,
                                               4,
                                               3,
                                               {topLayer, bottomLayer},
                                               compositeChannels,
                                               0)).isEmpty());

    ImageImport importer;
    importer.setCacheDirectory(directory.filePath(QStringLiteral("cache")));

    const QVariantMap result = importer.prepareImageImport(QUrl::fromLocalFile(psdPath).toString());
    QVERIFY(result.value(QStringLiteral("ok")).toBool());
    QVERIFY(result.value(QStringLiteral("source")).toString().isEmpty());

    const QVariantMap metadata = result.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(metadata.value(QStringLiteral("kind")).toString(), QStringLiteral("psd"));
    const QVariantMap psdMetadata = metadata.value(QStringLiteral("psd")).toMap();
    QCOMPARE(psdMetadata.value(QStringLiteral("layerCount")).toInt(), 2);
    QCOMPARE(psdMetadata.value(QStringLiteral("importedLayerCount")).toInt(), 2);

    const QVariantList layers = result.value(QStringLiteral("layers")).toList();
    QCOMPARE(layers.size(), 2);

    const QVariantMap top = layers.at(0).toMap();
    QCOMPARE(top.value(QStringLiteral("name")).toString(), QStringLiteral("Highlight"));
    QCOMPARE(top.value(QStringLiteral("x")).toInt(), 1);
    QCOMPARE(top.value(QStringLiteral("y")).toInt(), 1);
    QCOMPARE(top.value(QStringLiteral("width")).toInt(), 2);
    QCOMPARE(top.value(QStringLiteral("height")).toInt(), 2);
    QCOMPARE(top.value(QStringLiteral("visible")).toBool(), true);
    QCOMPARE(top.value(QStringLiteral("opacity")).toDouble(), 128.0 / 255.0);
    const QVariantMap topMetadata = top.value(QStringLiteral("metadata")).toMap();
    QCOMPARE(topMetadata.value(QStringLiteral("kind")).toString(), QStringLiteral("psd-layer"));
    QCOMPARE(topMetadata.value(QStringLiteral("psdLayer")).toMap().value(QStringLiteral("blendMode")).toString(), QStringLiteral("Normal"));

    const QImage topImage(QUrl(top.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!topImage.isNull());
    QCOMPARE(topImage.size(), QSize(2, 2));
    QCOMPARE(topImage.pixelColor(0, 0), QColor(255, 0, 0, 255));

    const QVariantMap bottom = layers.at(1).toMap();
    QCOMPARE(bottom.value(QStringLiteral("name")).toString(), QStringLiteral("Base"));
    QCOMPARE(bottom.value(QStringLiteral("visible")).toBool(), false);
    const QImage bottomImage(QUrl(bottom.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!bottomImage.isNull());
    QCOMPARE(bottomImage.size(), QSize(4, 4));
    QCOMPARE(bottomImage.pixelColor(0, 0), QColor(0, 0, 255, 255));
}

QTEST_APPLESS_MAIN(tst_ImageImport)

#include "tst_imageimport.moc"
