#pragma once

#include <QSize>

class BrushStrokeBuilder;
class CanvasFileAdapter;
class PaintingDocumentModel;

class DrawingDocumentController
{
public:
    DrawingDocumentController(PaintingDocumentModel *paintingModel,
                              CanvasFileAdapter *fileAdapter,
                              BrushStrokeBuilder *brushBuilder);

    void newCanvas(const QSize &canvasSize, int maxUndoSteps);
    void clearCanvas(const QSize &canvasSize, int maxUndoSteps);
    [[nodiscard]] bool openRaster(const QString &fileUrl, const QSize &canvasSize, int maxUndoSteps);
    [[nodiscard]] bool saveToFile(const QString &fileUrl, const QSize &canvasSize, qreal brushSize) const;
    void undo(const QSize &canvasSize, int maxUndoSteps);
    void redo(const QSize &canvasSize, int maxUndoSteps);

private:
    PaintingDocumentModel *m_paintingModel = nullptr;
    CanvasFileAdapter *m_fileAdapter = nullptr;
    BrushStrokeBuilder *m_brushBuilder = nullptr;
};
