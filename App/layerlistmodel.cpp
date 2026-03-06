#include "layerlistmodel.h"

#include <QtGlobal>

namespace {

constexpr auto kImageId = "imageId";
constexpr auto kSource = "source";
constexpr auto kX = "x";
constexpr auto kY = "y";
constexpr auto kOriginalWidth = "originalWidth";
constexpr auto kOriginalHeight = "originalHeight";
constexpr auto kScaleX = "scaleX";
constexpr auto kScaleY = "scaleY";
constexpr auto kReady = "ready";
constexpr auto kLayerName = "layerName";
constexpr auto kLayerVisible = "layerVisible";
constexpr auto kLayerOpacity = "layerOpacity";
constexpr auto kBlendModeKey = "blendModeKey";
constexpr auto kImportMetadata = "importMetadata";

} // namespace

LayerListModel::LayerListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LayerListModel::count() const
{
    return m_entries.size();
}

int LayerListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return count();
}

QVariant LayerListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const LayerEntry &entry = m_entries.at(index.row());
    switch (role) {
    case ImageIdRole:
        return entry.imageId;
    case SourceRole:
        return entry.source;
    case XRole:
        return entry.x;
    case YRole:
        return entry.y;
    case OriginalWidthRole:
        return entry.originalWidth;
    case OriginalHeightRole:
        return entry.originalHeight;
    case ScaleXRole:
        return entry.scaleX;
    case ScaleYRole:
        return entry.scaleY;
    case ReadyRole:
        return entry.ready;
    case LayerNameRole:
        return entry.layerName;
    case LayerVisibleRole:
        return entry.layerVisible;
    case LayerOpacityRole:
        return entry.layerOpacity;
    case BlendModeKeyRole:
        return entry.blendModeKey;
    case ImportMetadataRole:
        return entry.importMetadata;
    default:
        return {};
    }
}

QHash<int, QByteArray> LayerListModel::roleNames() const
{
    return {
        {ImageIdRole, QByteArrayLiteral("imageId")},
        {SourceRole, QByteArrayLiteral("source")},
        {XRole, QByteArrayLiteral("x")},
        {YRole, QByteArrayLiteral("y")},
        {OriginalWidthRole, QByteArrayLiteral("originalWidth")},
        {OriginalHeightRole, QByteArrayLiteral("originalHeight")},
        {ScaleXRole, QByteArrayLiteral("scaleX")},
        {ScaleYRole, QByteArrayLiteral("scaleY")},
        {ReadyRole, QByteArrayLiteral("ready")},
        {LayerNameRole, QByteArrayLiteral("layerName")},
        {LayerVisibleRole, QByteArrayLiteral("layerVisible")},
        {LayerOpacityRole, QByteArrayLiteral("layerOpacity")},
        {BlendModeKeyRole, QByteArrayLiteral("blendModeKey")},
        {ImportMetadataRole, QByteArrayLiteral("importMetadata")}
    };
}

QVariantMap LayerListModel::get(int index) const
{
    if (index < 0 || index >= m_entries.size()) {
        return {};
    }

    return layerEntryToMap(m_entries.at(index));
}

bool LayerListModel::setProperty(int index, const QString &property, const QVariant &value)
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }

    LayerEntry &entry = m_entries[index];
    if (!applyProperty(entry, property, value)) {
        return false;
    }

    const QModelIndex changedIndex = createIndex(index, 0);
    emit dataChanged(changedIndex, changedIndex);
    return true;
}

void LayerListModel::append(const QVariantMap &entryMap)
{
    const int insertIndex = m_entries.size();
    beginInsertRows(QModelIndex(), insertIndex, insertIndex);
    m_entries.push_back(layerEntryFromMap(entryMap));
    endInsertRows();
    emit countChanged();
}

void LayerListModel::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }

    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
}

void LayerListModel::remove(int index, int countValue)
{
    if (index < 0 || countValue <= 0 || index >= m_entries.size()) {
        return;
    }

    const int boundedCount = qMin(countValue, m_entries.size() - index);
    beginRemoveRows(QModelIndex(), index, index + boundedCount - 1);
    for (int i = 0; i < boundedCount; ++i) {
        m_entries.removeAt(index);
    }
    endRemoveRows();
    emit countChanged();
}

bool LayerListModel::move(int from, int to, int countValue)
{
    if (countValue <= 0 || from < 0 || to < 0 || from >= m_entries.size() || to > m_entries.size()) {
        return false;
    }

    if (from + countValue > m_entries.size()) {
        return false;
    }

    if (from == to || from == to - 1) {
        return true;
    }

    const int destination = to > from ? to + countValue : to;
    if (!beginMoveRows(QModelIndex(), from, from + countValue - 1, QModelIndex(), destination)) {
        return false;
    }

    QVector<LayerEntry> moving;
    moving.reserve(countValue);
    for (int i = 0; i < countValue; ++i) {
        moving.push_back(m_entries.at(from));
        m_entries.removeAt(from);
    }

    int insertAt = to;
    if (to > from) {
        insertAt -= countValue;
    }

    for (int i = 0; i < moving.size(); ++i) {
        m_entries.insert(insertAt + i, moving.at(i));
    }

    endMoveRows();
    return true;
}

