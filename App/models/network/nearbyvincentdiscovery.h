#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QThread>

#include <optional>

struct NearbyVincentPresence
{
    QString sessionId;
    bool online = false;
};

class NearbyVincentProtocol final
{
  public:
    [[nodiscard]] static QString serviceName();
    [[nodiscard]] static QByteArray encodePresence(const QString& sessionId, bool online);
    [[nodiscard]] static std::optional<NearbyVincentPresence>
    decodePresence(const QByteArray& payload);
    [[nodiscard]] static constexpr qsizetype maximumDatagramSize() noexcept { return 512; }
};

class NearbyVincentDiscoveryWorker;

class NearbyVincentDiscovery final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool anotherVincentUserDetected READ anotherVincentUserDetected NOTIFY
                   nearbyPresenceChanged)
    Q_PROPERTY(int nearbyDeviceCount READ nearbyDeviceCount NOTIFY nearbyPresenceChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

  public:
    struct Configuration
    {
        QHostAddress multicastGroup{QStringLiteral("239.255.86.67")};
        quint16 port = 52683;
        int heartbeatIntervalMs = 4000;
        int peerTimeoutMs = 14000;
        int pruneIntervalMs = 1000;
        bool ignoreLocalSenders = true;
        bool includeLoopbackInterfaces = false;
    };

    explicit NearbyVincentDiscovery(QObject* parent = nullptr);
    explicit NearbyVincentDiscovery(Configuration configuration, QObject* parent = nullptr);
    ~NearbyVincentDiscovery() override;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] bool anotherVincentUserDetected() const noexcept;
    [[nodiscard]] int nearbyDeviceCount() const noexcept;
    [[nodiscard]] QString errorString() const;

  public slots:
    void start();
    void stop();

  signals:
    void runningChanged();
    void nearbyPresenceChanged();
    void errorStringChanged();

  private:
    void applyRunning(bool running);
    void applyNearbyDeviceCount(int nearbyDeviceCount);
    void applyErrorString(const QString& errorString);

    QThread m_workerThread;
    NearbyVincentDiscoveryWorker* m_worker = nullptr;
    bool m_running = false;
    int m_nearbyDeviceCount = 0;
    QString m_errorString;
};
