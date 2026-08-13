#include <QFile>
#include <QString>
#include <QStringList>
#include <QtTest>

class tst_MainQmlContract : public QObject
{
    Q_OBJECT

private slots:
    void applicationWindowKeepsNativeControlsWhileUsingSolidVisualChrome();
    void applicationWindowUsesOnlyTheMacOsFullSizeTitleBarDragRegion();
    void applicationWindowPassesOnlyTheActiveMacOsDragHeightToCanvasPage();
    void applicationWindowCreatesAtFixedLaunchGeometry();
    void applicationWindowUsesDeferredCanvasIncubation();
    void applicationWindowRequiresOnlineLicenseBeforeCanvasIncubation();
    void applicationWindowProvidesApplicationMenuBar();
    void applicationMenuBarUsesNativeMacOsAndCompactThemedInWindowChromeElsewhere();
    void applicationMenuAssignsShortcutContracts();
    void applicationActionsAreTheOnlyOwnersOfPortableShortcuts();
};

void tst_MainQmlContract::applicationWindowKeepsNativeControlsWhileUsingSolidVisualChrome()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("LV.ApplicationWindow")));
    QVERIFY(mainSource.contains(QStringLiteral("solidChrome: true")));
    QVERIFY(!mainSource.contains(QStringLiteral("autoAttachRuntimeEvents: true")));
    QVERIFY(!mainSource.contains(QStringLiteral("flags:")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.CustomizeWindowHint")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.FramelessWindowHint")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.WindowTitleHint")));
}

void tst_MainQmlContract::applicationWindowUsesOnlyTheMacOsFullSizeTitleBarDragRegion()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("windowDragHandleEnabled: Qt.platform.os === \"osx\"")));
    QVERIFY(mainSource.contains(QStringLiteral("&& visibility !== QtQuickWindow.Window.FullScreen")));
    QVERIFY(!mainSource.contains(QStringLiteral("Qt.platform.os !== \"windows\"")));
    QVERIFY(!mainSource.contains(QStringLiteral("windowDragHandleEnabled: true")));
    QVERIFY(!mainSource.contains(QStringLiteral("windowDragHandleHeight: 0")));
    QVERIFY(!mainSource.contains(QStringLiteral("onTitlebarDragRequested: window.requestWindowMove()")));
    QVERIFY(!mainSource.contains(QStringLiteral("MouseArea")));
    QVERIFY(!mainSource.contains(QStringLiteral("DragHandler")));
}

void tst_MainQmlContract::applicationWindowPassesOnlyTheActiveMacOsDragHeightToCanvasPage()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("topChromeReservedHeight: window.windowDragHandleEnabled ? window.windowDragHandleHeight : 0")));
}

void tst_MainQmlContract::applicationWindowCreatesAtFixedLaunchGeometry()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("readonly property int initialWidth: 1400")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property int initialHeight: 880")));
    QVERIFY(mainSource.contains(QStringLiteral("width: initialWidth")));
    QVERIFY(mainSource.contains(QStringLiteral("height: initialHeight")));
    QVERIFY(mainSource.contains(QStringLiteral("visible: false")));
    QVERIFY(!mainSource.contains(QStringLiteral("visible: true")));
}

