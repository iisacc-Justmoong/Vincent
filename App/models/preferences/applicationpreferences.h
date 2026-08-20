#pragma once

#include <QObject>
#include <QUrl>

#include <memory>

class QSettings;

class ApplicationPreferences final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool startWithRecentCanvas READ startWithRecentCanvas WRITE
                   setStartWithRecentCanvas NOTIFY startWithRecentCanvasChanged)
    Q_PROPERTY(bool discoverNearbyVincentUsers READ discoverNearbyVincentUsers WRITE
                   setDiscoverNearbyVincentUsers NOTIFY discoverNearbyVincentUsersChanged)
    Q_PROPERTY(QUrl recentCanvasUrl READ recentCanvasUrl NOTIFY recentCanvasUrlChanged)

public:
    explicit ApplicationPreferences(QObject *parent = nullptr);
    explicit ApplicationPreferences(QSettings *settings, QObject *parent = nullptr);
    ~ApplicationPreferences() override;

    [[nodiscard]] bool startWithRecentCanvas() const noexcept;
    [[nodiscard]] bool discoverNearbyVincentUsers() const noexcept;
    [[nodiscard]] QUrl recentCanvasUrl() const;

    Q_INVOKABLE void setStartWithRecentCanvas(bool startWithRecentCanvas);
    Q_INVOKABLE void setDiscoverNearbyVincentUsers(bool discoverNearbyVincentUsers);
    Q_INVOKABLE bool recordRecentCanvas(const QUrl &fileUrl);
    Q_INVOKABLE void clearRecentCanvas();

signals:
    void startWithRecentCanvasChanged();
    void discoverNearbyVincentUsersChanged();
    void recentCanvasUrlChanged();

private:
    void load();
    [[nodiscard]] static QUrl canonicalLocalFileUrl(const QUrl &fileUrl);

    std::unique_ptr<QSettings> m_ownedSettings;
    QSettings *m_settings = nullptr;
    bool m_startWithRecentCanvas = false;
    bool m_discoverNearbyVincentUsers = true;
    QUrl m_recentCanvasUrl;
};
