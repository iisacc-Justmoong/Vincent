#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>
#include <QSignalSpy>
#include <QSysInfo>
#include <QUuid>
#include <QtTest>

#include "nearbyvincentdiscovery.h"

class tst_NearbyVincentDiscovery : public QObject
{
    Q_OBJECT

  private slots:
    void presenceBeaconIsAnonymousAndRoundTrips();
    void sharingBeaconAddsOnlyCanvasEndpoint();
    void invitationDatagramIsTargetedAndCarriesInviterProfile();
    void malformedAndForeignBeaconsAreRejected();
    void twoBackgroundServicesDiscoverAndForgetEachOther();
    void servicesOnTheSameDeviceAreNotReported();
    void applicationStartsDiscoveryAfterShowingTheWindow();
};

void tst_NearbyVincentDiscovery::presenceBeaconIsAnonymousAndRoundTrips()
{
    const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QByteArray payload = NearbyVincentProtocol::encodePresence(sessionId, true);
    QVERIFY(!payload.isEmpty());

    const std::optional<NearbyVincentPresence> presence =
        NearbyVincentProtocol::decodePresence(payload);
    QVERIFY(presence.has_value());
    QCOMPARE(presence->sessionId, sessionId);
    QVERIFY(presence->online);

    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    const QStringList keys = object.keys();
    const QSet<QString> actualKeys(keys.cbegin(), keys.cend());
    const QSet<QString> expectedKeys{QStringLiteral("service"), QStringLiteral("version"),
                                     QStringLiteral("session"), QStringLiteral("state")};
    QCOMPARE(actualKeys, expectedKeys);

    const QByteArray hostName = QSysInfo::machineHostName().toUtf8();
    if (!hostName.isEmpty())
    {
        QVERIFY(!payload.contains(hostName));
    }
    const QByteArray userName = qgetenv("USER");
    if (!userName.isEmpty())
    {
        QVERIFY(!payload.contains(userName));
    }
}

void tst_NearbyVincentDiscovery::sharingBeaconAddsOnlyCanvasEndpoint()
{
    const QString sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QByteArray payload = NearbyVincentProtocol::encodePresence(sessionId, true, 54321);
    const std::optional<NearbyVincentPresence> presence =
        NearbyVincentProtocol::decodePresence(payload);
    QVERIFY(presence.has_value());
    QCOMPARE(presence->sessionId, sessionId);
    QCOMPARE(presence->canvasPort, quint16(54321));

    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    const QStringList keys = object.keys();
    const QSet<QString> actualKeys(keys.cbegin(), keys.cend());
    const QSet<QString> expectedKeys{QStringLiteral("service"), QStringLiteral("version"),
                                     QStringLiteral("session"), QStringLiteral("state"),
                                     QStringLiteral("canvasPort")};
    QCOMPARE(actualKeys, expectedKeys);
    const QByteArray hostName = QSysInfo::machineHostName().toUtf8();
    if (!hostName.isEmpty())
    {
        QVERIFY(!payload.contains(hostName));
    }
    const QByteArray userName = qgetenv("USER");
    if (!userName.isEmpty())
    {
        QVERIFY(!payload.contains(userName));
    }

    QJsonObject invalidPort = object;
    invalidPort.insert(QStringLiteral("canvasPort"), 70000);
    QVERIFY(!NearbyVincentProtocol::decodePresence(
                 QJsonDocument(invalidPort).toJson(QJsonDocument::Compact))
                 .has_value());
}

void tst_NearbyVincentDiscovery::invitationDatagramIsTargetedAndCarriesInviterProfile()
{
    const QString invitationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString senderSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString targetSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QByteArray payload = NearbyVincentProtocol::encodeInvitation(
        invitationId, senderSessionId, targetSessionId, 54321, QStringLiteral("Host Artist"));
    QVERIFY(!payload.isEmpty());

    const std::optional<NearbyVincentInvitation> invitation =
        NearbyVincentProtocol::decodeInvitation(payload);
    QVERIFY(invitation.has_value());
    QCOMPARE(invitation->invitationId, invitationId);
    QCOMPARE(invitation->senderSessionId, senderSessionId);
    QCOMPARE(invitation->targetSessionId, targetSessionId);
    QCOMPARE(invitation->canvasPort, quint16(54321));
    QCOMPARE(invitation->inviterProfileName, QStringLiteral("Host Artist"));
    QVERIFY(!NearbyVincentProtocol::decodePresence(payload).has_value());

    const QByteArray invitationCapability =
        NearbyVincentProtocol::encodePresence(targetSessionId, true, 0, true);
    const std::optional<NearbyVincentPresence> invitablePresence =
        NearbyVincentProtocol::decodePresence(invitationCapability);
    QVERIFY(invitablePresence.has_value());
    QVERIFY(invitablePresence->invitationsAllowed);
    const QJsonObject capabilityObject = QJsonDocument::fromJson(invitationCapability).object();
    QVERIFY(capabilityObject.value(QStringLiteral("invitationsAllowed")).toBool());
    QVERIFY(!capabilityObject.contains(QStringLiteral("profileName")));

    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    const QStringList keys = object.keys();
    const QSet<QString> actualKeys(keys.cbegin(), keys.cend());
    const QSet<QString> expectedKeys{QStringLiteral("service"),    QStringLiteral("version"),
                                     QStringLiteral("type"),       QStringLiteral("invitation"),
                                     QStringLiteral("sender"),     QStringLiteral("target"),
                                     QStringLiteral("canvasPort"), QStringLiteral("profileName")};
    QCOMPARE(actualKeys, expectedKeys);

    QJsonObject invalidTarget = object;
    invalidTarget.insert(QStringLiteral("target"), QStringLiteral("not-a-session"));
    QVERIFY(!NearbyVincentProtocol::decodeInvitation(
                 QJsonDocument(invalidTarget).toJson(QJsonDocument::Compact))
                 .has_value());
}

