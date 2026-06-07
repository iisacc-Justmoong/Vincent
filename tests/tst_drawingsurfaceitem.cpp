#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "canvasdocumentviewmodel.h"
#include "models/painting/drawingsurfaceitem.h"
#include "paletteutils.h"

class tst_DrawingSurfaceItem : public QObject
{
    Q_OBJECT

private slots:
    void drawsAndSavesStroke();
    void erasesCommittedStrokePixels();
    void supportsUndoRedo();
    void opensRasterBackground();
};

void tst_DrawingSurfaceItem::drawsAndSavesStroke()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(128);
    item.setHeight(96);
    item.setDocumentViewModel(&viewModel);

    item.beginStroke(10, 10, 1.0, false);
    QVERIFY(item.appendStrokePoint(40, 40, 1.0, false));
    item.endStroke(60, 48, 1.0, false);

    QTRY_COMPARE(item.strokeCount(), 1);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("stroke-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    QVERIFY(QFileInfo::exists(outputPath));

    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(128, 96));
}

void tst_DrawingSurfaceItem::erasesCommittedStrokePixels()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(96);
    item.setHeight(64);
    item.setBrushSize(20);
    item.setBrushColor(QColor(QStringLiteral("#202020")));
    item.setDocumentViewModel(&viewModel);

    item.beginStroke(16, 32, 1.0, false);
    QVERIFY(item.appendStrokePoint(80, 32, 1.0, false));
    item.endStroke(80, 32, 1.0, false);
    QTRY_COMPARE(item.strokeCount(), 1);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString beforePath = dir.filePath(QStringLiteral("before-erase.png"));
    QVERIFY(item.saveToFile(beforePath));
    const QImage before(beforePath);
    QVERIFY(qAlpha(before.pixel(48, 32)) > 0);

    item.setToolMode(QStringLiteral("eraser"));
    item.beginStroke(16, 32, 1.0, false);
    QVERIFY(item.appendStrokePoint(80, 32, 1.0, false));
    item.endStroke(80, 32, 1.0, false);
    QTRY_COMPARE(item.strokeCount(), 2);

    const QString afterPath = dir.filePath(QStringLiteral("after-erase.png"));
    QVERIFY(item.saveToFile(afterPath));
    const QImage after(afterPath);
    QVERIFY(qAlpha(after.pixel(48, 32)) < qAlpha(before.pixel(48, 32)));
}

void tst_DrawingSurfaceItem::supportsUndoRedo()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(80);
    item.setHeight(80);
    item.setDocumentViewModel(&viewModel);

    item.beginStroke(8, 8, 1.0, false);
    item.endStroke(20, 20, 1.0, false);

    QTRY_COMPARE(item.strokeCount(), 1);
    QTRY_VERIFY(item.canUndo());

    item.undo();
    QTRY_COMPARE(item.strokeCount(), 0);
    QTRY_VERIFY(item.canRedo());

    item.redo();
    QTRY_COMPARE(item.strokeCount(), 1);
}

void tst_DrawingSurfaceItem::opensRasterBackground()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(120);
    item.setHeight(90);
    item.setDocumentViewModel(&viewModel);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage image(32, 24, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#26c6da")));
    const QString inputPath = dir.filePath(QStringLiteral("background.png"));
    QVERIFY(image.save(inputPath));

    QVERIFY(item.openRaster(inputPath));
    QVERIFY(item.hasBackground());
    QCOMPARE(item.backgroundSource(), QUrl::fromLocalFile(inputPath).toString());
    QCOMPARE(item.width(), 32.0);
    QCOMPARE(item.height(), 24.0);
}

QTEST_MAIN(tst_DrawingSurfaceItem)

#include "tst_drawingsurfaceitem.moc"
