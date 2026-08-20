#include "nearbyvincentdiscovery.h"

#include <QAbstractSocket>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSet>
#include <QTimer>
#include <QUdpSocket>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace
{
constexpr int protocolVersion = 1;
constexpr qsizetype maximumTrackedSessions = 256;

NearbyVincentDiscovery::Configuration
normalizedConfiguration(NearbyVincentDiscovery::Configuration configuration)
{
    const NearbyVincentDiscovery::Configuration defaults;
    if (configuration.multicastGroup.protocol() != QAbstractSocket::IPv4Protocol ||
        !configuration.multicastGroup.isMulticast())
    {
        configuration.multicastGroup = defaults.multicastGroup;
    }
    if (configuration.port == 0)
    {
        configuration.port = defaults.port;
    }
    configuration.heartbeatIntervalMs = qMax(50, configuration.heartbeatIntervalMs);
    configuration.peerTimeoutMs =
        qMax(configuration.heartbeatIntervalMs * 2, configuration.peerTimeoutMs);
    configuration.pruneIntervalMs =
        qBound(25, configuration.pruneIntervalMs, configuration.peerTimeoutMs);
    return configuration;
}

bool isCanonicalSessionId(const QString& sessionId)
{
    const QUuid uuid(sessionId);
    return !uuid.isNull() &&
           uuid.toString(QUuid::WithoutBraces).compare(sessionId, Qt::CaseInsensitive) == 0;
}

bool isUsableMulticastInterface(const QNetworkInterface& networkInterface,
                                bool includeLoopbackInterfaces)
{
    const QNetworkInterface::InterfaceFlags flags = networkInterface.flags();
    if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning) ||
        !flags.testFlag(QNetworkInterface::CanMulticast) ||
        (!includeLoopbackInterfaces && flags.testFlag(QNetworkInterface::IsLoopBack)))
    {
        return false;
    }

    for (const QNetworkAddressEntry& entry : networkInterface.addressEntries())
    {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
        {
            return true;
        }
    }
    return false;
}
} // namespace

QString NearbyVincentProtocol::serviceName()
{
    return QStringLiteral("com.iisacc.vincent.nearby");
}

