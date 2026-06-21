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
}

class tst_MacOSBuildWorkflowContract : public QObject
{
    Q_OBJECT

private slots:
    void buildGuideSeparatesLocalAndDistributionSigning();
    void buildScriptRunsLocalModeWithEmptyOptionalArgumentArrays();
};

void tst_MacOSBuildWorkflowContract::buildGuideSeparatesLocalAndDistributionSigning()
{
    const QString buildGuidePath = QFINDTESTDATA("../docs/BUILD.md");
    QVERIFY2(!buildGuidePath.isEmpty(), "docs/BUILD.md test data was not found");

    QFile buildGuide(buildGuidePath);
    QVERIFY(buildGuide.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString source = QString::fromUtf8(buildGuide.readAll());

    QVERIFY(source.contains(QStringLiteral("./build.sh local")));
    QVERIFY(source.contains(QStringLiteral("ctest --test-dir build --output-on-failure")));
    QVERIFY(source.contains(QStringLiteral("Apple Development")));
    QVERIFY(source.contains(QStringLiteral("VINCENT_BUILD_MODE=devid ./build.sh")));
    QVERIFY(source.contains(QStringLiteral("VINCENT_BUILD_MODE=mas ./build.sh")));
    QVERIFY(source.contains(QStringLiteral("Developer ID Application")));
    QVERIFY(source.contains(QStringLiteral("Apple Distribution")));
    QVERIFY(source.contains(QStringLiteral("NOTARY_APP_PASSWORD")));
    QVERIFY(source.contains(QStringLiteral("Bash 3.2")));
    QVERIFY(source.contains(QStringLiteral("CMAKE_EXTRA_ARGS")));
}

void tst_MacOSBuildWorkflowContract::buildScriptRunsLocalModeWithEmptyOptionalArgumentArrays()
{
#ifndef Q_OS_MACOS
    QSKIP("build.sh depends on macOS packaging tools");
#else
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

mkdir -p "$build_dir/Vincent.app/Contents/MacOS"
cat > "$build_dir/Vincent.app/Contents/MacOS/Vincent" <<'APP'
#!/bin/sh
exit 0
APP
chmod +x "$build_dir/Vincent.app/Contents/MacOS/Vincent"
cat > "$build_dir/Vincent.app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
        "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleShortVersionString</key>
    <string>2.2.1</string>
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
#endif
}

QTEST_APPLESS_MAIN(tst_MacOSBuildWorkflowContract)

#include "tst_macosbuildworkflowcontract.moc"
