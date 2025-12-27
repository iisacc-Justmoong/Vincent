#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QJSEngine>
#include <QQmlApplicationEngine>
#include <QQmlEngine>

#include "paletteutils.h"

extern "C" void mac_unifyTitlebar(QWindow *qw);

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    qmlRegisterSingletonType<PaletteUtils>(
        "Vincent",
        2,
        0,
        "PaletteUtils",
        [](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject * {
            Q_UNUSED(scriptEngine)
            return new PaletteUtils(engine);
        });

    QQmlApplicationEngine engine;
    const auto craftRoot = QString::fromLocal8Bit(qgetenv("CRAFTROOT"));

    if (!craftRoot.isEmpty()) {
        const QStringList candidateImportPaths = {
            craftRoot + QStringLiteral("/qml"),
            craftRoot + QStringLiteral("/lib/qml")
        };
        for (const QString &importPath : candidateImportPaths) {
            if (QDir(importPath).exists()) {
                engine.addImportPath(importPath);
            }
        }
    }

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Vincent", "Main");

    return app.exec();
}
