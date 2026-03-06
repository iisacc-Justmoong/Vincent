#include "imageexport.h"

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QSaveFile>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>

namespace {

struct OutputReference
{
    bool localFile = false;
    QString localPath;
    QString suffix;
};

struct PsdExportLayer
{
    QString name;
    QString blendModeKey = QStringLiteral("norm");
    quint8 opacity = 255;
    bool visible = true;
    int left = 0;
    int top = 0;
    QImage image;
};

OutputReference resolveOutputReference(const QString &fileUrl)
{
    OutputReference result;
    if (fileUrl.isEmpty()) {
        return result;
    }

    const QUrl url(fileUrl);
    if (url.isValid() && !url.scheme().isEmpty()) {
        result.localFile = url.isLocalFile();
        result.localPath = result.localFile ? url.toLocalFile() : QString();
        result.suffix = QFileInfo(result.localPath.isEmpty() ? url.path() : result.localPath).suffix().toLower();
        return result;
    }

    const QFileInfo fileInfo(fileUrl);
    result.localFile = true;
    result.localPath = fileInfo.absoluteFilePath();
    result.suffix = fileInfo.suffix().toLower();
    return result;
}

QVariantMap failureResult(const QString &error)
{
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), error}
    };
}

QVariantMap successResult()
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()}
    };
}

QString normalizedBlendModeKey(const QString &blendModeKey)
{
    const QByteArray bytes = blendModeKey.toLatin1();
    if (bytes.size() == 4) {
        return QString::fromLatin1(bytes.constData(), 4);
    }
    return QStringLiteral("norm");
}

QString resolvedLayerName(const QVariantMap &entry, int fallbackIndex)
{
    const QString name = entry.value(QStringLiteral("layerName")).toString();
    if (!name.isEmpty()) {
        return name;
    }
    return QStringLiteral("Layer %1").arg(fallbackIndex);
}

QImage loadDataUrlImage(const QUrl &url, QString &error)
{
    const QString urlText = url.toString();
    const int commaIndex = urlText.indexOf(QLatin1Char(','));
    if (commaIndex <= 0) {
        error = QStringLiteral("Image data URL is invalid.");
        return {};
    }

    const QString metadata = urlText.left(commaIndex);
    QByteArray payload = urlText.mid(commaIndex + 1).toUtf8();
    if (metadata.contains(QStringLiteral(";base64"))) {
        payload = QByteArray::fromBase64(payload);
    } else {
        payload = QByteArray::fromPercentEncoding(payload);
    }

    QImage image;
    if (!image.loadFromData(payload)) {
        error = QStringLiteral("Image data URL could not be decoded.");
        return {};
    }

    return image;
}

QImage loadSourceImage(const QString &source, QString &error)
{
    if (source.isEmpty()) {
        error = QStringLiteral("Layer source is empty.");
        return {};
    }

    const QUrl url(source);
    if (url.isValid() && url.scheme() == QLatin1String("data")) {
        return loadDataUrlImage(url, error);
    }

    QString path = source;
    if (url.isValid() && url.isLocalFile()) {
        path = url.toLocalFile();
    }

    QImage image(path);
    if (image.isNull()) {
        error = QStringLiteral("Layer image could not be loaded.");
        return {};
    }

    return image;
}

QRect nonTransparentBounds(const QImage &image)
{
    if (image.isNull()) {
        return {};
    }

    const QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);
    int left = argbImage.width();
    int top = argbImage.height();
    int right = -1;
    int bottom = -1;

    for (int y = 0; y < argbImage.height(); ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(argbImage.constScanLine(y));
        for (int x = 0; x < argbImage.width(); ++x) {
            if (qAlpha(row[x]) == 0) {
                continue;
            }
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }

    if (right < left || bottom < top) {
        return {};
    }

    return QRect(QPoint(left, top), QPoint(right, bottom));
}

