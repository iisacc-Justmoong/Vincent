#include "paintingrenderer.h"

#include "../brush/brushstrokebuilder.h"

#include <QPainter>

void PaintingRenderer::render(QPainter *painter,
                              const QVariantList &strokes,
                              const QImage &backgroundImage,
                              const QRectF &backgroundPlacement,
                              const BrushStrokeBuilder &brushBuilder,
                              qreal fallbackBrushSize) const
{
    painter->fillRect(QRectF(QPointF(0, 0), QSizeF(painter->device()->width(), painter->device()->height())), Qt::white);
    if (!backgroundImage.isNull() && !backgroundPlacement.isEmpty()) {
        painter->drawImage(backgroundPlacement, backgroundImage);
    }
    for (const QVariant &strokeVar : strokes) {
        brushBuilder.drawStroke(painter, strokeVar.toMap(), fallbackBrushSize);
    }
}

QImage PaintingRenderer::renderToImage(const QSize &imageSize,
                                       const QVariantList &strokes,
                                       const QImage &backgroundImage,
                                       const QRectF &backgroundPlacement,
                                       const BrushStrokeBuilder &brushBuilder,
                                       qreal fallbackBrushSize) const
{
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (!backgroundImage.isNull() && !backgroundPlacement.isEmpty()) {
        painter.drawImage(backgroundPlacement, backgroundImage);
    }
    for (const QVariant &strokeVar : strokes) {
        brushBuilder.drawStroke(&painter, strokeVar.toMap(), fallbackBrushSize);
    }
    painter.end();
    return image;
}
