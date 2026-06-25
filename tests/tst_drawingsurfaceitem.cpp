#include <QFileInfo>
#include <QImage>
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
    void createsInitialCanvasInsideWorkspaceMargins();
    void createsNewCanvasAtCurrentWorkspaceSize();
    void drawsAndSavesStroke();
    void erasesCommittedStrokePixels();
    void commitsTextToRasterCanvas();
    void commitsShapeToRasterCanvas();
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

QQuickItem *findItemByObjectName(QQuickItem *root, const QString &objectName)
{
    if (!root) {
        return nullptr;
    }

    if (root->objectName() == objectName) {
        return root;
    }

    const QList<QQuickItem *> children = root->childItems();
    for (QQuickItem *child : children) {
        if (QQuickItem *item = findItemByObjectName(child, objectName)) {
            return item;
        }
    }

    return nullptr;
}

int workspaceHorizontalInset(qreal width)
{
    return qMax(24, qRound(width * 0.09));
}

int workspaceTopInset(qreal height)
{
    return qMax(24, qRound(height * 0.12));
}

int workspaceBottomInset(qreal height)
{
    return qMax(24, qRound(height * 0.10));
}

QSize workspaceCanvasSize(qreal width, qreal height)
{
    return QSize(qMax(1, qRound(width) - workspaceHorizontalInset(width) * 2),
                 qMax(1, qRound(height) - workspaceTopInset(height) - workspaceBottomInset(height)));
}

} // namespace

void tst_DrawingSurfaceItem::createsInitialCanvasInsideWorkspaceMargins()
{
    qmlRegisterType<DrawingSurfaceItem>("Vincent", 2, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
    QTRY_VERIFY(component.isReady() || component.isError());
    QVERIFY2(component.isReady(), qPrintable(qmlErrorsToString(component.errors())));

    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    viewModel.setBrushFlow(0.42);
    viewModel.setBrushOpacity(0.64);
    viewModel.setBrushHardness(0.71);
    viewModel.setBrushSpacing(7.5);
    viewModel.setBrushSpacingRatio(0.33);
    viewModel.setPressureCurveMinimum(0.2);
    viewModel.setPressureCurveMaximum(0.8);
    viewModel.setPressureCurveCenter(0.6);
    viewModel.setStabilizerStrength(0.44);

    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("width"), 720);
    initialProperties.insert(QStringLiteral("height"), 480);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));
    initialProperties.insert(QStringLiteral("brushFlow"), 0.42);
    initialProperties.insert(QStringLiteral("brushOpacity"), 0.64);
    initialProperties.insert(QStringLiteral("brushHardness"), 0.71);
    initialProperties.insert(QStringLiteral("brushSpacing"), 7.5);
    initialProperties.insert(QStringLiteral("brushSpacingRatio"), 0.33);
    initialProperties.insert(QStringLiteral("pressureCurveMinimum"), 0.2);
    initialProperties.insert(QStringLiteral("pressureCurveCenter"), 0.6);
    initialProperties.insert(QStringLiteral("pressureCurveMaximum"), 0.8);
    initialProperties.insert(QStringLiteral("stabilizerStrength"), 0.44);

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);
    const QColor workspaceColor = rootItem->property("color").value<QColor>();
    QVERIFY(workspaceColor != QColor(Qt::white));

    QQuickItem *canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QCOMPARE(canvasPaper->property("color").value<QColor>(), QColor(Qt::white));
    QQuickItem *canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);

    DrawingSurfaceItem *canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    const QSize expectedCanvasSize = workspaceCanvasSize(720, 480);
    QTRY_COMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(720)));
    QTRY_COMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(480)));
    QTRY_COMPARE(canvasViewport->width(), static_cast<qreal>(expectedCanvasSize.width()));
    QTRY_COMPARE(canvasViewport->height(), static_cast<qreal>(expectedCanvasSize.height()));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(expectedCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(expectedCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(expectedCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(expectedCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), expectedCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), expectedCanvasSize.height());
    QCOMPARE(canvasItem->brushFlow(), 0.42);
    QCOMPARE(canvasItem->brushOpacity(), 0.64);
    QCOMPARE(canvasItem->brushHardness(), 0.71);
    QCOMPARE(canvasItem->brushSpacing(), 7.5);
    QCOMPARE(canvasItem->brushSpacingRatio(), 0.33);
    QCOMPARE(canvasItem->pressureCurveMinimum(), 0.2);
    QCOMPARE(canvasItem->pressureCurveCenter(), 0.6);
    QCOMPARE(canvasItem->pressureCurveMaximum(), 0.8);
    QCOMPARE(canvasItem->stabilizerStrength(), 0.44);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("initial-canvas.png"));
    QVERIFY(canvasItem->saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), expectedCanvasSize);
}