int LayerListModel::indexOfImageId(int imageId) const
{
    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).imageId == imageId) {
            return index;
        }
    }

    return -1;
}

QVariantList LayerListModel::exportEntries() const
{
    QVariantList entries;
    entries.reserve(m_entries.size());
    for (const LayerEntry &entry : m_entries) {
        entries.push_back(layerEntryToMap(entry));
    }
    return entries;
}

void LayerListModel::importEntries(const QVariantList &entries)
{
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(entries.size());
    for (const QVariant &entry : entries) {
        m_entries.push_back(layerEntryFromMap(entry.toMap()));
    }
    endResetModel();
    emit countChanged();
}

LayerListModel::LayerEntry LayerListModel::entryAt(int index) const
{
    if (index < 0 || index >= m_entries.size()) {
        return {};
    }

    return m_entries.at(index);
}

bool LayerListModel::hasImageId(int imageId) const
{
    return indexOfImageId(imageId) != -1;
}

LayerListModel::LayerEntry LayerListModel::layerEntryFromMap(const QVariantMap &entryMap)
{
    LayerEntry entry;
    entry.imageId = entryMap.value(QLatin1String(kImageId), -1).toInt();
    entry.source = entryMap.value(QLatin1String(kSource)).toString();
    entry.x = entryMap.value(QLatin1String(kX), 0.0).toReal();
    entry.y = entryMap.value(QLatin1String(kY), 0.0).toReal();
    entry.originalWidth = entryMap.value(QLatin1String(kOriginalWidth), 0).toInt();
    entry.originalHeight = entryMap.value(QLatin1String(kOriginalHeight), 0).toInt();
    entry.scaleX = entryMap.value(QLatin1String(kScaleX), 1.0).toReal();
    entry.scaleY = entryMap.value(QLatin1String(kScaleY), 1.0).toReal();
    entry.ready = entryMap.value(QLatin1String(kReady), false).toBool();
    entry.layerName = entryMap.value(QLatin1String(kLayerName)).toString();
    entry.layerVisible = entryMap.value(QLatin1String(kLayerVisible), true).toBool();
    entry.layerOpacity = entryMap.value(QLatin1String(kLayerOpacity), 1.0).toReal();
    entry.blendModeKey = entryMap.value(QLatin1String(kBlendModeKey)).toString();
    entry.importMetadata = entryMap.value(QLatin1String(kImportMetadata)).toMap();
    return entry;
}

QVariantMap LayerListModel::layerEntryToMap(const LayerEntry &entry)
{
    return {
        {QLatin1String(kImageId), entry.imageId},
        {QLatin1String(kSource), entry.source},
        {QLatin1String(kX), entry.x},
        {QLatin1String(kY), entry.y},
        {QLatin1String(kOriginalWidth), entry.originalWidth},
        {QLatin1String(kOriginalHeight), entry.originalHeight},
        {QLatin1String(kScaleX), entry.scaleX},
        {QLatin1String(kScaleY), entry.scaleY},
        {QLatin1String(kReady), entry.ready},
        {QLatin1String(kLayerName), entry.layerName},
        {QLatin1String(kLayerVisible), entry.layerVisible},
        {QLatin1String(kLayerOpacity), entry.layerOpacity},
        {QLatin1String(kBlendModeKey), entry.blendModeKey},
        {QLatin1String(kImportMetadata), entry.importMetadata}
    };
}

bool LayerListModel::applyProperty(LayerEntry &entry, const QString &property, const QVariant &value)
{
    if (property == QLatin1String(kImageId)) {
        entry.imageId = value.toInt();
        return true;
    }
    if (property == QLatin1String(kSource)) {
        entry.source = value.toString();
        return true;
    }
    if (property == QLatin1String(kX)) {
        entry.x = value.toReal();
        return true;
    }
    if (property == QLatin1String(kY)) {
        entry.y = value.toReal();
        return true;
    }
    if (property == QLatin1String(kOriginalWidth)) {
        entry.originalWidth = value.toInt();
        return true;
    }
    if (property == QLatin1String(kOriginalHeight)) {
        entry.originalHeight = value.toInt();
        return true;
    }
    if (property == QLatin1String(kScaleX)) {
        entry.scaleX = value.toReal();
        return true;
    }
    if (property == QLatin1String(kScaleY)) {
        entry.scaleY = value.toReal();
        return true;
    }
    if (property == QLatin1String(kReady)) {
        entry.ready = value.toBool();
        return true;
    }
    if (property == QLatin1String(kLayerName)) {
        entry.layerName = value.toString();
        return true;
    }
    if (property == QLatin1String(kLayerVisible)) {
        entry.layerVisible = value.toBool();
        return true;
    }
    if (property == QLatin1String(kLayerOpacity)) {
        entry.layerOpacity = value.toReal();
        return true;
    }
    if (property == QLatin1String(kBlendModeKey)) {
        entry.blendModeKey = value.toString();
        return true;
    }
    if (property == QLatin1String(kImportMetadata)) {
        entry.importMetadata = value.toMap();
        return true;
    }

    return false;
}