QByteArray channelBytes(const QImage &image, int channelId)
{
    const QImage argbImage = image.convertToFormat(QImage::Format_ARGB32);
    QByteArray bytes;
    bytes.resize(argbImage.width() * argbImage.height());

    int offset = 0;
    for (int y = 0; y < argbImage.height(); ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(argbImage.constScanLine(y));
        for (int x = 0; x < argbImage.width(); ++x) {
            quint8 value = 0;
            switch (channelId) {
            case -1:
                value = qAlpha(row[x]);
                break;
            case 0:
                value = qRed(row[x]);
                break;
            case 1:
                value = qGreen(row[x]);
                break;
            case 2:
                value = qBlue(row[x]);
                break;
            default:
                break;
            }
            bytes[offset++] = static_cast<char>(value);
        }
    }

    return bytes;
}

QByteArray rawChannelChunk(const QByteArray &channelData)
{
    QByteArray chunk;
    QBuffer buffer(&chunk);
    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << quint16(0);
    buffer.write(channelData);
    return chunk;
}

QByteArray encodeUnicodeLayerName(const QString &name)
{
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << quint32(name.size());
    for (const QChar ch : name) {
        stream << quint16(ch.unicode());
    }

    return encoded;
}

QByteArray buildLayerExtraData(const QString &name)
{
    QByteArray extraData;
    QBuffer buffer(&extraData);
    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << quint32(0);
    stream << quint32(0);

    const QByteArray pascalName = name.left(255).toLatin1();
    buffer.putChar(static_cast<char>(pascalName.size()));
    buffer.write(pascalName);

    const int pascalPadding = (4 - ((1 + pascalName.size()) % 4)) % 4;
    if (pascalPadding > 0) {
        buffer.write(QByteArray(pascalPadding, '\0'));
    }

    const QByteArray unicodeName = encodeUnicodeLayerName(name);
    buffer.write("8BIM", 4);
    buffer.write("luni", 4);
    stream << quint32(unicodeName.size());
    buffer.write(unicodeName);
    if ((unicodeName.size() % 2) == 1) {
        buffer.putChar('\0');
    }

    return extraData;
}

QVector<PsdExportLayer> buildAppLayers(const QVariantList &layers, QString &error)
{
    QVector<PsdExportLayer> result;
    result.reserve(layers.size());

    for (int index = 0; index < layers.size(); ++index) {
        const QVariantMap entry = layers.at(index).toMap();
        if (entry.isEmpty()) {
            continue;
        }

        QImage sourceImage = loadSourceImage(entry.value(QStringLiteral("source")).toString(), error);
        if (sourceImage.isNull()) {
            return {};
        }

        int originalWidth = entry.value(QStringLiteral("originalWidth")).toInt();
        int originalHeight = entry.value(QStringLiteral("originalHeight")).toInt();
        if (originalWidth <= 0) {
            originalWidth = sourceImage.width();
        }
        if (originalHeight <= 0) {
            originalHeight = sourceImage.height();
        }

        const qreal scaleX = entry.value(QStringLiteral("scaleX"), 1.0).toReal();
        const qreal scaleY = entry.value(QStringLiteral("scaleY"), 1.0).toReal();
        const int targetWidth = std::max(1, qRound(originalWidth * scaleX));
        const int targetHeight = std::max(1, qRound(originalHeight * scaleY));

        if (sourceImage.size() != QSize(targetWidth, targetHeight)) {
            sourceImage = sourceImage.scaled(targetWidth,
                                            targetHeight,
                                            Qt::IgnoreAspectRatio,
                                            Qt::SmoothTransformation);
        }

        PsdExportLayer layer;
        layer.name = resolvedLayerName(entry, index + 1);
        layer.blendModeKey = normalizedBlendModeKey(entry.value(QStringLiteral("blendModeKey")).toString());
        layer.opacity = static_cast<quint8>(qBound(0,
                                                   qRound(entry.value(QStringLiteral("layerOpacity"), 1.0).toReal() * 255.0),
                                                   255));
        layer.visible = entry.value(QStringLiteral("layerVisible"), true).toBool();
        layer.left = qRound(entry.value(QStringLiteral("x")).toReal());
        layer.top = qRound(entry.value(QStringLiteral("y")).toReal());
        layer.image = sourceImage.convertToFormat(QImage::Format_ARGB32);
        result.push_back(layer);
    }

    return result;
}

