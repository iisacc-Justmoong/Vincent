#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QWindow>
#include <QQuickStyle>
#include <QtQml>
#include <qqml.h>

#include "models/canvas/canvasdocumentviewmodel.h"
#include "models/painting/drawingsurfaceitem.h"
#include "models/brush/paletteutils.h"

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void qml_register_types_LVRS();

namespace {

void appendStartupLog(const QString &message)
{
    QString logDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (logDirectory.isEmpty()) {
        logDirectory = QDir::tempPath();
    }
    if (logDirectory.isEmpty()) {
        logDirectory = QString::fromLocal8Bit(qgetenv("TEMP"));
    }
    if (logDirectory.isEmpty()) {
        return;
    }

    QDir().mkpath(logDirectory);
    QFile logFile(logDirectory + QStringLiteral("/Vincent-startup.log"));
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&logFile);
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << ' ' << message << '\n';
    stream.flush();
}

void writeStartupLog(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QString logDirectory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (logDirectory.isEmpty()) {
        logDirectory = QDir::tempPath();
    }
    if (logDirectory.isEmpty()) {
        logDirectory = QString::fromLocal8Bit(qgetenv("TEMP"));
    }
    if (logDirectory.isEmpty()) {
        return;
    }

    QDir().mkpath(logDirectory);
    QFile logFile(logDirectory + QStringLiteral("/Vincent-startup.log"));
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&logFile);
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODate) << ' ';
    switch (type) {
    case QtDebugMsg:
        stream << "debug";
        break;
    case QtInfoMsg:
        stream << "info";
        break;
    case QtWarningMsg:
        stream << "warning";
        break;
    case QtCriticalMsg:
        stream << "critical";
        break;
    case QtFatalMsg:
        stream << "fatal";
        break;
    }

    stream << ": " << message;
    if (context.file) {
        stream << " (" << context.file << ':' << context.line << ')';
    }
    stream << '\n';
    stream.flush();
}

QStringList collectCandidateImportPaths()
{
    QStringList candidateImportPaths;

    const QString bundledQmlPath = QCoreApplication::applicationDirPath() + QStringLiteral("/qml");
    candidateImportPaths << bundledQmlPath;
    const bool hasBundledLvrs = QDir(bundledQmlPath + QStringLiteral("/LVRS")).exists();

    const QString craftRoot = QString::fromLocal8Bit(qgetenv("CRAFTROOT"));
    if (!craftRoot.isEmpty()) {
        candidateImportPaths << QStringList{
            craftRoot + QStringLiteral("/qml"),
            craftRoot + QStringLiteral("/lib/qml")
        };
    }

    const QString lvrsHostPrefix = QString::fromLocal8Bit(qgetenv("LVRS_HOST_PREFIX"));
    if (!hasBundledLvrs && !lvrsHostPrefix.isEmpty()) {
        candidateImportPaths << (lvrsHostPrefix + QStringLiteral("/lib/qt6/qml"));
    }

    if (!hasBundledLvrs) {
        const QString homePath = QDir::homePath();
        candidateImportPaths << QStringList{
            homePath + QStringLiteral("/.local/LVRS/platforms/macos/lib/qt6/qml"),
            homePath + QStringLiteral("/.local/LVRS/platforms/linux/lib/qt6/qml"),
            homePath + QStringLiteral("/.local/LVRS/platforms/windows/lib/qt6/qml")
        };
    }

    return candidateImportPaths;
}

void configureEngineImports(QQmlApplicationEngine &engine)
{
    const QStringList candidateImportPaths = collectCandidateImportPaths();

    QSet<QString> dedupedImportPaths;
    for (const QString &importPath : candidateImportPaths) {
        if (importPath.isEmpty() || dedupedImportPaths.contains(importPath)) {
            continue;
        }

        dedupedImportPaths.insert(importPath);
        if (QDir(importPath).exists()) {
            engine.addImportPath(importPath);
        }
    }
}

