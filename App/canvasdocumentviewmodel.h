#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include <QColor>

#include "layerlistmodel.h"

class PaletteUtils;

class CanvasDocumentViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList palette READ palette NOTIFY paletteChanged)
    Q_PROPERTY(int layerCount READ layerCount NOTIFY layerCountChanged)
    Q_PROPERTY(QColor brushColor READ brushColor WRITE setBrushColor NOTIFY brushColorChanged)
    Q_PROPERTY(qreal brushSize READ brushSize WRITE setBrushSize NOTIFY brushSizeChanged)
    Q_PROPERTY(QString toolMode READ toolMode WRITE setToolMode NOTIFY toolModeChanged)
    Q_PROPERTY(int selectedLayerId READ selectedLayerId WRITE setSelectedLayerId NOTIFY selectedLayerIdChanged)
    Q_PROPERTY(QString selectedLayerName READ selectedLayerName NOTIFY selectedLayerNameChanged)
    Q_PROPERTY(QVariantMap selectedLayerData READ selectedLayerData NOTIFY selectedLayerDataChanged)
    Q_PROPERTY(bool hasImportedLayer READ hasImportedLayer NOTIFY hasImportedLayerChanged)
    Q_PROPERTY(bool hasLayerSelection READ hasLayerSelection NOTIFY hasLayerSelectionChanged)
    Q_PROPERTY(bool freeTransformActive READ freeTransformActive WRITE setFreeTransformActive NOTIFY freeTransformActiveChanged)
    Q_PROPERTY(bool textEntryActive READ textEntryActive WRITE setTextEntryActive NOTIFY textEntryActiveChanged)
    Q_PROPERTY(int canvasWidth READ canvasWidth WRITE setCanvasWidth NOTIFY canvasWidthChanged)
    Q_PROPERTY(int canvasHeight READ canvasHeight WRITE setCanvasHeight NOTIFY canvasHeightChanged)
    Q_PROPERTY(QObject *layerListModel READ layerListModel CONSTANT)

public:
    explicit CanvasDocumentViewModel(PaletteUtils *paletteUtils, QObject *parent = nullptr);

    [[nodiscard]] QVariantList palette() const;
    [[nodiscard]] int layerCount() const;
    [[nodiscard]] QColor brushColor() const;
    [[nodiscard]] qreal brushSize() const;
    [[nodiscard]] QString toolMode() const;
    [[nodiscard]] int selectedLayerId() const;
    [[nodiscard]] QString selectedLayerName() const;
    [[nodiscard]] QVariantMap selectedLayerData() const;
    [[nodiscard]] bool hasImportedLayer() const;
    [[nodiscard]] bool hasLayerSelection() const;
    [[nodiscard]] bool freeTransformActive() const;
    [[nodiscard]] bool textEntryActive() const;
    [[nodiscard]] int canvasWidth() const;
    [[nodiscard]] int canvasHeight() const;
    [[nodiscard]] QObject *layerListModel();

    void setBrushColor(const QColor &brushColor);
    void setBrushSize(qreal brushSize);
    void setToolMode(const QString &toolMode);
    void setSelectedLayerId(int selectedLayerId);
    void setFreeTransformActive(bool freeTransformActive);
    void setTextEntryActive(bool textEntryActive);
    void setCanvasWidth(int canvasWidth);
    void setCanvasHeight(int canvasHeight);

    Q_INVOKABLE void resetDocument();
    Q_INVOKABLE int appendLayer(const QVariantMap &layerData);
    Q_INVOKABLE bool updateLayerPropertyById(int imageId, const QString &property, const QVariant &value);
    Q_INVOKABLE bool updateLayerById(int imageId, const QVariantMap &changes);
    Q_INVOKABLE bool moveLayer(int from, int to, int count = 1);
    Q_INVOKABLE bool removeLayerById(int imageId);
    Q_INVOKABLE void clearLayers();
    Q_INVOKABLE int findLayerIndexById(int imageId) const;
    Q_INVOKABLE QVariantMap layerAt(int index) const;
    Q_INVOKABLE QVariantMap layerById(int imageId) const;
    Q_INVOKABLE QVariantMap selectedLayer() const;
    Q_INVOKABLE QVariantList exportLayers() const;
    Q_INVOKABLE void importLayers(const QVariantList &layers);
    Q_INVOKABLE void setLayerVisibility(int imageId, bool visible);

signals:
    void paletteChanged();
    void layerCountChanged();
    void brushColorChanged();
    void brushSizeChanged();
    void toolModeChanged();
    void selectedLayerIdChanged();
    void selectedLayerNameChanged();
    void selectedLayerDataChanged();
    void hasImportedLayerChanged();
    void hasLayerSelectionChanged();
    void freeTransformActiveChanged();
    void textEntryActiveChanged();
    void canvasWidthChanged();
    void canvasHeightChanged();

private:
    QVariantList buildDefaultPalette(PaletteUtils *paletteUtils) const;
    int nextImageId();
    void sanitizeSelection();
    void emitSelectionSignals();

    LayerListModel m_layerListModel;
    QVariantList m_palette;
    QColor m_brushColor;
    qreal m_brushSize = 2.0;
    QString m_toolMode = QStringLiteral("brush");
    int m_selectedLayerId = -1;
    bool m_freeTransformActive = false;
    bool m_textEntryActive = false;
    int m_canvasWidth = 1;
    int m_canvasHeight = 1;
    int m_nextImageId = 0;
};
