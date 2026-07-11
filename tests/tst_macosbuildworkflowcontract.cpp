#include <QDir>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
bool writeExecutable(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    if (file.write(contents) != contents.size()) {
        return false;
    }
    file.close();

    return file.setPermissions(QFileDevice::ReadOwner
                               | QFileDevice::WriteOwner
                               | QFileDevice::ExeOwner
                               | QFileDevice::ReadGroup
                               | QFileDevice::ExeGroup
                               | QFileDevice::ReadOther
                               | QFileDevice::ExeOther);
}

QString legacyClionBuildTreeName()
{
    return QStringLiteral("cmake")
            + QStringLiteral("-")
            + QStringLiteral("build")
            + QStringLiteral("-")
            + QStringLiteral("debug");
}
}

class tst_MacOSBuildWorkflowContract : public QObject
{
    Q_OBJECT

private slots:
    void cmakeFixesApplicationVersionAt40();
    void cmakeRequiresRepositoryBuildDirectory();
    void repositoryGuidelinesUseOnlyBuildDirectory();
    void buildGuideSeparatesLocalAndDistributionSigning();
    void platformAppIconsAreBundledFromResources();
    void buildScriptUsesIncrementalBuildsAndStripsDistributionBundles();
    void buildScriptRunsLocalModeWithEmptyOptionalArgumentArrays();
};