void tst_NearbyVincentDiscovery::malformedAndForeignBeaconsAreRejected()
{
    QVERIFY(!NearbyVincentProtocol::decodePresence(QByteArrayLiteral("not-json")).has_value());
    QVERIFY(!NearbyVincentProtocol::decodePresence(QByteArray(2048, 'x')).has_value());

    QJsonObject foreignObject{
        {QStringLiteral("service"), QStringLiteral("com.example.foreign")},
        {QStringLiteral("version"), 1},
        {QStringLiteral("session"), QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {QStringLiteral("state"), QStringLiteral("online")}};
    QVERIFY(!NearbyVincentProtocol::decodePresence(
                 QJsonDocument(foreignObject).toJson(QJsonDocument::Compact))
                 .has_value());

    foreignObject.insert(QStringLiteral("service"), NearbyVincentProtocol::serviceName());
    foreignObject.insert(QStringLiteral("version"), 2);
    QVERIFY(!NearbyVincentProtocol::decodePresence(
                 QJsonDocument(foreignObject).toJson(QJsonDocument::Compact))
                 .has_value());

    foreignObject.insert(QStringLiteral("version"), 1);
    foreignObject.insert(QStringLiteral("session"), QStringLiteral("not-a-uuid"));
    QVERIFY(!NearbyVincentProtocol::decodePresence(
                 QJsonDocument(foreignObject).toJson(QJsonDocument::Compact))
                 .has_value());
}

void tst_NearbyVincentDiscovery::twoBackgroundServicesDiscoverAndForgetEachOther()
{
    NearbyVincentDiscovery::Configuration configuration;
    configuration.port = static_cast<quint16>(49152 + QRandomGenerator::global()->bounded(12000));
    configuration.heartbeatIntervalMs = 100;
    configuration.peerTimeoutMs = 700;
    configuration.pruneIntervalMs = 50;
    configuration.ignoreLocalSenders = false;
    configuration.includeLoopbackInterfaces = true;

    NearbyVincentDiscovery first(configuration);
    NearbyVincentDiscovery second(configuration);
    first.setHostedCanvasPort(54321);
    second.setInvitationsAllowed(true);
    QSignalSpy firstPresenceSpy(&first, &NearbyVincentDiscovery::nearbyPresenceChanged);
    QSignalSpy secondPresenceSpy(&second, &NearbyVincentDiscovery::nearbyPresenceChanged);
    QSignalSpy invitationSpy(&second, &NearbyVincentDiscovery::canvasInvitationReceived);

    first.start();
    second.start();

    QTRY_VERIFY_WITH_TIMEOUT(first.running(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(second.running(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(first.nearbyDeviceCount(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(second.nearbyDeviceCount(), 1, 5000);
    QVERIFY(first.anotherVincentUserDetected());
    QVERIFY(second.anotherVincentUserDetected());
    QVERIFY(!firstPresenceSpy.isEmpty());
    QVERIFY(!secondPresenceSpy.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(second.availableCanvasSessions().size(), 1, 5000);
    const QVariantMap advertisedCanvas = second.availableCanvasSessions().first().toMap();
    QCOMPARE(advertisedCanvas.value(QStringLiteral("sessionId")).toString(), first.sessionId());
    QCOMPARE(advertisedCanvas.value(QStringLiteral("port")).toInt(), 54321);
    QCOMPARE(first.availableCanvasSessions().size(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(first.availableInvitationTargets().size(), 1, 5000);
    QCOMPARE(second.availableInvitationTargets().size(), 0);

    const QString invitationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(first.sendCanvasInvitation(invitationId, second.sessionId(), 54321,
                                       QStringLiteral("Host Artist")));
    QTRY_COMPARE_WITH_TIMEOUT(invitationSpy.size(), 1, 5000);
    const QVariantMap invitation = invitationSpy.first().first().toMap();
    QCOMPARE(invitation.value(QStringLiteral("invitationId")).toString(), invitationId);
    QCOMPARE(invitation.value(QStringLiteral("sessionId")).toString(), first.sessionId());
    QCOMPARE(invitation.value(QStringLiteral("port")).toInt(), 54321);
    QCOMPARE(invitation.value(QStringLiteral("profileName")).toString(),
             QStringLiteral("Host Artist"));
    QVERIFY(!invitation.value(QStringLiteral("address")).toString().isEmpty());

    first.setHostedCanvasPort(0);
    QTRY_COMPARE_WITH_TIMEOUT(second.availableCanvasSessions().size(), 0, 5000);

    second.stop();
    QTRY_VERIFY_WITH_TIMEOUT(!second.running(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(first.nearbyDeviceCount(), 0, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(first.availableInvitationTargets().size(), 0, 5000);
    QVERIFY(!first.anotherVincentUserDetected());
}

void tst_NearbyVincentDiscovery::servicesOnTheSameDeviceAreNotReported()
{
    NearbyVincentDiscovery::Configuration configuration;
    configuration.port = static_cast<quint16>(49152 + QRandomGenerator::global()->bounded(12000));
    configuration.heartbeatIntervalMs = 100;
    configuration.peerTimeoutMs = 700;
    configuration.pruneIntervalMs = 50;
    configuration.ignoreLocalSenders = true;
    configuration.includeLoopbackInterfaces = true;

    NearbyVincentDiscovery first(configuration);
    NearbyVincentDiscovery second(configuration);
    first.start();
    second.start();

    QTRY_VERIFY_WITH_TIMEOUT(first.running(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(second.running(), 5000);
    QTest::qWait(350);
    QCOMPARE(first.nearbyDeviceCount(), 0);
    QCOMPARE(second.nearbyDeviceCount(), 0);
    QVERIFY(!first.anotherVincentUserDetected());
    QVERIFY(!second.anotherVincentUserDetected());
}

void tst_NearbyVincentDiscovery::applicationStartsDiscoveryAfterShowingTheWindow()
{
    const QString mainPath = QFINDTESTDATA("../App/main.cpp");
    QVERIFY2(!mainPath.isEmpty(), "App/main.cpp test data was not found");
    QFile mainFile(mainPath);
    QVERIFY(mainFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainFile.readAll());

    QVERIFY(mainSource.contains(
        QStringLiteral("setContextProperty(\"VincentNearbyDiscovery\", nearbyDiscovery)")));
    QVERIFY(mainSource.contains(
        QStringLiteral("setContextProperty(\"VincentApplicationPreferences\",")));
    QVERIFY(mainSource.contains(
        QStringLiteral("&ApplicationPreferences::discoverNearbyVincentUsersChanged")));
    const qsizetype showIndex = mainSource.indexOf(QStringLiteral("showLaunchWindow(engine);"));
    const qsizetype startIndex = mainSource.indexOf(QStringLiteral(
        "QTimer::singleShot(0, nearbyDiscovery, [applicationPreferences, nearbyDiscovery]()"));
    QVERIFY(showIndex >= 0);
    QVERIFY(startIndex > showIndex);
    QVERIFY(mainSource.contains(
        QStringLiteral("if (applicationPreferences->discoverNearbyVincentUsers())")));
    QVERIFY(mainSource.contains(QStringLiteral("nearbyDiscovery->start();")));
    QVERIFY(mainSource.contains(QStringLiteral("nearbyDiscovery->stop();")));

    const QString implementationPath =
        QFINDTESTDATA("../App/models/network/nearbyvincentdiscovery.cpp");
    QVERIFY2(!implementationPath.isEmpty(), "nearby discovery implementation was not found");
    QFile implementationFile(implementationPath);
    QVERIFY(implementationFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString implementationSource = QString::fromUtf8(implementationFile.readAll());
    QVERIFY(implementationSource.contains(QStringLiteral("moveToThread(&m_workerThread)")));
    QVERIFY(implementationSource.contains(
        QStringLiteral("Q_ASSERT(QThread::currentThread() == thread())")));
    QVERIFY(implementationSource.contains(
        QStringLiteral("QMetaObject::invokeMethod(m_worker, &NearbyVincentDiscoveryWorker::start, "
                       "Qt::QueuedConnection)")));
}

QTEST_GUILESS_MAIN(tst_NearbyVincentDiscovery)

#include "tst_nearbyvincentdiscovery.moc"
