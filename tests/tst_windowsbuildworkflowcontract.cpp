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
    QVERIFY(!source.contains(QStringLiteral("cmake-build-debug")));

    QVERIFY(source.contains(QStringLiteral("$env:QT_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("$env:LVRS_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("$env:IIPAINTENGINE_PREFIX")));
    QVERIFY(source.contains(QStringLiteral("Resolve-QtPrefix")));
    QVERIFY(source.contains(QStringLiteral("Import-VisualStudioEnvironment")));
    QVERIFY(source.contains(QStringLiteral("windeployqt.exe")));

    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"cmake.exe\"")));
    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"ninja.exe\"")));
    QVERIFY(source.contains(QStringLiteral("Get-RequiredCommand \"ctest.exe\"")));
    QVERIFY(source.contains(QStringLiteral("\"-S\", $RepositoryRoot")));
    QVERIFY(source.contains(QStringLiteral("\"-B\", $BuildDir")));
    QVERIFY(source.contains(QStringLiteral("\"-DCMAKE_PREFIX_PATH=$prefixPath\"")));
    QVERIFY(source.contains(QStringLiteral("Invoke-Native $CTest @(\"--test-dir\", $BuildDir, \"--output-on-failure\"")));

    QVERIFY(source.contains(QStringLiteral("\"--qmldir\", (Join-Path $RepositoryRoot \"App\\qml\")")));
    QVERIFY(source.contains(QStringLiteral("Copy-DependencyRuntimeFiles -Name \"LVRS\"")));
    QVERIFY(source.contains(QStringLiteral("Copy-DependencyRuntimeFiles -Name \"iiPaintEngine\"")));
    QVERIFY(source.contains(QStringLiteral("Copy-DependencyQmlImports -Name \"LVRS\"")));
    QVERIFY(source.contains(QStringLiteral("Verify-WindowsStage $StageDir")));
    QVERIFY(source.contains(QStringLiteral("Compress-Archive")));
    QVERIFY(source.contains(QStringLiteral("Vincent-$Version-Windows.zip")));
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
    QVERIFY(source.contains(QStringLiteral("if(APPLE)\n    # productbuild")));
    QVERIFY(source.contains(QStringLiteral("set(CPACK_GENERATOR \"productbuild\")")));
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
    QVERIFY(source.contains(QStringLiteral("dist/Vincent-Windows")));
    QVERIFY(source.contains(QStringLiteral("dist/Vincent-4.0-Windows.zip")));
    QVERIFY(source.contains(QStringLiteral("current user's Start Menu")));
}

QTEST_APPLESS_MAIN(tst_WindowsBuildWorkflowContract)

#include "tst_windowsbuildworkflowcontract.moc"
