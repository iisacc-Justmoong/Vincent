#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "profileimageprocessor.h"

class tst_ProfileImageProcessor : public QObject
{
    Q_OBJECT

private slots:
    void landscapeImageUsesFullHeightAndCenteredWidth();
    void portraitImageUsesFullWidthAndCenteredHeight();
    void failedReplacementKeepsPreviousCrop();
    void deletingProfileImageRemovesTemporaryCrop();
};

namespace {

QUrl writeSourceImage(const QImage &image, QTemporaryDir &directory, const QString &fileName)
{
    const QString path = directory.filePath(fileName);
    if (!image.save(path, "PNG")) {
        return {};
    }
    return QUrl::fromLocalFile(path);
}

QImage gradientImage(const QSize &size)
{
    QImage image(size, QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor(x % 256, y % 256, (x + y) % 256, 255));
        }
    }
    return image;
}

} // namespace

void tst_ProfileImageProcessor::landscapeImageUsesFullHeightAndCenteredWidth()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QImage source = gradientImage(QSize(120, 80));
    const QUrl sourceUrl = writeSourceImage(source, directory, QStringLiteral("landscape.png"));
    QVERIFY(sourceUrl.isValid());

    ProfileImageProcessor processor;
    QSignalSpy sourceSpy(&processor, &ProfileImageProcessor::imageSourceChanged);

    QVERIFY(processor.processProfileImage(sourceUrl));
    QCOMPARE(sourceSpy.count(), 1);
    QVERIFY(processor.errorString().isEmpty());
    QVERIFY(processor.imageSource().isLocalFile());
    QCOMPARE(QFileInfo(processor.imageSource().toLocalFile()).suffix().toLower(),
             QStringLiteral("png"));

    const QImage result(processor.imageSource().toLocalFile());
    QVERIFY(!result.isNull());
    QCOMPARE(result.size(), QSize(80, 80));
    QCOMPARE(result.pixelColor(40, 40), source.pixelColor(60, 40));
    QCOMPARE(result.pixelColor(40, 1), source.pixelColor(60, 1));
    QCOMPARE(result.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(result.pixelColor(79, 79).alpha(), 0);
}

void tst_ProfileImageProcessor::portraitImageUsesFullWidthAndCenteredHeight()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QImage source = gradientImage(QSize(60, 100));
    const QUrl sourceUrl = writeSourceImage(source, directory, QStringLiteral("portrait.png"));
    QVERIFY(sourceUrl.isValid());

    ProfileImageProcessor processor;
    QVERIFY(processor.processProfileImage(sourceUrl));

    const QImage result(processor.imageSource().toLocalFile());
    QVERIFY(!result.isNull());
    QCOMPARE(result.size(), QSize(60, 60));
    QCOMPARE(result.pixelColor(30, 30), source.pixelColor(30, 50));
    QCOMPARE(result.pixelColor(1, 30), source.pixelColor(1, 50));
    QCOMPARE(result.pixelColor(0, 0).alpha(), 0);
    QCOMPARE(result.pixelColor(59, 59).alpha(), 0);
}

void tst_ProfileImageProcessor::failedReplacementKeepsPreviousCrop()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl sourceUrl = writeSourceImage(gradientImage(QSize(48, 48)),
                                             directory,
                                             QStringLiteral("valid.png"));
    QVERIFY(sourceUrl.isValid());

    ProfileImageProcessor processor;
    QVERIFY(processor.processProfileImage(sourceUrl));
    const QUrl previousImageSource = processor.imageSource();
    QVERIFY(QFileInfo::exists(previousImageSource.toLocalFile()));

    QVERIFY(!processor.processProfileImage(
        QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing.png")))));
    QCOMPARE(processor.imageSource(), previousImageSource);
    QVERIFY(QFileInfo::exists(previousImageSource.toLocalFile()));
    QVERIFY(!processor.errorString().isEmpty());
}

void tst_ProfileImageProcessor::deletingProfileImageRemovesTemporaryCrop()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl sourceUrl = writeSourceImage(gradientImage(QSize(80, 60)),
                                             directory,
                                             QStringLiteral("profile.png"));
    QVERIFY(sourceUrl.isValid());

    ProfileImageProcessor processor;
    QSignalSpy sourceSpy(&processor, &ProfileImageProcessor::imageSourceChanged);
    QVERIFY(processor.processProfileImage(sourceUrl));
    const QString processedImagePath = processor.imageSource().toLocalFile();
    QVERIFY(QFileInfo::exists(processedImagePath));

    processor.clearProfileImage();

    QVERIFY(processor.imageSource().isEmpty());
    QVERIFY(processor.errorString().isEmpty());
    QVERIFY(!QFileInfo::exists(processedImagePath));
    QCOMPARE(sourceSpy.count(), 2);

    processor.clearProfileImage();
    QCOMPARE(sourceSpy.count(), 2);
}

QTEST_GUILESS_MAIN(tst_ProfileImageProcessor)

#include "tst_profileimageprocessor.moc"
