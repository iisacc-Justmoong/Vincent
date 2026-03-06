#include "canvasbackend.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {

QVariantMap rectMap(qreal x, qreal y, qreal width, qreal height)
{
    return {
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height}
    };
}

} // namespace

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
                                           const QVariantList &images,
                                           int selectedImageId) const
{
    return {
        {QStringLiteral("canvasWidth"), qMax(1, canvasWidth)},
        {QStringLiteral("canvasHeight"), qMax(1, canvasHeight)},
        {QStringLiteral("strokes"), cloneList(strokes)},
        {QStringLiteral("images"), cloneList(images)},
        {QStringLiteral("selectedImageId"), selectedImageId}
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

QVariantMap CanvasBackend::resetImagePlacement(int canvasWidth,
                                               int canvasHeight,
                                               int originalWidth,
                                               int originalHeight) const
{
    if (originalWidth <= 0 || originalHeight <= 0) {
        return {};
    }

    const qreal fitScale = std::min<qreal>({1.0,
                                            static_cast<qreal>(qMax(1, canvasWidth)) / originalWidth,
                                            static_cast<qreal>(qMax(1, canvasHeight)) / originalHeight});
    const qreal width = originalWidth * fitScale;
    const qreal height = originalHeight * fitScale;

    return {
        {QStringLiteral("scaleX"), fitScale},
        {QStringLiteral("scaleY"), fitScale},
        {QStringLiteral("x"), (qMax(1, canvasWidth) - width) / 2.0},
        {QStringLiteral("y"), (qMax(1, canvasHeight) - height) / 2.0},
        {QStringLiteral("ready"), true}
    };
}

QVariantMap CanvasBackend::resolveTransform(const QString &role,
                                            qreal dx,
                                            qreal dy,
                                            const QVariantMap &startRect,
                                            qreal minSize,
                                            int canvasWidth,
                                            int canvasHeight,
                                            bool constrainAspect) const
{
    const qreal startLeft = startRect.value(QStringLiteral("x")).toReal();
    const qreal startTop = startRect.value(QStringLiteral("y")).toReal();
    const qreal startWidth = startRect.value(QStringLiteral("w"),
                                             startRect.value(QStringLiteral("width"))).toReal();
    const qreal startHeight = startRect.value(QStringLiteral("h"),
                                              startRect.value(QStringLiteral("height"))).toReal();
    const qreal startRight = startLeft + startWidth;
    const qreal startBottom = startTop + startHeight;

    qreal newLeft = startLeft;
    qreal newTop = startTop;
    qreal newRight = startRight;
    qreal newBottom = startBottom;

    if (role == QLatin1String("topLeft")) {
        newLeft = startLeft + dx;
        newTop = startTop + dy;
    } else if (role == QLatin1String("top")) {
        newTop = startTop + dy;
    } else if (role == QLatin1String("topRight")) {
        newRight = startRight + dx;
        newTop = startTop + dy;
    } else if (role == QLatin1String("right")) {
        newRight = startRight + dx;
    } else if (role == QLatin1String("bottomRight")) {
        newRight = startRight + dx;
        newBottom = startBottom + dy;
    } else if (role == QLatin1String("bottom")) {
        newBottom = startBottom + dy;
    } else if (role == QLatin1String("bottomLeft")) {
        newLeft = startLeft + dx;
        newBottom = startBottom + dy;
    } else if (role == QLatin1String("left")) {
        newLeft = startLeft + dx;
    }

    const qreal minWidth = std::max(minSize, 8.0);
    const qreal minHeight = std::max(minSize, 8.0);
    const qreal maxWidth = std::max(minWidth, static_cast<qreal>(qMax(1, canvasWidth)) * 4.0);
    const qreal maxHeight = std::max(minHeight, static_cast<qreal>(qMax(1, canvasHeight)) * 4.0);

    qreal width = newRight - newLeft;
    if (width < minWidth) {
        if (role == QLatin1String("left") || role == QLatin1String("topLeft")
            || role == QLatin1String("bottomLeft")) {
            newLeft = newRight - minWidth;
        } else {
            newRight = newLeft + minWidth;
        }
    } else if (width > maxWidth) {
        if (role == QLatin1String("left") || role == QLatin1String("topLeft")
            || role == QLatin1String("bottomLeft")) {
            newLeft = newRight - maxWidth;
        } else {
            newRight = newLeft + maxWidth;
        }
    }

    qreal height = newBottom - newTop;
    if (height < minHeight) {
        if (role == QLatin1String("top") || role == QLatin1String("topLeft")
            || role == QLatin1String("topRight")) {
            newTop = newBottom - minHeight;
        } else {
            newBottom = newTop + minHeight;
        }
    } else if (height > maxHeight) {
        if (role == QLatin1String("top") || role == QLatin1String("topLeft")
            || role == QLatin1String("topRight")) {
            newTop = newBottom - maxHeight;
        } else {
            newBottom = newTop + maxHeight;
        }
    }

    if (constrainAspect && startWidth > 0 && startHeight > 0) {
        const qreal centerX = startLeft + startWidth / 2.0;
        const qreal centerY = startTop + startHeight / 2.0;
        const qreal minScale = std::max(minWidth / startWidth, minHeight / startHeight);
        const qreal maxScale = std::min(maxWidth / startWidth, maxHeight / startHeight);

        qreal scaleCandidate = 1.0;
        if (role == QLatin1String("left") || role == QLatin1String("right")) {
            scaleCandidate = (newRight - newLeft) / startWidth;
        } else if (role == QLatin1String("top") || role == QLatin1String("bottom")) {
            scaleCandidate = (newBottom - newTop) / startHeight;
        } else {
            const qreal scaleX = (newRight - newLeft) / startWidth;
            const qreal scaleY = (newBottom - newTop) / startHeight;
            scaleCandidate = std::max(std::abs(scaleX), std::abs(scaleY));
        }

        scaleCandidate = std::clamp(std::abs(scaleCandidate), minScale, maxScale);
        const qreal constrainedWidth = startWidth * scaleCandidate;
        const qreal constrainedHeight = startHeight * scaleCandidate;

        if (role == QLatin1String("topLeft")) {
            newRight = startRight;
            newBottom = startBottom;
            newLeft = newRight - constrainedWidth;
            newTop = newBottom - constrainedHeight;
        } else if (role == QLatin1String("topRight")) {
            newLeft = startLeft;
            newBottom = startBottom;
            newRight = newLeft + constrainedWidth;
            newTop = newBottom - constrainedHeight;
        } else if (role == QLatin1String("bottomRight")) {
            newLeft = startLeft;
            newTop = startTop;
            newRight = newLeft + constrainedWidth;
            newBottom = newTop + constrainedHeight;
        } else if (role == QLatin1String("bottomLeft")) {
            newRight = startRight;
            newTop = startTop;
            newLeft = newRight - constrainedWidth;
            newBottom = newTop + constrainedHeight;
        } else if (role == QLatin1String("left")) {
            newRight = startRight;
            newLeft = newRight - constrainedWidth;
            newTop = centerY - constrainedHeight / 2.0;
            newBottom = centerY + constrainedHeight / 2.0;
        } else if (role == QLatin1String("right")) {
            newLeft = startLeft;
            newRight = newLeft + constrainedWidth;
            newTop = centerY - constrainedHeight / 2.0;
            newBottom = centerY + constrainedHeight / 2.0;
        } else if (role == QLatin1String("top")) {
            newBottom = startBottom;
            newTop = newBottom - constrainedHeight;
            newLeft = centerX - constrainedWidth / 2.0;
            newRight = centerX + constrainedWidth / 2.0;
        } else if (role == QLatin1String("bottom")) {
            newTop = startTop;
            newBottom = newTop + constrainedHeight;
            newLeft = centerX - constrainedWidth / 2.0;
            newRight = centerX + constrainedWidth / 2.0;
        }
    }

    return rectMap(newLeft, newTop, newRight - newLeft, newBottom - newTop);
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
