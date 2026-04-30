#pragma once

#include <QColor>

class BrushStrokeBuilder;
class PaintingDocumentModel;

class DrawingStrokeController
{
public:
    DrawingStrokeController(BrushStrokeBuilder *brushBuilder, PaintingDocumentModel *paintingModel);

    void beginStroke(const QString &toolMode,
                     const QColor &brushColor,
                     qreal brushSize,
                     qreal pointX,
                     qreal pointY,
                     qreal rawPressure,
                     bool pressureSensitive,
                     int canvasWidth,
                     int canvasHeight,
                     int maxUndoSteps);
    [[nodiscard]] bool appendStrokePoint(qreal pointX,
                                         qreal pointY,
                                         qreal rawPressure,
                                         bool pressureSensitive);
    void endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);

private:
    BrushStrokeBuilder *m_brushBuilder = nullptr;
    PaintingDocumentModel *m_paintingModel = nullptr;
};
