#include "vincentupdatemanager.h"

#include "vincentupdatecredentialprovider.h"

#include <iiUpdateManager/UpdateManager.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

#include <algorithm>
#include <optional>
#include <utility>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <appmodel.h>
#endif

using iisacc::updates::UpdateManager;

namespace
{
struct DistributionChannelMarker
{
    bool present = false;
    QString value;
};

DistributionChannelMarker readDistributionChannelMarker(const QString &infoPlistPath)
{
    QFile infoPlist(infoPlistPath);
    if (!infoPlist.open(QIODevice::ReadOnly)) {
        return {};
    }

    const QByteArray contents = infoPlist.readAll();
    DistributionChannelMarker marker;
    marker.present = contents.contains("IISACCDistributionChannel");

    QXmlStreamReader xml(contents);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement() || xml.name() != QStringLiteral("key")) {
            continue;
        }
        if (xml.readElementText() != QStringLiteral("IISACCDistributionChannel")) {
            continue;
        }

        marker.present = true;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) {
                continue;
            }
            if (xml.name() == QStringLiteral("string")) {
                marker.value = xml.readElementText();
            }
            return marker;
        }
    }
    return marker;
}

bool selfUpdateSupportedForCurrentInstall()
{
#if defined(Q_OS_MACOS)
    return VincentUpdateManager::selfUpdateSupportedForMacApplicationDirectory(
        QCoreApplication::applicationDirPath());
#elif defined(Q_OS_WIN)
    UINT32 packageFullNameLength = 0;
    const LONG packageResult = GetCurrentPackageFullName(&packageFullNameLength, nullptr);
    return packageResult == APPMODEL_ERROR_NO_PACKAGE;
#else
    return true;
#endif
}

QString updateFailureMessage(UpdateManager::UpdateError error)
{
    switch (error) {
    case UpdateManager::UpdateError::CredentialUnavailable:
        return QStringLiteral("Sign in with a valid Vincent license before updating.");
    case UpdateManager::UpdateError::InvalidCredentials:
        return QStringLiteral("The stored Vincent license could not authorize this update.");
    case UpdateManager::UpdateError::InsufficientSpace:
        return QStringLiteral("There is not enough free space to download the update.");
    case UpdateManager::UpdateError::HashMismatch:
    case UpdateManager::UpdateError::SizeMismatch:
    case UpdateManager::UpdateError::SignatureInvalid:
        return QStringLiteral("The downloaded installer did not pass security verification.");
    case UpdateManager::UpdateError::UnsupportedInstaller:
        return QStringLiteral("This installer is not supported on the current platform.");
    case UpdateManager::UpdateError::InstallerLaunchFailed:
        return QStringLiteral("Vincent could not open the verified installer.");
    case UpdateManager::UpdateError::GrantNetwork:
    case UpdateManager::UpdateError::GrantTimeout:
    case UpdateManager::UpdateError::GrantHttpStatus:
    case UpdateManager::UpdateError::GrantRedirectRejected:
    case UpdateManager::UpdateError::InvalidGrant:
    case UpdateManager::UpdateError::GrantExpired:
    case UpdateManager::UpdateError::DownloadNetwork:
    case UpdateManager::UpdateError::DownloadTimeout:
    case UpdateManager::UpdateError::DownloadHttpStatus:
    case UpdateManager::UpdateError::DownloadRedirectRejected:
    case UpdateManager::UpdateError::DownloadInvalidResponse:
        return QStringLiteral("The update could not be downloaded. Check your connection and try again.");
    case UpdateManager::UpdateError::ResponseTooLarge:
    case UpdateManager::UpdateError::FileWrite:
        return QStringLiteral("Vincent could not safely save the downloaded installer.");
    case UpdateManager::UpdateError::InvalidState:
        return QStringLiteral("Check for an available update before starting the installer.");
    case UpdateManager::UpdateError::Cancelled:
        return QStringLiteral("The update was cancelled. You can start it again when ready.");
    }
    return QStringLiteral("The update could not be completed.");
}

QString checkFailureMessage(UpdateManager::Error error)
{
    switch (error) {
    case UpdateManager::Error::Network:
    case UpdateManager::Error::Timeout:
    case UpdateManager::Error::HttpStatus:
        return QStringLiteral("Vincent could not reach the update service. Check your connection and try again.");
    case UpdateManager::Error::RedirectRejected:
    case UpdateManager::Error::ResponseTooLarge:
    case UpdateManager::Error::InvalidManifest:
        return QStringLiteral("The update service returned an invalid response. Try again later.");
    case UpdateManager::Error::InvalidConfiguration:
        return QStringLiteral("Update checking is unavailable in this Vincent build.");
    }
    return QStringLiteral("Vincent could not check for updates.");
}
}

