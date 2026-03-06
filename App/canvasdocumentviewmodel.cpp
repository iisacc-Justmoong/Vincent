#include "canvasdocumentviewmodel.h"

#include "paletteutils.h"

#include <QVariantMap>
#include <QtGlobal>

namespace {

QVariantMap paletteEntry(const QString &name, const QString &color)
{
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("color"), color}
    };
}

} // namespace

CanvasDocumentViewModel::CanvasDocumentViewModel(PaletteUtils *paletteUtils, QObject *parent)
    : QObject(parent)
    , m_layerListModel(this)
    , m_palette(buildDefaultPalette(paletteUtils))
    , m_brushColor(QStringLiteral("#1a1a1a"))
{
    connect(&m_layerListModel, &LayerListModel::countChanged, this, [this]() {
        sanitizeSelection();
        emit layerCountChanged();
        emit hasImportedLayerChanged();
        emit hasLayerSelectionChanged();
        emit selectedLayerNameChanged();
        emit selectedLayerDataChanged();
    });

    connect(&m_layerListModel, &QAbstractItemModel::dataChanged, this, [this]() {
        sanitizeSelection();
        emit hasLayerSelectionChanged();
        emit selectedLayerNameChanged();
        emit selectedLayerDataChanged();
    });

    connect(&m_layerListModel, &QAbstractItemModel::modelReset, this, [this]() {
        sanitizeSelection();
        emit hasLayerSelectionChanged();
        emit selectedLayerNameChanged();
        emit selectedLayerDataChanged();
    });
}

QVariantList CanvasDocumentViewModel::palette() const
{
    return m_palette;
}

int CanvasDocumentViewModel::layerCount() const
{
    return m_layerListModel.count();
}

QColor CanvasDocumentViewModel::brushColor() const
{
    return m_brushColor;
}

qreal CanvasDocumentViewModel::brushSize() const
{
    return m_brushSize;
}

QString CanvasDocumentViewModel::toolMode() const
{
    return m_toolMode;
}

int CanvasDocumentViewModel::selectedLayerId() const
{
    return m_selectedLayerId;
}

QString CanvasDocumentViewModel::selectedLayerName() const
{
    const QVariantMap layer = selectedLayerData();
    return layer.value(QStringLiteral("layerName")).toString();
}

QVariantMap CanvasDocumentViewModel::selectedLayerData() const
{
    return selectedLayer();
}

bool CanvasDocumentViewModel::hasImportedLayer() const
{
    return layerCount() > 0;
}

bool CanvasDocumentViewModel::hasLayerSelection() const
{
    return m_selectedLayerId != -1 && m_layerListModel.hasImageId(m_selectedLayerId);
}

bool CanvasDocumentViewModel::freeTransformActive() const
{
    return m_freeTransformActive;
}

bool CanvasDocumentViewModel::textEntryActive() const
{
    return m_textEntryActive;
}

int CanvasDocumentViewModel::canvasWidth() const
{
    return m_canvasWidth;
}

int CanvasDocumentViewModel::canvasHeight() const
{
    return m_canvasHeight;
}

QObject *CanvasDocumentViewModel::layerListModel()
{
    return &m_layerListModel;
}

void CanvasDocumentViewModel::setBrushColor(const QColor &brushColor)
{
    if (m_brushColor == brushColor) {
        return;
    }

    m_brushColor = brushColor;
    emit brushColorChanged();
}

void CanvasDocumentViewModel::setBrushSize(qreal brushSize)
{
    const qreal boundedSize = qBound<qreal>(1.0, brushSize, 48.0);
    if (qFuzzyCompare(m_brushSize, boundedSize)) {
        return;
    }

    m_brushSize = boundedSize;
    emit brushSizeChanged();
}

void CanvasDocumentViewModel::setToolMode(const QString &toolMode)
{
    if (m_toolMode == toolMode) {
        return;
    }

    m_toolMode = toolMode;
    emit toolModeChanged();
}

