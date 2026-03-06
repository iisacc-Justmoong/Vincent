#include "imageimport.h"

#include <QByteArray>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QIODevice>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QtEndian>

#include <algorithm>

namespace {

struct InputReference
{
    bool localFile = false;
    QString localPath;
    QString sourceUrl;
    QString suffix;
};

struct PsdHeader
{
    quint16 version = 0;
    quint16 channels = 0;
    quint32 height = 0;
    quint32 width = 0;
    quint16 depth = 0;
    quint16 colorMode = 0;
};

struct PsdLayerChannelInfo
{
    qint16 id = 0;
    quint32 dataLength = 0;
};

struct PsdLayerRecord
{
    qint32 top = 0;
    qint32 left = 0;
    qint32 bottom = 0;
    qint32 right = 0;
    QVector<PsdLayerChannelInfo> channels;
    QString name;
    QString blendModeKey;
    quint8 opacity = 255;
    quint8 clipping = 0;
    quint8 flags = 0;
};

struct PsdDecodedLayer
{
    QString name;
    qint32 top = 0;
    qint32 left = 0;
    qint32 width = 0;
    qint32 height = 0;
    QString blendModeKey;
    qreal opacity = 1.0;
    bool visible = true;
    QImage image;
};

constexpr quint16 kPsdVersion = 1;
constexpr quint16 kColorModeGrayscale = 1;
constexpr quint16 kColorModeIndexed = 2;
constexpr quint16 kColorModeRgb = 3;
constexpr quint16 kColorModeCmyk = 4;

QString colorModeName(quint16 colorMode)
{
    switch (colorMode) {
    case kColorModeGrayscale:
        return QStringLiteral("Grayscale");
    case kColorModeIndexed:
        return QStringLiteral("Indexed");
    case kColorModeRgb:
        return QStringLiteral("RGB");
    case kColorModeCmyk:
        return QStringLiteral("CMYK");
    default:
        return QStringLiteral("Unknown");
    }
}

QString compressionName(quint16 compression)
{
    switch (compression) {
    case 0:
        return QStringLiteral("raw");
    case 1:
        return QStringLiteral("rle");
    case 2:
        return QStringLiteral("zip");
    case 3:
        return QStringLiteral("zip-prediction");
    default:
        return QStringLiteral("unknown");
    }
}

QString blendModeName(const QString &blendModeKey)
{
    if (blendModeKey == QStringLiteral("norm")) {
        return QStringLiteral("Normal");
    }
    if (blendModeKey == QStringLiteral("mul ")) {
        return QStringLiteral("Multiply");
    }
    if (blendModeKey == QStringLiteral("scrn")) {
        return QStringLiteral("Screen");
    }
    if (blendModeKey == QStringLiteral("over")) {
        return QStringLiteral("Overlay");
    }
    if (blendModeKey == QStringLiteral("dark")) {
        return QStringLiteral("Darken");
    }
    if (blendModeKey == QStringLiteral("lite")) {
        return QStringLiteral("Lighten");
    }
    return blendModeKey.isEmpty() ? QStringLiteral("Unknown") : blendModeKey;
}

bool psdCompositeHasAlpha(const PsdHeader &header)
{
    switch (header.colorMode) {
    case kColorModeGrayscale:
    case kColorModeIndexed:
        return header.channels > 1;
    case kColorModeRgb:
        return header.channels > 3;
    case kColorModeCmyk:
        return header.channels > 4;
    default:
        return false;
    }
}

InputReference resolveInputReference(const QString &fileUrl)
{
    InputReference result;

    if (fileUrl.isEmpty()) {
        return result;
    }

    const QUrl url(fileUrl);
    if (url.isValid() && !url.scheme().isEmpty()) {
        result.sourceUrl = url.toString();
        result.suffix = QFileInfo(url.path()).suffix().toLower();
        if (url.isLocalFile()) {
            result.localFile = true;
            result.localPath = url.toLocalFile();
            result.sourceUrl = QUrl::fromLocalFile(result.localPath).toString();
            result.suffix = QFileInfo(result.localPath).suffix().toLower();
        }
        return result;
    }

    const QFileInfo fileInfo(fileUrl);
    result.localFile = true;
    result.localPath = fileInfo.absoluteFilePath();
    result.sourceUrl = QUrl::fromLocalFile(result.localPath).toString();
    result.suffix = fileInfo.suffix().toLower();
    return result;
}

bool isPsdSuffix(const QString &suffix)
{
    return suffix == QStringLiteral("psd");
}

QSet<QString> supportedRasterSuffixes()
{
    static const QSet<QString> suffixes = [] {
        QSet<QString> result{
            QStringLiteral("png"),
            QStringLiteral("jpg"),
            QStringLiteral("jpeg"),
            QStringLiteral("bmp"),
            QStringLiteral("gif"),
            QStringLiteral("webp"),
            QStringLiteral("tif"),
            QStringLiteral("tiff"),
            QStringLiteral("psd")
        };

        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        for (const QByteArray &format : formats) {
            result.insert(QString::fromLatin1(format).toLower());
        }

        return result;
    }();

    return suffixes;
}

QVariantMap buildBasicImportMetadata(const InputReference &ref, const QFileInfo &fileInfo, const QString &kind)
{
    QVariantMap metadata{
        {QStringLiteral("kind"), kind},
        {QStringLiteral("originalSource"), ref.sourceUrl},
        {QStringLiteral("originalSuffix"), ref.suffix},
        {QStringLiteral("importedAtUtc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
    };

    if (fileInfo.exists()) {
        metadata.insert(QStringLiteral("fileName"), fileInfo.fileName());
        metadata.insert(QStringLiteral("filePath"), fileInfo.absoluteFilePath());
        metadata.insert(QStringLiteral("fileSize"), fileInfo.size());
    }

    return metadata;
}

QVariantMap buildPsdLayerMetadata(const QVariantMap &baseMetadata,
                                  const QVariantMap &psdMetadata,
                                  const PsdDecodedLayer &layer,
                                  int layerIndex)
{
    QVariantMap metadata = baseMetadata;
    metadata.insert(QStringLiteral("kind"), QStringLiteral("psd-layer"));
    metadata.insert(QStringLiteral("psd"), psdMetadata);
    metadata.insert(QStringLiteral("psdLayer"),
                    QVariantMap{
                        {QStringLiteral("index"), layerIndex},
                        {QStringLiteral("name"), layer.name},
                        {QStringLiteral("x"), layer.left},
                        {QStringLiteral("y"), layer.top},
                        {QStringLiteral("width"), layer.width},
                        {QStringLiteral("height"), layer.height},
                        {QStringLiteral("opacity"), layer.opacity},
                        {QStringLiteral("visible"), layer.visible},
                        {QStringLiteral("blendModeKey"), layer.blendModeKey},
                        {QStringLiteral("blendMode"), blendModeName(layer.blendModeKey)}
                    });
    return metadata;
}

bool readExact(QIODevice &device, char *data, qint64 size)
{
    return device.read(data, size) == size;
}

bool skipExact(QIODevice &device, qint64 size)
{
    if (size < 0) {
        return false;
    }

    if (device.isSequential()) {
        QByteArray sink;
        sink.resize(static_cast<int>(std::min<qint64>(size, 64 * 1024)));
        qint64 remaining = size;
        while (remaining > 0) {
            const qint64 chunkSize = std::min<qint64>(remaining, sink.size());
            if (device.read(sink.data(), chunkSize) != chunkSize) {
                return false;
            }
            remaining -= chunkSize;
        }
        return true;
    }

    return device.seek(device.pos() + size);
}

bool readByte(QIODevice &device, quint8 &value)
{
    char byte = '\0';
    if (!readExact(device, &byte, 1)) {
        return false;
    }
    value = static_cast<quint8>(byte);
    return true;
}

quint8 sampleToByte(const QByteArray &channelData, qsizetype sampleIndex, quint16 depth)
{
    if (depth == 8) {
        return static_cast<quint8>(channelData.at(sampleIndex));
    }

    const qsizetype byteOffset = sampleIndex * 2;
    const auto value = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(channelData.constData() + byteOffset));
    return static_cast<quint8>(value / 257);
}

QByteArray decodePackBitsRow(const QByteArray &encodedRow, qsizetype expectedSize)
{
    QByteArray decoded;
    decoded.reserve(expectedSize);

    qsizetype index = 0;
    while (index < encodedRow.size() && decoded.size() < expectedSize) {
        const qint8 control = static_cast<qint8>(encodedRow.at(index));
        ++index;

        if (control >= 0) {
            const qsizetype literalCount = static_cast<qsizetype>(control) + 1;
            if (index + literalCount > encodedRow.size()) {
                return {};
            }
            decoded.append(encodedRow.constData() + index, literalCount);
            index += literalCount;
            continue;
        }

        if (control == -128) {
            continue;
        }

        if (index >= encodedRow.size()) {
            return {};
        }

        const char repeatedByte = encodedRow.at(index);
        ++index;
        decoded.append(QByteArray(static_cast<qsizetype>(1 - control), repeatedByte));
    }

    if (decoded.size() != expectedSize) {
        return {};
    }

    return decoded;
}

bool readPsdHeader(QIODevice &device, PsdHeader &header, QString &error)
{
    char signature[4];
    if (!readExact(device, signature, sizeof(signature))) {
        error = QStringLiteral("PSD header is truncated.");
        return false;
    }

    if (QByteArray(signature, sizeof(signature)) != QByteArrayLiteral("8BPS")) {
        error = QStringLiteral("File is not a PSD document.");
        return false;
    }

    QDataStream stream(&device);
    stream.setByteOrder(QDataStream::BigEndian);

    stream >> header.version;
    if (header.version != kPsdVersion) {
        error = QStringLiteral("Only PSD version 1 documents are supported.");
        return false;
    }

    char reserved[6];
    if (!readExact(device, reserved, sizeof(reserved))) {
        error = QStringLiteral("PSD reserved header bytes are truncated.");
        return false;
    }

    stream >> header.channels >> header.height >> header.width >> header.depth >> header.colorMode;
    if (stream.status() != QDataStream::Ok) {
        error = QStringLiteral("PSD header could not be parsed.");
        return false;
    }

    if (header.width == 0 || header.height == 0) {
        error = QStringLiteral("PSD dimensions are invalid.");
        return false;
    }

    if (header.depth != 8 && header.depth != 16) {
        error = QStringLiteral("Only 8-bit and 16-bit PSD files are supported.");
        return false;
    }

    if (header.channels == 0) {
        error = QStringLiteral("PSD channel count is invalid.");
        return false;
    }

    return true;
}

bool readSectionLength(QIODevice &device, quint32 &length, QString &error)
{
    QDataStream stream(&device);
    stream.setByteOrder(QDataStream::BigEndian);
    stream >> length;
    if (stream.status() != QDataStream::Ok) {
        error = QStringLiteral("PSD section length is truncated.");
        return false;
    }
    return true;
}

int parseLayerCount(const QByteArray &layerMaskInfoData)
{
    if (layerMaskInfoData.size() < 6) {
        return 0;
    }

    QBuffer buffer;
    buffer.setData(layerMaskInfoData);
    buffer.open(QIODevice::ReadOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 layerInfoLength = 0;
    stream >> layerInfoLength;
    if (stream.status() != QDataStream::Ok || layerInfoLength < 2) {
        return 0;
    }

    if (layerInfoLength > static_cast<quint32>(layerMaskInfoData.size() - 4)) {
        return 0;
    }

    qint16 rawLayerCount = 0;
    stream >> rawLayerCount;
    if (stream.status() != QDataStream::Ok) {
        return 0;
    }

    return std::abs(rawLayerCount);
}

QString decodeUnicodeLayerName(const QByteArray &blockData)
{
    if (blockData.size() < 4) {
        return {};
    }

    QBuffer buffer;
    buffer.setData(blockData);
    buffer.open(QIODevice::ReadOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);

    quint32 characterCount = 0;
    stream >> characterCount;
    if (stream.status() != QDataStream::Ok) {
        return {};
    }

    if (characterCount > static_cast<quint32>((blockData.size() - 4) / 2)) {
        return {};
    }

    QString result;
    result.reserve(static_cast<int>(characterCount));
    for (quint32 index = 0; index < characterCount; ++index) {
        quint16 codeUnit = 0;
        stream >> codeUnit;
        if (stream.status() != QDataStream::Ok) {
            return {};
        }
        result.append(QChar(codeUnit));
    }

    return result;
}

bool readPascalString(QIODevice &device, int paddedTo, QString &value, QString &error)
{
    quint8 length = 0;
    if (!readByte(device, length)) {
        error = QStringLiteral("PSD layer name is truncated.");
        return false;
    }

    QByteArray bytes;
    bytes.resize(length);
    if (length > 0 && !readExact(device, bytes.data(), length)) {
        error = QStringLiteral("PSD layer name is truncated.");
        return false;
    }

    value = QString::fromLatin1(bytes);

    const int consumed = 1 + static_cast<int>(length);
    const int padding = (paddedTo - (consumed % paddedTo)) % paddedTo;
    if (padding > 0 && !skipExact(device, padding)) {
        error = QStringLiteral("PSD layer name padding is truncated.");
        return false;
    }

    return true;
}

bool parseLayerExtraData(const QByteArray &extraData, QString &layerName, QString &error)
{
    QBuffer buffer;
    buffer.setData(extraData);
    buffer.open(QIODevice::ReadOnly);

    quint32 layerMaskDataLength = 0;
    if (!readSectionLength(buffer, layerMaskDataLength, error) || !skipExact(buffer, layerMaskDataLength)) {
        error = QStringLiteral("PSD layer mask extra data is truncated.");
        return false;
    }

    quint32 layerBlendingRangesLength = 0;
    if (!readSectionLength(buffer, layerBlendingRangesLength, error) || !skipExact(buffer, layerBlendingRangesLength)) {
        error = QStringLiteral("PSD layer blending ranges are truncated.");
        return false;
    }

    if (!readPascalString(buffer, 4, layerName, error)) {
        return false;
    }

    while (buffer.bytesAvailable() >= 12) {
        char signature[4];
        char key[4];
        if (!readExact(buffer, signature, sizeof(signature)) || !readExact(buffer, key, sizeof(key))) {
            error = QStringLiteral("PSD additional layer info is truncated.");
            return false;
        }

        quint32 blockLength = 0;
        if (!readSectionLength(buffer, blockLength, error)) {
            return false;
        }

        QByteArray blockData;
        blockData.resize(blockLength);
        if (blockLength > 0 && !readExact(buffer, blockData.data(), blockLength)) {
            error = QStringLiteral("PSD additional layer info is truncated.");
            return false;
        }

        const QByteArray keyBytes(key, sizeof(key));
        if (keyBytes == QByteArrayLiteral("luni")) {
            const QString unicodeName = decodeUnicodeLayerName(blockData);
            if (!unicodeName.isEmpty()) {
                layerName = unicodeName;
            }
        }

        if ((blockLength % 2) == 1 && buffer.bytesAvailable() > 0 && !skipExact(buffer, 1)) {
            error = QStringLiteral("PSD additional layer info padding is truncated.");
            return false;
        }
    }

    return true;
}

bool isRenderableLayerChannel(quint16 colorMode, qint16 channelId)
{
    if (channelId == -1) {
        return true;
    }

    switch (colorMode) {
    case kColorModeGrayscale:
    case kColorModeIndexed:
        return channelId == 0;
    case kColorModeRgb:
        return channelId >= 0 && channelId <= 2;
    case kColorModeCmyk:
        return channelId >= 0 && channelId <= 3;
    default:
        return false;
    }
}

bool decodePsdChannelChunk(const QByteArray &chunkData,
                           qint32 width,
                           qint32 height,
                           quint16 depth,
                           QByteArray &decodedData,
                           QString &error)
{
    if (chunkData.size() < 2) {
        error = QStringLiteral("PSD layer channel chunk is truncated.");
        return false;
    }

    const qsizetype bytesPerSample = depth / 8;
    const qsizetype rowByteCount = static_cast<qsizetype>(width) * bytesPerSample;
    const qsizetype expectedSize = rowByteCount * static_cast<qsizetype>(height);
    const quint16 compression = qFromBigEndian<quint16>(reinterpret_cast<const uchar *>(chunkData.constData()));
    const QByteArray payload = chunkData.mid(2);

    if (compression == 0) {
        if (payload.size() < expectedSize) {
            error = QStringLiteral("PSD raw layer channel data is truncated.");
            return false;
        }
        decodedData = payload.left(expectedSize);
        return true;
    }

    if (compression != 1) {
        error = QStringLiteral("Only raw and RLE-compressed PSD layers are supported.");
        return false;
    }

    QBuffer buffer;
    buffer.setData(payload);
    buffer.open(QIODevice::ReadOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);

    QVector<quint16> rowLengths(height);
    for (qint32 row = 0; row < height; ++row) {
        stream >> rowLengths[row];
        if (stream.status() != QDataStream::Ok) {
            error = QStringLiteral("PSD layer RLE row table is truncated.");
            return false;
        }
    }

    QByteArray bytes;
    bytes.reserve(expectedSize);
    for (qint32 row = 0; row < height; ++row) {
        QByteArray encodedRow;
        encodedRow.resize(rowLengths[row]);
        if (rowLengths[row] > 0 && !readExact(buffer, encodedRow.data(), encodedRow.size())) {
            error = QStringLiteral("PSD layer RLE data is truncated.");
            return false;
        }

        const QByteArray decodedRow = decodePackBitsRow(encodedRow, rowByteCount);
        if (decodedRow.isEmpty() && rowByteCount > 0) {
            error = QStringLiteral("PSD layer RLE row could not be decoded.");
            return false;
        }
        bytes.append(decodedRow);
    }

    if (bytes.size() != expectedSize) {
        error = QStringLiteral("PSD layer channel size is invalid.");
        return false;
    }

    decodedData = bytes;
    return true;
}

QImage composePsdLayerImage(const PsdHeader &header,
                            const QByteArray &colorModeData,
                            const QHash<qint16, QByteArray> &channelData,
                            qint32 width,
                            qint32 height,
                            QString &error)
{
    QImage image(width, height, QImage::Format_ARGB32);
    if (image.isNull()) {
        error = QStringLiteral("PSD layer image could not be allocated.");
        return {};
    }

    auto sampleLayerChannel = [&](qint16 channelId, qsizetype sampleIndex, quint8 defaultValue, bool *present) {
        const auto it = channelData.constFind(channelId);
        if (it == channelData.constEnd()) {
            if (present) {
                *present = false;
            }
            return defaultValue;
        }
        if (present) {
            *present = true;
        }
        return sampleToByte(it.value(), sampleIndex, header.depth);
    };

    for (qint32 y = 0; y < height; ++y) {
        for (qint32 x = 0; x < width; ++x) {
            const qsizetype sampleIndex = static_cast<qsizetype>(y) * static_cast<qsizetype>(width) + static_cast<qsizetype>(x);
            quint8 red = 0;
            quint8 green = 0;
            quint8 blue = 0;
            quint8 alpha = 255;

            switch (header.colorMode) {
            case kColorModeGrayscale: {
                bool hasGray = false;
                const quint8 gray = sampleLayerChannel(0, sampleIndex, 0, &hasGray);
                if (!hasGray) {
                    error = QStringLiteral("PSD grayscale layer is missing a gray channel.");
                    return {};
                }
                red = gray;
                green = gray;
                blue = gray;
                alpha = sampleLayerChannel(-1, sampleIndex, 255, nullptr);
                break;
            }
            case kColorModeIndexed: {
                bool hasIndex = false;
                const quint8 paletteIndex = sampleLayerChannel(0, sampleIndex, 0, &hasIndex);
                if (!hasIndex) {
                    error = QStringLiteral("PSD indexed layer is missing an index channel.");
                    return {};
                }
                if (colorModeData.size() < 768) {
                    error = QStringLiteral("PSD indexed layer palette is missing.");
                    return {};
                }
                red = static_cast<quint8>(colorModeData.at(paletteIndex));
                green = static_cast<quint8>(colorModeData.at(256 + paletteIndex));
                blue = static_cast<quint8>(colorModeData.at(512 + paletteIndex));
                alpha = sampleLayerChannel(-1, sampleIndex, 255, nullptr);
                break;
            }
            case kColorModeRgb: {
                bool hasRed = false;
                bool hasGreen = false;
                bool hasBlue = false;
                red = sampleLayerChannel(0, sampleIndex, 0, &hasRed);
                green = sampleLayerChannel(1, sampleIndex, 0, &hasGreen);
                blue = sampleLayerChannel(2, sampleIndex, 0, &hasBlue);
                if (!hasRed || !hasGreen || !hasBlue) {
                    error = QStringLiteral("PSD RGB layer is missing color channels.");
                    return {};
                }
                alpha = sampleLayerChannel(-1, sampleIndex, 255, nullptr);
                break;
            }
            case kColorModeCmyk: {
                bool hasCyan = false;
                bool hasMagenta = false;
                bool hasYellow = false;
                bool hasBlack = false;
                const qreal cyan = sampleLayerChannel(0, sampleIndex, 0, &hasCyan) / 255.0;
                const qreal magenta = sampleLayerChannel(1, sampleIndex, 0, &hasMagenta) / 255.0;
                const qreal yellow = sampleLayerChannel(2, sampleIndex, 0, &hasYellow) / 255.0;
                const qreal black = sampleLayerChannel(3, sampleIndex, 0, &hasBlack) / 255.0;
                if (!hasCyan || !hasMagenta || !hasYellow || !hasBlack) {
                    error = QStringLiteral("PSD CMYK layer is missing color channels.");
                    return {};
                }
                red = static_cast<quint8>(std::round(255.0 * (1.0 - cyan) * (1.0 - black)));
                green = static_cast<quint8>(std::round(255.0 * (1.0 - magenta) * (1.0 - black)));
                blue = static_cast<quint8>(std::round(255.0 * (1.0 - yellow) * (1.0 - black)));
                alpha = sampleLayerChannel(-1, sampleIndex, 255, nullptr);
                break;
            }
            default:
                error = QStringLiteral("Unsupported PSD color mode.");
                return {};
            }

            image.setPixelColor(x, y, QColor(red, green, blue, alpha));
        }
    }

    return image;
}

bool parsePsdLayers(const QByteArray &layerMaskInfoData,
                    const PsdHeader &header,
                    const QByteArray &colorModeData,
                    QVector<PsdDecodedLayer> &layers,
                    QString &error)
{
    if (layerMaskInfoData.size() < 4) {
        return true;
    }

    QBuffer layerMaskBuffer;
    layerMaskBuffer.setData(layerMaskInfoData);
    layerMaskBuffer.open(QIODevice::ReadOnly);

    quint32 layerInfoLength = 0;
    if (!readSectionLength(layerMaskBuffer, layerInfoLength, error)) {
        return false;
    }

    if (layerInfoLength == 0) {
        return true;
    }

    if (layerInfoLength > static_cast<quint32>(layerMaskInfoData.size() - 4)) {
        error = QStringLiteral("PSD layer info length is invalid.");
        return false;
    }

    QByteArray layerInfoData;
    layerInfoData.resize(layerInfoLength);
    if (!readExact(layerMaskBuffer, layerInfoData.data(), layerInfoLength)) {
        error = QStringLiteral("PSD layer info is truncated.");
        return false;
    }

    QBuffer layerInfoBuffer;
    layerInfoBuffer.setData(layerInfoData);
    layerInfoBuffer.open(QIODevice::ReadOnly);

    QDataStream stream(&layerInfoBuffer);
    stream.setByteOrder(QDataStream::BigEndian);

    qint16 rawLayerCount = 0;
    stream >> rawLayerCount;
    if (stream.status() != QDataStream::Ok) {
        error = QStringLiteral("PSD layer count is truncated.");
        return false;
    }

    const int layerCount = std::abs(rawLayerCount);
    QVector<PsdLayerRecord> records;
    records.reserve(layerCount);

    for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
        PsdLayerRecord record;
        qint16 channelCount = 0;
        stream >> record.top >> record.left >> record.bottom >> record.right >> channelCount;
        if (stream.status() != QDataStream::Ok) {
            error = QStringLiteral("PSD layer record is truncated.");
            return false;
        }

        if (channelCount < 0) {
            error = QStringLiteral("PSD layer channel count is invalid.");
            return false;
        }

        record.channels.reserve(channelCount);
        for (int channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
            PsdLayerChannelInfo channelInfo;
            stream >> channelInfo.id >> channelInfo.dataLength;
            if (stream.status() != QDataStream::Ok) {
                error = QStringLiteral("PSD layer channel table is truncated.");
                return false;
            }
            record.channels.push_back(channelInfo);
        }

        char blendSignature[4];
        char blendModeKey[4];
        if (!readExact(layerInfoBuffer, blendSignature, sizeof(blendSignature))
            || !readExact(layerInfoBuffer, blendModeKey, sizeof(blendModeKey))) {
            error = QStringLiteral("PSD layer blend mode record is truncated.");
            return false;
        }

        record.blendModeKey = QString::fromLatin1(blendModeKey, sizeof(blendModeKey));
        if (QByteArray(blendSignature, sizeof(blendSignature)) != QByteArrayLiteral("8BIM")) {
            error = QStringLiteral("PSD layer blend mode signature is invalid.");
            return false;
        }

        if (!readByte(layerInfoBuffer, record.opacity)
            || !readByte(layerInfoBuffer, record.clipping)
            || !readByte(layerInfoBuffer, record.flags)) {
            error = QStringLiteral("PSD layer display attributes are truncated.");
            return false;
        }

        quint8 filler = 0;
        if (!readByte(layerInfoBuffer, filler)) {
            error = QStringLiteral("PSD layer filler byte is truncated.");
            return false;
        }

        quint32 extraDataLength = 0;
        if (!readSectionLength(layerInfoBuffer, extraDataLength, error)) {
            return false;
        }

        QByteArray extraData;
        extraData.resize(extraDataLength);
        if (extraDataLength > 0 && !readExact(layerInfoBuffer, extraData.data(), extraDataLength)) {
            error = QStringLiteral("PSD layer extra data is truncated.");
            return false;
        }

        if (!parseLayerExtraData(extraData, record.name, error)) {
            return false;
        }

        records.push_back(record);
    }

    for (int layerIndex = 0; layerIndex < records.size(); ++layerIndex) {
        const PsdLayerRecord &record = records.at(layerIndex);
        const qint32 layerWidth = std::max<qint32>(0, record.right - record.left);
        const qint32 layerHeight = std::max<qint32>(0, record.bottom - record.top);

        QHash<qint16, QByteArray> decodedChannels;
        for (const PsdLayerChannelInfo &channelInfo : record.channels) {
            QByteArray chunkData;
            chunkData.resize(channelInfo.dataLength);
            if (channelInfo.dataLength > 0 && !readExact(layerInfoBuffer, chunkData.data(), channelInfo.dataLength)) {
                error = QStringLiteral("PSD layer channel data is truncated.");
                return false;
            }

            if (layerWidth <= 0 || layerHeight <= 0 || !isRenderableLayerChannel(header.colorMode, channelInfo.id)) {
                continue;
            }

            QByteArray decodedData;
            if (!decodePsdChannelChunk(chunkData, layerWidth, layerHeight, header.depth, decodedData, error)) {
                return false;
            }
            decodedChannels.insert(channelInfo.id, decodedData);
        }

        if (layerWidth <= 0 || layerHeight <= 0) {
            continue;
        }

        PsdDecodedLayer layer;
        layer.name = record.name.isEmpty()
            ? QStringLiteral("Layer %1").arg(layerIndex + 1)
            : record.name;
        layer.top = record.top;
        layer.left = record.left;
        layer.width = layerWidth;
        layer.height = layerHeight;
        layer.blendModeKey = record.blendModeKey;
        layer.opacity = static_cast<qreal>(record.opacity) / 255.0;
        layer.visible = (record.flags & 0x02) == 0;
        layer.image = composePsdLayerImage(header, colorModeData, decodedChannels, layerWidth, layerHeight, error);
        if (layer.image.isNull()) {
            return false;
        }

        layers.push_back(layer);
    }

    return true;
}

bool readCompositeChannels(QIODevice &device,
                           const PsdHeader &header,
                           quint16 compression,
                           QVector<QByteArray> &channelData,
                           QString &error)
{
    const qsizetype bytesPerSample = header.depth / 8;
    const qsizetype samplesPerChannel = static_cast<qsizetype>(header.width) * static_cast<qsizetype>(header.height);
    const qsizetype bytesPerChannel = samplesPerChannel * bytesPerSample;
    const qsizetype rowByteCount = static_cast<qsizetype>(header.width) * bytesPerSample;

    channelData.resize(header.channels);

    if (compression == 0) {
        for (quint16 channelIndex = 0; channelIndex < header.channels; ++channelIndex) {
            channelData[channelIndex].resize(bytesPerChannel);
            if (!readExact(device, channelData[channelIndex].data(), bytesPerChannel)) {
                error = QStringLiteral("PSD composite image data is truncated.");
                return false;
            }
        }
        return true;
    }

    if (compression != 1) {
        error = QStringLiteral("Only raw and RLE-compressed PSD files are supported.");
        return false;
    }

    QDataStream stream(&device);
    stream.setByteOrder(QDataStream::BigEndian);

    QVector<quint16> rowLengths(static_cast<qsizetype>(header.channels) * static_cast<qsizetype>(header.height));
    for (qsizetype index = 0; index < rowLengths.size(); ++index) {
        stream >> rowLengths[index];
        if (stream.status() != QDataStream::Ok) {
            error = QStringLiteral("PSD RLE row table is truncated.");
            return false;
        }
    }

    for (quint16 channelIndex = 0; channelIndex < header.channels; ++channelIndex) {
        QByteArray channelBytes;
        channelBytes.reserve(bytesPerChannel);

        for (quint32 row = 0; row < header.height; ++row) {
            const qsizetype rowIndex = static_cast<qsizetype>(channelIndex) * static_cast<qsizetype>(header.height)
                + static_cast<qsizetype>(row);
            QByteArray encodedRow;
            encodedRow.resize(rowLengths[rowIndex]);
            if (!readExact(device, encodedRow.data(), encodedRow.size())) {
                error = QStringLiteral("PSD RLE image data is truncated.");
                return false;
            }

            const QByteArray decodedRow = decodePackBitsRow(encodedRow, rowByteCount);
            if (decodedRow.isEmpty() && rowByteCount > 0) {
                error = QStringLiteral("PSD RLE row could not be decoded.");
                return false;
            }
            channelBytes.append(decodedRow);
        }

        channelData[channelIndex] = channelBytes;
    }

    return true;
}

QImage composePsdImage(const PsdHeader &header,
                       const QByteArray &colorModeData,
                       const QVector<QByteArray> &channelData,
                       QString &error)
{
    if (channelData.size() < header.channels) {
        error = QStringLiteral("PSD channel data is incomplete.");
        return {};
    }

    QImage image(static_cast<int>(header.width), static_cast<int>(header.height), QImage::Format_ARGB32);
    if (image.isNull()) {
        error = QStringLiteral("PSD image could not be allocated.");
        return {};
    }

    for (quint32 y = 0; y < header.height; ++y) {
        for (quint32 x = 0; x < header.width; ++x) {
            const qsizetype sampleIndex = static_cast<qsizetype>(y) * static_cast<qsizetype>(header.width)
                + static_cast<qsizetype>(x);

            quint8 red = 0;
            quint8 green = 0;
            quint8 blue = 0;
            quint8 alpha = 255;

            switch (header.colorMode) {
            case kColorModeGrayscale: {
                const quint8 gray = sampleToByte(channelData[0], sampleIndex, header.depth);
                red = gray;
                green = gray;
                blue = gray;
                if (header.channels > 1) {
                    alpha = sampleToByte(channelData[1], sampleIndex, header.depth);
                }
                break;
            }
            case kColorModeIndexed: {
                if (colorModeData.size() < 768) {
                    error = QStringLiteral("PSD indexed color palette is missing.");
                    return {};
                }
                const quint8 paletteIndex = sampleToByte(channelData[0], sampleIndex, header.depth);
                red = static_cast<quint8>(colorModeData.at(paletteIndex));
                green = static_cast<quint8>(colorModeData.at(256 + paletteIndex));
                blue = static_cast<quint8>(colorModeData.at(512 + paletteIndex));
                if (header.channels > 1) {
                    alpha = sampleToByte(channelData[1], sampleIndex, header.depth);
                }
                break;
            }
            case kColorModeRgb: {
                if (header.channels < 3) {
                    error = QStringLiteral("PSD RGB composite is missing color channels.");
                    return {};
                }
                red = sampleToByte(channelData[0], sampleIndex, header.depth);
                green = sampleToByte(channelData[1], sampleIndex, header.depth);
                blue = sampleToByte(channelData[2], sampleIndex, header.depth);
                if (header.channels > 3) {
                    alpha = sampleToByte(channelData[3], sampleIndex, header.depth);
                }
                break;
            }
            case kColorModeCmyk: {
                if (header.channels < 4) {
                    error = QStringLiteral("PSD CMYK composite is missing color channels.");
                    return {};
                }
                const qreal cyan = sampleToByte(channelData[0], sampleIndex, header.depth) / 255.0;
                const qreal magenta = sampleToByte(channelData[1], sampleIndex, header.depth) / 255.0;
                const qreal yellow = sampleToByte(channelData[2], sampleIndex, header.depth) / 255.0;
                const qreal black = sampleToByte(channelData[3], sampleIndex, header.depth) / 255.0;
                red = static_cast<quint8>(std::round(255.0 * (1.0 - cyan) * (1.0 - black)));
                green = static_cast<quint8>(std::round(255.0 * (1.0 - magenta) * (1.0 - black)));
                blue = static_cast<quint8>(std::round(255.0 * (1.0 - yellow) * (1.0 - black)));
                if (header.channels > 4) {
                    alpha = sampleToByte(channelData[4], sampleIndex, header.depth);
                }
                break;
            }
            default:
                error = QStringLiteral("Unsupported PSD color mode.");
                return {};
            }

            image.setPixelColor(static_cast<int>(x), static_cast<int>(y), QColor(red, green, blue, alpha));
        }
    }

    return image;
}

bool inspectPsdDocument(const QString &filePath,
                        QVariantMap &metadata,
                        QVector<PsdDecodedLayer> &layers,
                        QString &error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("PSD file could not be opened.");
        return false;
    }

    PsdHeader header;
    if (!readPsdHeader(file, header, error)) {
        return false;
    }

    quint32 colorModeDataLength = 0;
    if (!readSectionLength(file, colorModeDataLength, error)) {
        return false;
    }

    QByteArray colorModeData;
    colorModeData.resize(colorModeDataLength);
    if (colorModeDataLength > 0 && !readExact(file, colorModeData.data(), colorModeDataLength)) {
        error = QStringLiteral("PSD color mode data is truncated.");
        return false;
    }

    quint32 imageResourcesLength = 0;
    if (!readSectionLength(file, imageResourcesLength, error) || !skipExact(file, imageResourcesLength)) {
        error = QStringLiteral("PSD image resources are truncated.");
        return false;
    }

    quint32 layerMaskInfoLength = 0;
    if (!readSectionLength(file, layerMaskInfoLength, error)) {
        return false;
    }

    QByteArray layerMaskInfoData;
    layerMaskInfoData.resize(layerMaskInfoLength);
    if (layerMaskInfoLength > 0 && !readExact(file, layerMaskInfoData.data(), layerMaskInfoLength)) {
        error = QStringLiteral("PSD layer and mask info is truncated.");
        return false;
    }

    if (!parsePsdLayers(layerMaskInfoData, header, colorModeData, layers, error)) {
        return false;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 compression = 0;
    stream >> compression;
    if (stream.status() != QDataStream::Ok) {
        error = QStringLiteral("PSD compression field is truncated.");
        return false;
    }

    metadata = {
        {QStringLiteral("version"), static_cast<int>(header.version)},
        {QStringLiteral("width"), static_cast<int>(header.width)},
        {QStringLiteral("height"), static_cast<int>(header.height)},
        {QStringLiteral("channels"), static_cast<int>(header.channels)},
        {QStringLiteral("depth"), static_cast<int>(header.depth)},
        {QStringLiteral("colorMode"), colorModeName(header.colorMode)},
        {QStringLiteral("compression"), compressionName(compression)},
        {QStringLiteral("layerCount"), parseLayerCount(layerMaskInfoData)},
        {QStringLiteral("importedLayerCount"), layers.size()},
        {QStringLiteral("hasAlpha"), psdCompositeHasAlpha(header)},
        {QStringLiteral("colorModeDataBytes"), static_cast<qint64>(colorModeDataLength)},
        {QStringLiteral("imageResourcesBytes"), static_cast<qint64>(imageResourcesLength)},
        {QStringLiteral("layerMaskInfoBytes"), static_cast<qint64>(layerMaskInfoLength)}
    };

    return true;
}

QImage loadPsdComposite(const QString &filePath, QVariantMap &metadata, QString &error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("PSD file could not be opened.");
        return {};
    }

    PsdHeader header;
    if (!readPsdHeader(file, header, error)) {
        return {};
    }

    quint32 colorModeDataLength = 0;
    if (!readSectionLength(file, colorModeDataLength, error)) {
        return {};
    }

    QByteArray colorModeData;
    colorModeData.resize(colorModeDataLength);
    if (colorModeDataLength > 0 && !readExact(file, colorModeData.data(), colorModeDataLength)) {
        error = QStringLiteral("PSD color mode data is truncated.");
        return {};
    }

    quint32 imageResourcesLength = 0;
    if (!readSectionLength(file, imageResourcesLength, error) || !skipExact(file, imageResourcesLength)) {
        error = QStringLiteral("PSD image resources are truncated.");
        return {};
    }

    quint32 layerMaskInfoLength = 0;
    if (!readSectionLength(file, layerMaskInfoLength, error)) {
        return {};
    }

    QByteArray layerMaskInfoData;
    layerMaskInfoData.resize(layerMaskInfoLength);
    if (layerMaskInfoLength > 0 && !readExact(file, layerMaskInfoData.data(), layerMaskInfoLength)) {
        error = QStringLiteral("PSD layer and mask info is truncated.");
        return {};
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);

    quint16 compression = 0;
    stream >> compression;
    if (stream.status() != QDataStream::Ok) {
        error = QStringLiteral("PSD compression field is truncated.");
        return {};
    }

    QVector<QByteArray> channelData;
    if (!readCompositeChannels(file, header, compression, channelData, error)) {
        return {};
    }

    metadata = {
        {QStringLiteral("version"), static_cast<int>(header.version)},
        {QStringLiteral("width"), static_cast<int>(header.width)},
        {QStringLiteral("height"), static_cast<int>(header.height)},
        {QStringLiteral("channels"), static_cast<int>(header.channels)},
        {QStringLiteral("depth"), static_cast<int>(header.depth)},
        {QStringLiteral("colorMode"), colorModeName(header.colorMode)},
        {QStringLiteral("compression"), compressionName(compression)},
        {QStringLiteral("layerCount"), parseLayerCount(layerMaskInfoData)},
        {QStringLiteral("hasAlpha"), psdCompositeHasAlpha(header)},
        {QStringLiteral("colorModeDataBytes"), static_cast<qint64>(colorModeDataLength)},
        {QStringLiteral("imageResourcesBytes"), static_cast<qint64>(imageResourcesLength)},
        {QStringLiteral("layerMaskInfoBytes"), static_cast<qint64>(layerMaskInfoLength)}
    };

    return composePsdImage(header, colorModeData, channelData, error);
}

bool saveImageAsPng(const QImage &image, const QString &targetPath, QString &error)
{
    QSaveFile outputFile(targetPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("Rasterized PSD cache file could not be opened.");
        return false;
    }

    if (!image.save(&outputFile, "PNG")) {
        outputFile.cancelWriting();
        error = QStringLiteral("Rasterized PSD cache file could not be written.");
        return false;
    }

    if (!outputFile.commit()) {
        error = QStringLiteral("Rasterized PSD cache file could not be finalized.");
        return false;
    }

    return true;
}

QString defaultCacheDirectory()
{
    QString cacheDirectory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDirectory.isEmpty()) {
        cacheDirectory = QDir::tempPath() + QStringLiteral("/Vincent");
    }
    return cacheDirectory + QStringLiteral("/imports");
}