void tst_MainQmlContract::applicationWindowUsesDeferredCanvasIncubation()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("readonly property int minimumWindowWidth: 640")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property int minimumWindowHeight: 400")));
    QVERIFY(mainSource.contains(QStringLiteral("minimumWidth: minimumWindowWidth")));
    QVERIFY(mainSource.contains(QStringLiteral("minimumHeight: minimumWindowHeight")));
    QVERIFY(mainSource.contains(QStringLiteral("Loader {")));
    QVERIFY(mainSource.contains(QStringLiteral("id: painterPageLoader")));
    QVERIFY(mainSource.contains(QStringLiteral("property bool canvasIncubationRequested: false")));
    QVERIFY(mainSource.contains(QStringLiteral("asynchronous: true")));
    QVERIFY(mainSource.contains(QStringLiteral("sourceComponent: CanvasViews.PainterCanvasPage")));
    QVERIFY(mainSource.contains(QStringLiteral("Qt.callLater(function ()")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasIncubationRequested = true;")));
    QVERIFY(mainSource.contains(QStringLiteral("LV.Label {")));
}

void tst_MainQmlContract::applicationWindowRequiresOnlineLicenseBeforeCanvasIncubation()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    const QString activationPagePath = QFINDTESTDATA("../App/qml/license/LicenseActivationPage.qml");
    const QString appEntryPath = QFINDTESTDATA("../App/main.cpp");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!activationPagePath.isEmpty(), "LicenseActivationPage.qml test data was not found");
    QVERIFY2(!appEntryPath.isEmpty(), "App/main.cpp test data was not found");

    auto readSource = [](const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString{};
        }
        return QString::fromUtf8(file.readAll());
    };

    const QString mainSource = readSource(mainQmlPath);
    const QString activationSource = readSource(activationPagePath);
    const QString appEntrySource = readSource(appEntryPath);
    QVERIFY(!mainSource.isEmpty());
    QVERIFY(!activationSource.isEmpty());
    QVERIFY(!appEntrySource.isEmpty());

    QVERIFY(mainSource.contains(QStringLiteral("readonly property bool licenseGranted: VincentLicenseManager.licensed")));
    QVERIFY(mainSource.contains(QStringLiteral("active: window.canvasIncubationRequested && window.licenseGranted")));
    QVERIFY(mainSource.contains(QStringLiteral("LicenseViews.LicenseActivationPage")));
    QVERIFY(mainSource.contains(QStringLiteral("visible: !window.licenseGranted")));
    QVERIFY(mainSource.contains(QStringLiteral("objectName: \"licensePersistenceWarning\"")));
    QVERIFY(mainSource.contains(QStringLiteral("window.licenseGranted && VincentLicenseManager.resultCode === \"secure_storage_unavailable\"")));
    QVERIFY(mainSource.contains(QStringLiteral("visible: window.licenseGranted && painterPageLoader.status !== Loader.Ready")));

    QVERIFY(activationSource.contains(QStringLiteral("LV.AppCard")));
    QCOMPARE(activationSource.count(QStringLiteral("LV.InputField")), 2);
    QVERIFY(activationSource.contains(QStringLiteral("objectName: \"licenseEmailInput\"")));
    QVERIFY(activationSource.contains(QStringLiteral("objectName: \"licenseKeyInput\"")));
    QVERIFY(activationSource.contains(QStringLiteral("maximumLength: 254")));
    QVERIFY(activationSource.contains(QStringLiteral("echoMode: TextInput.Password")));
    QVERIFY(activationSource.contains(QStringLiteral("Qt.ImhSensitiveData | Qt.ImhNoPredictiveText")));
    QVERIFY(activationSource.contains(QStringLiteral("Accessible.name: qsTr(\"Verified account email\")")));
    QVERIFY(activationSource.contains(QStringLiteral("Accessible.name: qsTr(\"Vincent license key\")")));
    QVERIFY(activationSource.contains(QStringLiteral("Accessible.name: qsTr(\"Activate Vincent\")")));
    QVERIFY(activationSource.contains(QStringLiteral("VincentLicenseManager.validateLicense(emailInput.text, licenseKeyInput.text)")));
    QVERIFY(activationSource.contains(QStringLiteral("visible: VincentLicenseManager.hasStoredLicense")));
    QVERIFY(activationSource.contains(QStringLiteral("VincentLicenseManager.retryStoredLicense()")));
    QVERIFY(activationSource.contains(QStringLiteral("VincentLicenseManager.forgetLicense()")));
    QVERIFY(activationSource.contains(QStringLiteral("objectName: \"retryStoredLicenseButton\"")));
    QVERIFY(activationSource.contains(QStringLiteral("objectName: \"forgetStoredLicenseButton\"")));
    QVERIFY(!mainSource.contains(QStringLiteral("VincentLicenseManager.forgetLicense()")));
    QVERIFY(activationSource.contains(QStringLiteral("text: qsTr(\"Vincent\")")));
    QVERIFY(!activationSource.contains(QStringLiteral("productIdInput")));
    QVERIFY(!activationSource.contains(QStringLiteral("productIdField")));

    QVERIFY(appEntrySource.contains(QStringLiteral("new LicenseManager(&engine)")));
    QVERIFY(appEntrySource.contains(QStringLiteral("setContextProperty(\"VincentLicenseManager\", licenseManager)")));
}

