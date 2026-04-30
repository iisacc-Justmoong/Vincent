#include "rasterdocumentio.h"

#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <QUrl>

namespace {

struct InputReference
{
    bool localFile = false;
    QString localPath;
    QString suffix;
};

InputReference resolveInputReference(const QString &fileUrl)
{
    InputReference result;
    if (fileUrl.isEmpty()) {
        return result;
    }

    const QUrl url(fileUrl);
    if (url.isValid() && !url.scheme().isEmpty()) {
        result.localFile = url.isLocalFile();
        if (result.localFile) {
            result.localPath = url.toLocalFile();
            result.suffix = QFileInfo(result.localPath).suffix().toLower();
        }
        return result;
    }

    const QFileInfo fileInfo(fileUrl);
    result.localFile = true;
    result.localPath = fileInfo.absoluteFilePath();
    result.suffix = fileInfo.suffix().toLower();
    return result;
}

QSet<QString> supportedRasterSuffixes()
{
    static const QSet<QString> suffixes = [] {
        QSet<QString> result;
        const QList<QByteArray> formats = QImageReader::supportedImageFormats();
        for (const QByteArray &format : formats) {
            const QString suffix = QString::fromLatin1(format).toLower();
            if (suffix == QStringLiteral("psd")) {
                continue;
            }
            result.insert(suffix);
        }
        return result;
    }();

    return suffixes;
}

QVariantMap failureResult(const QString &error)
{
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("error"), error}
    };
}

QVariantMap successResult(const QString &sourceUrl, int width, int height)
{
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("error"), QString()},
        {QStringLiteral("source"), sourceUrl},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height}
    };
}

} // namespace

RasterDocumentIO::RasterDocumentIO(QObject *parent)
    : QObject(parent)
{
}

bool RasterDocumentIO::supportsRasterFile(const QString &fileUrl) const
{
    const InputReference ref = resolveInputReference(fileUrl);
    if (!ref.localFile || ref.localPath.isEmpty()) {
        return false;
    }

    return supportedRasterSuffixes().contains(ref.suffix);
}

QVariantMap RasterDocumentIO::loadRasterDocument(const QString &fileUrl) const
{
    const InputReference ref = resolveInputReference(fileUrl);
    if (!ref.localFile || ref.localPath.isEmpty()) {
        return failureResult(QStringLiteral("Only local raster files are supported."));
    }

    if (!supportsRasterFile(fileUrl)) {
        return failureResult(QStringLiteral("Unsupported raster format."));
    }

    const QFileInfo fileInfo(ref.localPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return failureResult(QStringLiteral("Raster file does not exist."));
    }

    QImageReader reader(fileInfo.absoluteFilePath());
    reader.setAutoTransform(true);

    const QImage image = reader.read();
    if (image.isNull()) {
        const QString error = reader.errorString().isEmpty()
            ? QStringLiteral("Raster file could not be decoded.")
            : reader.errorString();
        return failureResult(error);
    }

    return successResult(QUrl::fromLocalFile(fileInfo.absoluteFilePath()).toString(),
                         image.width(),
                         image.height());
}
