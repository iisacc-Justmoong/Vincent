#include <QFile>
#include <QString>
#include <QtTest>

namespace
{
QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    return QString::fromUtf8(file.readAll());
}
}

class tst_WindowsBuildWorkflowContract : public QObject
{
    Q_OBJECT

private slots:
    void windowsBuildScriptDefinesRunnablePackageContract();
    void cmakeHasWindowsInstallAndPackageRules();
    void appEntryPointAvoidsNonExportedLvrsRuntimeSymbols();
    void buildGuideDocumentsWindowsScript();
};

void tst_WindowsBuildWorkflowContract::windowsBuildScriptDefinesRunnablePackageContract()
{
    const QString scriptPath = QFINDTESTDATA("../build-windows.ps1");
    QVERIFY2(!scriptPath.isEmpty(), "build-windows.ps1 test data was not found");
    const QString source = readTextFile(scriptPath);
    QVERIFY(!source.isEmpty());

    const QString gitignorePath = QFINDTESTDATA("../.gitignore");
    QVERIFY2(!gitignorePath.isEmpty(), ".gitignore test data was not found");
    const QString gitignoreSource = readTextFile(gitignorePath);
    QVERIFY(!gitignoreSource.contains(QStringLiteral("\nbuild-windows.ps1\n")));
    QVERIFY(!gitignoreSource.contains(QStringLiteral("\n/build-windows.ps1\n")));

    QVERIFY(source.contains(QStringLiteral("#Requires -Version 5.1")));
    QVERIFY(source.contains(QStringLiteral("Set-StrictMode -Version Latest")));
    QVERIFY(source.contains(QStringLiteral("$Version = \"4.0\"")));
    QVERIFY(source.contains(QStringLiteral("$BuildDir = Join-Path $RepositoryRoot \"build\"")));
    QVERIFY(source.contains(QStringLiteral("$MsiPath = Join-Path $BuildDir \"Vincent-$Version-Windows.msi\"")));
    QVERIFY(!source.contains(QStringLiteral("cmake-build-debug")));

    QVERIFY(source.contains(QStringLiteral("$env:QT_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("$env:LVRS_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("$env:IIPAINTENGINE_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("Resolve-QtPrefix")));
    QVERIFY(source.contains(QStringLiteral("Resolve-Objdump")));
    QVERIFY(source.contains(QStringLiteral("Assert-MinGwRuntimeCompatibility")));
    QVERIFY(source.contains(QStringLiteral("Import-VisualStudioEnvironment")));
    QVERIFY(source.contains(QStringLiteral("windeployqt.exe")));

    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"cmake.exe\"")));
    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"ninja.exe\"")));
    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"ctest.exe\"")));
    QVERIFY(source.contains(QStringLiteral("[switch]$CreateMsi")));
    QVERIFY(source.contains(QStringLiteral("Resolve-WixTools")));
    QVERIFY(source.contains(QStringLiteral("New-MsiInstaller")));
    QVERIFY(source.contains(QStringLiteral("Write-MsiProductFile")));
    QVERIFY(source.contains(QStringLiteral("AllowSameVersionUpgrades=\"yes\"")));
    QVERIFY(source.contains(QStringLiteral("InstallScope=\"perUser\"")));
    QVERIFY(source.contains(QStringLiteral("LocalAppDataFolder")));
    QVERIFY(source.contains(QStringLiteral("LocalProgramsFolder")));
    QVERIFY(source.contains(QStringLiteral("-sice:ICE38")));
    QVERIFY(source.contains(QStringLiteral("-sice:ICE64")));
    QVERIFY(source.contains(QStringLiteral("-sice:ICE91")));
    QVERIFY(!source.contains(QStringLiteral("ProgramFiles64Folder")));
    QVERIFY(source.contains(QStringLiteral("\"-S\", $RepositoryRoot")));
    QVERIFY(source.contains(QStringLiteral("\"-B\", $BuildDir")));
    QVERIFY(source.contains(QStringLiteral("\"-DCMAKE_PREFIX_PATH=$prefixPath\"")));
    QVERIFY(source.contains(QStringLiteral("Invoke-Native $CTest @(\"--test-dir\", $BuildDir, \"--output-on-failure\"")));
    QVERIFY(source.contains(QStringLiteral("\"--qmldir\", (Join-Path $RepositoryRoot \"App\\qml\")")));
    QVERIFY(source.contains(QStringLiteral("Copy-DependencyRuntimeFiles -Name \"LVRS\"")));
    QVERIFY(source.contains(QStringLiteral("Copy-DependencyRuntimeFiles -Name \"iiPaintEngine\"")));
    QVERIFY(source.contains(QStringLiteral("Copy-DependencyQmlImports -Name \"LVRS\"")));
    QVERIFY(source.contains(QStringLiteral("Remove-QmlResourcePreferDirectives -ModuleName \"LVRS\"")));
    QVERIFY(source.contains(QStringLiteral("$_ -notmatch '^\\s*prefer\\s+:/qt/qml/'")));
    QVERIFY(source.contains(QStringLiteral("Verify-WindowsStage -Directory $StageDir -ResolvedQtPrefix $QtPrefix")));
    QVERIFY(source.contains(QStringLiteral("libstdc++-6.dll")));
    QVERIFY(source.contains(QStringLiteral("__cxa_thread_atexit")));
    QVERIFY(source.contains(QStringLiteral("Rebuild those dependencies with the same MinGW kit as Qt")));
    QVERIFY(source.contains(QStringLiteral("Compress-Archive")));
    QVERIFY(source.contains(QStringLiteral("Vincent-$Version-Windows.zip")));
    QVERIFY(source.contains(QStringLiteral("Vincent-$Version-Windows.msi")));
    QVERIFY(source.contains(QStringLiteral("Remove-NonAsciiHarvestedFiles")));
    QVERIFY(source.contains(QStringLiteral("Install-ForCurrentUser")));
}