void CanvasDocumentViewModel::setSelectedLayerId(int selectedLayerId)
{
    if (selectedLayerId != -1 && !m_layerListModel.hasImageId(selectedLayerId)) {
        selectedLayerId = -1;
    }

    if (m_selectedLayerId == selectedLayerId) {
        return;
    }

    m_selectedLayerId = selectedLayerId;
    emitSelectionSignals();
}

void CanvasDocumentViewModel::setFreeTransformActive(bool freeTransformActive)
{
    if (m_freeTransformActive == freeTransformActive) {
        return;
    }

    m_freeTransformActive = freeTransformActive;
    emit freeTransformActiveChanged();
}

void CanvasDocumentViewModel::setTextEntryActive(bool textEntryActive)
{
    if (m_textEntryActive == textEntryActive) {
        return;
    }

    m_textEntryActive = textEntryActive;
    emit textEntryActiveChanged();
}

void CanvasDocumentViewModel::setCanvasWidth(int canvasWidth)
{
    const int boundedWidth = qMax(1, canvasWidth);
    if (m_canvasWidth == boundedWidth) {
        return;
    }

    m_canvasWidth = boundedWidth;
    emit canvasWidthChanged();
}

void CanvasDocumentViewModel::setCanvasHeight(int canvasHeight)
{
    const int boundedHeight = qMax(1, canvasHeight);
    if (m_canvasHeight == boundedHeight) {
        return;
    }

    m_canvasHeight = boundedHeight;
    emit canvasHeightChanged();
}

void CanvasDocumentViewModel::resetDocument()
{
    clearLayers();
    setSelectedLayerId(-1);
    setFreeTransformActive(false);
    setTextEntryActive(false);
}

int CanvasDocumentViewModel::appendLayer(const QVariantMap &layerData)
{
    QVariantMap nextLayer = layerData;
    if (!nextLayer.contains(QStringLiteral("imageId")) || nextLayer.value(QStringLiteral("imageId")).toInt() < 0) {
        nextLayer.insert(QStringLiteral("imageId"), nextImageId());
    } else {
        m_nextImageId = qMax(m_nextImageId, nextLayer.value(QStringLiteral("imageId")).toInt());
    }

    m_layerListModel.append(nextLayer);
    return nextLayer.value(QStringLiteral("imageId")).toInt();
}

bool CanvasDocumentViewModel::updateLayerPropertyById(int imageId, const QString &property, const QVariant &value)
{
    const int index = findLayerIndexById(imageId);
    if (index == -1) {
        return false;
    }

    return m_layerListModel.setProperty(index, property, value);
}

bool CanvasDocumentViewModel::updateLayerById(int imageId, const QVariantMap &changes)
{
    const int index = findLayerIndexById(imageId);
    if (index == -1) {
        return false;
    }

    bool changed = false;
    for (auto it = changes.constBegin(); it != changes.constEnd(); ++it) {
        changed = m_layerListModel.setProperty(index, it.key(), it.value()) || changed;
    }
    return changed;
}

bool CanvasDocumentViewModel::moveLayer(int from, int to, int count)
{
    return m_layerListModel.move(from, to, count);
}

bool CanvasDocumentViewModel::removeLayerById(int imageId)
{
    const int index = findLayerIndexById(imageId);
    if (index == -1) {
        return false;
    }

    const bool removedSelectedLayer = m_selectedLayerId == imageId;
    m_layerListModel.remove(index);

    if (removedSelectedLayer) {
        const int nextIndex = m_layerListModel.count() - 1;
        setSelectedLayerId(nextIndex >= 0 ? m_layerListModel.get(nextIndex).value(QStringLiteral("imageId")).toInt() : -1);
    } else {
        sanitizeSelection();
    }

    return true;
}

void CanvasDocumentViewModel::clearLayers()
{
    m_layerListModel.clear();
    sanitizeSelection();
}

int CanvasDocumentViewModel::findLayerIndexById(int imageId) const
{
    return m_layerListModel.indexOfImageId(imageId);
}

