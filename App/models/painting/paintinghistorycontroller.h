#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class CanvasBackend;

class PaintingHistoryController : public QObject
{
    Q_OBJECT

public:
    explicit PaintingHistoryController(QObject *parent = nullptr);

    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    void pushUndoState(int canvasWidth,
                       int canvasHeight,
                       const QVariantList &strokes,
                       const QVariantMap &background,
                       int maxUndoSteps);
    [[nodiscard]] QVariantMap undo(int canvasWidth,
                                   int canvasHeight,
                                   const QVariantList &strokes,
                                   const QVariantMap &background,
                                   int maxUndoSteps);
    [[nodiscard]] QVariantMap redo(int canvasWidth,
                                   int canvasHeight,
                                   const QVariantList &strokes,
                                   const QVariantMap &background,
                                   int maxUndoSteps);

signals:
    void canUndoChanged();
    void canRedoChanged();

private:
    CanvasBackend *m_canvasBackend = nullptr;
};