void tst_WindowsBuildWorkflowContract::cmakeHasWindowsInstallAndPackageRules()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");
    const QString source = readTextFile(cmakePath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("elseif(WIN32)\n    install(TARGETS Vincent RUNTIME DESTINATION \".\" COMPONENT Runtime)")));
    QVERIFY(source.contains(QStringLiteral("elseif(WIN32)\n    set(CPACK_GENERATOR \"ZIP\")")));
    QVERIFY(source.contains(QStringLiteral("set(CPACK_PACKAGE_FILE_NAME \"Vincent-${CPACK_PACKAGE_VERSION}-Windows\")")));
    QVERIFY(source.contains(QStringLiteral("if(WIN32)\n    add_custom_command(TARGET Vincent POST_BUILD")));
    QVERIFY(source.contains(QStringLiteral("if(WIN32 AND MINGW)")));
    QVERIFY(source.contains(QStringLiteral("libgcc_s_seh-1.dll")));
    QVERIFY(source.contains(QStringLiteral("libstdc++-6.dll")));
    QVERIFY(source.contains(QStringLiteral("libwinpthread-1.dll")));
    QVERIFY(source.contains(QStringLiteral("$<TARGET_FILE:LVRS::LVRS>")));
    QVERIFY(source.contains(QStringLiteral("$<TARGET_FILE:iiPaintEngine::iiPaintEngine>")));
    QVERIFY(source.contains(QStringLiteral("$<TARGET_FILE_DIR:Vincent>")));
    QVERIFY(source.contains(QStringLiteral("Copying Windows runtime DLLs next to Vincent.exe")));
    QVERIFY(source.contains(QStringLiteral("if(APPLE)\n    # productbuild")));
    QVERIFY(source.contains(QStringLiteral("set(CPACK_GENERATOR \"productbuild\")")));
    QVERIFY(source.contains(QStringLiteral("set_source_files_properties(\"${psd_sdk_SOURCE_DIR}/src/Psd/Psdminiz.c\" PROPERTIES LANGUAGE CXX)")));

    const QString testsCmakePath = QFINDTESTDATA("../tests/CMakeLists.txt");
    QVERIFY2(!testsCmakePath.isEmpty(), "tests/CMakeLists.txt test data was not found");
    const QString testsSource = readTextFile(testsCmakePath);
    QVERIFY(testsSource.contains(QStringLiteral("ENVIRONMENT_MODIFICATION")));
    QVERIFY(testsSource.contains(QStringLiteral("PATH=path_list_prepend:$<TARGET_FILE_DIR:iiPaintEngine::iiPaintEngine>")));
}

