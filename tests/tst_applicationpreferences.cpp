#include "applicationpreferences.h"

#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class tst_ApplicationPreferences : public QObject
{
    Q_OBJECT

private slots:
    void defaultsToNewCanvasAndNearbyDiscovery();
    void generalChoicesPersistAcrossInstances();
    void successfulCanvasPathBecomesTheRecentCanvas();
    void missingRecentCanvasIsDiscardedOnReload();
    void nonLocalRecentCanvasIsRejected();
};

void tst_ApplicationPreferences::defaultsToNewCanvasAndNearbyDiscovery()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("preferences.ini")),
                       QSettings::IniFormat);

    ApplicationPreferences preferences(&settings);

    QVERIFY(!preferences.startWithRecentCanvas());
    QVERIFY(preferences.discoverNearbyVincentUsers());
    QVERIFY(preferences.recentCanvasUrl().isEmpty());
}

void tst_ApplicationPreferences::generalChoicesPersistAcrossInstances()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("preferences.ini"));

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        ApplicationPreferences preferences(&settings);
        QSignalSpy startupSpy(&preferences,
                              &ApplicationPreferences::startWithRecentCanvasChanged);
        QSignalSpy discoverySpy(
            &preferences,
            &ApplicationPreferences::discoverNearbyVincentUsersChanged);

        preferences.setStartWithRecentCanvas(true);
        preferences.setDiscoverNearbyVincentUsers(false);

        QCOMPARE(startupSpy.size(), 1);
        QCOMPARE(discoverySpy.size(), 1);
        QCOMPARE(settings.status(), QSettings::NoError);

        QFile persistedSettings(settingsPath);
        QVERIFY(persistedSettings.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray persistedData = persistedSettings.readAll();
        QVERIFY(persistedData.contains("startWithRecentCanvas=true"));
        QVERIFY(persistedData.contains("discoverNearbyVincentUsers=false"));
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    ApplicationPreferences restored(&restoredSettings);
    QVERIFY(restored.startWithRecentCanvas());
    QVERIFY(!restored.discoverNearbyVincentUsers());
}

void tst_ApplicationPreferences::successfulCanvasPathBecomesTheRecentCanvas()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString canvasPath = directory.filePath(QStringLiteral("recent.iisc"));
    QFile canvas(canvasPath);
    QVERIFY(canvas.open(QIODevice::WriteOnly));
    QVERIFY(canvas.write("canvas") > 0);
    canvas.close();

    const QString settingsPath = directory.filePath(QStringLiteral("preferences.ini"));
    QSettings settings(settingsPath, QSettings::IniFormat);
    ApplicationPreferences preferences(&settings);
    QSignalSpy recentCanvasSpy(&preferences,
                               &ApplicationPreferences::recentCanvasUrlChanged);

    QVERIFY(preferences.recordRecentCanvas(QUrl::fromLocalFile(canvasPath)));
    QCOMPARE(recentCanvasSpy.size(), 1);
    QCOMPARE(preferences.recentCanvasUrl(),
             QUrl::fromLocalFile(QFileInfo(canvasPath).canonicalFilePath()));

    settings.sync();
    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    ApplicationPreferences restored(&restoredSettings);
    QCOMPARE(restored.recentCanvasUrl(), preferences.recentCanvasUrl());
}

void tst_ApplicationPreferences::missingRecentCanvasIsDiscardedOnReload()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("preferences.ini"));
    const QString canvasPath = directory.filePath(QStringLiteral("removed.iisc"));
    QFile canvas(canvasPath);
    QVERIFY(canvas.open(QIODevice::WriteOnly));
    QVERIFY(canvas.write("canvas") > 0);
    canvas.close();

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        ApplicationPreferences preferences(&settings);
        QVERIFY(preferences.recordRecentCanvas(QUrl::fromLocalFile(canvasPath)));
        settings.sync();
    }
    QVERIFY(QFile::remove(canvasPath));

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    ApplicationPreferences restored(&restoredSettings);
    QVERIFY(restored.recentCanvasUrl().isEmpty());
    restoredSettings.sync();
    QVERIFY(!restoredSettings.contains(QStringLiteral("General/recentCanvasUrl")));
}

void tst_ApplicationPreferences::nonLocalRecentCanvasIsRejected()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("preferences.ini")),
                       QSettings::IniFormat);
    ApplicationPreferences preferences(&settings);

    QVERIFY(!preferences.recordRecentCanvas(
        QUrl(QStringLiteral("https://iisacc.com/not-a-local-canvas.iisc"))));
    QVERIFY(preferences.recentCanvasUrl().isEmpty());
    QVERIFY(!settings.contains(QStringLiteral("General/recentCanvasUrl")));
}

QTEST_APPLESS_MAIN(tst_ApplicationPreferences)

#include "tst_applicationpreferences.moc"