void registerViewModels(QQmlApplicationEngine &engine, PaletteUtils *paletteUtils)
{
    QObject *registry = engine.singletonInstance<QObject *>(QStringLiteral("LVRS"),
                                                            QStringLiteral("ViewModels"));
    if (!registry) {
        return;
    }

    auto *documentViewModel = new CanvasDocumentViewModel(paletteUtils, registry);
    QMetaObject::invokeMethod(registry,
                              "set",
                              Q_ARG(QString, QStringLiteral("CanvasDocument")),
                              Q_ARG(QObject *, documentViewModel));
}

void ensureLaunchWindowVisible(QQmlApplicationEngine &engine)
{
    const QObjectList rootObjects = engine.rootObjects();
    appendStartupLog(QStringLiteral("ensureLaunchWindowVisible root object count %1")
                         .arg(rootObjects.size()));
    if (rootObjects.isEmpty()) {
        return;
    }

    auto *window = qobject_cast<QWindow *>(rootObjects.first());
    appendStartupLog(QStringLiteral("ensureLaunchWindowVisible root object %1 is window %2")
                         .arg(QString::fromLatin1(rootObjects.first()->metaObject()->className()),
                              window ? QStringLiteral("true") : QStringLiteral("false")));
    if (!window) {
        return;
    }

    const QSize fallbackSize(1400, 880);
    if (window->width() <= 0 || window->height() <= 0) {
        window->resize(fallbackSize);
    }
    window->setMinimumSize(QSize(640, 400));

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect availableGeometry = screen->availableGeometry();
        const QSize boundedSize = window->size().boundedTo(availableGeometry.size());
        if (boundedSize != window->size()) {
            window->resize(boundedSize);
        }
        if (!availableGeometry.contains(window->position())) {
            window->setPosition(availableGeometry.center() - QPoint(window->width() / 2,
                                                                    window->height() / 2));
        }
    }

    window->showNormal();
    window->raise();
    window->requestActivate();

#if defined(Q_OS_WIN)
    const HWND nativeWindow = reinterpret_cast<HWND>(window->winId());
    if (nativeWindow) {
        const BOOL showResult = ShowWindow(nativeWindow, SW_SHOWNORMAL);
        const BOOL positionResult = SetWindowPos(nativeWindow,
                                                 HWND_TOP,
                                                 window->x(),
                                                 window->y(),
                                                 window->width() > 0 ? window->width() : fallbackSize.width(),
                                                 window->height() > 0 ? window->height() : fallbackSize.height(),
                                                 SWP_SHOWWINDOW);
        appendStartupLog(QStringLiteral("ensureLaunchWindowVisible native window 0x%1 size %2x%3 showResult %4 positionResult %5 lastError %6")
                             .arg(reinterpret_cast<quintptr>(nativeWindow), 0, 16)
                             .arg(window->width())
                             .arg(window->height())
                             .arg(showResult)
                             .arg(positionResult)
                             .arg(GetLastError()));
    }
#endif
}

} // namespace

int main(int argc, char *argv[])
{
    qInstallMessageHandler(writeStartupLog);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Vincent"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    appendStartupLog(QStringLiteral("Vincent startup initialized"));

    qml_register_types_LVRS();
    appendStartupLog(QStringLiteral("LVRS QML types registered"));

    QQmlApplicationEngine engine;
    configureEngineImports(engine);
    qmlRegisterType<DrawingSurfaceItem>("Vincent", 2, 0, "DrawingSurfaceItem");

    auto *paletteUtils = new PaletteUtils(&engine);
    engine.rootContext()->setContextProperty("PaletteUtils", paletteUtils);
    registerViewModels(engine, paletteUtils);

    QObject::connect(&engine,
                     &QQmlApplicationEngine::objectCreationFailed,
                     &app,
                     []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);
    appendStartupLog(QStringLiteral("Loading Vincent.Main"));
    engine.loadFromModule(QStringLiteral("Vincent"), QStringLiteral("Main"));
    appendStartupLog(QStringLiteral("Loaded Vincent.Main root object count %1")
                         .arg(engine.rootObjects().size()));
    for (int delayMs : {0, 250, 1000, 2500}) {
        QTimer::singleShot(delayMs, &engine, [&engine]() {
            ensureLaunchWindowVisible(engine);
        });
    }

    return app.exec();
}
