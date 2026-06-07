#include <QFileInfo>
#include <QImage>
#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
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
    void createsInitialCanvasAtSurfaceSize();
    void drawsAndSavesStroke();
    void erasesCommittedStrokePixels();
    void supportsUndoRedo();
    void opensRasterBackground();
};

namespace {

QString qmlErrorsToString(const QList<QQmlError> &errors)
{
    QStringList messages;
    messages.reserve(errors.size());
    for (const QQmlError &error : errors) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

DrawingSurfaceItem *findDrawingSurfaceItem(QQuickItem *root)
{
    if (!root) {
        return nullptr;
    }

    if (auto *surfaceItem = qobject_cast<DrawingSurfaceItem *>(root)) {
        return surfaceItem;
    }

    const QList<QQuickItem *> children = root->childItems();
    for (QQuickItem *child : children) {
        if (auto *surfaceItem = findDrawingSurfaceItem(child)) {
            return surfaceItem;
        }
    }

    return nullptr;
}

} // namespace

void tst_DrawingSurfaceItem::createsInitialCanvasAtSurfaceSize()
{
    qmlRegisterType<DrawingSurfaceItem>("Vincent", 2, 0, "DrawingSurfaceItem");

    QQmlEngine engine;
    engine.addImportPath(QDir::homePath() + QStringLiteral("/.local/LVRS/platforms/macos/lib/qt6/qml"));
    engine.addImportPath(QDir::homePath() + QStringLiteral("/.local/LVRS/platforms/linux/lib/qt6/qml"));
    engine.addImportPath(QDir::homePath() + QStringLiteral("/.local/LVRS/platforms/windows/lib/qt6/qml"));

    static constexpr auto qmlSource = R"(
import QtQuick
import Vincent 2.0

Item {
    id: surface
    property var documentViewModel: null
    property int canvasWidth: documentViewModel ? documentViewModel.canvasWidth : 1
    property int canvasHeight: documentViewModel ? documentViewModel.canvasHeight : 1
    property bool canvasItemReady: false

    function resolvedCanvasWidth() {
        return Math.max(1, surface.canvasWidth > 1 ? surface.canvasWidth : Math.round(surface.width))
    }

    function resolvedCanvasHeight() {
        return Math.max(1, surface.canvasHeight > 1 ? surface.canvasHeight : Math.round(surface.height))
    }

    function syncCanvasItemSize() {
        if (!canvasItemReady) {
            return
        }
        canvasSurface.resizeCanvasSurface(resolvedCanvasWidth(), resolvedCanvasHeight())
    }

    onWidthChanged: {
        if (surface.canvasWidth <= 1) {
            syncCanvasItemSize()
        }
    }

    onHeightChanged: {
        if (surface.canvasHeight <= 1) {
            syncCanvasItemSize()
        }
    }

    onCanvasWidthChanged: syncCanvasItemSize()
    onCanvasHeightChanged: syncCanvasItemSize()

    DrawingSurfaceItem {
        id: canvasSurface
        anchors.centerIn: parent
        width: 1
        height: 1
        documentViewModel: surface.documentViewModel
        viewId: "testSurface"

        Component.onCompleted: {
            surface.canvasItemReady = true
            surface.syncCanvasItemSize()
        }
    }
}
)";

    QQmlComponent component(&engine);
    component.setData(qmlSource, QUrl(QStringLiteral("memory:DrawingSurfaceSizing.qml")));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem *canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QTRY_COMPARE(canvasItem->width(), 720.0);
    QTRY_COMPARE(canvasItem->height(), 480.0);
    QTRY_COMPARE(viewModel.canvasWidth(), 720);
    QTRY_COMPARE(viewModel.canvasHeight(), 480);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("initial-canvas.png"));
    QVERIFY(canvasItem->saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(720, 480));
}

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