VincentUpdateManager::VincentUpdateManager(LicenseManager *licenseManager, QObject *parent)
    : QObject(parent)
    , m_selfUpdateSupported(selfUpdateSupportedForCurrentInstall())
    , m_credentialProvider(std::make_unique<VincentUpdateCredentialProvider>(licenseManager))
    , m_backend(std::make_unique<UpdateManager>(
          QStringLiteral("vincent"),
          QCoreApplication::applicationVersion()))
{
    m_backend->setCredentialProvider(m_credentialProvider.get());
    connectBackend();
}

VincentUpdateManager::VincentUpdateManager(LicenseManager *licenseManager,
                                           QString currentVersion,
                                           QUrl manifestUrl,
                                           QUrl grantUrl,
                                           bool selfUpdateSupported,
                                           QObject *parent)
    : QObject(parent)
    , m_selfUpdateSupported(selfUpdateSupported)
    , m_credentialProvider(std::make_unique<VincentUpdateCredentialProvider>(licenseManager))
    , m_backend(std::make_unique<UpdateManager>(
          QStringLiteral("vincent"),
          std::move(currentVersion),
          std::move(manifestUrl),
          std::move(grantUrl)))
{
    m_backend->setCredentialProvider(m_credentialProvider.get());
    connectBackend();
}

VincentUpdateManager::~VincentUpdateManager() = default;

VincentUpdateManager::State VincentUpdateManager::state() const noexcept
{
    return m_state;
}

double VincentUpdateManager::progress() const noexcept
{
    return m_progress;
}

QString VincentUpdateManager::title() const
{
    return m_title;
}

QString VincentUpdateManager::message() const
{
    return m_message;
}

QString VincentUpdateManager::latestVersion() const
{
    return m_latestVersion;
}

bool VincentUpdateManager::selfUpdateSupported() const noexcept
{
    return m_selfUpdateSupported;
}

bool VincentUpdateManager::selfUpdateSupportedForMacApplicationDirectory(
    const QString &applicationDirectory)
{
    QDir executableDirectory(applicationDirectory);
    if (executableDirectory.dirName() != QStringLiteral("MacOS")
        || !executableDirectory.cdUp()
        || executableDirectory.dirName() != QStringLiteral("Contents")) {
        return true;
    }

    const QFileInfo receipt(
        executableDirectory.filePath(QStringLiteral("_MASReceipt/receipt")));
    if (receipt.isFile() && receipt.size() > 0) {
        return false;
    }

    const DistributionChannelMarker marker = readDistributionChannelMarker(
        executableDirectory.filePath(QStringLiteral("Info.plist")));
    if (!marker.present) {
        return true;
    }
    return marker.value == QStringLiteral("direct");
}

bool VincentUpdateManager::busy() const noexcept
{
    return m_state == State::Checking
        || m_state == State::Authorizing
        || m_state == State::Downloading
        || m_state == State::Verifying
        || m_state == State::LaunchingInstaller;
}

bool VincentUpdateManager::canUpdate() const noexcept
{
    return m_selfUpdateSupported && m_state == State::UpdateAvailable;
}

bool VincentUpdateManager::canCancel() const noexcept
{
    return m_selfUpdateSupported
        && (m_state == State::Authorizing
        || m_state == State::Downloading
        || m_state == State::Verifying
        || m_state == State::LaunchingInstaller);
}

bool VincentUpdateManager::checkForUpdates()
{
    if (!m_selfUpdateSupported) {
        return rejectStoreManagedSelfUpdate();
    }
    setProgress(0.0);
    return m_backend->checkForUpdates();
}

bool VincentUpdateManager::updateNow()
{
    if (!m_selfUpdateSupported) {
        return rejectStoreManagedSelfUpdate();
    }
    if (!canUpdate()) {
        return false;
    }
    setProgress(0.0);
    return m_backend->updateNow();
}

bool VincentUpdateManager::cancelUpdate()
{
    if (!m_selfUpdateSupported) {
        return false;
    }
    if (!canCancel()) {
        return false;
    }
    return m_backend->cancelUpdate();
}

