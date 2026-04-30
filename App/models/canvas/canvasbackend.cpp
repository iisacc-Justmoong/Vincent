#include "canvasbackend.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

CanvasBackend::CanvasBackend(QObject *parent)
    : QObject(parent)
{
}

bool CanvasBackend::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool CanvasBackend::canRedo() const
{
    return !m_redoStack.isEmpty();
}

QVariantMap CanvasBackend::captureSnapshot(int canvasWidth,
                                           int canvasHeight,
                                           const QVariantList &strokes,
                                           const QVariantMap &background) const
{
    return {
        {QStringLiteral("canvasWidth"), qMax(1, canvasWidth)},
        {QStringLiteral("canvasHeight"), qMax(1, canvasHeight)},
        {QStringLiteral("strokes"), cloneList(strokes)},
        {QStringLiteral("background"), cloneMap(background)}
    };
}

void CanvasBackend::pushUndoState(const QVariantMap &snapshot, int maxUndoSteps)
{
    const bool previousCanUndo = canUndo();
    const bool previousCanRedo = canRedo();

    m_undoStack.push_back(cloneMap(snapshot));
    trimStack(m_undoStack, maxUndoSteps);
    m_redoStack.clear();

    emitHistorySignals(previousCanUndo, previousCanRedo);
}

QVariantMap CanvasBackend::undo(const QVariantMap &currentSnapshot, int maxUndoSteps)
{
    if (m_undoStack.isEmpty()) {
        return {};
    }

    const bool previousCanUndo = canUndo();
    const bool previousCanRedo = canRedo();

    m_redoStack.push_back(cloneMap(currentSnapshot));
    trimStack(m_redoStack, maxUndoSteps);

    const QVariantMap snapshot = m_undoStack.takeLast();
    emitHistorySignals(previousCanUndo, previousCanRedo);
    return cloneMap(snapshot);
}

QVariantMap CanvasBackend::redo(const QVariantMap &currentSnapshot, int maxUndoSteps)
{
    if (m_redoStack.isEmpty()) {
        return {};
    }

    const bool previousCanUndo = canUndo();
    const bool previousCanRedo = canRedo();

    m_undoStack.push_back(cloneMap(currentSnapshot));
    trimStack(m_undoStack, maxUndoSteps);

    const QVariantMap snapshot = m_redoStack.takeLast();
    emitHistorySignals(previousCanUndo, previousCanRedo);
    return cloneMap(snapshot);
}

void CanvasBackend::clearHistory()
{
    if (m_undoStack.isEmpty() && m_redoStack.isEmpty()) {
        return;
    }

    const bool previousCanUndo = canUndo();
    const bool previousCanRedo = canRedo();

    m_undoStack.clear();
    m_redoStack.clear();

    emitHistorySignals(previousCanUndo, previousCanRedo);
}

QVariantMap CanvasBackend::documentFitTransform(int canvasWidth,
                                                int canvasHeight,
                                                int documentWidth,
                                                int documentHeight) const
{
    const qreal safeCanvasWidth = qMax(1, canvasWidth);
    const qreal safeCanvasHeight = qMax(1, canvasHeight);
    const qreal safeDocumentWidth = qMax(1, documentWidth);
    const qreal safeDocumentHeight = qMax(1, documentHeight);
    const qreal scale = std::min<qreal>({1.0,
                                         safeCanvasWidth / safeDocumentWidth,
                                         safeCanvasHeight / safeDocumentHeight});

    return {
        {QStringLiteral("scale"), scale},
        {QStringLiteral("offsetX"), (safeCanvasWidth - safeDocumentWidth * scale) / 2.0},
        {QStringLiteral("offsetY"), (safeCanvasHeight - safeDocumentHeight * scale) / 2.0}
    };
}

QVariant CanvasBackend::deepClone(const QVariant &value) const
{
    if (!value.isValid() || value.isNull()) {
        return value;
    }
    if (value.metaType().id() == QMetaType::QVariantMap) {
        return cloneMap(value.toMap());
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        return cloneList(value.toList());
    }
    return value;
}

QVariantMap CanvasBackend::cloneMap(const QVariantMap &value) const
{
    QVariantMap clone;
    for (auto it = value.constBegin(); it != value.constEnd(); ++it) {
        clone.insert(it.key(), deepClone(it.value()));
    }
    return clone;
}

QVariantList CanvasBackend::cloneList(const QVariantList &value) const
{
    QVariantList clone;
    clone.reserve(value.size());
    for (const QVariant &entry : value) {
        clone.push_back(deepClone(entry));
    }
    return clone;
}

void CanvasBackend::trimStack(QVector<QVariantMap> &stack, int maxUndoSteps)
{
    const int boundedMax = qMax(1, maxUndoSteps);
    while (stack.size() > boundedMax) {
        stack.removeFirst();
    }
}

void CanvasBackend::emitHistorySignals(bool previousCanUndo, bool previousCanRedo)
{
    if (previousCanUndo != canUndo()) {
        emit canUndoChanged();
    }
    if (previousCanRedo != canRedo()) {
        emit canRedoChanged();
    }
}