QString cacheKeyForFile(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    const QByteArray payload =
        QFileInfo(fileInfo.canonicalFilePath().isEmpty() ? fileInfo.absoluteFilePath() : fileInfo.canonicalFilePath()).absoluteFilePath().toUtf8()
        + '|'
        + QByteArray::number(fileInfo.size())
        + '|'
        + QByteArray::number(fileInfo.lastModified().toMSecsSinceEpoch());
    return QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
}

QVariantMap failureResult(const QString &error)
{
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("source"), QString()},
        {QStringLiteral("error"), error}
    };
}

} // namespace

ImageImport::ImageImport(QObject *parent)
    : QObject(parent)
    , m_cacheDirectory(defaultCacheDirectory())
{
}

bool ImageImport::supportsImageFile(const QString &fileUrl) const
{
    const InputReference ref = resolveInputReference(fileUrl);
    if (ref.suffix.isEmpty()) {
        return false;
    }
    return supportedRasterSuffixes().contains(ref.suffix);
}

QVariantMap ImageImport::prepareImageImport(const QString &fileUrl)
{
    const InputReference ref = resolveInputReference(fileUrl);
    if (ref.sourceUrl.isEmpty()) {
        return failureResult(QStringLiteral("Image path is empty."));
    }

    const QFileInfo sourceInfo(ref.localPath);
    QVariantMap metadata = buildBasicImportMetadata(ref, sourceInfo, QStringLiteral("raster"));

    if (!supportsImageFile(fileUrl)) {
        return failureResult(QStringLiteral("Unsupported image format."));
    }

    if (!isPsdSuffix(ref.suffix)) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("source"), ref.sourceUrl},
            {QStringLiteral("error"), QString()},
            {QStringLiteral("metadata"), metadata}
        };
    }

    if (!ref.localFile || ref.localPath.isEmpty()) {
        return failureResult(QStringLiteral("PSD import requires a local file."));
    }

    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return failureResult(QStringLiteral("PSD file does not exist."));
    }

    QString error;
    QVariantMap psdMetadata;
    QVector<PsdDecodedLayer> psdLayers;
    if (!inspectPsdDocument(ref.localPath, psdMetadata, psdLayers, error)) {
        return failureResult(error.isEmpty() ? QStringLiteral("PSD could not be inspected.") : error);
    }

    QDir cacheDir(m_cacheDirectory);
    if (!cacheDir.exists() && !cacheDir.mkpath(QStringLiteral("."))) {
        return failureResult(QStringLiteral("Import cache directory could not be created."));
    }

    const QString baseCacheKey = cacheKeyForFile(ref.localPath);
    metadata.insert(QStringLiteral("kind"), QStringLiteral("psd"));
    metadata.insert(QStringLiteral("psd"), psdMetadata);

    if (!psdLayers.isEmpty()) {
        QVariantList layerPayloads;
        layerPayloads.reserve(psdLayers.size());

        for (int index = 0; index < psdLayers.size(); ++index) {
            const PsdDecodedLayer &layer = psdLayers.at(index);
            QString layerError;
            const QString layerPath = cacheDir.filePath(baseCacheKey + QStringLiteral("-layer-%1.png").arg(index));
            if (!saveImageAsPng(layer.image, layerPath, layerError)) {
                return failureResult(layerError);
            }

            layerPayloads.push_back(QVariantMap{
                {QStringLiteral("source"), QUrl::fromLocalFile(layerPath).toString()},
                {QStringLiteral("x"), layer.left},
                {QStringLiteral("y"), layer.top},
                {QStringLiteral("width"), layer.width},
                {QStringLiteral("height"), layer.height},
                {QStringLiteral("name"), layer.name},
                {QStringLiteral("opacity"), layer.opacity},
                {QStringLiteral("visible"), layer.visible},
                {QStringLiteral("blendModeKey"), layer.blendModeKey},
                {QStringLiteral("metadata"), buildPsdLayerMetadata(metadata, psdMetadata, layer, index)}
            });
        }

        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("source"), QString()},
            {QStringLiteral("error"), QString()},
            {QStringLiteral("metadata"), metadata},
            {QStringLiteral("layers"), layerPayloads}
        };
    }

    QVariantMap compositeMetadata;
    const QImage image = loadPsdComposite(ref.localPath, compositeMetadata, error);
    if (image.isNull()) {
        return failureResult(error.isEmpty() ? QStringLiteral("PSD could not be decoded.") : error);
    }

    const QString targetPath = cacheDir.filePath(baseCacheKey + QStringLiteral(".png"));
    if (!saveImageAsPng(image, targetPath, error)) {
        return failureResult(error);
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("source"), QUrl::fromLocalFile(targetPath).toString()},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("metadata"), metadata}
    };
}

void ImageImport::setCacheDirectory(const QString &cacheDirectory)
{
    m_cacheDirectory = cacheDirectory;
}
