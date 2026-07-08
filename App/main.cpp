#include <QDir>
#include <QGuiApplication>
#include <QMetaObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSet>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QQuickStyle>
#include <QtQml>
#include <qqml.h>

#include "models/canvas/canvasdocumentviewmodel.h"
#include "models/painting/drawingsurfaceitem.h"
#include "models/brush/paletteutils.h"

namespace {

QStringList collectCandidateImportPaths()
{
    QStringList candidateImportPaths;

    const QString craftRoot = QString::fromLocal8Bit(qgetenv("CRAFTROOT"));
    if (!craftRoot.isEmpty()) {
        candidateImportPaths << QStringList{
            craftRoot + QStringLiteral("/qml"),
            craftRoot + QStringLiteral("/lib/qml")
        };
    }

    const QString lvrsHostPrefix = QString::fromLocal8Bit(qgetenv("LVRS_HOST_PREFIX"));
    if (!lvrsHostPrefix.isEmpty()) {
        candidateImportPaths << (lvrsHostPrefix + QStringLiteral("/lib/qt6/qml"));
    }

    const QString homePath = QDir::homePath();
    candidateImportPaths << QStringList{
        homePath + QStringLiteral("/.local/LVRS/platforms/macos/lib/qt6/qml"),
        homePath + QStringLiteral("/.local/LVRS/platforms/linux/lib/qt6/qml"),
        homePath + QStringLiteral("/.local/LVRS/platforms/windows/lib/qt6/qml")
    };

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

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Vincent"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

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
    engine.loadFromModule(QStringLiteral("Vincent"), QStringLiteral("Main"));

    return app.exec();
}