void tst_MainQmlContract::applicationWindowProvidesApplicationMenuBar()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("import QtQuick.Controls as Controls")));
    QVERIFY(mainSource.contains(QStringLiteral("import QtQuick.Window as QtQuickWindow")));
    QVERIFY(mainSource.contains(QStringLiteral("menuBar: Controls.MenuBar")));
    const qsizetype fileMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"File\")"));
    const qsizetype editMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Edit\")"));
    const qsizetype windowMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Window\")"));
    const qsizetype helpMenuIndex = mainSource.indexOf(QStringLiteral("title: qsTr(\"Help\")"));
    QVERIFY(fileMenuIndex >= 0);
    QVERIFY(editMenuIndex > fileMenuIndex);
    QVERIFY(windowMenuIndex > editMenuIndex);
    QVERIFY(helpMenuIndex > windowMenuIndex);
    QVERIFY(mainSource.contains(QStringLiteral("function requestNewCanvas()")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openNewCanvasDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openFileDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.openSaveDialog();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.undoActiveRasterSurface();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.redoActiveRasterSurface();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.addLayer();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.deleteCurrentLayer();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.setToolMode(toolMode);")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.selectShapeTool(shapeKind);")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.fitCanvasToWindow();")));
    QVERIFY(mainSource.contains(QStringLiteral("window.canvasPage.resetCanvasView();")));
    QVERIFY(mainSource.contains(QStringLiteral("QtQuickWindow.Window.FullScreen")));
    QVERIFY(mainSource.contains(QStringLiteral("Qt.quit()")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"New Canvas...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Open Image...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Save Image As...\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Clear Canvas\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Undo\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Redo\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Add Layer\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Delete Current Layer\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Tools\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Shape Kind\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Fit Canvas to Window\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Reset Canvas View\")")));
    QVERIFY(mainSource.contains(QStringLiteral("title: qsTr(\"Keyboard Shortcuts\")")));
    QVERIFY(mainSource.contains(QStringLiteral("text: qsTr(\"Vincent %1\").arg(Qt.application.version)")));
}

void tst_MainQmlContract::applicationMenuBarUsesNativeMacOsAndCompactThemedInWindowChromeElsewhere()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("id: applicationMenuBar")));
    QVERIFY(mainSource.contains(QStringLiteral("implicitHeight: LV.Theme.controlHeightSm")));
    QVERIFY(mainSource.contains(QStringLiteral("palette.button: window.windowColor")));
    QVERIFY(mainSource.contains(QStringLiteral("delegate: Controls.MenuBarItem")));
    QVERIFY(mainSource.contains(QStringLiteral("topPadding: LV.Theme.gap2")));
    QVERIFY(mainSource.contains(QStringLiteral("bottomPadding: LV.Theme.gap2")));
    QVERIFY(mainSource.contains(QStringLiteral("contentItem: LV.Label")));
    QVERIFY(mainSource.contains(QStringLiteral("color: window.windowColor")));

    const QString appEntryPath = QFINDTESTDATA("../App/main.cpp");
    QVERIFY2(!appEntryPath.isEmpty(), "App/main.cpp test data was not found");
    QFile appEntry(appEntryPath);
    QVERIFY(appEntry.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString appEntrySource = QString::fromUtf8(appEntry.readAll());
    QVERIFY(appEntrySource.contains(QStringLiteral("QGuiApplication::setApplicationVersion(QStringLiteral(VINCENT_VERSION))")));
    QVERIFY(!appEntrySource.contains(QStringLiteral("AA_DontUseNativeMenuBar")));
}

