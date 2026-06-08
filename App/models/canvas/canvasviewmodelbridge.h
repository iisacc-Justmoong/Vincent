#pragma once

#include <QColor>
#include <QPointer>
#include <QString>
#include <QtAdapter/CanvasBrushConfig.h>

class QObject;

class CanvasViewModelBridge
{
public:
    void setDocumentViewModel(QObject *documentViewModel);
    [[nodiscard]] QObject *documentViewModel() const;
    [[nodiscard]] bool canMutateDocument() const;

    void syncToolState(CanvasBrushConfig &brushConfig, QString &toolMode) const;
    void syncCanvasSize(qreal width, qreal height) const;

private:
    QPointer<QObject> m_documentViewModel;
};