void tst_WindowsBuildWorkflowContract::appEntryPointAvoidsNonExportedLvrsRuntimeSymbols()
{
    const QString mainPath = QFINDTESTDATA("../App/main.cpp");
    QVERIFY2(!mainPath.isEmpty(), "App/main.cpp test data was not found");
    const QString source = readTextFile(mainPath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("QGuiApplication app(argc, argv);")));
    QVERIFY(source.contains(QStringLiteral("qInstallMessageHandler(writeStartupLog);")));
    QVERIFY(source.contains(QStringLiteral("void appendStartupLog(const QString &message)")));
    QVERIFY(source.contains(QStringLiteral("Vincent-startup.log")));
    QVERIFY(source.contains(QStringLiteral("QDir::tempPath()")));
    QVERIFY(source.contains(QStringLiteral("qgetenv(\"TEMP\")")));
    QVERIFY(source.contains(QStringLiteral("Vincent startup initialized")));
    QVERIFY(source.contains(QStringLiteral("Loaded Vincent.Main root object count")));
    QVERIFY(source.contains(QStringLiteral("void qml_register_types_LVRS();")));
    QVERIFY(source.contains(QStringLiteral("qml_register_types_LVRS();")));
    QVERIFY(source.contains(QStringLiteral("ensureLaunchWindowVisible(engine);")));
    QVERIFY(source.contains(QStringLiteral("window->resize(fallbackSize);")));
    QVERIFY(source.contains(QStringLiteral("window->setMinimumSize(QSize(640, 400));")));
    QVERIFY(source.contains(QStringLiteral("ensureLaunchWindowVisible native window")));
    QVERIFY(source.contains(QStringLiteral("window->showNormal();")));
    QVERIFY(source.contains(QStringLiteral("SetWindowPos(nativeWindow,")));
    QVERIFY(source.contains(QStringLiteral("SWP_SHOWWINDOW")));
    QVERIFY(source.contains(QStringLiteral("QCoreApplication::applicationDirPath() + QStringLiteral(\"/qml\")")));
    QVERIFY(source.contains(QStringLiteral("const bool hasBundledLvrs")));
    QVERIFY(source.contains(QStringLiteral("if (!hasBundledLvrs && !lvrsHostPrefix.isEmpty())")));
    QVERIFY(source.contains(QStringLiteral("engine.loadFromModule(QStringLiteral(\"Vincent\"), QStringLiteral(\"Main\"))")));
    QVERIFY(source.contains(QStringLiteral("engine.singletonInstance<QObject *>(QStringLiteral(\"LVRS\"),")));
    QVERIFY(source.contains(QStringLiteral("QMetaObject::invokeMethod(registry,")));
    QVERIFY(!source.contains(QStringLiteral("runBootstrappedQmlApp")));
    QVERIFY(!source.contains(QStringLiteral("ViewModelRegistry *")));
    QVERIFY(!source.contains(QStringLiteral("#include <backend/runtime/appentry.h>")));
    QVERIFY(!source.contains(QStringLiteral("#include <backend/state/viewmodelregistry.h>")));
}

void tst_WindowsBuildWorkflowContract::buildGuideDocumentsWindowsScript()
{
    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");
    const QString source = readTextFile(buildGuidePath);
    QVERIFY(!source.isEmpty());

    QVERIFY(source.contains(QStringLiteral("## 1b. Windows Build, Package, and Current-User Install Script")));
    QVERIFY(source.contains(QStringLiteral("powershell -ExecutionPolicy Bypass -File .\\build-windows.ps1 -Clean")));
    QVERIFY(source.contains(QStringLiteral("powershell -ExecutionPolicy Bypass -File .\\build-windows.ps1 -InstallForCurrentUser")));
    QVERIFY(source.contains(QStringLiteral("QT_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("LVRS_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("IIPAINTENGINE_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("cmake -S . -B build")));
    QVERIFY(source.contains(QStringLiteral("ctest --test-dir build --output-on-failure")));
    QVERIFY(source.contains(QStringLiteral("windeployqt")));
    QVERIFY(source.contains(QStringLiteral("LVRS QML imports")));
    QVERIFY(source.contains(QStringLiteral("MinGW ABI")));
    QVERIFY(source.contains(QStringLiteral("__cxa_thread_atexit")));
    QVERIFY(source.contains(QStringLiteral("dist/Vincent-Windows")));
    QVERIFY(source.contains(QStringLiteral("dist/Vincent-4.0-Windows.zip")));
    QVERIFY(source.contains(QStringLiteral("build/Vincent-4.0-Windows.msi")));
    QVERIFY(source.contains(QStringLiteral("AllowSameVersionUpgrades")));
    QVERIFY(source.contains(QStringLiteral("current user's Start Menu")));
}

QTEST_APPLESS_MAIN(tst_WindowsBuildWorkflowContract)

#include "tst_windowsbuildworkflowcontract.moc"
