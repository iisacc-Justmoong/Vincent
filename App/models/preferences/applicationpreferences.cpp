#include "applicationpreferences.h"

#include <QFileInfo>
#include <QSettings>

namespace
{
constexpr auto startWithRecentCanvasKey = "General/startWithRecentCanvas";
constexpr auto discoverNearbyVincentUsersKey = "General/discoverNearbyVincentUsers";
constexpr auto recentCanvasUrlKey = "General/recentCanvasUrl";
}

ApplicationPreferences::ApplicationPreferences(QObject *parent)
    : QObject(parent)
    , m_ownedSettings(std::make_unique<QSettings>())
    , m_settings(m_ownedSettings.get())
{
    load();
}

ApplicationPreferences::ApplicationPreferences(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    Q_ASSERT(m_settings);
    load();
}

ApplicationPreferences::~ApplicationPreferences() = default;

bool ApplicationPreferences::startWithRecentCanvas() const noexcept
{
    return m_startWithRecentCanvas;
}

bool ApplicationPreferences::discoverNearbyVincentUsers() const noexcept
{
    return m_discoverNearbyVincentUsers;
}

QUrl ApplicationPreferences::recentCanvasUrl() const
{
    return m_recentCanvasUrl;
}

void ApplicationPreferences::setStartWithRecentCanvas(bool startWithRecentCanvas)
{
    if (m_startWithRecentCanvas == startWithRecentCanvas) {
        return;
    }

    m_startWithRecentCanvas = startWithRecentCanvas;
    m_settings->setValue(QLatin1String(startWithRecentCanvasKey), startWithRecentCanvas);
    m_settings->sync();
    emit startWithRecentCanvasChanged();
}

void ApplicationPreferences::setDiscoverNearbyVincentUsers(bool discoverNearbyVincentUsers)
{
    if (m_discoverNearbyVincentUsers == discoverNearbyVincentUsers) {
        return;
    }

    m_discoverNearbyVincentUsers = discoverNearbyVincentUsers;
    m_settings->setValue(QLatin1String(discoverNearbyVincentUsersKey),
                         discoverNearbyVincentUsers);
    m_settings->sync();
    emit discoverNearbyVincentUsersChanged();
}

bool ApplicationPreferences::recordRecentCanvas(const QUrl &fileUrl)
{
    const QUrl canonicalUrl = canonicalLocalFileUrl(fileUrl);
    if (canonicalUrl.isEmpty()) {
        return false;
    }
    if (m_recentCanvasUrl == canonicalUrl) {
        return true;
    }

    m_recentCanvasUrl = canonicalUrl;
    m_settings->setValue(QLatin1String(recentCanvasUrlKey), canonicalUrl);
    m_settings->sync();
    emit recentCanvasUrlChanged();
    return true;
}

void ApplicationPreferences::clearRecentCanvas()
{
    m_settings->remove(QLatin1String(recentCanvasUrlKey));
    m_settings->sync();
    if (m_recentCanvasUrl.isEmpty()) {
        return;
    }

    m_recentCanvasUrl = QUrl{};
    emit recentCanvasUrlChanged();
}

void ApplicationPreferences::load()
{
    m_startWithRecentCanvas =
        m_settings->value(QLatin1String(startWithRecentCanvasKey), false).toBool();
    m_discoverNearbyVincentUsers =
        m_settings->value(QLatin1String(discoverNearbyVincentUsersKey), true).toBool();

    const QUrl storedRecentCanvasUrl =
        m_settings->value(QLatin1String(recentCanvasUrlKey)).toUrl();
    m_recentCanvasUrl = canonicalLocalFileUrl(storedRecentCanvasUrl);
    if (m_recentCanvasUrl.isEmpty()) {
        if (m_settings->contains(QLatin1String(recentCanvasUrlKey))) {
            m_settings->remove(QLatin1String(recentCanvasUrlKey));
            m_settings->sync();
        }
        return;
    }

    if (m_recentCanvasUrl != storedRecentCanvasUrl) {
        m_settings->setValue(QLatin1String(recentCanvasUrlKey), m_recentCanvasUrl);
        m_settings->sync();
    }
}

QUrl ApplicationPreferences::canonicalLocalFileUrl(const QUrl &fileUrl)
{
    if (!fileUrl.isLocalFile()) {
        return {};
    }

    const QFileInfo fileInfo(fileUrl.toLocalFile());
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return {};
    }

    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? QUrl{} : QUrl::fromLocalFile(canonicalPath);
}
