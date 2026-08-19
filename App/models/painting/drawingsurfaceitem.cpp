#include "drawingsurfaceitem.h"

#include "../canvas/canvasviewmodelbridge.h"
#include "../document/psdcompatibilitydocument.h"
#include "../document/psdimagereader.h"
#include "../document/psdimagewriter.h"

#include <QAbstractTextDocumentLayout>
#include <QByteArray>
#include <QBuffer>
#include <QClipboard>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QList>
#include <QMimeData>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPoint>
#include <QPointF>
#include <QPointingDevice>
#include <QQuickItemGrabResult>
#include <QRectF>
#include <QRegularExpression>
#include <QSize>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTabletEvent>
#include <QTemporaryFile>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QTextOption>
#include <QTransform>
#include <QUrl>
#include <QVariantMap>
#include <QVector>
#include <QtMath>
#include <QtGlobal>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <span>

namespace {

constexpr qreal minimumTextFontPixelSize = 8.0;
constexpr qreal maximumTextFontPixelSize = 144.0;
constexpr qreal minimumTextBoxWidth = 8.0;
constexpr qreal minimumShapeDimension = 2.0;
constexpr qreal speechBubbleTailMinimumHeight = 4.0;
constexpr qreal speechBubbleTailHeightRatio = 0.24;
constexpr qreal speechBubbleTailMaximumHeightRatio = 0.35;
constexpr qreal speechBubbleTailLeftBaseXRatio = 0.26;
constexpr qreal speechBubbleTailTipXRatio = 0.18;
constexpr qreal speechBubbleTailRightBaseXRatio = 0.44;
constexpr qreal ellipseBubbleTailLeftAngle = 2.15;
constexpr qreal ellipseBubbleTailRightAngle = 1.70;
constexpr int ellipseBubbleArcSegmentCount = 32;
constexpr int maximumInsertedImageDimension = 32768;
constexpr qint64 maximumInsertedImagePixelCount = 64LL * 1024LL * 1024LL;
constexpr qint64 maximumRemoteImageDownloadBytes = 64LL * 1024LL * 1024LL;
constexpr int remoteImageDownloadTimeoutMs = 30000;

QString localFileSource(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    if (url.isValid() && url.isLocalFile()) {
        return url.toString();
    }
    return QUrl::fromLocalFile(fileUrl).toString();
}

QString localFilePath(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }
    return fileUrl;
}

bool hasPsdSuffix(const QString &fileUrl)
{
    return localFilePath(fileUrl).endsWith(QStringLiteral(".psd"), Qt::CaseInsensitive);
}

bool hasIiscSuffix(const QString &fileUrl)
{
    return localFilePath(fileUrl).endsWith(QStringLiteral(".iisc"), Qt::CaseInsensitive);
}

std::optional<QTransform> documentToSelectedRasterTransform(
    const iiSharedCanvas::CanvasItem &item)
{
    const iiSharedCanvas::Document *document = item.document();
    const QByteArray selectedLayerId = item.selectedLayerId().toUtf8();
    if (!document || selectedLayerId.isEmpty()) {
        return std::nullopt;
    }

    const iiSharedCanvas::Layer *layer = iiSharedCanvas::findLayer(
        *document,
        std::string(selectedLayerId.constData(), static_cast<std::size_t>(selectedLayerId.size())));
    if (!layer) {
        return std::nullopt;
    }

    const AffineTransform &transform = layer->transform;
    const QTransform rasterToDocument(transform.m11,
                                      transform.m12,
                                      transform.m21,
                                      transform.m22,
                                      transform.translationX,
                                      transform.translationY);
    bool invertible = false;
    const QTransform documentToRaster = rasterToDocument.inverted(&invertible);
    return invertible ? std::optional<QTransform>{documentToRaster} : std::nullopt;
}

QImage imageFromFileUrl(const QString &fileUrl)
{
    const QString filePath = localFilePath(fileUrl);
    if (PsdImageReader::canReadPath(filePath)) {
        return PsdImageReader::readMergedImage(filePath);
    }

    return QImage(filePath);
}

QString cachedPsdPreviewSource(const QString &fileUrl, const QImage &image)
{
    const QString filePath = localFilePath(fileUrl);
    if (!PsdImageReader::canReadPath(filePath) || image.isNull()) {
        return localFileSource(fileUrl);
    }

    QFileInfo fileInfo(filePath);
    const QByteArray key = (filePath
                            + QStringLiteral("|")
                            + QString::number(fileInfo.size())
                            + QStringLiteral("|")
                            + QString::number(fileInfo.lastModified().toMSecsSinceEpoch()))
            .toUtf8();
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha256).toHex());
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation).isEmpty()
        ? QDir::tempPath()
        : QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir cacheDir(cacheRoot + QStringLiteral("/psd-previews"));
    if (!cacheDir.exists()) {
        cacheDir.mkpath(QStringLiteral("."));
    }

    const QString previewPath = cacheDir.filePath(digest + QStringLiteral(".png"));
    if (!QFileInfo::exists(previewPath)) {
        image.save(previewPath, "PNG");
    }
    return QUrl::fromLocalFile(previewPath).toString();
}

bool isTabletEvent(QEvent::Type type)
{
    return type == QEvent::TabletPress
        || type == QEvent::TabletMove
        || type == QEvent::TabletRelease;
}

QTabletEvent makeSyntheticTabletStrokeEvent(QEvent::Type eventType,
                                            qreal pointX,
                                            qreal pointY,
                                            qreal rawPressure,
                                            Qt::MouseButton button,
                                            Qt::MouseButtons buttons)
{
    const QPointF position(pointX, pointY);
    return QTabletEvent(eventType,
                        QPointingDevice::primaryPointingDevice(),
                        position,
                        position,
                        rawPressure,
                        0.0F,
                        0.0F,
                        0.0F,
                        0.0,
                        0.0F,
                        Qt::NoModifier,
                        button,
                        buttons);
}

