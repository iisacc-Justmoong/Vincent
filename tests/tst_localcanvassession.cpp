#include <QSignalSpy>
#include <QRandomGenerator>
#include <QUuid>
#include <QtTest>

#include "localcanvassession.h"
#include "nearbyvincentdiscovery.h"
#include "recentcanvascontainer.h"

namespace
{
QByteArray canvasSnapshot(const QByteArray& marker)
{
    RecentCanvasContainer container;
    container.sharedCanvasDocument = marker;
    container.backgroundLayerPresent = true;
    return encodeRecentCanvasContainer(container);
}
} // namespace

class tst_LocalCanvasSession : public QObject
{
    Q_OBJECT

  private slots:
    void hostAndClientSynchronizeVersionedSnapshots();
    void invitationDecisionControlsCanvasConnection();
    void invalidSnapshotsAndEndpointsAreRejected();
};

void tst_LocalCanvasSession::hostAndClientSynchronizeVersionedSnapshots()
{
    LocalCanvasSession host(nullptr);
    QSignalSpy hostSnapshotSpy(&host, &LocalCanvasSession::snapshotReceived);
    QVERIFY(host.startHosting(QStringLiteral("Host Artist")));
    QVERIFY(host.hosting());
    QVERIFY(host.listenPort() > 0);

    const QByteArray initialSnapshot = canvasSnapshot(QByteArrayLiteral("host-initial-iisc"));
    QVERIFY(!initialSnapshot.isEmpty());
    QVERIFY(host.publishSnapshot(initialSnapshot));
    QCOMPARE(host.revision(), quint64(1));

    LocalCanvasSession client(nullptr);
    QSignalSpy clientSnapshotSpy(&client, &LocalCanvasSession::snapshotReceived);
    QVERIFY(client.joinCanvasAt(host.sessionId(), QStringLiteral("127.0.0.1"), host.listenPort(),
                                QStringLiteral("Guest Artist")));
    QTRY_VERIFY_WITH_TIMEOUT(client.connected(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(clientSnapshotSpy.size(), 1, 5000);
    QCOMPARE(clientSnapshotSpy.first().at(0).toByteArray(), initialSnapshot);
    QCOMPARE(client.revision(), quint64(1));

    QTRY_COMPARE_WITH_TIMEOUT(host.participantCount(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(client.participantCount(), 1, 5000);
    const QVariantMap hostViewOfGuest = host.participantProfiles().first().toMap();
    QCOMPARE(hostViewOfGuest.value(QStringLiteral("profileName")).toString(),
             QStringLiteral("Guest Artist"));
    QVERIFY(hostViewOfGuest.value(QStringLiteral("removable")).toBool());
    const QVariantMap clientViewOfHost = client.participantProfiles().first().toMap();
    QCOMPARE(clientViewOfHost.value(QStringLiteral("profileName")).toString(),
             QStringLiteral("Host Artist"));
    QVERIFY(clientViewOfHost.value(QStringLiteral("isHost")).toBool());
    QVERIFY(!client.currentUserIsHost());

    client.setLocalProfileName(QStringLiteral("Guest Renamed"));
    QTRY_COMPARE_WITH_TIMEOUT(
        host.participantProfiles().first().toMap().value(QStringLiteral("profileName")).toString(),
        QStringLiteral("Guest Renamed"), 5000);

    const QByteArray guestSnapshot = canvasSnapshot(QByteArrayLiteral("guest-edit-iisc"));
    QVERIFY(client.publishSnapshot(guestSnapshot));
    QTRY_COMPARE_WITH_TIMEOUT(hostSnapshotSpy.size(), 1, 5000);
    QCOMPARE(hostSnapshotSpy.first().at(0).toByteArray(), guestSnapshot);
    QCOMPARE(hostSnapshotSpy.first().at(2).toString(), client.peerId());
    QTRY_COMPARE_WITH_TIMEOUT(host.revision(), quint64(2), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(client.revision(), quint64(2), 5000);

    QVERIFY(host.removeParticipant(client.peerId()));
    QTRY_VERIFY_WITH_TIMEOUT(!client.active(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(host.participantCount(), 0, 5000);
    QVERIFY(client.errorString().contains(QStringLiteral("removed"), Qt::CaseInsensitive));
}

void tst_LocalCanvasSession::invitationDecisionControlsCanvasConnection()
{
    NearbyVincentDiscovery::Configuration configuration;
    configuration.port = static_cast<quint16>(49152 + QRandomGenerator::global()->bounded(12000));
    configuration.heartbeatIntervalMs = 100;
    configuration.peerTimeoutMs = 700;
    configuration.pruneIntervalMs = 50;
    configuration.ignoreLocalSenders = false;
    configuration.includeLoopbackInterfaces = true;

    NearbyVincentDiscovery hostDiscovery(configuration);
    NearbyVincentDiscovery guestDiscovery(configuration);
    LocalCanvasSession host(&hostDiscovery);
    LocalCanvasSession guest(&guestDiscovery);
    guest.setInvitationsAllowed(true);
    QSignalSpy pendingInvitationSpy(&guest, &LocalCanvasSession::pendingInvitationsChanged);

    hostDiscovery.start();
    guestDiscovery.start();
    QTRY_VERIFY_WITH_TIMEOUT(hostDiscovery.running(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(guestDiscovery.running(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(host.availableInvitees().size(), 1, 5000);

    QVERIFY(host.invitePeer(guestDiscovery.sessionId(), QStringLiteral("Host Artist")));
    QTRY_COMPARE_WITH_TIMEOUT(guest.pendingInvitationCount(), 1, 5000);
    QVERIFY(!pendingInvitationSpy.isEmpty());
    QCOMPARE(guest.pendingInvitation().value(QStringLiteral("profileName")).toString(),
             QStringLiteral("Host Artist"));
    QCOMPARE(guest.pendingInvitation().value(QStringLiteral("sessionId")).toString(),
             hostDiscovery.sessionId());
    QVERIFY(host.hosting());

    QVERIFY(guest.respondToPendingInvitation(false, QStringLiteral("Guest Artist")));
    QCOMPARE(guest.pendingInvitationCount(), 0);
    QVERIFY(!guest.active());

    QVERIFY(host.invitePeer(guestDiscovery.sessionId(), QStringLiteral("Host Artist")));
    QTRY_COMPARE_WITH_TIMEOUT(guest.pendingInvitationCount(), 1, 5000);
    QVERIFY(guest.respondToPendingInvitation(true, QStringLiteral("Guest Artist")));
    QTRY_VERIFY_WITH_TIMEOUT(guest.connected(), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(host.participantCount(), 1, 5000);
    QCOMPARE(guest.pendingInvitationCount(), 0);

    hostDiscovery.stop();
    guestDiscovery.stop();
}

void tst_LocalCanvasSession::invalidSnapshotsAndEndpointsAreRejected()
{
    LocalCanvasSession session(nullptr);
    QVERIFY(!session.publishSnapshot(QByteArrayLiteral("not-a-vincent-canvas")));
    QVERIFY(!session.errorString().isEmpty());
    QCOMPARE(session.revision(), quint64(0));

    QVERIFY(!session.joinCanvasAt(QStringLiteral("not-a-session"), QStringLiteral("127.0.0.1"),
                                  12345, QStringLiteral("Guest")));
    QVERIFY(!session.joinCanvasAt(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                  QStringLiteral("239.255.1.1"), 12345, QStringLiteral("Guest")));
    QVERIFY(!session.joinCanvasAt(QUuid::createUuid().toString(QUuid::WithoutBraces),
                                  QStringLiteral("203.0.113.1"), 12345, QStringLiteral("Guest")));
    QVERIFY(!session.active());
}

QTEST_GUILESS_MAIN(tst_LocalCanvasSession)

#include "tst_localcanvassession.moc"
