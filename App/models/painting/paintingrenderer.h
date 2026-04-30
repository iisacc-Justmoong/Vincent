#pragma once

#include <QImage>
#include <QRectF>
#include <QSize>
#include <QVariantList>

class QPainter;
class BrushStrokeBuilder;

class PaintingRenderer
{
public:
    void render(QPainter *painter,
                const QVariantList &strokes,
                const QImage &backgroundImage,
                const QRectF &backgroundPlacement,
                const BrushStrokeBuilder &brushBuilder,
                qreal fallbackBrushSize) const;
    [[nodiscard]] QImage renderToImage(const QSize &imageSize,
                                       const QVariantList &strokes,
                                       const QImage &backgroundImage,
                                       const QRectF &backgroundPlacement,
                                       const BrushStrokeBuilder &brushBuilder,
                                       qreal fallbackBrushSize) const;
};