QImage transparentCanvasImage(const QSize &size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

QImage opaqueCanvasBackgroundImage(const QImage &rasterImage)
{
    if (rasterImage.isNull()) {
        return {};
    }

    QImage image(rasterImage.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.drawImage(QPointF(0.0, 0.0), rasterImage);
    painter.end();
    return image;
}

QString writableCacheDirectoryPath(const QString& subdirectoryName)
{
    const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation).isEmpty()
        ? QDir::tempPath()
        : QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir cacheDir(cacheRoot + QLatin1Char('/') + subdirectoryName);
    if (!cacheDir.exists() && !cacheDir.mkpath(QStringLiteral(".")))
    {
        return {};
    }
    return cacheDir.absolutePath();
}

QString writableCacheFilePath(const QString &subdirectoryName, const QString &fileTemplate)
{
    const QString cacheDirectoryPath = writableCacheDirectoryPath(subdirectoryName);
    if (cacheDirectoryPath.isEmpty())
    {
        return {};
    }
    const QDir cacheDir(cacheDirectoryPath);

    QTemporaryFile file(cacheDir.filePath(fileTemplate));
    file.setAutoRemove(false);
    if (!file.open()) {
        return {};
    }

    const QString filePath = file.fileName();
    file.close();
    return filePath;
}

QSize boundedThumbnailMaximumSize(qreal maximumWidth, qreal maximumHeight)
{
    const int width = qBound(1, qRound(maximumWidth > 0 ? maximumWidth : 32.0), 512);
    const int height = qBound(1, qRound(maximumHeight > 0 ? maximumHeight : 32.0), 512);
    return QSize(width, height);
}

void paintThumbnailChecker(QPainter &painter, const QSize &size)
{
    constexpr int checkerSize = 4;
    const QColor light(255, 255, 255, 220);
    const QColor dark(185, 190, 198, 220);
    for (int y = 0; y < size.height(); y += checkerSize) {
        for (int x = 0; x < size.width(); x += checkerSize) {
            const bool alternate = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            painter.fillRect(QRect(x, y, checkerSize, checkerSize), alternate ? light : dark);
        }
    }
}

QImage thumbnailImage(const QImage &sourceImage, const QSize &maximumSize)
{
    if (maximumSize.isEmpty()) {
        return {};
    }

    QImage thumbnail(maximumSize, QImage::Format_ARGB32_Premultiplied);
    thumbnail.fill(Qt::transparent);

    QPainter painter(&thumbnail);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    paintThumbnailChecker(painter, maximumSize);

    if (!sourceImage.isNull()) {
        const QImage layerImage = sourceImage.format() == QImage::Format_ARGB32_Premultiplied
            ? sourceImage
            : sourceImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        QSize scaledSize = layerImage.size().scaled(maximumSize, Qt::KeepAspectRatio);
        scaledSize = QSize(qMax(1, scaledSize.width()), qMax(1, scaledSize.height()));
        const QImage scaledLayer = layerImage.scaled(scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QPointF topLeft((maximumSize.width() - scaledSize.width()) / 2.0,
                              (maximumSize.height() - scaledSize.height()) / 2.0);
        painter.drawImage(topLeft, scaledLayer);
    }

    painter.end();
    return thumbnail;
}

QString cachedPngSourceForImage(const QString &subdirectoryName, const QImage &image)
{
    if (image.isNull()) {
        return {};
    }

    QByteArray cacheKey;
    cacheKey.append(QByteArray::number(image.width()));
    cacheKey.append('x');
    cacheKey.append(QByteArray::number(image.height()));
    cacheKey.append('|');
    cacheKey.append(QByteArray::number(static_cast<int>(image.format())));
    cacheKey.append('|');
    cacheKey.append(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes());

    const QString digest = QString::fromLatin1(QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha256).toHex());
    const QString cacheDirectoryPath = writableCacheDirectoryPath(subdirectoryName);
    if (cacheDirectoryPath.isEmpty())
    {
        return {};
    }
    const QDir cacheDir(cacheDirectoryPath);
    const QString imagePath = cacheDir.filePath(digest + QStringLiteral(".png"));
    const QFileInfo cachedImageInfo(imagePath);
    if (cachedImageInfo.isFile() && !cachedImageInfo.isSymLink())
    {
        QImageReader cachedImageReader(imagePath);
        const QImage cachedImage = cachedImageReader.read();
        if (!cachedImage.isNull() && cachedImage.size() == image.size())
        {
            return QUrl::fromLocalFile(imagePath).toString();
        }
    }

    QSaveFile cacheFile(imagePath);
    if (!cacheFile.open(QIODevice::WriteOnly))
    {
        return {};
    }

    QImageWriter imageWriter(&cacheFile, "PNG");
    if (!imageWriter.write(image))
    {
        cacheFile.cancelWriting();
        return {};
    }
    if (!cacheFile.commit())
    {
        return {};
    }

    QImageReader writtenImageReader(imagePath);
    const QImage writtenImage = writtenImageReader.read();
    if (writtenImage.isNull() || writtenImage.size() != image.size())
    {
        QFile::remove(imagePath);
        return {};
    }
    return QUrl::fromLocalFile(imagePath).toString();
}

QVariantMap imageImportResult(const QString& status)
{
    QVariantMap result;
    result.insert(QStringLiteral("status"), status);
    return result;
}

QImage firstLocalClipboardImage(const QList<QUrl>& urls)
{
    for (const QUrl& url : urls)
    {
        if (!url.isLocalFile())
        {
            continue;
        }

        const QFileInfo fileInfo(url.toLocalFile());
        if (!fileInfo.isFile() || !fileInfo.isReadable())
        {
            continue;
        }

        const QImage image = imageFromFileUrl(url.toString());
        if (!image.isNull())
        {
            return image;
        }
    }
    return {};
}

bool imageSizeExceedsSafetyLimit(const QSize& size)
{
    if (size.width() > maximumInsertedImageDimension ||
        size.height() > maximumInsertedImageDimension)
    {
        return true;
    }

    const qint64 pixelCount = static_cast<qint64>(size.width()) * size.height();
    return pixelCount > maximumInsertedImagePixelCount;
}

bool imageExceedsSafetyLimit(const QImage& image)
{
    return imageSizeExceedsSafetyLimit(image.size());
}

QSize fittedOpenedRasterSize(const QSize &imageSize, const QSize &maximumSize)
{
    if (imageSize.isEmpty()) {
        return {};
    }

    const QSize boundedMaximum(qMax(1, maximumSize.width()), qMax(1, maximumSize.height()));
    if (imageSize.width() <= boundedMaximum.width() && imageSize.height() <= boundedMaximum.height()) {
        return imageSize;
    }

    const QSize fitted = imageSize.scaled(boundedMaximum, Qt::KeepAspectRatio);
    return QSize(qMax(1, fitted.width()), qMax(1, fitted.height()));
}

QVariantMap imageObjectForImage(const QImage& image, const QString& source,
                                qreal maximumObjectWidth, qreal maximumObjectHeight)
{
    if (image.isNull() || source.isEmpty())
    {
        return {};
    }

    const bool hasMaximumObjectSize = maximumObjectWidth > 0 || maximumObjectHeight > 0;
    const QSize maximumSize(maximumObjectWidth > 0 ? qRound(maximumObjectWidth) : image.width(),
                            maximumObjectHeight > 0 ? qRound(maximumObjectHeight) : image.height());
    const QSize objectSize =
        hasMaximumObjectSize ? fittedOpenedRasterSize(image.size(), maximumSize) : image.size();
    if (objectSize.isEmpty())
    {
        return {};
    }

    QVariantMap object;
    object.insert(QStringLiteral("source"), source);
    object.insert(QStringLiteral("width"), objectSize.width());
    object.insert(QStringLiteral("height"), objectSize.height());
    object.insert(QStringLiteral("originalWidth"), image.width());
    object.insert(QStringLiteral("originalHeight"), image.height());
    return object;
}

bool isSupportedImageMimeFormat(const QString& format)
{
    const QString normalized = format.trimmed().toLower();
    return normalized.startsWith(QStringLiteral("image/")) ||
           normalized == QStringLiteral("application/x-qt-image");
}

QStringList dropEventFormats(QObject* dropEvent)
{
    return dropEvent ? dropEvent->property("formats").toStringList() : QStringList();
}

QList<QUrl> dropEventUrls(QObject* dropEvent)
{
    if (!dropEvent)
    {
        return {};
    }

    const QVariant urlsValue = dropEvent->property("urls");
    if (urlsValue.canConvert<QList<QUrl>>())
    {
        return urlsValue.value<QList<QUrl>>();
    }

    QList<QUrl> urls;
    const QVariantList urlValues = urlsValue.toList();
    urls.reserve(urlValues.size());
    for (const QVariant& urlValue : urlValues)
    {
        const QUrl url = urlValue.toUrl();
        if (url.isValid())
        {
            urls.append(url);
        }
    }
    return urls;
}

QByteArray dropEventData(QObject* dropEvent, const QString& format)
{
    QByteArray data;
    if (!dropEvent)
    {
        return data;
    }

    QMetaObject::invokeMethod(dropEvent, "getDataAsArrayBuffer", Qt::DirectConnection,
                              Q_RETURN_ARG(QByteArray, data), Q_ARG(QString, format));
    return data;
}

bool isRemoteImageUrl(const QUrl& url)
{
    const QString scheme = url.scheme().toLower();
    return url.isValid() && (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"));
}

bool isImageDataUrl(const QUrl& url)
{
    return url.isValid() &&
           url.scheme().compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0 &&
           url.toEncoded().startsWith("data:image/");
}

QByteArray encodedImageFromDataUrl(const QUrl& url)
{
    const QByteArray encodedUrl = url.toEncoded(QUrl::FullyEncoded);
    const qsizetype commaIndex = encodedUrl.indexOf(',');
    if (commaIndex < 0)
    {
        return {};
    }

    const QByteArray metadata = encodedUrl.mid(5, commaIndex - 5).toLower();
    if (!metadata.startsWith("image/"))
    {
        return {};
    }

    const QByteArray payload = encodedUrl.mid(commaIndex + 1);
    if (metadata.contains(";base64"))
    {
        return QByteArray::fromBase64(payload, QByteArray::AbortOnBase64DecodingErrors);
    }
    return QByteArray::fromPercentEncoding(payload);
}

QList<QUrl> imageUrlsFromHtml(const QString& html)
{
    if (html.trimmed().isEmpty())
    {
        return {};
    }

    QTextDocument document;
    document.setHtml(html);

    QList<QUrl> urls;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next())
    {
        for (QTextBlock::iterator iterator = block.begin(); !iterator.atEnd(); ++iterator)
        {
            const QTextFragment fragment = iterator.fragment();
            if (!fragment.isValid() || !fragment.charFormat().isImageFormat())
            {
                continue;
            }

            const QUrl url(fragment.charFormat().toImageFormat().name());
            if (url.isValid())
            {
                urls.append(url);
            }
        }
    }
    return urls;
}

QUrl imageUrlFromText(const QString& text)
{
    const QString firstLine =
        text.split(QRegularExpression(QStringLiteral("[\\r\\n\\t]")), Qt::SkipEmptyParts)
            .value(0)
            .trimmed();
    if (firstLine.isEmpty())
    {
        return {};
    }

    const QUrl url(firstLine);
    if (url.isLocalFile() || isRemoteImageUrl(url) || isImageDataUrl(url))
    {
        return url;
    }
    return {};
}

void appendUniqueUrl(QList<QUrl>& urls, const QUrl& url)
{
    if (!url.isValid() || urls.contains(url))
    {
        return;
    }
    urls.append(url);
}

QString suggestedImageName(const QUrl& sourceUrl)
{
    if (!sourceUrl.isValid() || isImageDataUrl(sourceUrl))
    {
        return {};
    }

    const QString decodedPath = QUrl::fromPercentEncoding(sourceUrl.path().toUtf8());
    return QFileInfo(decodedPath).fileName();
}

QVariantMap cachedInsertedImageObject(const QImage& image, const QUrl& originalSource,
                                      qreal maximumObjectWidth, qreal maximumObjectHeight)
{
    if (image.isNull())
    {
        return imageImportResult(QStringLiteral("decode-failed"));
    }
    if (imageExceedsSafetyLimit(image))
    {
        return imageImportResult(QStringLiteral("image-too-large"));
    }

    const QString cachedSource = cachedPngSourceForImage(QStringLiteral("inserted-images"), image);
    if (cachedSource.isEmpty())
    {
        return imageImportResult(QStringLiteral("cache-write-failed"));
    }

    QVariantMap object =
        imageObjectForImage(image, cachedSource, maximumObjectWidth, maximumObjectHeight);
    if (object.isEmpty())
    {
        return imageImportResult(QStringLiteral("decode-failed"));
    }

    object.insert(QStringLiteral("status"), QStringLiteral("ready"));
    if (originalSource.isValid() && !originalSource.isEmpty() && !isImageDataUrl(originalSource))
    {
        QUrl storedSource = originalSource;
        if (isRemoteImageUrl(storedSource))
        {
            storedSource.setUserInfo(QString());
            storedSource.setQuery(QString());
            storedSource.setFragment(QString());
        }
        object.insert(QStringLiteral("originalSource"), storedSource.toString());
        const QString suggestedName = suggestedImageName(originalSource);
        if (!suggestedName.isEmpty())
        {
            object.insert(QStringLiteral("suggestedName"), suggestedName);
        }
    }
    return object;
}

QVariantMap imageObjectForEncodedData(const QByteArray& encodedData, const QUrl& originalSource,
                                      qreal maximumObjectWidth, qreal maximumObjectHeight)
{
    if (encodedData.isEmpty())
    {
        return imageImportResult(QStringLiteral("decode-failed"));
    }

    QBuffer buffer;
    buffer.setData(encodedData);
    if (!buffer.open(QIODevice::ReadOnly))
    {
        return imageImportResult(QStringLiteral("decode-failed"));
    }

    QImageReader imageReader(&buffer);
    imageReader.setAutoTransform(true);
    const QSize decodedSize = imageReader.size();
    if (decodedSize.isValid() && imageSizeExceedsSafetyLimit(decodedSize))
    {
        return imageImportResult(QStringLiteral("image-too-large"));
    }

    const QImage image = imageReader.read();
    return cachedInsertedImageObject(image, originalSource, maximumObjectWidth,
                                     maximumObjectHeight);
}

QVariantMap imageObjectForLocalDrop(const QUrl& url, qreal maximumObjectWidth,
                                    qreal maximumObjectHeight)
{
    if (!url.isLocalFile())
    {
        return imageImportResult(QStringLiteral("decode-failed"));
    }

    const QString filePath = url.toLocalFile();
    const QFileInfo fileInfo(filePath);
    if (!fileInfo.isFile() || !fileInfo.isReadable())
    {
        return imageImportResult(QStringLiteral("decode-failed"));
    }

    if (PsdImageReader::canReadPath(filePath))
    {
        return cachedInsertedImageObject(PsdImageReader::readMergedImage(filePath), url,
                                         maximumObjectWidth, maximumObjectHeight);
    }

    QImageReader imageReader(filePath);
    imageReader.setAutoTransform(true);
    const QSize decodedSize = imageReader.size();
    if (decodedSize.isValid() && imageSizeExceedsSafetyLimit(decodedSize))
    {
        return imageImportResult(QStringLiteral("image-too-large"));
    }

    return cachedInsertedImageObject(imageReader.read(), url, maximumObjectWidth,
                                     maximumObjectHeight);
}

QString normalizedShapeKind(const QString &shapeKind)
{
    const QString normalized = shapeKind.trimmed().toLower();
    if (normalized == QStringLiteral("triagle")) {
        return QStringLiteral("triangle");
    }
    if (normalized == QStringLiteral("ellipse")
        || normalized == QStringLiteral("triangle")
        || normalized == QStringLiteral("diamond")
        || normalized == QStringLiteral("star")
        || normalized == QStringLiteral("rectanglebubble")
        || normalized == QStringLiteral("ellipsebubble")) {
        return normalized;
    }
    return QStringLiteral("rectangle");
}

QPainterPath starPath(const QRectF &rect)
{
    QPainterPath path;
    const QPointF center = rect.center();
    const qreal outerRadiusX = rect.width() / 2.0;
    const qreal outerRadiusY = rect.height() / 2.0;
    const qreal innerRadiusX = outerRadiusX * 0.45;
    const qreal innerRadiusY = outerRadiusY * 0.45;

    for (int index = 0; index < 10; ++index) {
        const bool outerPoint = index % 2 == 0;
        const qreal radiusX = outerPoint ? outerRadiusX : innerRadiusX;
        const qreal radiusY = outerPoint ? outerRadiusY : innerRadiusY;
        const qreal angle = -M_PI_2 + index * M_PI / 5.0;
        const QPointF point(center.x() + qCos(angle) * radiusX,
                            center.y() + qSin(angle) * radiusY);
        if (index == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}

qreal speechBubbleTailHeight(qreal height)
{
    return qMin(qMax(speechBubbleTailMinimumHeight, height * speechBubbleTailHeightRatio),
                height * speechBubbleTailMaximumHeightRatio);
}

qreal speechBubbleBodyHeight(qreal height)
{
    return qMax(minimumShapeDimension, height - speechBubbleTailHeight(height));
}

QPointF ellipsePoint(const QRectF &rect, qreal angle)
{
    return QPointF(rect.center().x() + qCos(angle) * rect.width() / 2.0,
                   rect.center().y() + qSin(angle) * rect.height() / 2.0);
}

void appendEllipseArc(QPainterPath &path, const QRectF &rect, qreal startAngle, qreal endAngle)
{
    for (int index = 1; index <= ellipseBubbleArcSegmentCount; ++index) {
        const qreal angle = startAngle + (endAngle - startAngle) * index / ellipseBubbleArcSegmentCount;
        path.lineTo(ellipsePoint(rect, angle));
    }
}

QPainterPath rectangleBubblePath(const QRectF &rect)
{
    const qreal bodyBottom = rect.top() + speechBubbleBodyHeight(rect.height());
    const qreal tailLeftX = rect.left() + rect.width() * speechBubbleTailLeftBaseXRatio;
    const qreal tailTipX = rect.left() + rect.width() * speechBubbleTailTipXRatio;
    const qreal tailRightX = rect.left() + rect.width() * speechBubbleTailRightBaseXRatio;

    QPainterPath path;
    path.moveTo(rect.topLeft());
    path.lineTo(rect.topRight());
    path.lineTo(rect.right(), bodyBottom);
    path.lineTo(tailRightX, bodyBottom);
    path.lineTo(tailTipX, rect.bottom());
    path.lineTo(tailLeftX, bodyBottom);
    path.lineTo(rect.left(), bodyBottom);
    path.closeSubpath();
    return path;
}

QPainterPath ellipseBubblePath(const QRectF &rect)
{
    QRectF bodyRect = rect;
    bodyRect.setHeight(speechBubbleBodyHeight(rect.height()));

    const QPointF tailLeftPoint = ellipsePoint(bodyRect, ellipseBubbleTailLeftAngle);
    const QPointF tailTip(rect.left() + rect.width() * speechBubbleTailTipXRatio, rect.bottom());

    QPainterPath path;
    path.moveTo(tailLeftPoint);
    appendEllipseArc(path, bodyRect, ellipseBubbleTailLeftAngle, ellipseBubbleTailRightAngle + M_PI * 2.0);
    path.lineTo(tailTip);
    path.closeSubpath();
    return path;
}

QPainterPath shapePath(const QRectF &rect, const QString &shapeKind)
{
    QPainterPath path;
    const QString kind = normalizedShapeKind(shapeKind);

    if (kind == QStringLiteral("ellipse")) {
        path.addEllipse(rect);
        return path;
    }
    if (kind == QStringLiteral("triangle")) {
        path.moveTo(rect.center().x(), rect.top());
        path.lineTo(rect.right(), rect.bottom());
        path.lineTo(rect.left(), rect.bottom());
        path.closeSubpath();
        return path;
    }
    if (kind == QStringLiteral("diamond")) {
        path.moveTo(rect.center().x(), rect.top());
        path.lineTo(rect.right(), rect.center().y());
        path.lineTo(rect.center().x(), rect.bottom());
        path.lineTo(rect.left(), rect.center().y());
        path.closeSubpath();
        return path;
    }
    if (kind == QStringLiteral("star")) {
        return starPath(rect);
    }
    if (kind == QStringLiteral("rectanglebubble")) {
        return rectangleBubblePath(rect);
    }
    if (kind == QStringLiteral("ellipsebubble")) {
        return ellipseBubblePath(rect);
    }

    path.addRect(rect);
    return path;
}

void drawTextObject(QPainter &painter, const QVariantMap &object)
{
    const QString text = object.value(QStringLiteral("text")).toString();
    if (text.trimmed().isEmpty()) {
        return;
    }

    QRectF textRect(object.value(QStringLiteral("x")).toReal(),
                    object.value(QStringLiteral("y")).toReal(),
                    qMax<qreal>(minimumTextBoxWidth, object.value(QStringLiteral("width")).toReal()),
                    qMax<qreal>(minimumTextFontPixelSize, object.value(QStringLiteral("height")).toReal()));
    if (textRect.isEmpty()) {
        return;
    }

    const int fontPixelSize = qRound(qBound<qreal>(minimumTextFontPixelSize,
                                                  object.value(QStringLiteral("fontPixelSize")).toReal(),
                                                  maximumTextFontPixelSize));
    QFont font;
    font.setPixelSize(fontPixelSize);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    QTextDocument textDocument;
    textDocument.setDocumentMargin(0);
    textDocument.setDefaultFont(font);
    textDocument.setDefaultTextOption(textOption);
    textDocument.setPlainText(text);
    textDocument.setTextWidth(textRect.width());

    painter.save();
    painter.setClipRect(textRect);
    painter.translate(textRect.topLeft());

    QAbstractTextDocumentLayout::PaintContext paintContext;
    const QColor color(object.value(QStringLiteral("color")).toString());
    paintContext.palette.setColor(QPalette::Text, color.isValid() ? color : QColor(QStringLiteral("#1a1a1a")));
    textDocument.documentLayout()->draw(&painter, paintContext);
    painter.restore();
}

void drawShapeObject(QPainter &painter, const QVariantMap &object)
{
    QRectF shapeRect(object.value(QStringLiteral("x")).toReal(),
                     object.value(QStringLiteral("y")).toReal(),
                     object.value(QStringLiteral("width")).toReal(),
                     object.value(QStringLiteral("height")).toReal());
    shapeRect = shapeRect.normalized();
    if (shapeRect.width() < minimumShapeDimension || shapeRect.height() < minimumShapeDimension) {
        return;
    }

    const QColor color(object.value(QStringLiteral("color")).toString());
    const QColor fillColor = color.isValid() ? color : QColor(QStringLiteral("#1a1a1a"));
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawPath(shapePath(shapeRect, object.value(QStringLiteral("shapeKind")).toString()));
    painter.restore();
}

void drawImageObject(QPainter &painter, const QVariantMap &object)
{
    const QImage image = imageFromFileUrl(object.value(QStringLiteral("source")).toString());
    if (image.isNull()) {
        return;
    }

    QRectF imageRect(object.value(QStringLiteral("x")).toReal(),
                     object.value(QStringLiteral("y")).toReal(),
                     object.value(QStringLiteral("width")).toReal(),
                     object.value(QStringLiteral("height")).toReal());
    imageRect = imageRect.normalized();
    if (imageRect.width() < 1.0 || imageRect.height() < 1.0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(imageRect, image);
    painter.restore();
}

void drawRasterLayerObject(QPainter &painter,
                           const QVariantMap &object,
                           const QHash<int, QImage> &rasterLayersByObjectId)
{
    const int objectId = object.value(QStringLiteral("id")).toInt();
    const QImage image = rasterLayersByObjectId.value(objectId);
    if (image.isNull()) {
        return;
    }

    const bool visible = !object.contains(QStringLiteral("visible"))
        || object.value(QStringLiteral("visible")).toBool();
    if (!visible) {
        return;
    }

    const qreal opacity = object.contains(QStringLiteral("opacity"))
        ? qBound<qreal>(0.0, object.value(QStringLiteral("opacity")).toReal(), 1.0)
        : 1.0;

    painter.save();
    painter.setOpacity(opacity);
    painter.drawImage(QPointF(0.0, 0.0), image);
    painter.restore();
}

void drawObject(QPainter &painter,
                const QVariant &objectValue,
                const QHash<int, QImage> &rasterLayersByObjectId = {})
{
    const QVariantMap object = objectValue.toMap();
    const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("layer")) {
        drawRasterLayerObject(painter, object, rasterLayersByObjectId);
        return;
    }
    if (type == QStringLiteral("text")) {
        drawTextObject(painter, object);
        return;
    }
    if (type == QStringLiteral("shape")) {
        drawShapeObject(painter, object);
        return;
    }
    if (type == QStringLiteral("image")) {
        drawImageObject(painter, object);
    }
}

QImage compositeImageWithObjects(QImage image,
                                 const QVariantList &objects,
                                 const QHash<int, QImage> &rasterLayersByObjectId = {})
{
    if (image.isNull()) {
        return {};
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    for (const QVariant &object : objects) {
        drawObject(painter, object, rasterLayersByObjectId);
    }
    painter.end();
    return image;
}

QImage rasterizedObjectLayer(const QVariant &objectValue, const QRect &bounds)
{
    QImage image(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.translate(-bounds.topLeft());
    drawObject(painter, objectValue);
    painter.end();
    return image;
}

QRect drawableObjectThumbnailBounds(const QVariantMap &object)
{
    const QRectF objectRect(object.value(QStringLiteral("x")).toReal(),
                            object.value(QStringLiteral("y")).toReal(),
                            object.value(QStringLiteral("width")).toReal(),
                            object.value(QStringLiteral("height")).toReal());
    QRect bounds = objectRect.normalized().toAlignedRect();
    if (bounds.width() < 1 || bounds.height() < 1) {
        bounds = QRect(qFloor(objectRect.x()), qFloor(objectRect.y()), 1, 1);
    }
    return bounds;
}

QImage rasterizedRasterLayer(const QImage &layerImage, const QRect &bounds)
{
    QImage image(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (layerImage.isNull()) {
        return image;
    }

    QPainter painter(&image);
    painter.translate(-bounds.topLeft());
    painter.drawImage(QPointF(0.0, 0.0), layerImage);
    painter.end();
    return image;
}

QList<PsdImageWriter::Layer> psdLayersFromSession(const QImage &rasterImage,
                                                  const QVariantList &objects,
                                                  const PsdCompatibilityDocument &document,
                                                  const QHash<int, QImage> &rasterLayersByObjectId,
                                                  bool includeBackgroundLayer)
{
    const QList<PsdLayerRecord> records = document.layers();
    if (records.isEmpty() && objects.isEmpty()) {
        return {};
    }

    QList<PsdImageWriter::Layer> layers;
    layers.reserve(records.size());

    int recordIndex = 0;
    if (includeBackgroundLayer && !records.isEmpty()) {
        layers.append(PsdImageWriter::Layer{records.constFirst().name(), records.constFirst().bounds(), rasterImage});
        recordIndex = 1;
    }

    for (const QVariant &objectValue : objects) {
        if (objectValue.toMap().isEmpty()) {
            continue;
        }
        if (recordIndex >= records.size()) {
            break;
        }

        const PsdLayerRecord record = records.at(recordIndex);
        const QVariantMap object = objectValue.toMap();
        const QImage rasterLayerImage = object.value(QStringLiteral("type")).toString() == QStringLiteral("layer")
            ? rasterizedRasterLayer(rasterLayersByObjectId.value(object.value(QStringLiteral("id")).toInt()),
                                    record.bounds())
            : rasterizedObjectLayer(objectValue, record.bounds());
        layers.append(PsdImageWriter::Layer{record.name(), record.bounds(), rasterLayerImage});
        ++recordIndex;
    }

    return layers;
}

bool writeLayeredPsdFile(const QString &filePath,
                         const QImage &rasterImage,
                         const QVariantList &objects,
                         const QHash<int, QImage> &rasterLayersByObjectId,
                         bool includeBackgroundLayer)
{
    const PsdCompatibilityDocument document = PsdCompatibilityDocument::fromVincentSession(rasterImage.size(),
                                                                                          objects,
                                                                                          includeBackgroundLayer);
    if (!document.isPsdCanvasSizeCompatible()) {
        return false;
    }

    const QImage baseImage = includeBackgroundLayer ? opaqueCanvasBackgroundImage(rasterImage) : rasterImage;
    const QImage mergedImage = compositeImageWithObjects(baseImage, objects, rasterLayersByObjectId);
    return PsdImageWriter::writeLayeredImage(filePath,
                                            mergedImage,
                                            psdLayersFromSession(baseImage,
                                                                 objects,
                                                                 document,
                                                                 rasterLayersByObjectId,
                                                                 includeBackgroundLayer),
                                            document.toManifest());
}

} // namespace

DrawingSurfaceItem::DrawingSurfaceItem(QQuickItem *parent)
    : iiSharedCanvas::CanvasItem(parent)
    , m_viewModelBridge(new CanvasViewModelBridge())
{
    connect(this,
            &iiSharedCanvas::CanvasItem::undoRedoChanged,
            this,
            &DrawingSurfaceItem::emitUndoRedoSignals);
    connect(this,
            &iiSharedCanvas::CanvasItem::strokeCountChanged,
            this,
            &DrawingSurfaceItem::rasterContentChanged);
}

DrawingSurfaceItem::~DrawingSurfaceItem()
{
    delete m_viewModelBridge;
}

QObject *DrawingSurfaceItem::documentViewModel() const
{
    return m_viewModelBridge->documentViewModel();
}

QString DrawingSurfaceItem::viewId() const
{
    return m_viewId;
}

QString DrawingSurfaceItem::backgroundSource() const
{
    return m_backgroundSource;
}

bool DrawingSurfaceItem::hasBackground() const
{
    return m_hasBackground;
}

void DrawingSurfaceItem::setDocumentViewModel(QObject *documentViewModel)
{
    if (m_viewModelBridge->documentViewModel() == documentViewModel) {
        return;
    }

    m_viewModelBridge->setDocumentViewModel(documentViewModel);
    CanvasToolState nextToolState;
    nextToolState.color = brushColor();
    nextToolState.size = brushSize();
    nextToolState.flow = brushFlow();
    nextToolState.opacity = brushOpacity();
    nextToolState.hardness = brushHardness();
    nextToolState.spacing = brushSpacing();
    nextToolState.spacingRatio = brushSpacingRatio();
    nextToolState.flowEnabled = brushFlowEnabled();
    nextToolState.opacityEnabled = brushOpacityEnabled();
    nextToolState.hardnessEnabled = brushHardnessEnabled();
    nextToolState.spacingEnabled = brushSpacingEnabled();
    nextToolState.pressureCurveMinimum = pressureCurveMinimum();
    nextToolState.pressureCurveCenter = pressureCurveCenter();
    nextToolState.pressureCurveMaximum = pressureCurveMaximum();
    nextToolState.pressureToOpacityEnabled = pressureToOpacityEnabled();
    nextToolState.stabilizerStrength = stabilizerStrength();
    QString nextToolMode = toolMode();
    m_viewModelBridge->syncToolState(nextToolState, nextToolMode);
    setBrushColor(nextToolState.color);
    setBrushSize(nextToolState.size);
    setBrushFlow(nextToolState.flow);
    setBrushOpacity(nextToolState.opacity);
    setBrushHardness(nextToolState.hardness);
    setBrushSpacing(nextToolState.spacing);
    setBrushSpacingRatio(nextToolState.spacingRatio);
    setBrushFlowEnabled(nextToolState.flowEnabled);
    setBrushOpacityEnabled(nextToolState.opacityEnabled);
    setBrushHardnessEnabled(nextToolState.hardnessEnabled);
    setBrushSpacingEnabled(nextToolState.spacingEnabled);
    setPressureCurveMinimum(nextToolState.pressureCurveMinimum);
    setPressureCurveMaximum(nextToolState.pressureCurveMaximum);
    setPressureCurveCenter(nextToolState.pressureCurveCenter);
    setPressureToOpacityEnabled(nextToolState.pressureToOpacityEnabled);
    setStabilizerStrength(nextToolState.stabilizerStrength);
    setToolMode(nextToolMode);
    syncCanvasSize();
    if (!documentReady()) {
        createRasterDocument(canvasSize().width(), canvasSize().height());
    }
    emit documentViewModelChanged();
}

void DrawingSurfaceItem::setViewId(const QString &viewId)
{
    if (m_viewId == viewId) {
        return;
    }
    m_viewId = viewId;
    emit viewIdChanged();
}

void DrawingSurfaceItem::newCanvas()
{
    if (!canMutateDocument()) {
        return;
    }

    syncCanvasSize();
    const bool created = createRasterDocument(canvasSize().width(), canvasSize().height());
    if (created && m_hasBackground) {
        m_hasBackground = false;
        m_backgroundSource.clear();
        emit backgroundChanged();
    }
    if (created) {
        emit rasterContentChanged();
    }
}

void DrawingSurfaceItem::clearCanvas()
{
    newCanvas();
}

bool DrawingSurfaceItem::openRaster(const QString &fileUrl, qreal maximumCanvasWidth, qreal maximumCanvasHeight)
{
    if (!canMutateDocument()) {
        return false;
    }

    if (hasIiscSuffix(fileUrl)) {
        return openSharedCanvasDocument(fileUrl);
    }

    QImage image = imageFromFileUrl(fileUrl);
    if (image.isNull()) {
        return false;
    }

    const bool hasMaximumCanvasSize = maximumCanvasWidth > 0 || maximumCanvasHeight > 0;
    const QSize maximumSize(
            maximumCanvasWidth > 0 ? qRound(maximumCanvasWidth) : image.width(),
            maximumCanvasHeight > 0 ? qRound(maximumCanvasHeight) : image.height());
    const QSize rasterSize = hasMaximumCanvasSize
            ? fittedOpenedRasterSize(image.size(), maximumSize)
            : image.size();
    if (rasterSize.isEmpty()) {
        return false;
    }

    if (image.size() != rasterSize) {
        image = image.scaled(rasterSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    const bool opened = replaceRasterCanvas(image);
    if (!opened) {
        return false;
    }

    m_backgroundSource = localFileSource(fileUrl);
    m_hasBackground = true;
    syncCanvasSize();
    emit undoRedoChanged();
    emit backgroundChanged();
    emit rasterContentChanged();
    return true;
}

QVariantMap DrawingSurfaceItem::imageObjectForFile(const QString& fileUrl, qreal maximumObjectWidth,
                                                   qreal maximumObjectHeight) const
{
    const QImage image = imageFromFileUrl(fileUrl);
    if (image.isNull())
    {
        return {};
    }

    QVariantMap object = imageObjectForImage(image, cachedPsdPreviewSource(fileUrl, image),
                                             maximumObjectWidth, maximumObjectHeight);
    if (hasPsdSuffix(fileUrl))
    {
        object.insert(QStringLiteral("originalSource"), localFileSource(fileUrl));
        object.insert(QStringLiteral("sourceFormat"), QStringLiteral("psd"));
    }
    return object;
}

QVariantMap DrawingSurfaceItem::clipboardImageObject(qreal maximumObjectWidth,
                                                     qreal maximumObjectHeight) const
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard)
    {
        return imageImportResult(QStringLiteral("clipboard-unavailable"));
    }

    const QMimeData* mimeData = clipboard->mimeData(QClipboard::Clipboard);
    if (!mimeData)
    {
        return imageImportResult(QStringLiteral("no-image"));
    }

    const bool imageAdvertised = mimeData->hasImage();
    QImage image = imageAdvertised ? clipboard->image(QClipboard::Clipboard) : QImage();
    if (image.isNull() && mimeData->hasUrls())
    {
        image = firstLocalClipboardImage(mimeData->urls());
    }
    if (image.isNull())
    {
        return imageImportResult(imageAdvertised ? QStringLiteral("decode-failed")
                                                 : QStringLiteral("no-image"));
    }

    return cachedInsertedImageObject(image, {}, maximumObjectWidth, maximumObjectHeight);
}

bool DrawingSurfaceItem::canImportDroppedImage(QObject* dropEvent) const
{
    if (!dropEvent)
    {
        return false;
    }

    const QStringList formats = dropEventFormats(dropEvent);
    for (const QString& format : formats)
    {
        if (isSupportedImageMimeFormat(format))
        {
            return true;
        }
    }

    const auto urlCanRepresentImage = [](const QUrl& url)
    {
        if (isRemoteImageUrl(url) || isImageDataUrl(url))
        {
            return true;
        }
        if (!url.isLocalFile())
        {
            return false;
        }

        const QString filePath = url.toLocalFile();
        const QFileInfo fileInfo(filePath);
        return fileInfo.isFile() && fileInfo.isReadable() &&
               (PsdImageReader::canReadPath(filePath) ||
                !QImageReader::imageFormat(filePath).isEmpty());
    };

    const QList<QUrl> urls = dropEventUrls(dropEvent);
    for (const QUrl& url : urls)
    {
        if (urlCanRepresentImage(url))
        {
            return true;
        }
    }

    const QList<QUrl> htmlImageUrls = imageUrlsFromHtml(dropEvent->property("html").toString());
    for (const QUrl& url : htmlImageUrls)
    {
        if (urlCanRepresentImage(url))
        {
            return true;
        }
    }

    return urlCanRepresentImage(imageUrlFromText(dropEvent->property("text").toString()));
}

void DrawingSurfaceItem::importDroppedImage(QObject* dropEvent, qreal maximumObjectWidth,
                                            qreal maximumObjectHeight)
{
    if (!dropEvent)
    {
        emit droppedImageFailed(QStringLiteral("no-image"));
        return;
    }

    const qreal dropX = dropEvent->property("x").toReal();
    const qreal dropY = dropEvent->property("y").toReal();
    const auto emitReadyOrTerminalFailure = [this, dropX, dropY](const QVariantMap& object)
    {
        const QString status = object.value(QStringLiteral("status")).toString();
        if (status == QStringLiteral("ready"))
        {
            emit droppedImageReady(object, dropX, dropY);
            return true;
        }
        if (!status.isEmpty() && status != QStringLiteral("decode-failed"))
        {
            emit droppedImageFailed(status);
            return true;
        }
        return false;
    };

    bool imageDataAdvertised = false;
    const QStringList formats = dropEventFormats(dropEvent);
    for (const QString& format : formats)
    {
        if (!isSupportedImageMimeFormat(format))
        {
            continue;
        }

        imageDataAdvertised = true;
        const QByteArray encodedData = dropEventData(dropEvent, format);
        if (encodedData.isEmpty())
        {
            continue;
        }

        const QVariantMap object =
            imageObjectForEncodedData(encodedData, {}, maximumObjectWidth, maximumObjectHeight);
        if (emitReadyOrTerminalFailure(object))
        {
            return;
        }
    }

    bool imageUrlAdvertised = false;
    const QList<QUrl> droppedUrls = dropEventUrls(dropEvent);
    for (const QUrl& url : droppedUrls)
    {
        if (!url.isLocalFile())
        {
            continue;
        }

        imageUrlAdvertised = true;
        const QVariantMap object =
            imageObjectForLocalDrop(url, maximumObjectWidth, maximumObjectHeight);
        if (emitReadyOrTerminalFailure(object))
        {
            return;
        }
    }

    QList<QUrl> webImageUrls;
    const QList<QUrl> htmlImageUrls = imageUrlsFromHtml(dropEvent->property("html").toString());
    for (const QUrl& url : htmlImageUrls)
    {
        appendUniqueUrl(webImageUrls, url);
    }
    for (const QUrl& url : droppedUrls)
    {
        appendUniqueUrl(webImageUrls, url);
    }
    appendUniqueUrl(webImageUrls, imageUrlFromText(dropEvent->property("text").toString()));

    for (const QUrl& url : webImageUrls)
    {
        if (!isImageDataUrl(url))
        {
            continue;
        }

        imageUrlAdvertised = true;
        const QVariantMap object = imageObjectForEncodedData(
            encodedImageFromDataUrl(url), url, maximumObjectWidth, maximumObjectHeight);
        if (emitReadyOrTerminalFailure(object))
        {
            return;
        }
    }

    for (const QUrl& url : webImageUrls)
    {
        if (!isRemoteImageUrl(url))
        {
            continue;
        }

        requestRemoteDroppedImage(url, maximumObjectWidth, maximumObjectHeight, dropX, dropY);
        return;
    }

    emit droppedImageFailed(imageDataAdvertised || imageUrlAdvertised
                                ? QStringLiteral("decode-failed")
                                : QStringLiteral("no-image"));
}

void DrawingSurfaceItem::requestRemoteDroppedImage(const QUrl& url, qreal maximumObjectWidth,
                                                   qreal maximumObjectHeight, qreal dropX,
                                                   qreal dropY)
{
    if (!isRemoteImageUrl(url))
    {
        emit droppedImageFailed(QStringLiteral("download-failed"));
        return;
    }

    if (!m_networkAccessManager)
    {
        m_networkAccessManager = new QNetworkAccessManager(this);
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setMaximumRedirectsAllowed(5);
    request.setTransferTimeout(remoteImageDownloadTimeoutMs);
    const QString version = QCoreApplication::applicationVersion().isEmpty()
                                ? QStringLiteral("development")
                                : QCoreApplication::applicationVersion();
    request.setRawHeader("User-Agent", QStringLiteral("Vincent/%1").arg(version).toUtf8());

    QNetworkReply* reply = m_networkAccessManager->get(request);
    const auto downloadTooLarge = std::make_shared<bool>(false);
    connect(reply, &QNetworkReply::downloadProgress, reply,
            [reply, downloadTooLarge](qint64 receivedBytes, qint64 totalBytes)
            {
                if (receivedBytes > maximumRemoteImageDownloadBytes ||
                    totalBytes > maximumRemoteImageDownloadBytes)
                {
                    *downloadTooLarge = true;
                    reply->abort();
                }
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, downloadTooLarge, url, maximumObjectWidth, maximumObjectHeight, dropX,
             dropY]()
            {
                const QNetworkReply::NetworkError networkError = reply->error();
                const QByteArray encodedData = reply->readAll();
                reply->deleteLater();

                if (*downloadTooLarge || encodedData.size() > maximumRemoteImageDownloadBytes)
                {
                    emit droppedImageFailed(QStringLiteral("download-too-large"));
                    return;
                }
                if (networkError != QNetworkReply::NoError)
                {
                    emit droppedImageFailed(QStringLiteral("download-failed"));
                    return;
                }

                const QVariantMap object = imageObjectForEncodedData(
                    encodedData, url, maximumObjectWidth, maximumObjectHeight);
                const QString status = object.value(QStringLiteral("status")).toString();
                if (status != QStringLiteral("ready"))
                {
                    emit droppedImageFailed(status.isEmpty() ? QStringLiteral("decode-failed")
                                                             : status);
                    return;
                }
                emit droppedImageReady(object, dropX, dropY);
            });
}

QVariantMap DrawingSurfaceItem::psdImportDocument(const QString &fileUrl) const
{
    const QString filePath = localFilePath(fileUrl);
    if (!PsdImageReader::canReadPath(filePath)) {
        return {};
    }

    const PsdImportedDocument imported = PsdImageReader::readDocument(filePath);
    if (!imported.isValid()) {
        return {};
    }

    QVariantList layers;
    layers.reserve(imported.layers.size());
    for (const PsdImportedLayer &layer : imported.layers) {
        if (layer.image.isNull()) {
            continue;
        }

        QVariantMap layerMap;
        layerMap.insert(QStringLiteral("name"), layer.name);
        layerMap.insert(QStringLiteral("left"), layer.bounds.left());
        layerMap.insert(QStringLiteral("top"), layer.bounds.top());
        layerMap.insert(QStringLiteral("right"), layer.bounds.right() + 1);
        layerMap.insert(QStringLiteral("bottom"), layer.bounds.bottom() + 1);
        layerMap.insert(QStringLiteral("width"), layer.bounds.width());
        layerMap.insert(QStringLiteral("height"), layer.bounds.height());
        layerMap.insert(QStringLiteral("blendModeKey"), layer.blendModeKey);
        layerMap.insert(QStringLiteral("opacity"), layer.opacity);
        layerMap.insert(QStringLiteral("opacityRatio"), qBound<qreal>(0.0, layer.opacity / 255.0, 1.0));
        layerMap.insert(QStringLiteral("visible"), layer.visible);
        layerMap.insert(QStringLiteral("hasUserMask"), layer.hasUserMask);
        layerMap.insert(QStringLiteral("hasVectorMask"), layer.hasVectorMask);
        layerMap.insert(QStringLiteral("source"), cachedPngSourceForImage(QStringLiteral("psd-import-layers"), layer.image));
        layerMap.insert(QStringLiteral("thumbnailSource"),
                        cachedPngSourceForImage(QStringLiteral("layer-thumbnails"),
                                                thumbnailImage(layer.image,
                                                               boundedThumbnailMaximumSize(32, 32))));
        layers.append(layerMap);
    }

    QVariantMap document;
    document.insert(QStringLiteral("valid"), !layers.isEmpty() || !imported.mergedImage.isNull());
    document.insert(QStringLiteral("canvasWidth"), imported.canvasSize.width());
    document.insert(QStringLiteral("canvasHeight"), imported.canvasSize.height());
    document.insert(QStringLiteral("bitsPerChannel"), imported.bitsPerChannel);
    document.insert(QStringLiteral("colorMode"), imported.colorMode);
    document.insert(QStringLiteral("hasRealMergedImage"), imported.hasRealMergedImage);
    document.insert(QStringLiteral("xmpMetadata"), imported.xmpMetadata);
    document.insert(QStringLiteral("vincentManifest"), imported.vincentManifest);
    document.insert(QStringLiteral("layers"), layers);
    document.insert(QStringLiteral("compatibilityWarnings"), imported.compatibilityWarnings);
    if (!imported.mergedImage.isNull()) {
        document.insert(QStringLiteral("mergedSource"),
                        cachedPngSourceForImage(QStringLiteral("psd-previews"), imported.mergedImage));
    }
    return document;
}

bool DrawingSurfaceItem::saveToFile(const QString &fileUrl)
{
    if (hasIiscSuffix(fileUrl)) {
        return saveSharedCanvasDocument(fileUrl);
    }
    if (hasPsdSuffix(fileUrl)) {
        syncCanvasSize();
        return writeLayeredPsdFile(localFilePath(fileUrl), currentRasterCanvasImage(canvasSize()), {}, {}, true);
    }

    syncCanvasSize();
    const QImage image = currentRasterCanvasImage(canvasSize());
    return !image.isNull() && image.save(localFilePath(fileUrl));
}

bool DrawingSurfaceItem::saveToFileWithObjects(const QString &fileUrl, const QVariantList &objects)
{
    return saveToFileWithObjectsAndRasterLayers(fileUrl, objects, {});
}

bool DrawingSurfaceItem::saveToFileWithObjectsAndRasterLayers(const QString &fileUrl,
                                                              const QVariantList &objects,
                                                              const QVariantList &rasterLayers,
                                                              bool includeBackgroundLayer)
{
    if (hasIiscSuffix(fileUrl)) {
        if (!objects.isEmpty() || !rasterLayers.isEmpty()) {
            return false;
        }
        return saveSharedCanvasDocument(fileUrl);
    }

    syncCanvasSize();
    const QImage rasterImage = currentRasterCanvasImage(canvasSize());
    if (rasterImage.isNull()) {
        return false;
    }

    QHash<int, QImage> rasterLayersByObjectId;
    for (const QVariant &layerValue : rasterLayers) {
        const QVariantMap layerDescriptor = layerValue.toMap();
        const int objectId = layerDescriptor.value(QStringLiteral("objectId")).toInt();
        if (objectId <= 0) {
            continue;
        }

        QImage layerImage;
        if (auto *layerItem = qobject_cast<DrawingSurfaceItem *>(
                    layerDescriptor.value(QStringLiteral("item")).value<QObject *>())) {
            layerItem->syncCanvasSize();
            layerImage = layerItem->currentRasterCanvasImage(canvasSize());
        }
        if (layerImage.isNull()) {
            layerImage = imageFromFileUrl(layerDescriptor.value(QStringLiteral("snapshotSource")).toString());
        }
        if (layerImage.isNull()) {
            continue;
        }
        if (layerImage.size() != canvasSize()) {
            QImage resizedLayer = transparentCanvasImage(canvasSize());
            QPainter painter(&resizedLayer);
            painter.drawImage(QPointF(0.0, 0.0), layerImage);
            painter.end();
            layerImage = resizedLayer;
        }
        if (layerImage.format() != QImage::Format_ARGB32_Premultiplied) {
            layerImage = layerImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
        rasterLayersByObjectId.insert(objectId, layerImage);
    }

    if (hasPsdSuffix(fileUrl)) {
        return writeLayeredPsdFile(localFilePath(fileUrl),
                                   rasterImage,
                                   objects,
                                   rasterLayersByObjectId,
                                   includeBackgroundLayer);
    }

    const QImage baseImage = includeBackgroundLayer ? opaqueCanvasBackgroundImage(rasterImage) : rasterImage;
    const QImage image = compositeImageWithObjects(baseImage, objects, rasterLayersByObjectId);
    return image.save(localFilePath(fileUrl));
}

QString DrawingSurfaceItem::cacheRasterSnapshotSource()
{
    syncCanvasSize();
    const QImage image = currentRasterCanvasImage(canvasSize());
    if (image.isNull()) {
        return {};
    }

    const QString snapshotPath = writableCacheFilePath(QStringLiteral("raster-layer-snapshots"),
                                                       QStringLiteral("layer-XXXXXX.png"));
    if (snapshotPath.isEmpty()) {
        return {};
    }
    if (!image.save(snapshotPath, "PNG")) {
        QFile::remove(snapshotPath);
        return {};
    }
    return QUrl::fromLocalFile(snapshotPath).toString();
}

QString DrawingSurfaceItem::cacheRasterThumbnailSource(qreal maximumWidth, qreal maximumHeight)
{
    syncCanvasSize();
    const QImage image = currentRasterCanvasImage(canvasSize());
    if (image.isNull()) {
        return {};
    }

    const QImage thumbnail = thumbnailImage(image, boundedThumbnailMaximumSize(maximumWidth, maximumHeight));
    return cachedPngSourceForImage(QStringLiteral("layer-thumbnails"), thumbnail);
}

QString DrawingSurfaceItem::cacheGrabbedThumbnailSource(QObject *grabResult)
{
    auto *result = qobject_cast<QQuickItemGrabResult *>(grabResult);
    if (!result) {
        return {};
    }

    const QString thumbnailPath = writableCacheFilePath(QStringLiteral("layer-thumbnails"),
                                                        QStringLiteral("grab-XXXXXX.png"));
    if (thumbnailPath.isEmpty()) {
        return {};
    }

    if (!result->saveToFile(thumbnailPath)) {
        QFile::remove(thumbnailPath);
        return {};
    }
    return QUrl::fromLocalFile(thumbnailPath).toString();
}

QString DrawingSurfaceItem::cacheDrawableObjectThumbnailSource(const QVariantMap &object,
                                                               qreal maximumWidth,
                                                               qreal maximumHeight) const
{
    if (object.isEmpty()) {
        return {};
    }

    const QRect bounds = drawableObjectThumbnailBounds(object);
    const QImage image = rasterizedObjectLayer(object, bounds);
    if (image.isNull()) {
        return {};
    }

    const QImage thumbnail = thumbnailImage(image, boundedThumbnailMaximumSize(maximumWidth, maximumHeight));
    return cachedPngSourceForImage(QStringLiteral("layer-thumbnails"), thumbnail);
}

bool DrawingSurfaceItem::restoreRasterSnapshot(const QString &fileUrl)
{
    if (!canMutateDocument()) {
        return false;
    }

    QImage image = imageFromFileUrl(fileUrl);
    if (image.isNull()) {
        return false;
    }

    syncCanvasSize();
    if (image.size() != canvasSize()) {
        QImage resizedLayer = transparentCanvasImage(canvasSize());
        QPainter painter(&resizedLayer);
        painter.drawImage(QPointF(0.0, 0.0), image);
        painter.end();
        image = resizedLayer;
    }
    if (image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    const bool restored = replaceRasterCanvas(image);
    if (restored) {
        emitUndoRedoSignals();
        emit rasterContentChanged();
    }
    return restored;
}

QVariantMap DrawingSurfaceItem::psdCompatibilityManifest(const QVariantList &objects,
                                                         bool includeBackgroundLayer) const
{
    return PsdCompatibilityDocument::fromVincentSession(canvasSize(), objects, includeBackgroundLayer).toManifest();
}

void DrawingSurfaceItem::undo()
{
    const bool applied = iiSharedCanvas::CanvasItem::undo();
    if (applied) {
        emit rasterContentChanged();
    }
}

void DrawingSurfaceItem::redo()
{
    const bool applied = iiSharedCanvas::CanvasItem::redo();
    if (applied) {
        emit rasterContentChanged();
    }
}

void DrawingSurfaceItem::resizeCanvasSurface(qreal canvasWidth, qreal canvasHeight)
{
    const qreal boundedWidth = qMax<qreal>(1.0, qRound(canvasWidth));
    const qreal boundedHeight = qMax<qreal>(1.0, qRound(canvasHeight));

    if (qFuzzyCompare(width(), boundedWidth) && qFuzzyCompare(height(), boundedHeight)) {
        syncCanvasSize();
        return;
    }

    m_isApplyingCanvasSurfaceSize = true;
    setWidth(boundedWidth);
    setHeight(boundedHeight);
    m_isApplyingCanvasSurfaceSize = false;

    syncCanvasSize();
}

void DrawingSurfaceItem::beginStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (!canMutateDocument()) {
        return;
    }

    if (pressureSensitive) {
        QTabletEvent event = makeSyntheticTabletStrokeEvent(QEvent::TabletPress,
                                                            pointX,
                                                            pointY,
                                                            rawPressure,
                                                            Qt::LeftButton,
                                                            Qt::LeftButton);
        iiSharedCanvas::CanvasItem::event(&event);
        return;
    }

    QMouseEvent event = makeMouseEvent(QEvent::MouseButtonPress,
                                       pointX,
                                       pointY,
                                       Qt::LeftButton,
                                       Qt::LeftButton);
    iiSharedCanvas::CanvasItem::mousePressEvent(&event);
}

bool DrawingSurfaceItem::appendStrokePoint(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (!canMutateDocument()) {
        return false;
    }

    if (pressureSensitive) {
        QTabletEvent event = makeSyntheticTabletStrokeEvent(QEvent::TabletMove,
                                                            pointX,
                                                            pointY,
                                                            rawPressure,
                                                            Qt::NoButton,
                                                            Qt::LeftButton);
        iiSharedCanvas::CanvasItem::event(&event);
        return true;
    }

    QMouseEvent event = makeMouseEvent(QEvent::MouseMove,
                                       pointX,
                                       pointY,
                                       Qt::NoButton,
                                       Qt::LeftButton);
    iiSharedCanvas::CanvasItem::mouseMoveEvent(&event);
    return true;
}

void DrawingSurfaceItem::endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (!canMutateDocument()) {
        return;
    }

    if (pressureSensitive) {
        QTabletEvent event = makeSyntheticTabletStrokeEvent(QEvent::TabletRelease,
                                                            pointX,
                                                            pointY,
                                                            rawPressure,
                                                            Qt::LeftButton,
                                                            Qt::NoButton);
        iiSharedCanvas::CanvasItem::event(&event);
        emit rasterContentChanged();
        return;
    }

    QMouseEvent event = makeMouseEvent(QEvent::MouseButtonRelease,
                                       pointX,
                                       pointY,
                                       Qt::LeftButton,
                                       Qt::NoButton);
    iiSharedCanvas::CanvasItem::mouseReleaseEvent(&event);
    emit rasterContentChanged();
}

bool DrawingSurfaceItem::commitText(qreal pointX,
                                    qreal pointY,
                                    qreal boxWidth,
                                    const QString &text,
                                    qreal fontPixelSize,
                                    const QColor &color)
{
    if (!canMutateDocument() || text.trimmed().isEmpty()) {
        return false;
    }

    syncCanvasSize();
    const QSize targetSize = canvasSize();
    if (targetSize.isEmpty()) {
        return false;
    }

    QImage image = selectedRasterCanvasImage();
    const std::optional<QTransform> documentToRaster =
        documentToSelectedRasterTransform(*this);
    if (image.isNull() || !documentToRaster) {
        return false;
    }

    const qreal maxX = qMax<qreal>(0.0, targetSize.width() - 1.0);
    const qreal maxY = qMax<qreal>(0.0, targetSize.height() - 1.0);
    const qreal boundedX = qBound<qreal>(0.0, pointX, maxX);
    const qreal boundedY = qBound<qreal>(0.0, pointY, maxY);
    const qreal availableWidth = qMax<qreal>(1.0, targetSize.width() - boundedX);
    const qreal textWidth = qBound<qreal>(minimumTextBoxWidth, boxWidth, availableWidth);
    const int boundedFontPixelSize = qRound(qBound<qreal>(minimumTextFontPixelSize,
                                                          fontPixelSize,
                                                          maximumTextFontPixelSize));

    QFont font;
    font.setPixelSize(boundedFontPixelSize);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    QTextDocument textDocument;
    textDocument.setDocumentMargin(0);
    textDocument.setDefaultFont(font);
    textDocument.setDefaultTextOption(textOption);
    textDocument.setPlainText(text);
    textDocument.setTextWidth(textWidth);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setWorldTransform(*documentToRaster);
    painter.translate(QPointF(boundedX, boundedY));

    QAbstractTextDocumentLayout::PaintContext paintContext;
    paintContext.palette.setColor(QPalette::Text, color.isValid() ? color : brushColor());
    textDocument.documentLayout()->draw(&painter, paintContext);
    painter.end();

    const bool committed = replaceSelectedRaster(image);
    if (committed) {
        emitUndoRedoSignals();
        emit rasterContentChanged();
    }
    return committed;
}

bool DrawingSurfaceItem::commitShape(qreal pointX,
                                     qreal pointY,
                                     qreal boxWidth,
                                     qreal boxHeight,
                                     const QString &shapeKind,
                                     const QColor &color)
{
    if (!canMutateDocument()) {
        return false;
    }

    syncCanvasSize();
    const QSize targetSize = canvasSize();
    if (targetSize.isEmpty()) {
        return false;
    }

    QImage image = selectedRasterCanvasImage();
    const std::optional<QTransform> documentToRaster =
        documentToSelectedRasterTransform(*this);
    if (image.isNull() || !documentToRaster) {
        return false;
    }
    QRectF shapeRect(QPointF(pointX, pointY), QSizeF(boxWidth, boxHeight));
    shapeRect = shapeRect.normalized().intersected(
        QRectF(0.0, 0.0, targetSize.width(), targetSize.height()));
    if (shapeRect.width() < minimumShapeDimension || shapeRect.height() < minimumShapeDimension) {
        return false;
    }

    const QColor fillColor = color.isValid() ? color : brushColor();
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setWorldTransform(*documentToRaster);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawPath(shapePath(shapeRect, shapeKind));
    painter.end();

    const bool committed = replaceSelectedRaster(image);
    if (committed) {
        emitUndoRedoSignals();
        emit rasterContentChanged();
    }
    return committed;
}

bool DrawingSurfaceItem::fillAt(qreal pointX, qreal pointY, const QColor &color)
{
    if (!canMutateDocument()) {
        return false;
    }

    syncCanvasSize();
    const QSize targetSize = canvasSize();
    if (targetSize.isEmpty()) {
        return false;
    }

    QImage image = selectedRasterCanvasImage();
    const std::optional<QTransform> documentToRaster =
        documentToSelectedRasterTransform(*this);
    if (image.isNull() || !documentToRaster) {
        return false;
    }

    const QPointF rasterPoint = documentToRaster->map(QPointF(pointX, pointY));
    if (!qIsFinite(rasterPoint.x()) || !qIsFinite(rasterPoint.y())
        || rasterPoint.x() < 0.0 || rasterPoint.x() >= image.width()
        || rasterPoint.y() < 0.0 || rasterPoint.y() >= image.height()) {
        return false;
    }
    const int seedX = static_cast<int>(qFloor(rasterPoint.x()));
    const int seedY = static_cast<int>(qFloor(rasterPoint.y()));
    const QColor targetColor = image.pixelColor(seedX, seedY);
    QColor replacementColor = color.isValid() ? color : brushColor();
    if (!replacementColor.isValid()) {
        replacementColor = QColor(Qt::transparent);
    }
    if (targetColor.rgba() == replacementColor.rgba()) {
        return false;
    }

    QVector<QPoint> pending;
    pending.reserve(qMin(image.width() * image.height(), 4096));
    pending.append(QPoint(seedX, seedY));

    while (!pending.isEmpty()) {
        const QPoint point = pending.takeLast();
        if (point.x() < 0 || point.x() >= image.width() || point.y() < 0 || point.y() >= image.height()) {
            continue;
        }
        if (image.pixelColor(point).rgba() != targetColor.rgba()) {
            continue;
        }

        image.setPixelColor(point, replacementColor);
        pending.append(QPoint(point.x() + 1, point.y()));
        pending.append(QPoint(point.x() - 1, point.y()));
        pending.append(QPoint(point.x(), point.y() + 1));
        pending.append(QPoint(point.x(), point.y() - 1));
    }

    const bool committed = replaceSelectedRaster(image);
    if (committed) {
        emitUndoRedoSignals();
        emit rasterContentChanged();
    }
    return committed;
}

bool DrawingSurfaceItem::event(QEvent *event)
{
    if (event && isTabletEvent(event->type()) && isOverlayToolActive()) {
        event->accept();
        return true;
    }
    if (event && isTabletEvent(event->type()) && !canMutateDocument()) {
        event->accept();
        return true;
    }
    return iiSharedCanvas::CanvasItem::event(event);
}

void DrawingSurfaceItem::mousePressEvent(QMouseEvent *event)
{
    if (isOverlayToolActive()) {
        event->accept();
        return;
    }
    if (!canMutateDocument()) {
        event->accept();
        return;
    }
    iiSharedCanvas::CanvasItem::mousePressEvent(event);
}

void DrawingSurfaceItem::mouseMoveEvent(QMouseEvent *event)
{
    if (isOverlayToolActive()) {
        event->accept();
        return;
    }
    if (!canMutateDocument()) {
        event->accept();
        return;
    }
    iiSharedCanvas::CanvasItem::mouseMoveEvent(event);
}

void DrawingSurfaceItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (isOverlayToolActive()) {
        event->accept();
        return;
    }
    if (!canMutateDocument()) {
        event->accept();
        return;
    }
    iiSharedCanvas::CanvasItem::mouseReleaseEvent(event);
}

void DrawingSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    iiSharedCanvas::CanvasItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() == oldGeometry.size()) {
        return;
    }
    if (m_isApplyingCanvasSurfaceSize) {
        return;
    }
    syncCanvasSize();
}

QSize DrawingSurfaceItem::canvasSize() const
{
    return QSize(qMax(1, qRound(width())), qMax(1, qRound(height())));
}

bool DrawingSurfaceItem::canMutateDocument() const
{
    return m_viewModelBridge->canMutateDocument();
}

bool DrawingSurfaceItem::isTextToolActive() const
{
    return toolMode() == QStringLiteral("text");
}

bool DrawingSurfaceItem::isShapeToolActive() const
{
    return toolMode() == QStringLiteral("shape");
}

bool DrawingSurfaceItem::isFillToolActive() const
{
    return toolMode() == QStringLiteral("fill");
}

bool DrawingSurfaceItem::isPanToolActive() const
{
    return toolMode() == QStringLiteral("pan");
}

bool DrawingSurfaceItem::isMoveToolActive() const
{
    return toolMode() == QStringLiteral("move");
}

bool DrawingSurfaceItem::isZoomToolActive() const
{
    return toolMode() == QStringLiteral("zoom");
}

bool DrawingSurfaceItem::isOverlayToolActive() const
{
    return isTextToolActive() || isShapeToolActive() || isFillToolActive() || isPanToolActive() || isMoveToolActive() || isZoomToolActive();
}

QImage DrawingSurfaceItem::currentRasterCanvasImage(const QSize &targetSize)
{
    QImage image;
    const RasterLayer *pixels = framePixels();
    if (pixels && pixels->width > 0 && pixels->height > 0
        && pixels->width <= std::numeric_limits<int>::max() / 4) {
        const QImage view(reinterpret_cast<const uchar *>(pixels->pixels.data()),
                          pixels->width,
                          pixels->height,
                          pixels->width * 4,
                          QImage::Format_ARGB32);
        image = view.copy();
    }
    if (image.isNull()) {
        image = transparentCanvasImage(targetSize);
    } else if (image.size() != targetSize) {
        QImage resized = transparentCanvasImage(targetSize);
        QPainter painter(&resized);
        painter.drawImage(QPointF(0.0, 0.0), image);
        painter.end();
        image = resized;
    }
    if (image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    return image;
}

QImage DrawingSurfaceItem::selectedRasterCanvasImage() const
{
    const RasterLayer *pixels = selectedRasterPixels();
    if (!pixels || pixels->width <= 0 || pixels->height <= 0
        || pixels->width > std::numeric_limits<int>::max() / 4) {
        return {};
    }

    const std::size_t width = static_cast<std::size_t>(pixels->width);
    const std::size_t height = static_cast<std::size_t>(pixels->height);
    if (width > std::numeric_limits<std::size_t>::max() / height
        || pixels->pixels.size() != width * height) {
        return {};
    }

    const QImage view(reinterpret_cast<const uchar *>(pixels->pixels.data()),
                      pixels->width,
                      pixels->height,
                      pixels->width * 4,
                      QImage::Format_ARGB32);
    return view.copy();
}

bool DrawingSurfaceItem::replaceRasterCanvas(const QImage &source)
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return false;
    }

    const QImage image = source.convertToFormat(QImage::Format_ARGB32);
    m_isApplyingCanvasSurfaceSize = true;
    setWidth(image.width());
    setHeight(image.height());
    m_isApplyingCanvasSurfaceSize = false;
    if (!createRasterDocument(image.width(), image.height())) {
        return false;
    }

    return replaceSelectedRaster(image);
}

bool DrawingSurfaceItem::replaceSelectedRaster(const QImage &source)
{
    const RasterLayer *selected = selectedRasterPixels();
    if (source.isNull() || !selected
        || source.width() != selected->width || source.height() != selected->height) {
        return false;
    }

    const QImage image = source.convertToFormat(QImage::Format_ARGB32);

    RasterLayer pixels = makeRasterLayer(image.width(), image.height());
    for (int y = 0; y < image.height(); ++y) {
        const auto *row = reinterpret_cast<const std::uint32_t *>(image.constScanLine(y));
        std::copy_n(row,
                    static_cast<std::size_t>(image.width()),
                    pixels.pixels.begin()
                        + static_cast<std::ptrdiff_t>(y) * image.width());
    }
    return replaceSelectedPixels(pixels);
}

bool DrawingSurfaceItem::openSharedCanvasDocument(const QString &fileUrl)
{
    const QString filePath = localFilePath(fileUrl);
    QFile file(filePath);
    const iiSharedCanvas::SerializationLimits limits;
    if (!file.open(QIODevice::ReadOnly)
        || file.size() < static_cast<qint64>(iiSharedCanvas::IiscHeaderSize)
        || static_cast<std::uint64_t>(file.size()) > limits.maximumContainerBytes) {
        return false;
    }

    const QByteArray bytes = file.readAll();
    if (bytes.size() != file.size()) {
        return false;
    }
    iiSharedCanvas::IiscDecodeResult decoded = iiSharedCanvas::decodeIisc(
        std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())),
        limits);
    if (!decoded.ok()) {
        return false;
    }

    if (!document()
        && !createDocument(decoded.document.extent.width,
                           decoded.document.extent.height,
                           decoded.document.timeline.frameCount)) {
        return false;
    }
    iiSharedCanvas::Document *target = document();
    *target = std::move(decoded.document);
    if (!bind(*target)) {
        return false;
    }
    for (const iiSharedCanvas::Layer &layer : target->layers) {
        const iiSharedCanvas::Asset *asset = iiSharedCanvas::resolveAssetAt(*target, layer, 0);
        if (asset && std::holds_alternative<iiSharedCanvas::RasterAsset>(*asset)) {
            selectLayer(QString::fromUtf8(layer.id.data(),
                                          static_cast<qsizetype>(layer.id.size())));
            break;
        }
    }

    resizeCanvasSurface(target->extent.width, target->extent.height);
    m_backgroundSource = localFileSource(fileUrl);
    m_hasBackground = true;
    emit backgroundChanged();
    emit rasterContentChanged();
    return true;
}

