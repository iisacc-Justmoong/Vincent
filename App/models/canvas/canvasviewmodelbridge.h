#pragma once

#include <QColor>
#include <QPointer>

class QObject;

class CanvasViewModelBridge
{
public:
    void setDocumentViewModel(QObject *documentViewModel);
    [[nodiscard]] QObject *documentViewModel() const;
    [[nodiscard]] bool canMutateDocument() const;

    void syncToolState(QColor &brushColor, qreal &brushSize, QString &toolMode) const;
    void syncCanvasSize(qreal width, qreal height) const;

private:
    QPointer<QObject> m_documentViewModel;
};
