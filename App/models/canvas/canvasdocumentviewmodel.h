#pragma once

#include <QObject>
#include <QColor>
#include <QVariantList>

class PaletteUtils;

class CanvasDocumentViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList palette READ palette NOTIFY paletteChanged)
    Q_PROPERTY(QColor brushColor READ brushColor WRITE setBrushColor NOTIFY brushColorChanged)
    Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY brushSizeChanged)
    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth WRITE setCanvasWidth NOTIFY canvasWidthChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight WRITE setCanvasHeight NOTIFY canvasHeightChanged)

public:
    explicit CanvasDocumentViewModel(PaletteUtils *paletteUtils, QObject *parent = nullptr);

    [[nodiscard]] QVariantList palette() const;
    [[nodiscard]] QColor brushColor() const;
    [[nodiscard]] qreal brushSize() const;
    [[nodiscard]] QString toolMode() const;
    [[nodiscard]] int canvasWidth() const;
    [[nodiscard]] int canvasHeight() const;

    void setBrushColor(const QColor &brushColor);
    void setBrushSize(qreal brushSize);
    void setToolMode(const QString &toolMode);
    void setCanvasWidth(int canvasWidth);
    void setCanvasHeight(int canvasHeight);

signals:
    void paletteChanged();
    void brushColorChanged();
    void brushSizeChanged();
    void toolModeChanged();
    void canvasWidthChanged();
    void canvasHeightChanged();

private:
    QVariantList buildDefaultPalette(PaletteUtils *paletteUtils) const;
    QVariantList m_palette;
    QColor m_brushColor;
    qreal m_brushSize = 2.0;
    QString m_toolMode = QStringLiteral("brush");
    int m_canvasWidth = 1;
    int m_canvasHeight = 1;
};
