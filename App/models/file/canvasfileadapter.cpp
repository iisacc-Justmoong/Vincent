#include "canvasfileadapter.h"

#include "../../rasterdocumentio.h"

#include <QImageReader>
#include <QUrl>

CanvasFileAdapter::CanvasFileAdapter()
    : m_rasterDocumentIO(new RasterDocumentIO())
{
}

CanvasFileAdapter::~CanvasFileAdapter()
{
    delete m_rasterDocumentIO;
}

RasterLoadResult CanvasFileAdapter::openRaster(const QString &fileUrl) const
{
    const QVariantMap openResult = m_rasterDocumentIO->loadRasterDocument(fileUrl);
    if (!openResult.value(QStringLiteral("ok")).toBool()) {
        return {};
    }

    const QString sourceUrl = openResult.value(QStringLiteral("source")).toString();
    const QString localPath = toLocalPath(sourceUrl);
    if (localPath.isEmpty()) {
        return {};
    }

    QImageReader reader(localPath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return {};
    }

    RasterLoadResult result;
    result.ok = true;
    result.sourceUrl = sourceUrl;
    result.width = openResult.value(QStringLiteral("width")).toInt();
    result.height = openResult.value(QStringLiteral("height")).toInt();
    result.image = image;
    return result;
}

bool CanvasFileAdapter::saveImage(const QString &fileUrl, const QImage &image) const
{
    const QString path = toLocalPath(fileUrl);
    return !path.isEmpty() && !image.isNull() && image.save(path);
}

QString CanvasFileAdapter::toLocalPath(const QString &fileUrl) const
{
    if (fileUrl.isEmpty()) {
        return {};
    }

    const QUrl url(fileUrl);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }
    if (url.isValid() && !url.scheme().isEmpty()) {
        return {};
    }
    return fileUrl;
}
