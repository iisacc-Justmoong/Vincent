#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVector>

class LayerListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum LayerRoles {
        ImageIdRole = Qt::UserRole + 1,
        SourceRole,
        XRole,
        YRole,
        OriginalWidthRole,
        OriginalHeightRole,
        ScaleXRole,
        ScaleYRole,
        ReadyRole,
        LayerNameRole,
        LayerVisibleRole,
        LayerOpacityRole,
        BlendModeKeyRole,
        ImportMetadataRole
    };

    struct LayerEntry
    {
        int imageId = -1;
        QString source;
        qreal x = 0.0;
        qreal y = 0.0;
        int originalWidth = 0;
        int originalHeight = 0;
        qreal scaleX = 1.0;
        qreal scaleY = 1.0;
        bool ready = false;
        QString layerName;
        bool layerVisible = true;
        qreal layerOpacity = 1.0;
        QString blendModeKey;
        QVariantMap importMetadata;
    };

    explicit LayerListModel(QObject *parent = nullptr);

    [[nodiscard]] int count() const;
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap get(int index) const;
    Q_INVOKABLE bool setProperty(int index, const QString &property, const QVariant &value);
    Q_INVOKABLE void append(const QVariantMap &entry);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void remove(int index, int count = 1);
    Q_INVOKABLE bool move(int from, int to, int count = 1);
    Q_INVOKABLE int indexOfImageId(int imageId) const;
    Q_INVOKABLE QVariantList exportEntries() const;
    Q_INVOKABLE void importEntries(const QVariantList &entries);

    [[nodiscard]] LayerEntry entryAt(int index) const;
    [[nodiscard]] bool hasImageId(int imageId) const;

signals:
    void countChanged();

private:
    static LayerEntry layerEntryFromMap(const QVariantMap &entryMap);
    static QVariantMap layerEntryToMap(const LayerEntry &entry);
    static bool applyProperty(LayerEntry &entry, const QString &property, const QVariant &value);

    QVector<LayerEntry> m_entries;
};
