#include "canvasviewmodelbridge.h"

#include <QObject>
#include <QVariant>
#include <QtGlobal>

namespace {

void readColorProperty(const QObject *source, const char *propertyName, QColor &target)
{
    const QVariant value = source->property(propertyName);
    if (value.isValid()) {
        target = value.value<QColor>();
    }
}

void readBoundedRealProperty(const QObject *source,
                             const char *propertyName,
                             qreal &target,
                             qreal minimum,
                             qreal maximum)
{
    const QVariant value = source->property(propertyName);
    if (value.isValid()) {
        target = qBound(minimum, value.toReal(), maximum);
    }
}

void readMinimumRealProperty(const QObject *source,
                             const char *propertyName,
                             qreal &target,
                             qreal minimum)
{
    const QVariant value = source->property(propertyName);
    if (value.isValid()) {
        target = qMax(minimum, value.toReal());
    }
}

} // namespace

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

void CanvasViewModelBridge::syncToolState(CanvasBrushConfig &brushConfig, QString &toolMode) const
{
    if (!m_documentViewModel) {
        return;
    }

    readColorProperty(m_documentViewModel, "brushColor", brushConfig.color);
    readBoundedRealProperty(m_documentViewModel, "brushSize", brushConfig.size, 1.0, 48.0);
    readBoundedRealProperty(m_documentViewModel, "brushFlow", brushConfig.flow, 0.0, 1.0);
    readBoundedRealProperty(m_documentViewModel, "brushOpacity", brushConfig.opacity, 0.0, 1.0);
    readBoundedRealProperty(m_documentViewModel, "brushHardness", brushConfig.hardness, 0.01, 1.0);
    readMinimumRealProperty(m_documentViewModel, "brushSpacing", brushConfig.spacing, 0.0);
    readBoundedRealProperty(m_documentViewModel, "brushSpacingRatio", brushConfig.spacingRatio, 0.0, 1.0);
    readBoundedRealProperty(m_documentViewModel, "pressureCurveMinimum", brushConfig.pressureCurveMinimum, 0.0, 1.0);
    readBoundedRealProperty(m_documentViewModel, "pressureCurveCenter", brushConfig.pressureCurveCenter, 0.0, 1.0);
    readBoundedRealProperty(m_documentViewModel, "pressureCurveMaximum", brushConfig.pressureCurveMaximum, 0.0, 1.0);
    readBoundedRealProperty(m_documentViewModel, "stabilizerStrength", brushConfig.stabilizerStrength, 0.0, 1.0);
    brushConfig.flowEnabled = true;
    brushConfig.opacityEnabled = true;
    brushConfig.hardnessEnabled = true;
    brushConfig.spacingEnabled = true;

    const QVariant toolModeValue = m_documentViewModel->property("toolMode");
    if (toolModeValue.isValid()) {
        const QString nextTool = toolModeValue.toString();
        toolMode = nextTool == QStringLiteral("eraser")
                || nextTool == QStringLiteral("move")
                || nextTool == QStringLiteral("fill")
                || nextTool == QStringLiteral("text")
                || nextTool == QStringLiteral("shape")
            ? nextTool
            : QStringLiteral("brush");
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