void tst_MacOSBuildWorkflowContract::cmakeFixesApplicationVersionAt40()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");

    QFile cmakeFile(cmakePath);
    QVERIFY(cmakeFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString cmakeSource = QString::fromUtf8(cmakeFile.readAll());

    const QString infoPlistPath = QFINDTESTDATA("../packaging/macos/Info.plist");
    QVERIFY2(!infoPlistPath.isEmpty(), "packaging/macos/Info.plist test data was not found");

    QFile infoPlist(infoPlistPath);
    QVERIFY(infoPlist.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString infoPlistSource = QString::fromUtf8(infoPlist.readAll());

    QVERIFY(cmakeSource.contains(QStringLiteral("project(Vincent VERSION 4.0 LANGUAGES C CXX)")));
    QVERIFY(cmakeSource.contains(QStringLiteral("if(CMAKE_HOST_SYSTEM_NAME STREQUAL \"Darwin\" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)")));
    const qsizetype deploymentTargetIndex = cmakeSource.indexOf(QStringLiteral("set(CMAKE_OSX_DEPLOYMENT_TARGET"));
    const qsizetype projectIndex = cmakeSource.indexOf(QStringLiteral("project(Vincent VERSION 4.0 LANGUAGES C CXX)"));
    QVERIFY(deploymentTargetIndex >= 0);
    QVERIFY(projectIndex > deploymentTargetIndex);
    QVERIFY(cmakeSource.contains(QStringLiteral("set(VINCENT_BUNDLE_VERSION \"${PROJECT_VERSION}\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("MACOSX_BUNDLE_BUNDLE_VERSION \"${VINCENT_BUNDLE_VERSION}\"")));
    QVERIFY(cmakeSource.contains(QStringLiteral("MACOSX_BUNDLE_SHORT_VERSION_STRING \"${PROJECT_VERSION}\"")));
    QVERIFY(infoPlistSource.contains(QStringLiteral("<string>@PROJECT_VERSION@</string>")));
    QVERIFY(infoPlistSource.contains(QStringLiteral("<string>@VINCENT_BUNDLE_VERSION@</string>")));
}

void tst_MacOSBuildWorkflowContract::cmakeRequiresRepositoryBuildDirectory()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");

    QFile cmakeFile(cmakePath);
    QVERIFY(cmakeFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString cmakeSource = QString::fromUtf8(cmakeFile.readAll());

    QVERIFY(cmakeSource.contains(QStringLiteral("repository-local build/ directory")));
    QVERIFY(cmakeSource.contains(QStringLiteral("cmake -S ${CMAKE_SOURCE_DIR} -B ${_vincent_required_build_dir}")));
    QVERIFY(cmakeSource.contains(QStringLiteral("NOT _vincent_actual_build_dir STREQUAL _vincent_required_build_dir")));
}

void tst_MacOSBuildWorkflowContract::repositoryGuidelinesUseOnlyBuildDirectory()
{
    const QString agentsPath = QFINDTESTDATA("../AGENTS.md");
    QVERIFY2(!agentsPath.isEmpty(), "AGENTS.md test data was not found");

    QFile agentsFile(agentsPath);
    QVERIFY(agentsFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString agentsSource = QString::fromUtf8(agentsFile.readAll());

    QVERIFY(agentsSource.contains(QStringLiteral("repository-local `build/` directory")));
    QVERIFY(agentsSource.contains(QStringLiteral("alternate build trees are not supported")));
    QVERIFY(!agentsSource.contains(legacyClionBuildTreeName()));
}

void tst_MacOSBuildWorkflowContract::buildGuideSeparatesLocalAndDistributionSigning()
{
    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");

    QFile buildGuide(buildGuidePath);
    QVERIFY(buildGuide.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(buildGuide.readAll());

    QVERIFY(source.contains(QStringLiteral("./build.sh local")));
    QVERIFY(source.contains(QStringLiteral("repository-local `build/` CMake binary directory")));
    QVERIFY(source.contains(QStringLiteral("alternate build trees are rejected")));
    QVERIFY(!source.contains(legacyClionBuildTreeName()));
    QVERIFY(source.contains(QStringLiteral("ctest --test-dir build --output-on-failure")));
    QVERIFY(source.contains(QStringLiteral("Apple Development")));
    QVERIFY(source.contains(QStringLiteral("dist/Vincent.pkg")));
    QVERIFY(source.contains(QStringLiteral("dist/Vincent-appstore.pkg")));
    QVERIFY(source.contains(QStringLiteral("unsigned local installer packages")));
    QVERIFY(source.contains(QStringLiteral("VINCENT_BUILD_MODE=devid ./build.sh")));
    QVERIFY(source.contains(QStringLiteral("VINCENT_BUILD_MODE=mas ./build.sh")));
    QVERIFY(source.contains(QStringLiteral("Developer ID Application")));
    QVERIFY(source.contains(QStringLiteral("Apple Distribution")));
    QVERIFY(source.contains(QStringLiteral("NOTARY_APP_PASSWORD")));
    QVERIFY(source.contains(QStringLiteral("Bash 3.2")));
    QVERIFY(source.contains(QStringLiteral("CMAKE_EXTRA_ARGS")));
    QVERIFY(source.contains(QStringLiteral("Contents/Resources/Appicon.icns")));
    QVERIFY(source.contains(QStringLiteral("stale installer package")));
    QVERIFY(source.contains(QStringLiteral("pkgutil --payload-files dist/Vincent.pkg")));
    QVERIFY(source.contains(QStringLiteral("./Vincent.app/Contents/Resources/Appicon.icns")));
    QVERIFY(source.contains(QStringLiteral("./Vincent.app/Contents/Resources/icon.icns")));
    QVERIFY(source.contains(QStringLiteral("Transporter's Active list")));
    QVERIFY(source.contains(QStringLiteral("dist/Vincent-appstore.pkg")));
    QVERIFY(source.contains(QStringLiteral("App Store Connect record")));
    QVERIFY(source.contains(QStringLiteral("cmp resources/Appicon.icns")));
}

void tst_MacOSBuildWorkflowContract::platformAppIconsAreBundledFromResources()
{
    const QString cmakePath = QFINDTESTDATA("../CMakeLists.txt");
    QVERIFY2(!cmakePath.isEmpty(), "CMakeLists.txt test data was not found");
    QFile cmakeFile(cmakePath);
    QVERIFY(cmakeFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString cmakeSource = QString::fromUtf8(cmakeFile.readAll());

    const QString infoPlistPath = QFINDTESTDATA("../packaging/macos/Info.plist");
    QVERIFY2(!infoPlistPath.isEmpty(), "packaging/macos/Info.plist test data was not found");
    QFile infoPlist(infoPlistPath);
    QVERIFY(infoPlist.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString infoPlistSource = QString::fromUtf8(infoPlist.readAll());

    const QString syncScriptPath = QFINDTESTDATA("../tools/sync_app_icon_assets.sh");
    QVERIFY2(!syncScriptPath.isEmpty(), "tools/sync_app_icon_assets.sh test data was not found");
    QFile syncScript(syncScriptPath);
    QVERIFY(syncScript.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString syncScriptSource = QString::fromUtf8(syncScript.readAll());

    const QString gitignorePath = QFINDTESTDATA("../.gitignore");
    QVERIFY2(!gitignorePath.isEmpty(), ".gitignore test data was not found");
    QFile gitignore(gitignorePath);
    QVERIFY(gitignore.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString gitignoreSource = QString::fromUtf8(gitignore.readAll());

    QVERIFY2(!QFINDTESTDATA("../resources/Appicon.icns").isEmpty(), "macOS app icon is missing");
    QVERIFY2(!QFINDTESTDATA("../resources/Appicon.ico").isEmpty(), "Windows app icon is missing");
    QVERIFY(cmakeSource.contains(QStringLiteral("set(VINCENT_MACOS_APP_ICON \"${CMAKE_SOURCE_DIR}/resources/Appicon.icns\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("set(VINCENT_WINDOWS_APP_ICON \"${CMAKE_SOURCE_DIR}/resources/Appicon.ico\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("set(VINCENT_LEGACY_MACOS_APP_ICON_FILE \"icon.icns\")")));
    QVERIFY(cmakeSource.contains(QStringLiteral("MACOSX_PACKAGE_LOCATION \"Resources\"")));
    QVERIFY(cmakeSource.contains(QStringLiteral("MACOSX_BUNDLE_ICON_FILE \"${VINCENT_MACOS_APP_ICON_FILE}\"")));
    QVERIFY(cmakeSource.contains(QStringLiteral("$<TARGET_BUNDLE_DIR:Vincent>/Contents/Resources/${VINCENT_LEGACY_MACOS_APP_ICON_FILE}")));
    QVERIFY(cmakeSource.contains(QStringLiteral("configure_file(\"${VINCENT_WINDOWS_RESOURCE_TEMPLATE}\"")));
    QVERIFY(cmakeSource.contains(QStringLiteral("target_sources(Vincent PRIVATE \"${_vincent_windows_resource_file}\")")));
    QVERIFY(infoPlistSource.contains(QStringLiteral("<string>@VINCENT_MACOS_APP_ICON_FILE@</string>")));
    QVERIFY(syncScriptSource.contains(QStringLiteral("resources/Appicon.icns")));
    QVERIFY(syncScriptSource.contains(QStringLiteral("packaging/macos/Vincent.xcassets/AppIcon.appiconset")));
    QVERIFY(syncScriptSource.contains(QStringLiteral("iconutil -c iconset")));
    QVERIFY(syncScriptSource.contains(QStringLiteral("AppIcon-1024.png")));
    QVERIFY(gitignoreSource.contains(QStringLiteral("!/resources/Appicon.icns")));
    QVERIFY(gitignoreSource.contains(QStringLiteral("!/resources/Appicon.ico")));
}

void tst_MacOSBuildWorkflowContract::buildScriptUsesIncrementalBuildsAndStripsDistributionBundles()
{
    const QString buildScriptPath = QFINDTESTDATA("../build.sh");
    QVERIFY2(!buildScriptPath.isEmpty(), "build.sh test data was not found");
    QFile buildScript(buildScriptPath);
    QVERIFY(buildScript.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(buildScript.readAll());

    QVERIFY(source.contains(QStringLiteral("Usage: ./build.sh [--clean] [local|devid|mas|all]")));
    QVERIFY(source.contains(QStringLiteral("CLEAN_BUILD_DIR=\"${CLEAN_BUILD_DIR:-0}\"")));
    QVERIFY(source.contains(QStringLiteral("--clean)")));
    QCOMPARE(source.count(QStringLiteral("CLEAN_BUILD_DIR=\"1\"")), 1);
    QVERIFY(source.contains(QStringLiteral("MACDEPLOYQT_NO_STRIP=\"${MACDEPLOYQT_NO_STRIP:-}\"")));
    QVERIFY(source.contains(QStringLiteral("if [[ -z \"$MACDEPLOYQT_NO_STRIP\" ]]; then")));
    QVERIFY(source.contains(QStringLiteral("MACDEPLOYQT_NO_STRIP=\"1\"")));
    QVERIFY(source.contains(QStringLiteral("MACDEPLOYQT_NO_STRIP=\"0\"")));
    QVERIFY(source.contains(QStringLiteral("if [[ \"$MACDEPLOYQT_NO_STRIP\" == \"1\" ]]; then cmd+=(\"-no-strip\"); fi")));
    QVERIFY(source.contains(QStringLiteral("need_cmd /usr/bin/otool")));
    QVERIFY(source.contains(QStringLiteral("assert_portable_macho_links()")));
    QVERIFY(source.contains(QStringLiteral("/usr/bin/otool -L \"$binary\"")));
    QVERIFY(source.contains(QStringLiteral("/usr/bin/otool -l \"$binary\"")));
    QVERIFY(source.contains(QStringLiteral("LC_RPATH")));
    QVERIFY(source.contains(QStringLiteral("deployed app contains non-portable absolute Mach-O paths")));
    QVERIFY(source.contains(QStringLiteral("assert_portable_macho_links \"$out_app\"")));
}

void tst_MacOSBuildWorkflowContract::buildScriptRunsLocalModeWithEmptyOptionalArgumentArrays()
{
#ifndef Q_OS_MACOS
    QSKIP("build.sh depends on macOS packaging tools");
#else
    const QString gitignorePath = QFINDTESTDATA("../.gitignore");
    QVERIFY2(!gitignorePath.isEmpty(), ".gitignore test data was not found");

    QFile gitignoreFile(gitignorePath);
    QVERIFY(gitignoreFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString gitignoreSource = QString::fromUtf8(gitignoreFile.readAll());
    QVERIFY(!gitignoreSource.contains(QStringLiteral("\nbuild.sh\n")));
    QVERIFY(!gitignoreSource.contains(QStringLiteral("\n/build.sh\n")));

    const QString sourceBuildScriptPath = QFINDTESTDATA("../build.sh");
    if (sourceBuildScriptPath.isEmpty()) {
        QSKIP("build.sh is a local workflow script and is not available in this checkout");
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir temp(tempDir.path());
    QVERIFY(temp.mkpath(QStringLiteral("bin")));
    QVERIFY(temp.mkpath(QStringLiteral("App/qml")));

    const QString buildScriptPath = temp.filePath(QStringLiteral("build.sh"));
    QVERIFY(QFile::copy(sourceBuildScriptPath, buildScriptPath));
    QVERIFY(QFile::setPermissions(buildScriptPath,
                                  QFileDevice::ReadOwner
                                          | QFileDevice::WriteOwner
                                          | QFileDevice::ExeOwner
                                          | QFileDevice::ReadGroup
                                          | QFileDevice::ExeGroup
                                          | QFileDevice::ReadOther
                                          | QFileDevice::ExeOther));

    const QString binDir = temp.filePath(QStringLiteral("bin"));
    const QByteArray fakeCmake = R"SH(#!/bin/sh
set -eu

if [ "${1:-}" = "--build" ]; then
    exit 0
fi

build_dir=""
expect_build_dir=0
for arg in "$@"; do
    if [ "$expect_build_dir" = "1" ]; then
        build_dir="$arg"
        expect_build_dir=0
        continue
    fi
    if [ "$arg" = "-B" ]; then
        expect_build_dir=1
    fi
done

if [ -z "$build_dir" ]; then
    build_dir="./build"
fi

mkdir -p "$build_dir/Vincent.app/Contents/MacOS" "$build_dir/Vincent.app/Contents/Resources"
cat > "$build_dir/Vincent.app/Contents/MacOS/Vincent" <<'APP'
#!/bin/sh
exit 0
APP
chmod +x "$build_dir/Vincent.app/Contents/MacOS/Vincent"
printf 'fake icon\n' > "$build_dir/Vincent.app/Contents/Resources/Appicon.icns"
cat > "$build_dir/Vincent.app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
        "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIconFile</key>
    <string>Appicon.icns</string>
    <key>CFBundleShortVersionString</key>
    <string>4.0</string>
</dict>
</plist>
PLIST
)SH";

    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("cmake")), fakeCmake));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("macdeployqt")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("codesign")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("security")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("ninja")),
                            QByteArray("#!/bin/sh\nexit 0\n")));
    const QByteArray fakeXcrun = R"SH(#!/bin/sh
set -eu

if [ "${1:-}" = "--find" ]; then
    printf '/usr/bin/%s\n' "$2"
    exit 0
fi

tool="${1:-}"
shift || true
case "$tool" in
    productbuild)
        out=""
        for arg in "$@"; do
            out="$arg"
        done
        mkdir -p "$(dirname "$out")"
        printf 'fake package\n' > "$out"
        ;;
    *)
        printf 'unsupported fake xcrun tool: %s\n' "$tool" >&2
        exit 1
        ;;
esac
)SH";
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("xcrun")), fakeXcrun));

    const QByteArray fakePkgutil = R"SH(#!/bin/sh