bool appendStrokeLayer(const QString &strokeDataUrl, QVector<PsdExportLayer> &fileOrderLayers, QImage &composite, QString &error)
{
    if (strokeDataUrl.isEmpty()) {
        return true;
    }

    const QImage strokeCanvas = loadSourceImage(strokeDataUrl, error).convertToFormat(QImage::Format_ARGB32);
    if (strokeCanvas.isNull()) {
        return false;
    }

    QPainter compositePainter(&composite);
    compositePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    compositePainter.drawImage(QPoint(0, 0), strokeCanvas);
    compositePainter.end();

    const QRect strokeBounds = nonTransparentBounds(strokeCanvas);
    if (strokeBounds.isNull()) {
        return true;
    }

    PsdExportLayer strokeLayer;
    strokeLayer.name = QStringLiteral("Paint");
    strokeLayer.left = strokeBounds.x();
    strokeLayer.top = strokeBounds.y();
    strokeLayer.image = strokeCanvas.copy(strokeBounds).convertToFormat(QImage::Format_ARGB32);
    fileOrderLayers.push_back(strokeLayer);
    return true;
}

QByteArray buildLayerInfoData(const QVector<PsdExportLayer> &layers)
{
    QByteArray layerInfoData;
    QBuffer buffer(&layerInfoData);
    buffer.open(QIODevice::WriteOnly);

    QDataStream stream(&buffer);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << qint16(layers.size());

    QVector<QVector<QPair<qint16, QByteArray>>> layerChannels;
    layerChannels.reserve(layers.size());

    for (const PsdExportLayer &layer : layers) {
        const QVector<QPair<qint16, QByteArray>> channels = {
            qMakePair<qint16, QByteArray>(-1, rawChannelChunk(channelBytes(layer.image, -1))),
            qMakePair<qint16, QByteArray>(0, rawChannelChunk(channelBytes(layer.image, 0))),
            qMakePair<qint16, QByteArray>(1, rawChannelChunk(channelBytes(layer.image, 1))),
            qMakePair<qint16, QByteArray>(2, rawChannelChunk(channelBytes(layer.image, 2)))
        };
        layerChannels.push_back(channels);
    }

    for (int index = 0; index < layers.size(); ++index) {
        const PsdExportLayer &layer = layers.at(index);
        const QVector<QPair<qint16, QByteArray>> &channels = layerChannels.at(index);

        stream << qint32(layer.top)
               << qint32(layer.left)
               << qint32(layer.top + layer.image.height())
               << qint32(layer.left + layer.image.width());
        stream << qint16(channels.size());
        for (const auto &channel : channels) {
            stream << channel.first;
            stream << quint32(channel.second.size());
        }

        buffer.write("8BIM", 4);
        const QByteArray blendModeKey = layer.blendModeKey.toLatin1().leftJustified(4, ' ', true).left(4);
        buffer.write(blendModeKey.constData(), blendModeKey.size());
        buffer.putChar(static_cast<char>(layer.opacity));
        buffer.putChar('\0');
        buffer.putChar(static_cast<char>(layer.visible ? 0x00 : 0x02));
        buffer.putChar('\0');

        const QByteArray extraData = buildLayerExtraData(layer.name);
        stream << quint32(extraData.size());
        buffer.write(extraData);
    }

    for (const QVector<QPair<qint16, QByteArray>> &channels : layerChannels) {
        for (const auto &channel : channels) {
            buffer.write(channel.second);
        }
    }

    return layerInfoData;
}

bool writeCompositeChannels(QIODevice &device, const QImage &image)
{
    const QByteArray alpha = channelBytes(image, -1);
    const QByteArray red = channelBytes(image, 0);
    const QByteArray green = channelBytes(image, 1);
    const QByteArray blue = channelBytes(image, 2);

    return device.write(red) == red.size()
        && device.write(green) == green.size()
        && device.write(blue) == blue.size()
        && device.write(alpha) == alpha.size();
}