bool DrawingSurfaceItem::saveSharedCanvasDocument(const QString &fileUrl)
{
    if (!document()) {
        return false;
    }
    const iiSharedCanvas::IiscEncodeResult encoded = iiSharedCanvas::encodeIisc(*document());
    if (!encoded.ok()
        || encoded.bytes.size() > static_cast<std::size_t>(std::numeric_limits<qint64>::max())) {
        return false;
    }

    QSaveFile file(localFilePath(fileUrl));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    const qint64 byteCount = static_cast<qint64>(encoded.bytes.size());
    if (file.write(reinterpret_cast<const char *>(encoded.bytes.data()), byteCount) != byteCount) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

void DrawingSurfaceItem::syncCanvasSize()
{
    m_viewModelBridge->syncCanvasSize(width(), height());
}

void DrawingSurfaceItem::emitUndoRedoSignals()
{
    emit canUndoChanged();
    emit canRedoChanged();
}

QMouseEvent DrawingSurfaceItem::makeMouseEvent(QEvent::Type eventType,
                                               qreal pointX,
                                               qreal pointY,
                                               Qt::MouseButton button,
                                               Qt::MouseButtons buttons) const
{
    const QPointF position{pointX, pointY};
    return QMouseEvent{eventType,
                       position,
                       position,
                       position,
                       button,
                       buttons,
                       Qt::NoModifier};
}