set -eu

case "${1:-}" in
    --check-signature)
        printf 'Package "%s":\n   Status: no signature\n' "$(basename "${2:-}")"
        ;;
    --payload-files)
        printf './Vincent.app/Contents/MacOS/Vincent\n'
        printf './Vincent.app/Contents/Resources/Appicon.icns\n'
        ;;
    *)
        printf 'unsupported fake pkgutil command: %s\n' "${1:-}" >&2
        exit 1
        ;;
esac
)SH";
    QVERIFY(writeExecutable(QDir(binDir).filePath(QStringLiteral("pkgutil")), fakePkgutil));

    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("PATH"), binDir + QStringLiteral(":") + environment.value(QStringLiteral("PATH")));
    environment.insert(QStringLiteral("RUN_TESTS"), QStringLiteral("0"));
    environment.insert(QStringLiteral("VINCENT_BUILD_MODE"), QStringLiteral("local"));
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(tempDir.path());
    process.start(QStringLiteral("/bin/bash"), QStringList{QStringLiteral("./build.sh")});

    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(30000), qPrintable(process.errorString()));

    const QString output = QString::fromUtf8(process.readAllStandardOutput() + process.readAllStandardError());
    QVERIFY2(process.exitStatus() == QProcess::NormalExit, qPrintable(output));
    QVERIFY2(process.exitCode() == 0, qPrintable(output));
    QVERIFY2(!output.contains(QStringLiteral("unbound variable")), qPrintable(output));
    QVERIFY2(output.contains(QStringLiteral("done")), qPrintable(output));
    QVERIFY2(QFile::exists(temp.filePath(QStringLiteral("dist/Vincent.pkg"))), qPrintable(output));
    QVERIFY2(QFile::exists(temp.filePath(QStringLiteral("dist/Vincent-appstore.pkg"))), qPrintable(output));
#endif
}

QTEST_APPLESS_MAIN(tst_MacOSBuildWorkflowContract)

#include "tst_macosbuildworkflowcontract.moc"
