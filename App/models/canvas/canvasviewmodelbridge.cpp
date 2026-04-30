#include "canvasviewmodelbridge.h"

#include <QObject>
#include <QVariant>
#include <QtGlobal>

void CanvasViewModelBridge::setDocumentViewModel(QObject *documentViewModel)
{
    m_documentViewModel = documentViewModel;
}

QObject *CanvasViewModelBridge::documentViewModel() const
{
    return m_documentViewModel;
}

bool CanvasViewModelBridge::canMutateDocument() const
{
    return !m_documentViewModel.isNull();
}

void CanvasViewModelBridge::syncToolState(QColor &brushColor, qreal &brushSize, QString &toolMode) const
{
    if (!m_documentViewModel) {
        return;
    }

    const QVariant brushColorValue = m_documentViewModel->property("brushColor");
    if (brushColorValue.isValid()) {
        brushColor = brushColorValue.value<QColor>();
    }

    const QVariant brushSizeValue = m_documentViewModel->property("brushSize");
    if (brushSizeValue.isValid()) {
        brushSize = qBound<qreal>(1.0, brushSizeValue.toReal(), 48.0);
    }

    const QVariant toolModeValue = m_documentViewModel->property("toolMode");
    if (toolModeValue.isValid()) {
        const QString nextTool = toolModeValue.toString();
        toolMode = nextTool == QStringLiteral("eraser") ? QStringLiteral("eraser") : QStringLiteral("brush");
    }
}

void CanvasViewModelBridge::syncCanvasSize(qreal width, qreal height) const
{
    if (!m_documentViewModel) {
        return;
    }

    m_documentViewModel->setProperty("canvasWidth", qMax(1, qRound(width)));
    m_documentViewModel->setProperty("canvasHeight", qMax(1, qRound(height)));
}
