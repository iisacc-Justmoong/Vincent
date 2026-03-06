#include "imageimport.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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

constexpr quint16 kPsdVersion = 1;
constexpr quint16 kColorModeGrayscale = 1;
constexpr quint16 kColorModeIndexed = 2;
constexpr quint16 kColorModeRgb = 3;
constexpr quint16 kColorModeCmyk = 4;

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

QImage loadPsdComposite(const QString &filePath, QString &error)
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
    if (!readSectionLength(file, layerMaskInfoLength, error) || !skipExact(file, layerMaskInfoLength)) {
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

    return composePsdImage(header, colorModeData, channelData, error);
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

    if (!supportsImageFile(fileUrl)) {
        return failureResult(QStringLiteral("Unsupported image format."));
    }

    if (!isPsdSuffix(ref.suffix)) {
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("source"), ref.sourceUrl},
            {QStringLiteral("error"), QString()}
        };
    }

    if (!ref.localFile || ref.localPath.isEmpty()) {
        return failureResult(QStringLiteral("PSD import requires a local file."));
    }

    const QFileInfo sourceInfo(ref.localPath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return failureResult(QStringLiteral("PSD file does not exist."));
    }

    QString error;
    const QImage image = loadPsdComposite(ref.localPath, error);
    if (image.isNull()) {
        return failureResult(error.isEmpty() ? QStringLiteral("PSD could not be decoded.") : error);
    }

    QDir cacheDir(m_cacheDirectory);
    if (!cacheDir.exists() && !cacheDir.mkpath(QStringLiteral("."))) {
        return failureResult(QStringLiteral("Import cache directory could not be created."));
    }

    const QString targetPath = cacheDir.filePath(cacheKeyForFile(ref.localPath) + QStringLiteral(".png"));
    QSaveFile outputFile(targetPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        return failureResult(QStringLiteral("Rasterized PSD cache file could not be opened."));
    }

    if (!image.save(&outputFile, "PNG")) {
        outputFile.cancelWriting();
        return failureResult(QStringLiteral("Rasterized PSD cache file could not be written."));
    }

    if (!outputFile.commit()) {
        return failureResult(QStringLiteral("Rasterized PSD cache file could not be finalized."));
    }

    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("source"), QUrl::fromLocalFile(targetPath).toString()},
        {QStringLiteral("error"), QString()}
    };
}

void ImageImport::setCacheDirectory(const QString &cacheDirectory)
{
    m_cacheDirectory = cacheDirectory;
}
