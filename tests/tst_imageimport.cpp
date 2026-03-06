#include <QtTest>

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

    const QImage image(QUrl(result.value(QStringLiteral("source")).toString()).toLocalFile());
    QVERIFY(!image.isNull());
    QCOMPARE(image.pixelColor(0, 0), QColor(0, 128, 0, 255));
    QCOMPARE(image.pixelColor(1, 0), QColor(0, 128, 0, 64));
}

QTEST_APPLESS_MAIN(tst_ImageImport)

#include "tst_imageimport.moc"
