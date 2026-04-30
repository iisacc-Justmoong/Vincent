#include "paintinghistorycontroller.h"

#include "../canvas/canvasbackend.h"

PaintingHistoryController::PaintingHistoryController(QObject *parent)
    : QObject(parent)
    , m_canvasBackend(new CanvasBackend(this))
{
    connect(m_canvasBackend, &CanvasBackend::canUndoChanged, this, &PaintingHistoryController::canUndoChanged);
    connect(m_canvasBackend, &CanvasBackend::canRedoChanged, this, &PaintingHistoryController::canRedoChanged);
}

bool PaintingHistoryController::canUndo() const
{
    return m_canvasBackend->canUndo();
}

bool PaintingHistoryController::canRedo() const
{
    return m_canvasBackend->canRedo();
}

void PaintingHistoryController::pushUndoState(int canvasWidth,
                                              int canvasHeight,
                                              const QVariantList &strokes,
                                              const QVariantMap &background,
                                              int maxUndoSteps)
{
    m_canvasBackend->pushUndoState(m_canvasBackend->captureSnapshot(canvasWidth,
                                                                    canvasHeight,
                                                                    strokes,
                                                                    background),
                                   maxUndoSteps);
}

QVariantMap PaintingHistoryController::undo(int canvasWidth,
                                            int canvasHeight,
                                            const QVariantList &strokes,
                                            const QVariantMap &background,
                                            int maxUndoSteps)
{
    return m_canvasBackend->undo(m_canvasBackend->captureSnapshot(canvasWidth,
                                                                  canvasHeight,
                                                                  strokes,
                                                                  background),
                                 maxUndoSteps);
}

QVariantMap PaintingHistoryController::redo(int canvasWidth,
                                            int canvasHeight,
                                            const QVariantList &strokes,
                                            const QVariantMap &background,
                                            int maxUndoSteps)
{
    return m_canvasBackend->redo(m_canvasBackend->captureSnapshot(canvasWidth,
                                                                  canvasHeight,
                                                                  strokes,
                                                                  background),
                                 maxUndoSteps);
}