QByteArray NearbyVincentProtocol::encodePresence(const QString& sessionId, bool online)
{
    if (!isCanonicalSessionId(sessionId))
    {
        return {};
    }

    const QJsonObject object{
        {QStringLiteral("service"), serviceName()},
        {QStringLiteral("version"), protocolVersion},
        {QStringLiteral("session"), sessionId.toLower()},
        {QStringLiteral("state"), online ? QStringLiteral("online") : QStringLiteral("offline")}};
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<NearbyVincentPresence>
NearbyVincentProtocol::decodePresence(const QByteArray& payload)
{
    if (payload.isEmpty() || payload.size() > maximumDatagramSize())
    {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    const QJsonValue versionValue = object.value(QStringLiteral("version"));
    if (object.value(QStringLiteral("service")).toString() != serviceName() ||
        !versionValue.isDouble() || versionValue.toDouble() != protocolVersion)
    {
        return std::nullopt;
    }

    const QString sessionId = object.value(QStringLiteral("session")).toString();
    if (!isCanonicalSessionId(sessionId))
    {
        return std::nullopt;
    }

    const QString state = object.value(QStringLiteral("state")).toString();
    if (state != QStringLiteral("online") && state != QStringLiteral("offline"))
    {
        return std::nullopt;
    }

    return NearbyVincentPresence{sessionId.toLower(), state == QStringLiteral("online")};
}

class NearbyVincentDiscoveryWorker final : public QObject
{
    Q_OBJECT

  public:
    NearbyVincentDiscoveryWorker(NearbyVincentDiscovery::Configuration configuration,
                                 QString sessionId)
        : m_configuration(std::move(configuration)), m_sessionId(std::move(sessionId))
    {
    }

  public slots:
    void start()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (m_socket)
        {
            return;
        }

        m_clock.start();
        m_socket = new QUdpSocket(this);
        connect(m_socket, &QUdpSocket::readyRead, this,
                &NearbyVincentDiscoveryWorker::receivePendingDatagrams);

        const auto bindMode = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
        if (!m_socket->bind(QHostAddress::AnyIPv4, m_configuration.port, bindMode))
        {
            setError(m_socket->errorString());
            delete m_socket;
            m_socket = nullptr;
            emit runningChanged(false);
            return;
        }

        m_socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
        m_socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);

        m_heartbeatTimer = new QTimer(this);
        m_heartbeatTimer->setInterval(m_configuration.heartbeatIntervalMs);
        connect(m_heartbeatTimer, &QTimer::timeout, this,
                &NearbyVincentDiscoveryWorker::sendHeartbeat);

        m_pruneTimer = new QTimer(this);
        m_pruneTimer->setInterval(m_configuration.pruneIntervalMs);
        connect(m_pruneTimer, &QTimer::timeout, this,
                &NearbyVincentDiscoveryWorker::pruneExpiredPeers);

        refreshNetworkInterfaces();
        sendPresence(true);
        m_heartbeatTimer->start();
        m_pruneTimer->start();
        emit runningChanged(true);
    }

    void stop()
    {
        Q_ASSERT(QThread::currentThread() == thread());
        if (!m_socket)
        {
            if (!m_peers.isEmpty())
            {
                m_peers.clear();
                emit nearbyDeviceCountChanged(0);
            }
            setError({});
            emit runningChanged(false);
            return;
        }

        m_heartbeatTimer->stop();
        m_pruneTimer->stop();
        sendPresence(false);

        delete m_heartbeatTimer;
        m_heartbeatTimer = nullptr;
        delete m_pruneTimer;
        m_pruneTimer = nullptr;
        delete m_socket;
        m_socket = nullptr;
        m_joinedInterfaces.clear();
        m_joinedDefaultInterface = false;
        m_localAddresses.clear();

        if (!m_peers.isEmpty())
        {
            m_peers.clear();
            emit nearbyDeviceCountChanged(0);
        }
        setError({});
        emit runningChanged(false);
    }

  signals:
    void runningChanged(bool running);
    void nearbyDeviceCountChanged(int nearbyDeviceCount);
    void errorStringChanged(const QString& errorString);

  private:
    struct Peer
    {
        QHostAddress senderAddress;
        qint64 lastSeenMs = 0;
    };

    void sendHeartbeat()
    {
        refreshNetworkInterfaces();
        sendPresence(true);
    }

    void refreshNetworkInterfaces()
    {
        if (!m_socket)
        {
            return;
        }

        QSet<QHostAddress> localAddresses{QHostAddress::LocalHost};
        QHash<int, QNetworkInterface> currentInterfaces;
        const QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& networkInterface : allInterfaces)
        {
            const QNetworkInterface::InterfaceFlags flags = networkInterface.flags();
            if (flags.testFlag(QNetworkInterface::IsUp) &&
                flags.testFlag(QNetworkInterface::IsRunning))
            {
                for (const QNetworkAddressEntry& entry : networkInterface.addressEntries())
                {
                    if (!entry.ip().isNull())
                    {
                        localAddresses.insert(entry.ip());
                    }
                }
            }
            if (isUsableMulticastInterface(networkInterface,
                                           m_configuration.includeLoopbackInterfaces))
            {
                currentInterfaces.insert(networkInterface.index(), networkInterface);
            }
        }
        m_localAddresses = std::move(localAddresses);

        for (auto iterator = m_joinedInterfaces.begin(); iterator != m_joinedInterfaces.end();)
        {
            if (!currentInterfaces.contains(iterator.key()))
            {
                m_socket->leaveMulticastGroup(m_configuration.multicastGroup, iterator.value());
                iterator = m_joinedInterfaces.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }

        for (auto iterator = currentInterfaces.cbegin(); iterator != currentInterfaces.cend();
             ++iterator)
        {
            if (m_joinedInterfaces.contains(iterator.key()))
            {
                continue;
            }
            if (m_socket->joinMulticastGroup(m_configuration.multicastGroup, iterator.value()))
            {
                m_joinedInterfaces.insert(iterator.key(), iterator.value());
            }
        }

        if (!m_joinedInterfaces.isEmpty() && m_joinedDefaultInterface)
        {
            m_socket->leaveMulticastGroup(m_configuration.multicastGroup);
            m_joinedDefaultInterface = false;
        }
        else if (m_joinedInterfaces.isEmpty() && !m_joinedDefaultInterface)
        {
            m_joinedDefaultInterface = m_socket->joinMulticastGroup(m_configuration.multicastGroup);
        }

        if (m_joinedInterfaces.isEmpty() && !m_joinedDefaultInterface)
        {
            setError(m_socket->errorString());
        }
    }

    void sendPresence(bool online)
    {
        if (!m_socket)
        {
            return;
        }

        const QByteArray payload = NearbyVincentProtocol::encodePresence(m_sessionId, online);
        bool sent = false;
        for (const QNetworkInterface& networkInterface : std::as_const(m_joinedInterfaces))
        {
            m_socket->setMulticastInterface(networkInterface);
            sent = m_socket->writeDatagram(payload, m_configuration.multicastGroup,
                                           m_configuration.port) == payload.size() ||
                   sent;
        }

        if (m_joinedInterfaces.isEmpty())
        {
            m_socket->setMulticastInterface(QNetworkInterface{});
            sent = m_socket->writeDatagram(payload, m_configuration.multicastGroup,
                                           m_configuration.port) == payload.size();
        }

        const bool listening = !m_joinedInterfaces.isEmpty() || m_joinedDefaultInterface;
        if (sent && listening)
        {
            setError({});
        }
        else if (!listening)
        {
            setError(QStringLiteral("Local network multicast listening is unavailable."));
        }
        else
        {
            setError(m_socket->errorString());
        }
    }

    void receivePendingDatagrams()
    {
        while (m_socket && m_socket->hasPendingDatagrams())
        {
            const QNetworkDatagram datagram =
                m_socket->receiveDatagram(NearbyVincentProtocol::maximumDatagramSize() + 1);
            if (!datagram.isValid() ||
                datagram.data().size() > NearbyVincentProtocol::maximumDatagramSize())
            {
                continue;
            }

            const std::optional<NearbyVincentPresence> presence =
                NearbyVincentProtocol::decodePresence(datagram.data());
            if (!presence || presence->sessionId == m_sessionId)
            {
                continue;
            }
            if (m_configuration.ignoreLocalSenders &&
                (datagram.senderAddress().isLoopback() ||
                 m_localAddresses.contains(datagram.senderAddress())))
            {
                continue;
            }

            const int previousCount = peerDeviceCount();
            if (presence->online)
            {
                if (!m_peers.contains(presence->sessionId) &&
                    m_peers.size() >= maximumTrackedSessions)
                {
                    const auto oldestPeer = std::min_element(
                        m_peers.cbegin(), m_peers.cend(), [](const Peer& left, const Peer& right)
                        { return left.lastSeenMs < right.lastSeenMs; });
                    if (oldestPeer != m_peers.cend())
                    {
                        m_peers.erase(oldestPeer);
                    }
                }
                m_peers.insert(presence->sessionId,
                               Peer{datagram.senderAddress(), m_clock.elapsed()});
            }
            else
            {
                m_peers.remove(presence->sessionId);
            }
            emitDeviceCountIfChanged(previousCount);
        }
    }

    void pruneExpiredPeers()
    {
        const int previousCount = peerDeviceCount();
        const qint64 oldestAllowed = m_clock.elapsed() - m_configuration.peerTimeoutMs;
        for (auto iterator = m_peers.begin(); iterator != m_peers.end();)
        {
            if (iterator->lastSeenMs < oldestAllowed)
            {
                iterator = m_peers.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
        emitDeviceCountIfChanged(previousCount);
    }

    [[nodiscard]] int peerDeviceCount() const
    {
        QSet<QHostAddress> addresses;
        for (const Peer& peer : m_peers)
        {
            addresses.insert(peer.senderAddress);
        }
        return addresses.size();
    }

    void emitDeviceCountIfChanged(int previousCount)
    {
        const int currentCount = peerDeviceCount();
        if (currentCount != previousCount)
        {
            emit nearbyDeviceCountChanged(currentCount);
        }
    }

    void setError(const QString& errorString)
    {
        if (m_errorString == errorString)
        {
            return;
        }
        m_errorString = errorString;
        emit errorStringChanged(m_errorString);
    }

    NearbyVincentDiscovery::Configuration m_configuration;
    QString m_sessionId;
    QUdpSocket* m_socket = nullptr;
    QTimer* m_heartbeatTimer = nullptr;
    QTimer* m_pruneTimer = nullptr;
    QElapsedTimer m_clock;
    QHash<int, QNetworkInterface> m_joinedInterfaces;
    bool m_joinedDefaultInterface = false;
    QSet<QHostAddress> m_localAddresses;
    QHash<QString, Peer> m_peers;
    QString m_errorString;
};

NearbyVincentDiscovery::NearbyVincentDiscovery(QObject* parent)
    : NearbyVincentDiscovery(Configuration{}, parent)
{
}

NearbyVincentDiscovery::NearbyVincentDiscovery(Configuration configuration, QObject* parent)
    : QObject(parent),
      m_worker(new NearbyVincentDiscoveryWorker(normalizedConfiguration(std::move(configuration)),
                                                QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    m_workerThread.setObjectName(QStringLiteral("Vincent nearby discovery"));
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &NearbyVincentDiscoveryWorker::runningChanged, this,
            &NearbyVincentDiscovery::applyRunning);
    connect(m_worker, &NearbyVincentDiscoveryWorker::nearbyDeviceCountChanged, this,
            &NearbyVincentDiscovery::applyNearbyDeviceCount);
    connect(m_worker, &NearbyVincentDiscoveryWorker::errorStringChanged, this,
            &NearbyVincentDiscovery::applyErrorString);
    m_workerThread.start();
}

NearbyVincentDiscovery::~NearbyVincentDiscovery()
{
    if (!m_workerThread.isRunning())
    {
        return;
    }

    QMetaObject::invokeMethod(m_worker, &NearbyVincentDiscoveryWorker::stop,
                              Qt::BlockingQueuedConnection);
    m_workerThread.quit();
    m_workerThread.wait();
    m_worker = nullptr;
}

bool NearbyVincentDiscovery::running() const noexcept
{
    return m_running;
}

bool NearbyVincentDiscovery::anotherVincentUserDetected() const noexcept
{
    return m_nearbyDeviceCount > 0;
}

int NearbyVincentDiscovery::nearbyDeviceCount() const noexcept
{
    return m_nearbyDeviceCount;
}

QString NearbyVincentDiscovery::errorString() const
{
    return m_errorString;
}

void NearbyVincentDiscovery::start()
{
    if (!m_workerThread.isRunning())
    {
        return;
    }
    QMetaObject::invokeMethod(m_worker, &NearbyVincentDiscoveryWorker::start, Qt::QueuedConnection);
}

void NearbyVincentDiscovery::stop()
{
    if (!m_workerThread.isRunning())
    {
        applyRunning(false);
        applyNearbyDeviceCount(0);
        return;
    }
    QMetaObject::invokeMethod(m_worker, &NearbyVincentDiscoveryWorker::stop,
                              Qt::BlockingQueuedConnection);
    applyRunning(false);
    applyNearbyDeviceCount(0);
    applyErrorString({});
}

void NearbyVincentDiscovery::applyRunning(bool running)
{
    if (m_running == running)
    {
        return;
    }
    m_running = running;
    emit runningChanged();
}

void NearbyVincentDiscovery::applyNearbyDeviceCount(int nearbyDeviceCount)
{
    if (m_nearbyDeviceCount == nearbyDeviceCount)
    {
        return;
    }
    m_nearbyDeviceCount = nearbyDeviceCount;
    emit nearbyPresenceChanged();
}

void NearbyVincentDiscovery::applyErrorString(const QString& errorString)
{
    if (m_errorString == errorString)
    {
        return;
    }
    m_errorString = errorString;
    emit errorStringChanged();
}

#include "nearbyvincentdiscovery.moc"
