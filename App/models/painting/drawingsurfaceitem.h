#pragma once

#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtAdapter/CanvasAdapter.h>

class CanvasViewModelBridge;
class QEvent;
class QMouseEvent;
class QObject;

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
    Q_INVOKABLE bool openRaster(const QString &fileUrl,
                                qreal maximumCanvasWidth = 0,
                                qreal maximumCanvasHeight = 0);
    Q_INVOKABLE QVariantMap imageObjectForFile(const QString &fileUrl,
                                               qreal maximumObjectWidth = 0,
                                               qreal maximumObjectHeight = 0) const;
    Q_INVOKABLE QVariantMap psdImportDocument(const QString &fileUrl) const;
    Q_INVOKABLE bool saveToFile(const QString &fileUrl);
    Q_INVOKABLE bool saveToFileWithObjects(const QString &fileUrl, const QVariantList &objects);
    Q_INVOKABLE bool saveToFileWithObjectsAndRasterLayers(const QString &fileUrl,
                                                          const QVariantList &objects,
                                                          const QVariantList &rasterLayers,
                                                          bool includeBackgroundLayer = true);
    Q_INVOKABLE QString cacheRasterSnapshotSource();
    Q_INVOKABLE QString cacheRasterThumbnailSource(qreal maximumWidth = 32, qreal maximumHeight = 32);
    Q_INVOKABLE QString cacheGrabbedThumbnailSource(QObject *grabResult);
    Q_INVOKABLE QString cacheDrawableObjectThumbnailSource(const QVariantMap &object,
                                                           qreal maximumWidth = 32,
                                                           qreal maximumHeight = 32) const;
    Q_INVOKABLE bool restoreRasterSnapshot(const QString &fileUrl);
    Q_INVOKABLE QVariantMap psdCompatibilityManifest(const QVariantList &objects,
                                                     bool includeBackgroundLayer = true) const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void resizeCanvasSurface(qreal canvasWidth, qreal canvasHeight);
    Q_INVOKABLE void beginStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);
    Q_INVOKABLE bool appendStrokePoint(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);
    Q_INVOKABLE void endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);
    Q_INVOKABLE bool commitText(qreal pointX,
                                qreal pointY,
                                qreal boxWidth,
                                const QString &text,
                                qreal fontPixelSize,
                                const QColor &color);
    Q_INVOKABLE bool commitShape(qreal pointX,
                                 qreal pointY,
                                 qreal boxWidth,
                                 qreal boxHeight,
                                 const QString &shapeKind,
                                 const QColor &color);
    Q_INVOKABLE bool fillAt(qreal pointX, qreal pointY, const QColor &color);

signals:
    void documentViewModelChanged();
    void viewIdChanged();
    void backgroundChanged();
    void canUndoChanged();
    void canRedoChanged();
    void rasterContentChanged();

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    [[nodiscard]] QSize canvasSize() const;
    [[nodiscard]] bool canMutateDocument() const;
    [[nodiscard]] bool isTextToolActive() const;
    [[nodiscard]] bool isShapeToolActive() const;
    [[nodiscard]] bool isFillToolActive() const;
    [[nodiscard]] bool isPanToolActive() const;
    [[nodiscard]] bool isMoveToolActive() const;
    [[nodiscard]] bool isZoomToolActive() const;
    [[nodiscard]] bool isOverlayToolActive() const;
    [[nodiscard]] QImage currentRasterCanvasImage(const QSize &targetSize);
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
    bool m_isApplyingCanvasSurfaceSize = false;
};
