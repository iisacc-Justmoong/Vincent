#include "drawingdocumentcontroller.h"

#include "paintingdocumentmodel.h"
#include "../brush/brushstrokebuilder.h"
#include "../file/canvasfileadapter.h"
#include "../../canvasbackend.h"

#include <QRectF>

DrawingDocumentController::DrawingDocumentController(PaintingDocumentModel *paintingModel,
                                                     CanvasFileAdapter *fileAdapter,
                                                     BrushStrokeBuilder *brushBuilder)
    : m_paintingModel(paintingModel)
    , m_fileAdapter(fileAdapter)
    , m_brushBuilder(brushBuilder)
{
}

void DrawingDocumentController::newCanvas(const QSize &canvasSize, int maxUndoSteps)
{
    if (!m_paintingModel) {
        return;
    }
    m_paintingModel->pushUndoState(canvasSize.width(), canvasSize.height(), maxUndoSteps);
    m_paintingModel->clear();
}

void DrawingDocumentController::clearCanvas(const QSize &canvasSize, int maxUndoSteps)
{
    if (!m_paintingModel) {
        return;
    }
    m_paintingModel->pushUndoState(canvasSize.width(), canvasSize.height(), maxUndoSteps);
    m_paintingModel->clear();
}

bool DrawingDocumentController::openRaster(const QString &fileUrl, const QSize &canvasSize, int maxUndoSteps)
{
    if (!m_paintingModel || !m_fileAdapter) {
        return false;
    }

    const RasterLoadResult raster = m_fileAdapter->openRaster(fileUrl);
    if (!raster.ok) {
        return false;
    }

    m_paintingModel->pushUndoState(canvasSize.width(), canvasSize.height(), maxUndoSteps);
    const QVariantMap fit = CanvasBackend().documentFitTransform(canvasSize.width(),
                                                                 canvasSize.height(),
                                                                 raster.width,
                                                                 raster.height);
    m_paintingModel->setBackground(raster.sourceUrl,
                                   raster.image,
                                   QRectF(fit.value(QStringLiteral("offsetX")).toReal(),
                                          fit.value(QStringLiteral("offsetY")).toReal(),
                                          raster.width * fit.value(QStringLiteral("scale")).toReal(),
                                          raster.height * fit.value(QStringLiteral("scale")).toReal()));
    return true;
}

bool DrawingDocumentController::saveToFile(const QString &fileUrl, const QSize &canvasSize, qreal brushSize) const
{
    return m_fileAdapter
        && m_paintingModel
        && m_brushBuilder
        && m_fileAdapter->saveImage(fileUrl,
                                    m_paintingModel->renderToImage(canvasSize,
                                                                   *m_brushBuilder,
                                                                   brushSize));
}

void DrawingDocumentController::undo(const QSize &canvasSize, int maxUndoSteps)
{
    if (!m_paintingModel) {
        return;
    }
    m_paintingModel->undo(canvasSize.width(), canvasSize.height(), maxUndoSteps);
}

void DrawingDocumentController::redo(const QSize &canvasSize, int maxUndoSteps)
{
    if (!m_paintingModel) {
        return;
    }
    m_paintingModel->redo(canvasSize.width(), canvasSize.height(), maxUndoSteps);
}