void tst_MainQmlContract::applicationMenuAssignsShortcutContracts()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");

    QFile mainQml(mainQmlPath);
    QVERIFY(mainQml.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mainSource = QString::fromUtf8(mainQml.readAll());

    QVERIFY(mainSource.contains(QStringLiteral("readonly property string menuCommandModifier: Qt.platform.os === \"osx\" ? \"Meta\" : \"Ctrl\"")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property string shortcutSaveImageAs: menuCommandModifier + \"+S\"")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property string shortcutRedo: Qt.platform.os === \"osx\" ? \"Meta+Shift+Z\" : (Qt.platform.os === \"windows\" ? \"Ctrl+Y\" : \"Ctrl+Shift+Z\")")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property string shortcutToggleFullScreen: Qt.platform.os === \"osx\" ? \"Ctrl+Meta+F\" : \"F11\"")));
    QVERIFY(mainSource.contains(QStringLiteral("function shortcutReference(commandName, shortcutText)")));

    const QStringList actionShortcutContracts = {
            QStringLiteral("shortcutNewCanvas|newCanvasAction"),
            QStringLiteral("shortcutOpenImage|openImageAction"),
            QStringLiteral("shortcutSaveImageAs|saveImageAsAction"),
            QStringLiteral("shortcutClearCanvas|clearCanvasAction"),
            QStringLiteral("shortcutQuit|quitAction"),
            QStringLiteral("shortcutUndo|undoAction"),
            QStringLiteral("shortcutRedo|redoAction"),
            QStringLiteral("shortcutAddLayer|addLayerAction"),
            QStringLiteral("shortcutDeleteCurrentLayer|deleteCurrentLayerAction"),
            QStringLiteral("shortcutBrushTool|brushToolAction"),
            QStringLiteral("shortcutEraserTool|eraserToolAction"),
            QStringLiteral("shortcutHandPanTool|handPanToolAction"),
            QStringLiteral("shortcutMoveTool|moveToolAction"),
            QStringLiteral("shortcutZoomTool|zoomToolAction"),
            QStringLiteral("shortcutShapeTool|shapeToolAction"),
            QStringLiteral("shortcutFillTool|fillToolAction"),
            QStringLiteral("shortcutTextTool|textToolAction"),
            QStringLiteral("shortcutRectangleShape|rectangleShapeAction"),
            QStringLiteral("shortcutEllipseShape|ellipseShapeAction"),
            QStringLiteral("shortcutTriangleShape|triangleShapeAction"),
            QStringLiteral("shortcutDiamondShape|diamondShapeAction"),
            QStringLiteral("shortcutStarShape|starShapeAction"),
            QStringLiteral("shortcutRectangleBubbleShape|rectangleBubbleShapeAction"),
            QStringLiteral("shortcutEllipseBubbleShape|ellipseBubbleShapeAction"),
            QStringLiteral("shortcutDecreaseBrushSize|decreaseBrushSizeAction"),
            QStringLiteral("shortcutIncreaseBrushSize|increaseBrushSizeAction"),
            QStringLiteral("shortcutFitCanvasToWindow|fitCanvasToWindowAction"),
            QStringLiteral("shortcutResetCanvasView|resetCanvasViewAction"),
            QStringLiteral("shortcutMinimizeWindow|minimizeWindowAction"),
            QStringLiteral("shortcutToggleFullScreen|toggleFullScreenAction"),
    };

    QVERIFY(mainSource.contains(QStringLiteral("Controls.Action {")));
    QVERIFY(!mainSource.contains(QStringLiteral("Shortcut {")));

    for (const QString &actionShortcutContract : actionShortcutContracts) {
        const QStringList parts = actionShortcutContract.split(QLatin1Char('|'));
        QCOMPARE(parts.size(), 2);
        const QString shortcutContract = parts.at(0);
        const QString actionId = parts.at(1);

        QVERIFY2(mainSource.contains(QStringLiteral("readonly property string ") + shortcutContract),
                 qPrintable(shortcutContract + QStringLiteral(" property is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("id: ") + actionId),
                 qPrintable(actionId + QStringLiteral(" action is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("shortcut: window.") + shortcutContract),
                 qPrintable(shortcutContract + QStringLiteral(" action shortcut is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral("action: ") + actionId),
                 qPrintable(actionId + QStringLiteral(" menu binding is missing")));
        QVERIFY2(mainSource.contains(QStringLiteral(", window.") + shortcutContract + QStringLiteral(")")),
                 qPrintable(shortcutContract + QStringLiteral(" help reference is missing")));
    }
}

void tst_MainQmlContract::applicationActionsAreTheOnlyOwnersOfPortableShortcuts()
{
    const QString mainQmlPath = QFINDTESTDATA("../App/qml/Main.qml");
    const QString pageQmlPath = QFINDTESTDATA("../App/qml/canvas/PainterCanvasPage.qml");
    const QString toolbarQmlPath = QFINDTESTDATA("../App/qml/brush/CanvasToolBar.qml");
    const QString surfaceQmlPath = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!mainQmlPath.isEmpty(), "Main.qml test data was not found");
    QVERIFY2(!pageQmlPath.isEmpty(), "PainterCanvasPage.qml test data was not found");
    QVERIFY2(!toolbarQmlPath.isEmpty(), "CanvasToolBar.qml test data was not found");
    QVERIFY2(!surfaceQmlPath.isEmpty(), "DrawingSurface.qml test data was not found");

    auto readSource = [](const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QString{};
        }
        return QString::fromUtf8(file.readAll());
    };

    const QString mainSource = readSource(mainQmlPath);
    const QString pageSource = readSource(pageQmlPath);
    const QString toolbarSource = readSource(toolbarQmlPath);
    const QString surfaceSource = readSource(surfaceQmlPath);
    QVERIFY(!mainSource.isEmpty());
    QVERIFY(!pageSource.isEmpty());
    QVERIFY(!toolbarSource.isEmpty());
    QVERIFY(!surfaceSource.isEmpty());

    QVERIFY(pageSource.contains(QStringLiteral("readonly property bool dialogActive: canvasToolBar.dialogActive")));
    QVERIFY(pageSource.contains(QStringLiteral("readonly property bool textEditingActive: drawingSurface.textEditingActive || painterPage.layerRenameActive")));
    QVERIFY(pageSource.contains(QStringLiteral("toolShortcutsEnabled: !painterPage.layerRenameActive && !canvasToolBar.dialogActive")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property bool canvasCommandsEnabled")));
    QVERIFY(mainSource.contains(QStringLiteral("readonly property bool canvasEditingCommandsEnabled")));
    QVERIFY(mainSource.contains(QStringLiteral("enabled: window.canvasCommandsEnabled")));
    QVERIFY(mainSource.contains(QStringLiteral("enabled: window.canvasEditingCommandsEnabled")));

    QVERIFY(!toolbarSource.contains(QStringLiteral("sequence: StandardKey.New")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("sequence: StandardKey.Open")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("sequence: StandardKey.Save")));
    QVERIFY(!toolbarSource.contains(QStringLiteral("Meta+Shift+K")));

    const QStringList latinToolSequences = {
            QStringLiteral("sequences: [\"B\", \"ㅠ\"]"),
            QStringLiteral("sequences: [\"E\", \"ㄷ\"]"),
            QStringLiteral("sequences: [\"H\", \"ㅗ\"]"),
            QStringLiteral("sequences: [\"V\", \"ㅍ\"]"),
            QStringLiteral("sequences: [\"Z\", \"ㅋ\"]"),
            QStringLiteral("sequences: [\"U\", \"ㅕ\"]"),
            QStringLiteral("sequences: [\"G\", \"ㅎ\"]"),
            QStringLiteral("sequences: [\"T\", \"ㅅ\"]"),
    };
    for (const QString &sequence : latinToolSequences) {
        QVERIFY2(!surfaceSource.contains(sequence), qPrintable(sequence));
    }
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㅠ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("sequences: [\"ㄷ\"]")));
    QVERIFY(surfaceSource.contains(QStringLiteral("enabled: surface.toolShortcutsEnabled && !surface.textEditingActive")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [\"[\"]")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [\"]\"]")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [StandardKey.Undo]")));
    QVERIFY(!surfaceSource.contains(QStringLiteral("sequences: [StandardKey.Redo]")));
}

QTEST_APPLESS_MAIN(tst_MainQmlContract)

#include "tst_mainqmlcontract.moc"