QVariantMap CanvasDocumentViewModel::layerAt(int index) const
{
    return m_layerListModel.get(index);
}

QVariantMap CanvasDocumentViewModel::layerById(int imageId) const
{
    return m_layerListModel.get(findLayerIndexById(imageId));
}

QVariantMap CanvasDocumentViewModel::selectedLayer() const
{
    return layerById(m_selectedLayerId);
}

QVariantList CanvasDocumentViewModel::exportLayers() const
{
    return m_layerListModel.exportEntries();
}

void CanvasDocumentViewModel::importLayers(const QVariantList &layers)
{
    m_layerListModel.importEntries(layers);

    int maxImageId = 0;
    for (const QVariant &layer : layers) {
        maxImageId = qMax(maxImageId, layer.toMap().value(QStringLiteral("imageId")).toInt());
    }
    m_nextImageId = maxImageId;
    sanitizeSelection();
}

void CanvasDocumentViewModel::setLayerVisibility(int imageId, bool visible)
{
    updateLayerPropertyById(imageId, QStringLiteral("layerVisible"), visible);
}

QVariantList CanvasDocumentViewModel::buildDefaultPalette(PaletteUtils *paletteUtils) const
{
    const QVariantList primary = {
        paletteEntry(QStringLiteral("Ink Black"), QStringLiteral("#1a1a1a")),
        paletteEntry(QStringLiteral("Signal Red"), QStringLiteral("#e53935")),
        paletteEntry(QStringLiteral("Amber"), QStringLiteral("#fb8c00")),
        paletteEntry(QStringLiteral("Sun Yellow"), QStringLiteral("#fdd835")),
        paletteEntry(QStringLiteral("Leaf Green"), QStringLiteral("#43a047")),
        paletteEntry(QStringLiteral("Sky Blue"), QStringLiteral("#1e88e5")),
        paletteEntry(QStringLiteral("Violet"), QStringLiteral("#5e35b1")),
        paletteEntry(QStringLiteral("Clay"), QStringLiteral("#8d6e63")),
        paletteEntry(QStringLiteral("Pure White"), QStringLiteral("#ffffff")),
        paletteEntry(QStringLiteral("Pitch Black"), QStringLiteral("#000000"))
    };

    const QVariantList extended = {
        paletteEntry(QStringLiteral("Coral"), QStringLiteral("#ff7043")),
        paletteEntry(QStringLiteral("Rose"), QStringLiteral("#f06292")),
        paletteEntry(QStringLiteral("Lilac"), QStringLiteral("#ba68c8")),
        paletteEntry(QStringLiteral("Cerulean"), QStringLiteral("#0091ea")),
        paletteEntry(QStringLiteral("Seafoam"), QStringLiteral("#26c6da")),
        paletteEntry(QStringLiteral("Forest"), QStringLiteral("#2e7d32")),
        paletteEntry(QStringLiteral("Olive"), QStringLiteral("#827717")),
        paletteEntry(QStringLiteral("Burnt Sienna"), QStringLiteral("#d84315")),
        paletteEntry(QStringLiteral("Slate"), QStringLiteral("#546e7a"))
    };

    if (paletteUtils) {
        return paletteUtils->buildDefaultPalette(primary, extended);
    }

    QVariantList fallback = primary;
    fallback.append(extended);
    return fallback;
}

int CanvasDocumentViewModel::nextImageId()
{
    ++m_nextImageId;
    return m_nextImageId;
}

void CanvasDocumentViewModel::sanitizeSelection()
{
    if (m_selectedLayerId == -1) {
        return;
    }

    if (!m_layerListModel.hasImageId(m_selectedLayerId)) {
        m_selectedLayerId = -1;
        emitSelectionSignals();
    }
}

void CanvasDocumentViewModel::emitSelectionSignals()
{
    emit selectedLayerIdChanged();
    emit hasLayerSelectionChanged();
    emit selectedLayerNameChanged();
    emit selectedLayerDataChanged();
}