bool writePsdDocument(const QString &targetPath,
                      int canvasWidth,
                      int canvasHeight,
                      const QVariantList &layers,
                      const QString &strokeDataUrl,
                      QString &error)
{
    const int safeCanvasWidth = qMax(1, canvasWidth);
    const int safeCanvasHeight = qMax(1, canvasHeight);

    QVector<PsdExportLayer> appLayers = buildAppLayers(layers, error);
    if (!error.isEmpty()) {
        return false;
    }

    QImage composite(safeCanvasWidth, safeCanvasHeight, QImage::Format_ARGB32);
    composite.fill(Qt::transparent);

    {
        QPainter painter(&composite);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (const PsdExportLayer &layer : appLayers) {
            if (!layer.visible) {
                continue;
            }
            painter.setOpacity(layer.opacity / 255.0);
            painter.drawImage(QPoint(layer.left, layer.top), layer.image);
        }
        painter.setOpacity(1.0);
    }

    QVector<PsdExportLayer> fileOrderLayers;
    fileOrderLayers.reserve(appLayers.size() + 1);
    if (!appendStrokeLayer(strokeDataUrl, fileOrderLayers, composite, error)) {
        return false;
    }

    for (int index = appLayers.size() - 1; index >= 0; --index) {
        fileOrderLayers.push_back(appLayers.at(index));
    }

    const QByteArray layerInfoData = buildLayerInfoData(fileOrderLayers);

    QByteArray layerAndMaskData;
    QBuffer layerAndMaskBuffer(&layerAndMaskData);
    layerAndMaskBuffer.open(QIODevice::WriteOnly);

    QDataStream layerAndMaskStream(&layerAndMaskBuffer);
    layerAndMaskStream.setByteOrder(QDataStream::BigEndian);
    layerAndMaskStream << quint32(layerInfoData.size());
    layerAndMaskBuffer.write(layerInfoData);
    layerAndMaskStream << quint32(0);

    QSaveFile outputFile(targetPath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("PSD file could not be opened for writing.");
        return false;
    }

    QDataStream stream(&outputFile);
    stream.setByteOrder(QDataStream::BigEndian);

    outputFile.write("8BPS", 4);
    stream << quint16(1);
    outputFile.write(QByteArray(6, '\0'));
    stream << quint16(4);
    stream << quint32(safeCanvasHeight);
    stream << quint32(safeCanvasWidth);
    stream << quint16(8);
    stream << quint16(3);
    stream << quint32(0);
    stream << quint32(0);
    stream << quint32(layerAndMaskData.size());
    outputFile.write(layerAndMaskData);
    stream << quint16(0);

    if (stream.status() != QDataStream::Ok || !writeCompositeChannels(outputFile, composite)) {
        outputFile.cancelWriting();
        error = QStringLiteral("PSD image data could not be written.");
        return false;
    }

    if (!outputFile.commit()) {
        error = QStringLiteral("PSD file could not be finalized.");
        return false;
    }

    return true;
}

} // namespace

ImageExport::ImageExport(QObject *parent)
    : QObject(parent)
{
}

QVariantMap ImageExport::saveDocumentAsPsd(const QString &fileUrl,
                                           int canvasWidth,
                                           int canvasHeight,
                                           const QVariantList &layers,
                                           const QString &strokeDataUrl) const
{
    const OutputReference ref = resolveOutputReference(fileUrl);
    if (!ref.localFile || ref.localPath.isEmpty()) {
        return failureResult(QStringLiteral("PSD export requires a local file path."));
    }

    if (ref.suffix != QStringLiteral("psd")) {
        return failureResult(QStringLiteral("Target file must use the .psd extension."));
    }

    QString error;
    if (!writePsdDocument(ref.localPath, canvasWidth, canvasHeight, layers, strokeDataUrl, error)) {
        return failureResult(error.isEmpty() ? QStringLiteral("PSD export failed.") : error);
    }

    return successResult();
}
