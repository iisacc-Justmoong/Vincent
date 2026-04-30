#include "drawingstrokecontroller.h"

#include "../brush/brushstrokebuilder.h"
#include "paintingdocumentmodel.h"

DrawingStrokeController::DrawingStrokeController(BrushStrokeBuilder *brushBuilder,
                                                 PaintingDocumentModel *paintingModel)
    : m_brushBuilder(brushBuilder)
    , m_paintingModel(paintingModel)
{
}

void DrawingStrokeController::beginStroke(const QString &toolMode,
                                          const QColor &brushColor,
                                          qreal brushSize,
                                          qreal pointX,
                                          qreal pointY,
                                          qreal rawPressure,
                                          bool pressureSensitive,
                                          int canvasWidth,
                                          int canvasHeight,
                                          int maxUndoSteps)
{
    if (!m_brushBuilder || !m_paintingModel) {
        return;
    }

    m_paintingModel->pushUndoState(canvasWidth, canvasHeight, maxUndoSteps);
    m_paintingModel->beginStroke(m_brushBuilder->beginStroke(toolMode,
                                                             brushColor,
                                                             brushSize,
                                                             pointX,
                                                             pointY,
                                                             rawPressure,
                                                             pressureSensitive));
}

bool DrawingStrokeController::appendStrokePoint(qreal pointX,
                                                qreal pointY,
                                                qreal rawPressure,
                                                bool pressureSensitive)
{
    if (!m_brushBuilder || !m_paintingModel) {
        return false;
    }

    QVariantMap stroke = m_paintingModel->currentStroke();
    if (stroke.isEmpty()) {
        return false;
    }
    if (!m_brushBuilder->appendPoint(stroke, pointX, pointY, rawPressure, pressureSensitive)) {
        return false;
    }
    m_paintingModel->updateCurrentStroke(stroke);
    return true;
}

void DrawingStrokeController::endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    const bool appended = appendStrokePoint(pointX, pointY, rawPressure, pressureSensitive);
    Q_UNUSED(appended)
    if (m_paintingModel) {
        m_paintingModel->endStroke();
    }
}
