#pragma once

#include <QSize>
#include <QString>
#include <QtAdapter/CanvasAdapter.h>

class CanvasViewModelBridge;
class QEvent;
class QMouseEvent;

class DrawingSurfaceItem : public CanvasAdapter
{
    Q_OBJECT
    Q_PROPERTY(QObject *documentViewModel READ documentViewModel WRITE setDocumentViewModel NOTIFY documentViewModelChanged)
    Q_PROPERTY(QString viewId READ viewId WRITE setViewId NOTIFY viewIdChanged)
    Q_PROPERTY(QString backgroundSource READ backgroundSource NOTIFY backgroundChanged)
    Q_PROPERTY(bool hasBackground READ hasBackground NOTIFY backgroundChanged)

public:
    explicit DrawingSurfaceItem(QQuickItem *parent = nullptr);
    ~DrawingSurfaceItem() override;

    [[nodiscard]] QObject *documentViewModel() const;
    [[nodiscard]] QString viewId() const;
    [[nodiscard]] QString backgroundSource() const;
    [[nodiscard]] bool hasBackground() const;

    void setDocumentViewModel(QObject *documentViewModel);
    void setViewId(const QString &viewId);

    Q_INVOKABLE void newCanvas();
    Q_INVOKABLE void clearCanvas();
    Q_INVOKABLE bool openRaster(const QString &fileUrl);
    Q_INVOKABLE bool saveToFile(const QString &fileUrl);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void beginStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);
    Q_INVOKABLE bool appendStrokePoint(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);
    Q_INVOKABLE void endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);

signals:
    void documentViewModelChanged();
    void viewIdChanged();
    void backgroundChanged();
    void canUndoChanged();
    void canRedoChanged();

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    [[nodiscard]] QSize canvasSize() const;
    [[nodiscard]] bool canMutateDocument() const;
    void syncCanvasSize();
    void emitUndoRedoSignals();
    QMouseEvent makeMouseEvent(QEvent::Type eventType,
                               qreal pointX,
                               qreal pointY,
                               Qt::MouseButton button,
                               Qt::MouseButtons buttons) const;

    CanvasViewModelBridge *m_viewModelBridge = nullptr;
    QString m_viewId;
    QString m_backgroundSource;
    bool m_hasBackground = false;
};
