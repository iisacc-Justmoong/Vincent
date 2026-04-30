#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>

class BrushStrokeBuilder;
class CanvasFileAdapter;
class PaintingDocumentModel;

class DrawingSurfaceItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor brushColor READ brushColor WRITE setBrushColor NOTIFY brushColorChanged)
    Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY brushSizeChanged)
    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
    Q_PROPERTY(QObject *documentViewModel READ documentViewModel WRITE setDocumentViewModel NOTIFY documentViewModelChanged)
    Q_PROPERTY(QString viewId READ viewId WRITE setViewId NOTIFY viewIdChanged)
    Q_PROPERTY(QString backgroundSource READ backgroundSource NOTIFY backgroundChanged)
    Q_PROPERTY(bool hasBackground READ hasBackground NOTIFY backgroundChanged)
    Q_PROPERTY(int strokeCount READ strokeCount NOTIFY strokeCountChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY canRedoChanged)

public:
    explicit DrawingSurfaceItem(QQuickItem *parent = nullptr);
    ~DrawingSurfaceItem() override;

    void paint(QPainter *painter) override;

    [[nodiscard]] QColor brushColor() const;
    [[nodiscard]] qreal brushSize() const;
    [[nodiscard]] QString toolMode() const;
    [[nodiscard]] QObject *documentViewModel() const;
    [[nodiscard]] QString viewId() const;
    [[nodiscard]] QString backgroundSource() const;
    [[nodiscard]] bool hasBackground() const;
    [[nodiscard]] int strokeCount() const;
    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    void setBrushColor(const QColor &brushColor);
    void setBrushSize(qreal brushSize);
    void setToolMode(const QString &toolMode);
    void setDocumentViewModel(QObject *documentViewModel);
    void setViewId(const QString &viewId);

    Q_INVOKABLE void newCanvas();
    Q_INVOKABLE void clearCanvas();
    Q_INVOKABLE bool openRaster(const QString &fileUrl);
    Q_INVOKABLE bool saveToFile(const QString &fileUrl) const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void beginStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);
    Q_INVOKABLE bool appendStrokePoint(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);
    Q_INVOKABLE void endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive);

signals:
    void brushColorChanged();
    void brushSizeChanged();
    void toolModeChanged();
    void documentViewModelChanged();
    void viewIdChanged();
    void backgroundChanged();
    void strokeCountChanged();
    void canUndoChanged();
    void canRedoChanged();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    [[nodiscard]] bool canMutateDocument() const;
    void syncWithDocumentViewModel();
    void syncDocumentCanvasSize();

    BrushStrokeBuilder *m_brushBuilder;
    CanvasFileAdapter *m_fileAdapter;
    PaintingDocumentModel *m_paintingModel;
    QColor m_brushColor = QColor(QStringLiteral("#1a1a1a"));
    qreal m_brushSize = 2.0;
    QString m_toolMode = QStringLiteral("brush");
    QPointer<QObject> m_documentViewModel;
    QString m_viewId;
};
