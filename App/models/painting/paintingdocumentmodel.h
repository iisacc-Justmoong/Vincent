#pragma once

#include <QObject>
#include <QImage>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>

class QPainter;
class BrushStrokeBuilder;
class CanvasBackend;

class PaintingDocumentModel : public QObject
{
    Q_OBJECT

public:
    explicit PaintingDocumentModel(QObject *parent = nullptr);

    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] int strokeCount() const;
    [[nodiscard]] QVariantMap currentStroke() const;
    [[nodiscard]] QString backgroundSource() const;
    [[nodiscard]] bool hasBackground() const;
    [[nodiscard]] const QImage &backgroundImage() const;
    [[nodiscard]] QRectF backgroundPlacement() const;

    void pushUndoState(int canvasWidth, int canvasHeight, int maxUndoSteps);
    void clear();
    void setBackground(const QString &sourceUrl, const QImage &image, const QRectF &placement);
    void beginStroke(const QVariantMap &stroke);
    void updateCurrentStroke(const QVariantMap &stroke);
    void endStroke();
    void undo(int canvasWidth, int canvasHeight, int maxUndoSteps);
    void redo(int canvasWidth, int canvasHeight, int maxUndoSteps);
    void render(QPainter *painter, const BrushStrokeBuilder &brushBuilder, qreal fallbackBrushSize) const;
    [[nodiscard]] QImage renderToImage(const QSize &imageSize,
                                       const BrushStrokeBuilder &brushBuilder,
                                       qreal fallbackBrushSize) const;

signals:
    void canUndoChanged();
    void canRedoChanged();
    void strokeCountChanged();
    void backgroundChanged();

private:
    [[nodiscard]] QVariantMap captureSnapshot(int canvasWidth, int canvasHeight) const;
    void applySnapshot(const QVariantMap &snapshot);
    [[nodiscard]] static QVariant cloneDeep(const QVariant &value);
    [[nodiscard]] static QVariantMap cloneMap(const QVariantMap &value);
    [[nodiscard]] static QVariantList cloneList(const QVariantList &value);

    CanvasBackend *m_canvasBackend;
    QVariantList m_strokes;
    QVariantMap m_currentStroke;
    QString m_backgroundSource;
    QRectF m_backgroundPlacement;
    QImage m_backgroundImage;
};