void tst_DrawingSurfaceItem::createsNewCanvasAtCurrentWorkspaceSize()
{
    qmlRegisterType<DrawingSurfaceItem>("Vincent", 2, 0, "DrawingSurfaceItem");

    QQmlEngine engine;

    QQmlComponent component(&engine);
    const QString drawingSurfaceQml = QFINDTESTDATA("../App/qml/painting/DrawingSurface.qml");
    QVERIFY2(!drawingSurfaceQml.isEmpty(), "DrawingSurface.qml test data was not found");
    component.loadUrl(QUrl::fromLocalFile(drawingSurfaceQml));
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
    QCOMPARE(rootItem->property("maximumAntialiasingBrushHardness").toReal(),
             CanvasDocumentViewModel::maximumAntialiasingBrushHardness());
    QQuickItem *canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QQuickItem *canvasViewport = findItemByObjectName(rootItem, QStringLiteral("canvasViewport"));
    QVERIFY(canvasViewport);
    const QSize initialCanvasSize = workspaceCanvasSize(720, 480);
    QTRY_COMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(720)));
    QTRY_COMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(480)));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(initialCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(initialCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(initialCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(initialCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), initialCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), initialCanvasSize.height());
    QCOMPARE(canvasItem->brushHardness(), CanvasDocumentViewModel::maximumAntialiasingBrushHardness());
    QVERIFY(rootItem->property("color").value<QColor>() != canvasPaper->property("color").value<QColor>());
    QVERIFY(rootItem->setProperty("canvasWidth", 720));
    QVERIFY(rootItem->setProperty("canvasHeight", 480));

    QVERIFY(rootItem->setProperty("canvasWidth", 300));
    QVERIFY(rootItem->setProperty("canvasHeight", 200));
    QCoreApplication::processEvents();
    QCOMPARE(canvasItem->width(), static_cast<qreal>(initialCanvasSize.width()));
    QCOMPARE(canvasItem->height(), static_cast<qreal>(initialCanvasSize.height()));
    QVERIFY(rootItem->setProperty("canvasWidth", 720));
    QVERIFY(rootItem->setProperty("canvasHeight", 480));

    rootItem->setWidth(960);
    rootItem->setHeight(540);
    QCoreApplication::processEvents();
    const QSize expandedCanvasSize = workspaceCanvasSize(960, 540);
    QCOMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(960)));
    QCOMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(540)));
    QCOMPARE(canvasViewport->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QCOMPARE(canvasViewport->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QCOMPARE(canvasItem->width(), static_cast<qreal>(initialCanvasSize.width()));
    QCOMPARE(canvasItem->height(), static_cast<qreal>(initialCanvasSize.height()));
    QCOMPARE(canvasPaper->width(), static_cast<qreal>(initialCanvasSize.width()));
    QCOMPARE(canvasPaper->height(), static_cast<qreal>(initialCanvasSize.height()));

    QVERIFY(QMetaObject::invokeMethod(rootItem, "newCanvas", Qt::DirectConnection));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), expandedCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), expandedCanvasSize.height());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("new-workspace-sized-canvas.png"));
    QVERIFY(canvasItem->saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), expandedCanvasSize);

    QVERIFY(rootItem->setProperty("canvasWidth", expandedCanvasSize.width()));
    QVERIFY(rootItem->setProperty("canvasHeight", expandedCanvasSize.height()));
    rootItem->setWidth(800);
    rootItem->setHeight(600);
    QCoreApplication::processEvents();
    const QSize compactCanvasSize = workspaceCanvasSize(800, 600);
    QCOMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(800)));
    QCOMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(600)));
    QCOMPARE(canvasViewport->width(), static_cast<qreal>(compactCanvasSize.width()));
    QCOMPARE(canvasViewport->height(), static_cast<qreal>(compactCanvasSize.height()));
    QCOMPARE(canvasItem->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QCOMPARE(canvasItem->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QCOMPARE(canvasPaper->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QCOMPARE(canvasPaper->height(), static_cast<qreal>(expandedCanvasSize.height()));

    QVERIFY(QMetaObject::invokeMethod(rootItem, "clearCanvas", Qt::DirectConnection));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(compactCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(compactCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(compactCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(compactCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), compactCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), compactCanvasSize.height());
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

void tst_DrawingSurfaceItem::commitsTextToRasterCanvas()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(180);
    item.setHeight(96);
    item.setBrushColor(QColor(QStringLiteral("#d32f2f")));
    item.setDocumentViewModel(&viewModel);

    QVERIFY(item.commitText(16, 18, 140, QStringLiteral("Vincent"), 28, QColor(QStringLiteral("#d32f2f"))));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("text-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(180, 96));

    bool hasTextColorPixel = false;
    for (int y = 0; y < saved.height() && !hasTextColorPixel; ++y) {
        for (int x = 0; x < saved.width(); ++x) {
            const QColor pixel = saved.pixelColor(x, y);
            if (pixel.alpha() > 0 && pixel.red() > 140 && pixel.green() < 100 && pixel.blue() < 100) {
                hasTextColorPixel = true;
                break;
            }
        }
    }
    QVERIFY(hasTextColorPixel);
}

void tst_DrawingSurfaceItem::commitsShapeToRasterCanvas()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(180);
    item.setHeight(120);
    item.setBrushColor(QColor(QStringLiteral("#1976d2")));
    item.setDocumentViewModel(&viewModel);

    const QStringList shapeKinds{
        QStringLiteral("rectangle"),
        QStringLiteral("ellipse"),
        QStringLiteral("triangle"),
        QStringLiteral("diamond"),
        QStringLiteral("star"),
        QStringLiteral("rectanglebubble"),
        QStringLiteral("ellipsebubble")
    };

    for (int index = 0; index < shapeKinds.size(); ++index) {
        const qreal x = 10 + index % 4 * 40;
        const qreal y = 12 + index / 4 * 48;
        QVERIFY(item.commitShape(x, y, 30, 28, shapeKinds.at(index), QColor(QStringLiteral("#1976d2")), 3));
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("shape-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(180, 120));

    bool hasShapeColorPixel = false;
    for (int y = 0; y < saved.height() && !hasShapeColorPixel; ++y) {
        for (int x = 0; x < saved.width(); ++x) {
            const QColor pixel = saved.pixelColor(x, y);
            if (pixel.alpha() > 0 && pixel.blue() > 120 && pixel.red() < 80 && pixel.green() > 70) {
                hasShapeColorPixel = true;
                break;
            }
        }
    }
    QVERIFY(hasShapeColorPixel);
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