void VincentUpdateManager::connectBackend()
{
    connect(m_backend.get(), &UpdateManager::checkStarted, this, [this]() {
        setState(State::Checking);
        setPresentation(QStringLiteral("Checking for updates…"),
                        QStringLiteral("Vincent contacts iisacc only for this manual check."));
    });
    connect(m_backend.get(), &UpdateManager::updateAvailable, this,
            [this](const QString &version, const QUrl &) {
                setLatestVersion(version);
                setState(State::UpdateAvailable);
                setPresentation(QStringLiteral("Vincent %1 is available").arg(version),
                                QStringLiteral("Choose Update now to authorize, download, verify, and open the installer."));
            });
    connect(m_backend.get(), &UpdateManager::upToDate, this,
            [this](const QString &version) {
                setLatestVersion(version);
                setState(State::UpToDate);
                setPresentation(QStringLiteral("Vincent is up to date"),
                                QStringLiteral("You already have the newest available version."));
            });
    connect(m_backend.get(), &UpdateManager::checkFailed, this,
            [this](UpdateManager::Error error, const QString &) {
                setState(State::Failed);
                setPresentation(QStringLiteral("Unable to check for updates"),
                                checkFailureMessage(error));
            });
    connect(m_backend.get(), &UpdateManager::stateChanged,
            this, &VincentUpdateManager::applyBackendState);
    connect(m_backend.get(), &UpdateManager::downloadProgress, this,
            [this](qint64 bytesReceived, qint64 totalBytes) {
                const double ratio = totalBytes > 0
                    ? std::clamp(static_cast<double>(bytesReceived)
                                     / static_cast<double>(totalBytes),
                                 0.0,
                                 1.0)
                    : 0.0;
                setProgress(ratio);
                const int percent = static_cast<int>(ratio * 100.0);
                setPresentation(QStringLiteral("Downloading Vincent %1").arg(m_latestVersion),
                                totalBytes > 0
                                    ? QStringLiteral("Download progress: %1%").arg(percent)
                                    : QStringLiteral("Downloading the verified installer…"));
            });
    connect(m_backend.get(), &UpdateManager::installerLaunched, this,
            [this](const QString &version) {
                setLatestVersion(version);
                setProgress(1.0);
                setState(State::InstallerLaunched);
                setPresentation(
                    QStringLiteral("Installer opened"),
                    QStringLiteral("Save your work before closing Vincent. Vincent will not quit automatically; follow the installer instructions and close the app only when you are ready."));
            });
    connect(m_backend.get(), &UpdateManager::updateFailed, this,
            [this](UpdateManager::UpdateError error, const QString &) {
                if (error == UpdateManager::UpdateError::Cancelled) {
                    setState(State::UpdateAvailable);
                    setPresentation(QStringLiteral("Update cancelled"),
                                    updateFailureMessage(error));
                    return;
                }
                setState(State::Failed);
                setPresentation(QStringLiteral("Unable to update Vincent"),
                                updateFailureMessage(error));
            });
}

void VincentUpdateManager::applyBackendState()
{
    switch (m_backend->state()) {
    case UpdateManager::State::Idle:
        break;
    case UpdateManager::State::Checking:
        setState(State::Checking);
        break;
    case UpdateManager::State::UpdateAvailable:
        setState(State::UpdateAvailable);
        break;
    case UpdateManager::State::Authorizing:
        setState(State::Authorizing);
        setPresentation(QStringLiteral("Authorizing update…"),
                        QStringLiteral("Vincent is securely authorizing this one update request."));
        break;
    case UpdateManager::State::Downloading:
        setState(State::Downloading);
        setPresentation(QStringLiteral("Downloading Vincent %1").arg(m_latestVersion),
                        QStringLiteral("Downloading the verified installer…"));
        break;
    case UpdateManager::State::Verifying:
        setState(State::Verifying);
        setPresentation(QStringLiteral("Verifying installer…"),
                        QStringLiteral("Vincent is checking the package against the current platform safety policy."));
        break;
    case UpdateManager::State::LaunchingInstaller:
        setState(State::LaunchingInstaller);
        setPresentation(QStringLiteral("Opening installer…"),
                        QStringLiteral("Keep Vincent open until the installer appears."));
        break;
    case UpdateManager::State::InstallerLaunched:
        setState(State::InstallerLaunched);
        break;
    case UpdateManager::State::Failed:
        break;
    }
}

void VincentUpdateManager::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

void VincentUpdateManager::setProgress(double progress)
{
    const double boundedProgress = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_progress, boundedProgress)) {
        return;
    }
    m_progress = boundedProgress;
    emit progressChanged();
}

void VincentUpdateManager::setPresentation(QString title, QString message)
{
    if (m_title == title && m_message == message) {
        return;
    }
    m_title = std::move(title);
    m_message = std::move(message);
    emit presentationChanged();
}

void VincentUpdateManager::setLatestVersion(QString version)
{
    if (m_latestVersion == version) {
        return;
    }
    m_latestVersion = std::move(version);
    emit presentationChanged();
}

bool VincentUpdateManager::rejectStoreManagedSelfUpdate()
{
    setProgress(0.0);
    setState(State::Failed);
    setPresentation(
        QStringLiteral("Updates are managed by your app store"),
        QStringLiteral("Use the store that installed Vincent to check for and install updates."));
    return false;
}
