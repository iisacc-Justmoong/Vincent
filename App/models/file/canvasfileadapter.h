#pragma once

#include <QImage>
#include <QString>

class RasterDocumentIO;

struct RasterLoadResult
{
    bool ok = false;
    QString sourceUrl;
    int width = 0;
    int height = 0;
    QImage image;
};

class CanvasFileAdapter
{
public:
    CanvasFileAdapter();
    ~CanvasFileAdapter();

    [[nodiscard]] RasterLoadResult openRaster(const QString &fileUrl) const;
    [[nodiscard]] bool saveImage(const QString &fileUrl, const QImage &image) const;
    [[nodiscard]] QString toLocalPath(const QString &fileUrl) const;

private:
    RasterDocumentIO *m_rasterDocumentIO;
};
