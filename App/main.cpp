#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSet>
#include <QStringList>
#include <QtQml>

#include <backend/state/viewmodelregistry.h>
#include <backend/runtime/appentry.h>

#include "brushengine.h"
#include "canvasbackend.h"
#include "canvasdocumentviewmodel.h"
#include "imageexport.h"
#include "imageimport.h"
#include "paletteutils.h"

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
    auto *registry = engine.singletonInstance<ViewModelRegistry *>(QStringLiteral("LVRS"),
                                                                   QStringLiteral("ViewModels"));
    if (!registry) {
        return;
    }

    auto *documentViewModel = new CanvasDocumentViewModel(paletteUtils, registry);
    registry->set(QStringLiteral("CanvasDocument"), documentViewModel);
}

} // namespace

int main(int argc, char *argv[])
{
    lvrs::QmlAppLaunchSpec launchSpec;
    launchSpec.bootstrap.applicationName = QStringLiteral("Vincent");
    launchSpec.bootstrap.quickStyleName = QStringLiteral("Basic");
    launchSpec.moduleUri = QStringLiteral("Vincent");
    launchSpec.rootObject = QStringLiteral("Main");
    launchSpec.configureEngine = [](QQmlApplicationEngine &engine) {
        configureEngineImports(engine);
        auto *paletteUtils = new PaletteUtils(&engine);
        engine.rootContext()->setContextProperty("CanvasBackend", new CanvasBackend(&engine));
        engine.rootContext()->setContextProperty("BrushEngine", new BrushEngine(&engine));
        engine.rootContext()->setContextProperty("ImageExport", new ImageExport(&engine));
        engine.rootContext()->setContextProperty("ImageImport", new ImageImport(&engine));
        engine.rootContext()->setContextProperty("PaletteUtils", paletteUtils);
        registerViewModels(engine, paletteUtils);
    };

    return lvrs::runBootstrappedQmlApp(argc, argv, launchSpec);
}
