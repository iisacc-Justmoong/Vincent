#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickItem>
#include <QSignalSpy>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

#include "canvasdocumentviewmodel.h"
#include "models/painting/drawingsurfaceitem.h"
#include "paletteutils.h"
#include "psdimagereader.h"

class tst_DrawingSurfaceItem : public QObject
{
    Q_OBJECT

private slots:
    void createsInitialCanvasInsideWorkspaceMargins();
    void createsNewCanvasAtCurrentWorkspaceSize();
    void constrainsShapeDragWithShiftModifier();
    void pansCanvasWithHandToolDrag();
    void zoomsCanvasWithHorizontalDrag();
    void movesAndResizesDrawableObjects();
    void deletesSelectedDrawableObject();
    void deletesBackgroundLayerLikeRegularLayer();
    void addsBlankLayerRowsWithoutTransformHitTesting();
    void savesRasterLayerItemsAsIndependentCanvasLayers();
    void deletingRasterLayerRemovesItsPaintFromQmlComposite();
    void addsManyRasterLayersWithoutSnapshotChurn();
    void renamesLayerRowsAndDrawableObjectMetadata();
    void layersExposeHierarchyRowsAndReorderDrawableObjects();
    void drawsAndSavesStroke();
    void erasesCommittedStrokePixels();
    void commitsTextToRasterCanvas();
    void commitsShapeToRasterCanvas();
    void commitsSpeechBubbleTailsAsIntegratedSolidShapes();
    void fillsContiguousRasterRegion();
    void cachesLayerBitmapThumbnails();
    void savesCompositeDrawableObjectsWithoutFlatteningRaster();
    void savesBlankCanvasAsOpaquePsdBackground();
    void savesPsdLayerRecordsInBottomToTopOrder();
    void savesCompositeDrawableObjectsAsLayeredPsdWithMetadata();
    void opensLayeredPsdThroughPsdSdkReader();
    void createsPsdDrawablePreviewForQmlImage();
    void supportsUndoRedo();
    void opensRasterBackground();
    void opensLargeRasterWithinCurrentCanvasBounds();
    void opensRasterThroughQmlAsDrawableImageObjectWithinWorkspaceBounds();
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

qreal fittedCanvasZoomScale(qreal viewportWidth, qreal viewportHeight, qreal canvasWidth, qreal canvasHeight)
{
    return qMin<qreal>(1.0, qMin(viewportWidth / canvasWidth, viewportHeight / canvasHeight));
}

bool isBlueShapePixel(const QColor &pixel)
{
    return pixel.alpha() > 0 && pixel.blue() > 120 && pixel.red() < 80 && pixel.green() > 70;
}

quint16 readUInt16(const QByteArray &bytes, int offset)
{
    return static_cast<quint16>((static_cast<uchar>(bytes.at(offset)) << 8)
                               | static_cast<uchar>(bytes.at(offset + 1)));
}

quint32 readUInt32(const QByteArray &bytes, int offset)
{
    return (static_cast<quint32>(static_cast<uchar>(bytes.at(offset))) << 24)
        | (static_cast<quint32>(static_cast<uchar>(bytes.at(offset + 1))) << 16)
        | (static_cast<quint32>(static_cast<uchar>(bytes.at(offset + 2))) << 8)
        | static_cast<quint32>(static_cast<uchar>(bytes.at(offset + 3)));
}

qsizetype imageResourcesLengthOffset(const QByteArray &psd)
{
    return 30 + readUInt32(psd, 26);
}

qsizetype layerMaskLengthOffset(const QByteArray &psd)
{
    const qsizetype resourcesOffset = imageResourcesLengthOffset(psd);
    return resourcesOffset + 4 + readUInt32(psd, static_cast<int>(resourcesOffset));
}

int paddedPascalStringSize(int textLength)
{
    return ((textLength + 1 + 3) / 4) * 4;
}

QStringList psdLayerRecordNames(const QByteArray &psd)
{
    QStringList names;
    const qsizetype layerOffset = layerMaskLengthOffset(psd);
    if (layerOffset + 10 >= psd.size()) {
        return names;
    }

    const qsizetype layerInfoEnd = layerOffset + 8 + readUInt32(psd, static_cast<int>(layerOffset + 4));
    qsizetype cursor = layerOffset + 8;
    if (cursor + 2 > psd.size()) {
        return names;
    }

    const int layerCount = qAbs(static_cast<qint16>(readUInt16(psd, static_cast<int>(cursor))));
    cursor += 2;
    names.reserve(layerCount);

    for (int layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
        if (cursor + 18 > psd.size() || cursor >= layerInfoEnd) {
            return {};
        }

        cursor += 16;
        const int channelCount = readUInt16(psd, static_cast<int>(cursor));
        cursor += 2 + channelCount * 6;
        if (cursor + 16 > psd.size()) {
            return {};
        }

        cursor += 12;
        const quint32 extraDataLength = readUInt32(psd, static_cast<int>(cursor));
        cursor += 4;
        const qsizetype extraDataEnd = cursor + extraDataLength;
        if (extraDataEnd > psd.size() || cursor + 8 > psd.size()) {
            return {};
        }

        const quint32 layerMaskLength = readUInt32(psd, static_cast<int>(cursor));
        cursor += 4 + layerMaskLength;
        if (cursor + 4 > psd.size()) {
            return {};
        }

        const quint32 blendingRangesLength = readUInt32(psd, static_cast<int>(cursor));
        cursor += 4 + blendingRangesLength;
        if (cursor >= psd.size()) {
            return {};
        }

        const int nameLength = static_cast<uchar>(psd.at(cursor));
        ++cursor;
        if (cursor + nameLength > psd.size()) {
            return {};
        }

        names.append(QString::fromLatin1(psd.mid(cursor, nameLength)));
        cursor += paddedPascalStringSize(nameLength) - 1;
        cursor = extraDataEnd;
    }

    return names;
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
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 1);
    QVariantList initialObjects = rootItem->property("drawableObjects").toList();
    QCOMPARE(initialObjects.size(), 1);
    QCOMPARE(initialObjects.first().toMap().value(QStringLiteral("type")).toString(), QStringLiteral("layer"));
    QCOMPARE(initialObjects.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Layer 1"));
    QVariantList initialRows;
    QTRY_VERIFY([&]() {
        initialRows = rootItem->property("layerHierarchyRows").toList();
        return initialRows.size() == 2;
    }());
    QCOMPARE(initialRows.at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("object-1"));
    QCOMPARE(initialRows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(initialRows.at(1).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("raster-canvas"));
    QCOMPARE(initialRows.at(1).toMap().value(QStringLiteral("selected")).toBool(), false);
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

    QQmlExpression createWorkspaceCanvas(engine.rootContext(),
                                         object.data(),
                                         QStringLiteral("newCanvas();"));
    createWorkspaceCanvas.evaluate();
    QVERIFY2(!createWorkspaceCanvas.hasError(), qPrintable(createWorkspaceCanvas.error().toString()));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(expandedCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(expandedCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), expandedCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), expandedCanvasSize.height());
    QVariantList workspaceCanvasObjects = rootItem->property("drawableObjects").toList();
    QCOMPARE(workspaceCanvasObjects.size(), 1);
    const QVariantMap workspaceDefaultLayer = workspaceCanvasObjects.first().toMap();
    QCOMPARE(workspaceDefaultLayer.value(QStringLiteral("type")).toString(), QStringLiteral("layer"));
    QCOMPARE(workspaceDefaultLayer.value(QStringLiteral("name")).toString(), QStringLiteral("Layer 1"));
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), workspaceDefaultLayer.value(QStringLiteral("id")).toInt());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("new-workspace-sized-canvas.png"));
    QVERIFY(canvasItem->saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), expandedCanvasSize);

    const QSize explicitCanvasSize(333, 222);
    QQmlExpression createExplicitCanvas(engine.rootContext(),
                                        object.data(),
                                        QStringLiteral("newCanvas(333, 222);"));
    createExplicitCanvas.evaluate();
    QVERIFY2(!createExplicitCanvas.hasError(), qPrintable(createExplicitCanvas.error().toString()));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(explicitCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(explicitCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(explicitCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(explicitCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), explicitCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), explicitCanvasSize.height());
    QCOMPARE(rootItem->property("canvasZoomScale").toReal(), 1.0);

    const QString explicitOutputPath = dir.filePath(QStringLiteral("new-explicit-sized-canvas.png"));
    QVERIFY(canvasItem->saveToFile(explicitOutputPath));
    const QImage explicitSaved(explicitOutputPath);
    QVERIFY(!explicitSaved.isNull());
    QCOMPARE(explicitSaved.size(), explicitCanvasSize);

    QVERIFY(rootItem->setProperty("canvasWidth", explicitCanvasSize.width()));
    QVERIFY(rootItem->setProperty("canvasHeight", explicitCanvasSize.height()));
    rootItem->setWidth(800);
    rootItem->setHeight(600);
    QCoreApplication::processEvents();
    const QSize compactCanvasSize = workspaceCanvasSize(800, 600);
    QCOMPARE(canvasViewport->x(), static_cast<qreal>(workspaceHorizontalInset(800)));
    QCOMPARE(canvasViewport->y(), static_cast<qreal>(workspaceTopInset(600)));
    QCOMPARE(canvasViewport->width(), static_cast<qreal>(compactCanvasSize.width()));
    QCOMPARE(canvasViewport->height(), static_cast<qreal>(compactCanvasSize.height()));
    QCOMPARE(canvasItem->width(), static_cast<qreal>(explicitCanvasSize.width()));
    QCOMPARE(canvasItem->height(), static_cast<qreal>(explicitCanvasSize.height()));
    QCOMPARE(canvasPaper->width(), static_cast<qreal>(explicitCanvasSize.width()));
    QCOMPARE(canvasPaper->height(), static_cast<qreal>(explicitCanvasSize.height()));

    const QSize highResolutionCanvasSize(4096, 3072);
    QQmlExpression createHighResolutionCanvas(engine.rootContext(),
                                              object.data(),
                                              QStringLiteral("newCanvas(4096, 3072);"));
    createHighResolutionCanvas.evaluate();
    QVERIFY2(!createHighResolutionCanvas.hasError(), qPrintable(createHighResolutionCanvas.error().toString()));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(highResolutionCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(highResolutionCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(highResolutionCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(highResolutionCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), highResolutionCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), highResolutionCanvasSize.height());

    const qreal expectedHighResolutionZoom = fittedCanvasZoomScale(canvasViewport->width(),
                                                                   canvasViewport->height(),
                                                                   highResolutionCanvasSize.width(),
                                                                   highResolutionCanvasSize.height());
    QVERIFY(expectedHighResolutionZoom < 1.0);
    QVERIFY(qAbs(rootItem->property("canvasZoomScale").toReal() - expectedHighResolutionZoom) < 0.0001);
    QCOMPARE(canvasItem->scale(), rootItem->property("canvasZoomScale").toReal());
    QCOMPARE(canvasPaper->scale(), rootItem->property("canvasZoomScale").toReal());
    QVERIFY(canvasItem->width() * canvasItem->scale() <= canvasViewport->width() + 0.5);
    QVERIFY(canvasItem->height() * canvasItem->scale() <= canvasViewport->height() + 0.5);

    QVERIFY(QMetaObject::invokeMethod(rootItem, "clearCanvas", Qt::DirectConnection));
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(compactCanvasSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(compactCanvasSize.height()));
    QTRY_COMPARE(canvasPaper->width(), static_cast<qreal>(compactCanvasSize.width()));
    QTRY_COMPARE(canvasPaper->height(), static_cast<qreal>(compactCanvasSize.height()));
    QTRY_COMPARE(viewModel.canvasWidth(), compactCanvasSize.width());
    QTRY_COMPARE(viewModel.canvasHeight(), compactCanvasSize.height());
    QCOMPARE(rootItem->property("canvasZoomScale").toReal(), 1.0);
}

void tst_DrawingSurfaceItem::constrainsShapeDragWithShiftModifier()
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
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("shape"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem *canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QTRY_VERIFY(canvasItem->width() > 160);
    QTRY_VERIFY(canvasItem->height() > 160);

    QQmlExpression constrainedDrag(engine.rootContext(),
                                   object.data(),
                                   QStringLiteral("beginShapeDrag(20, 20, false); updateShapeDrag(140, 70, true);"));
    constrainedDrag.evaluate();
    QVERIFY2(!constrainedDrag.hasError(), qPrintable(constrainedDrag.error().toString()));

    QCOMPARE(rootItem->property("shapeAspectLocked").toBool(), true);
    const qreal constrainedWidth = qAbs(rootItem->property("shapeCurrentX").toReal() - rootItem->property("shapeStartX").toReal());
    const qreal constrainedHeight = qAbs(rootItem->property("shapeCurrentY").toReal() - rootItem->property("shapeStartY").toReal());
    QCOMPARE(constrainedWidth, constrainedHeight);
    QCOMPARE(constrainedWidth, 120.0);

    QQmlExpression freeDrag(engine.rootContext(),
                            object.data(),
                            QStringLiteral("updateShapeDrag(140, 70, false);"));
    freeDrag.evaluate();
    QVERIFY2(!freeDrag.hasError(), qPrintable(freeDrag.error().toString()));

    QCOMPARE(rootItem->property("shapeAspectLocked").toBool(), false);
    const qreal freeWidth = qAbs(rootItem->property("shapeCurrentX").toReal() - rootItem->property("shapeStartX").toReal());
    const qreal freeHeight = qAbs(rootItem->property("shapeCurrentY").toReal() - rootItem->property("shapeStartY").toReal());
    QCOMPARE(freeWidth, 120.0);
    QCOMPARE(freeHeight, 50.0);
}

void tst_DrawingSurfaceItem::pansCanvasWithHandToolDrag()
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
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("pan"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem *canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QQuickItem *canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QTRY_COMPARE(rootItem->property("canvasPanOffsetX").toReal(), 0.0);
    QTRY_COMPARE(rootItem->property("canvasPanOffsetY").toReal(), 0.0);

    const qreal initialCanvasX = canvasItem->x();
    const qreal initialCanvasY = canvasItem->y();
    const qreal initialPaperX = canvasPaper->x();
    const qreal initialPaperY = canvasPaper->y();

    QQmlExpression panRightUp(engine.rootContext(),
                              object.data(),
                              QStringLiteral("beginPanDrag(100, 120); updatePanDrag(145, 85); commitPanDrag();"));
    panRightUp.evaluate();
    QVERIFY2(!panRightUp.hasError(), qPrintable(panRightUp.error().toString()));

    QTRY_COMPARE(rootItem->property("canvasPanOffsetX").toReal(), 45.0);
    QTRY_COMPARE(rootItem->property("canvasPanOffsetY").toReal(), -35.0);
    QTRY_COMPARE(canvasItem->x(), initialCanvasX + 45.0);
    QTRY_COMPARE(canvasItem->y(), initialCanvasY - 35.0);
    QTRY_COMPARE(canvasPaper->x(), initialPaperX + 45.0);
    QTRY_COMPARE(canvasPaper->y(), initialPaperY - 35.0);

    QQmlExpression resetPan(engine.rootContext(),
                            object.data(),
                            QStringLiteral("resetCanvasPan();"));
    resetPan.evaluate();
    QVERIFY2(!resetPan.hasError(), qPrintable(resetPan.error().toString()));

    QTRY_COMPARE(rootItem->property("canvasPanOffsetX").toReal(), 0.0);
    QTRY_COMPARE(rootItem->property("canvasPanOffsetY").toReal(), 0.0);
    QTRY_COMPARE(canvasItem->x(), initialCanvasX);
    QTRY_COMPARE(canvasItem->y(), initialCanvasY);
    QTRY_COMPARE(canvasPaper->x(), initialPaperX);
    QTRY_COMPARE(canvasPaper->y(), initialPaperY);
}

void tst_DrawingSurfaceItem::zoomsCanvasWithHorizontalDrag()
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
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("zoom"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem *canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QQuickItem *canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QTRY_COMPARE(rootItem->property("canvasZoomScale").toReal(), 1.0);
    QCOMPARE(canvasItem->scale(), 1.0);
    QCOMPARE(canvasPaper->scale(), 1.0);

    QQmlExpression zoomIn(engine.rootContext(),
                          object.data(),
                          QStringLiteral("beginZoomDrag(100); updateZoomDrag(220); commitZoomDrag();"));
    zoomIn.evaluate();
    QVERIFY2(!zoomIn.hasError(), qPrintable(zoomIn.error().toString()));

    const qreal zoomedInScale = rootItem->property("canvasZoomScale").toReal();
    QVERIFY(zoomedInScale > 1.0);
    QCOMPARE(canvasItem->scale(), zoomedInScale);
    QCOMPARE(canvasPaper->scale(), zoomedInScale);

    QQmlExpression zoomOut(engine.rootContext(),
                           object.data(),
                           QStringLiteral("beginZoomDrag(220); updateZoomDrag(80); commitZoomDrag();"));
    zoomOut.evaluate();
    QVERIFY2(!zoomOut.hasError(), qPrintable(zoomOut.error().toString()));

    const qreal zoomedOutScale = rootItem->property("canvasZoomScale").toReal();
    QVERIFY(zoomedOutScale < zoomedInScale);
    QCOMPARE(canvasItem->scale(), zoomedOutScale);
    QCOMPARE(canvasPaper->scale(), zoomedOutScale);
}

void tst_DrawingSurfaceItem::movesAndResizesDrawableObjects()
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
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    QQmlExpression moveObject(engine.rootContext(),
                              object.data(),
                              QStringLiteral("appendDrawableObject({ id: 2, type: \"shape\", x: 10, y: 20, width: 30, height: 28, shapeKind: \"rectangle\", color: \"#1976d2\" });"
                                             "beginDrawableObjectTransform(20, 30); updateDrawableObjectTransform(40, 60); commitDrawableObjectTransform();"));
    moveObject.evaluate();
    QVERIFY2(!moveObject.hasError(), qPrintable(moveObject.error().toString()));

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    QVariantMap movedObject = objects.last().toMap();
    QCOMPARE(movedObject.value(QStringLiteral("x")).toReal(), 30.0);
    QCOMPARE(movedObject.value(QStringLiteral("y")).toReal(), 50.0);
    QCOMPARE(movedObject.value(QStringLiteral("width")).toReal(), 30.0);
    QCOMPARE(movedObject.value(QStringLiteral("height")).toReal(), 28.0);

    QQmlExpression resizeObject(engine.rootContext(),
                                object.data(),
                                QStringLiteral("beginDrawableObjectTransform(60, 78); updateDrawableObjectTransform(90, 100); commitDrawableObjectTransform();"));
    resizeObject.evaluate();
    QVERIFY2(!resizeObject.hasError(), qPrintable(resizeObject.error().toString()));

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap resizedObject = objects.last().toMap();
    QCOMPARE(resizedObject.value(QStringLiteral("x")).toReal(), 30.0);
    QCOMPARE(resizedObject.value(QStringLiteral("y")).toReal(), 50.0);
    QCOMPARE(resizedObject.value(QStringLiteral("width")).toReal(), 60.0);
    QCOMPARE(resizedObject.value(QStringLiteral("height")).toReal(), 50.0);
}

void tst_DrawingSurfaceItem::deletesSelectedDrawableObject()
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
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    QQmlExpression deleteObject(engine.rootContext(),
                                object.data(),
                                QStringLiteral("appendDrawableObject({ id: 2, type: \"shape\", x: 10, y: 20, width: 30, height: 28, shapeKind: \"rectangle\", color: \"#1976d2\" });"
                                               "appendDrawableObject({ id: 3, type: \"text\", x: 40, y: 50, width: 120, height: 32, text: \"Label\", fontPixelSize: 18, color: \"#111111\" });"
                                               "deleteSelectedDrawableObject();"));
    const QVariant deleteResult = deleteObject.evaluate();
    QVERIFY2(!deleteObject.hasError(), qPrintable(deleteObject.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap remainingObject = objects.last().toMap();
    QCOMPARE(remainingObject.value(QStringLiteral("id")).toInt(), 2);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), -1);

    QQmlExpression deleteWithoutSelection(engine.rootContext(),
                                          object.data(),
                                          QStringLiteral("deleteSelectedDrawableObject();"));
    const QVariant deleteWithoutSelectionResult = deleteWithoutSelection.evaluate();
    QVERIFY2(!deleteWithoutSelection.hasError(), qPrintable(deleteWithoutSelection.error().toString()));
    QCOMPARE(deleteWithoutSelectionResult.toBool(), false);

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
}

void tst_DrawingSurfaceItem::deletesBackgroundLayerLikeRegularLayer()
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
    initialProperties.insert(QStringLiteral("width"), 320);
    initialProperties.insert(QStringLiteral("height"), 240);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    QQuickItem *canvasPaper = findItemByObjectName(rootItem, QStringLiteral("canvasPaper"));
    QVERIFY(canvasPaper);
    QCOMPARE(canvasPaper->property("color").value<QColor>(), QColor(Qt::white));
    QQuickItem *transparencyGridBackground =
            findItemByObjectName(rootItem, QStringLiteral("transparencyGridBackground"));
    QVERIFY(transparencyGridBackground);
    QCOMPARE(transparencyGridBackground->property("visible").toBool(), false);
    QCOMPARE(transparencyGridBackground->property("source").toUrl().scheme(), QStringLiteral("data"));

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("object-1"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Layer 1"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("raster-canvas"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("selected")).toBool(), false);
    QTRY_VERIFY([&]() {
        QQmlExpression layerReady(engine.rootContext(),
                                  object.data(),
                                  QStringLiteral("rasterLayerItemById(1) !== null"));
        const QVariant result = layerReady.evaluate();
        return !layerReady.hasError() && result.toBool();
    }());

    QQmlExpression canDeleteBackground(engine.rootContext(),
                                       object.data(),
                                       QStringLiteral("canDeleteCurrentLayer();"));
    const QVariant canDeleteResult = canDeleteBackground.evaluate();
    QVERIFY2(!canDeleteBackground.hasError(), qPrintable(canDeleteBackground.error().toString()));
    QCOMPARE(canDeleteResult.toBool(), true);

    QQmlExpression deleteBackground(engine.rootContext(),
                                    object.data(),
                                    QStringLiteral("deleteLayerByKey(\"raster-canvas\");"));
    const QVariant deleteResult = deleteBackground.evaluate();
    QVERIFY2(!deleteBackground.hasError(), qPrintable(deleteBackground.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);
    QCOMPARE(rootItem->property("backgroundLayerPresent").toBool(), false);
    QCOMPARE(canvasPaper->property("color").value<QColor>(), QColor(Qt::transparent));
    QCOMPARE(transparencyGridBackground->property("visible").toBool(), true);

    rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.first().toMap().value(QStringLiteral("key")).toString(), QStringLiteral("object-1"));
    QCOMPARE(rows.first().toMap().value(QStringLiteral("selected")).toBool(), true);

    QQmlExpression currentLayerKey(engine.rootContext(),
                                   object.data(),
                                   QStringLiteral("currentLayerKey();"));
    const QVariant currentLayerKeyResult = currentLayerKey.evaluate();
    QVERIFY2(!currentLayerKey.hasError(), qPrintable(currentLayerKey.error().toString()));
    QCOMPARE(currentLayerKeyResult.toString(), QStringLiteral("object-1"));

    QQmlExpression canDeleteDefaultLayer(engine.rootContext(),
                                         object.data(),
                                         QStringLiteral("canDeleteCurrentLayer();"));
    const QVariant canDeleteDefaultLayerResult = canDeleteDefaultLayer.evaluate();
    QVERIFY2(!canDeleteDefaultLayer.hasError(), qPrintable(canDeleteDefaultLayer.error().toString()));
    QCOMPARE(canDeleteDefaultLayerResult.toBool(), true);

    QQmlExpression hasActiveRasterSurface(engine.rootContext(),
                                          object.data(),
                                          QStringLiteral("hasActiveRasterSurface();"));
    const QVariant hasActiveRasterResult = hasActiveRasterSurface.evaluate();
    QVERIFY2(!hasActiveRasterSurface.hasError(), qPrintable(hasActiveRasterSurface.error().toString()));
    QCOMPARE(hasActiveRasterResult.toBool(), true);

    QQmlExpression activeRasterSurface(engine.rootContext(),
                                       object.data(),
                                       QStringLiteral("activeRasterSurface();"));
    const QVariant activeRasterResult = activeRasterSurface.evaluate();
    QVERIFY2(!activeRasterSurface.hasError(), qPrintable(activeRasterSurface.error().toString()));
    QVERIFY(!activeRasterResult.isNull());
    QVERIFY(activeRasterResult.value<QObject *>() != nullptr);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("without-background.psd"));
    QQmlExpression savePsd(engine.rootContext(),
                           object.data(),
                           QStringLiteral("saveToFile(\"%1\");").arg(QUrl::fromLocalFile(psdPath).toString()));
    const QVariant saveResult = savePsd.evaluate();
    QVERIFY2(!savePsd.hasError(), qPrintable(savePsd.error().toString()));
    QCOMPARE(saveResult.toBool(), true);

    QFile psdFile(psdPath);
    QVERIFY(psdFile.open(QIODevice::ReadOnly));
    QCOMPARE(psdLayerRecordNames(psdFile.readAll()), QStringList({QStringLiteral("Layer 1")}));

    const PsdImportedDocument importedDocument = PsdImageReader::readDocument(psdPath);
    QVERIFY(importedDocument.isValid());
    QCOMPARE(importedDocument.layers.size(), 1);
    QCOMPARE(importedDocument.layers.first().name, QStringLiteral("Layer 1"));
    const QVariantList manifestLayers = importedDocument.vincentManifest.value(QStringLiteral("layers")).toList();
    QCOMPARE(manifestLayers.size(), 1);
    QCOMPARE(manifestLayers.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Layer 1"));
}

void tst_DrawingSurfaceItem::addsBlankLayerRowsWithoutTransformHitTesting()
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
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem *canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    QTRY_VERIFY(canvasItem->width() > 1);
    QTRY_VERIFY(canvasItem->height() > 1);

    QQmlExpression addLayer(engine.rootContext(),
                            object.data(),
                            QStringLiteral("addEmptyLayer();"));
    const QVariant addedLayerId = addLayer.evaluate();
    QVERIFY2(!addLayer.hasError(), qPrintable(addLayer.error().toString()));
    QCOMPARE(addedLayerId.toInt(), 2);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 2);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    const QVariantMap layerObject = objects.last().toMap();
    QCOMPARE(layerObject.value(QStringLiteral("type")).toString(), QStringLiteral("layer"));
    QCOMPARE(layerObject.value(QStringLiteral("name")).toString(), QStringLiteral("Layer 2"));
    QCOMPARE(layerObject.value(QStringLiteral("x")).toReal(), 0.0);
    QCOMPARE(layerObject.value(QStringLiteral("y")).toReal(), 0.0);
    QCOMPARE(layerObject.value(QStringLiteral("width")).toReal(), canvasItem->width());
    QCOMPARE(layerObject.value(QStringLiteral("height")).toReal(), canvasItem->height());

    QVariantList rows;
    QTRY_VERIFY([&]() {
        rows = rootItem->property("layerHierarchyRows").toList();
        return rows.size() == 3
            && !rows.at(0).toMap().value(QStringLiteral("iconSource")).toString().isEmpty();
    }());
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("object-2"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Layer 2"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("iconGlyph")).toString(), QString());
    const QString layerThumbnailSource = rows.at(0).toMap().value(QStringLiteral("iconSource")).toString();
    const QString layerThumbnailPath = QUrl(layerThumbnailSource).toLocalFile();
    if (!layerThumbnailPath.isEmpty()) {
        QVERIFY(QFileInfo::exists(layerThumbnailPath));
        QCOMPARE(QImage(layerThumbnailPath).size(), QSize(32, 32));
    }
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("object-1"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Layer 1"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("raster-canvas"));
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("iconGlyph")).toString(), QString());
    QVERIFY(!rows.at(2).toMap().value(QStringLiteral("iconSource")).toString().isEmpty());

    QQmlExpression coalesceThumbnailRefresh(engine.rootContext(),
                                            object.data(),
                                            QStringLiteral("requestRasterLayerThumbnailRefresh(2);"
                                                           "requestRasterLayerThumbnailRefresh(2);"
                                                           "Object.keys(pendingRasterLayerThumbnailRefreshes).length;"));
    const QVariant pendingRefreshCount = coalesceThumbnailRefresh.evaluate();
    QVERIFY2(!coalesceThumbnailRefresh.hasError(), qPrintable(coalesceThumbnailRefresh.error().toString()));
    QCOMPARE(pendingRefreshCount.toInt(), 1);

    QQmlExpression flushThumbnailRefresh(engine.rootContext(),
                                         object.data(),
                                         QStringLiteral("flushPendingLayerThumbnailRefreshes();"
                                                        "Object.keys(pendingRasterLayerThumbnailRefreshes).length;"));
    const QVariant remainingRefreshCount = flushThumbnailRefresh.evaluate();
    QVERIFY2(!flushThumbnailRefresh.hasError(), qPrintable(flushThumbnailRefresh.error().toString()));
    QCOMPARE(remainingRefreshCount.toInt(), 0);

    QQmlExpression clearThumbnailState(engine.rootContext(),
                                       object.data(),
                                       QStringLiteral("setRasterLayerThumbnailSource(2, \"image://old-thumbnail\");"
                                                      "requestRasterLayerThumbnailRefresh(2);"
                                                      "clearRasterLayerThumbnailState(2);"
                                                      "[Object.keys(pendingRasterLayerThumbnailRefreshes).length,"
                                                      " rasterLayerThumbnailSources[\"2\"] === undefined].join(\",\");"));
    const QVariant clearedThumbnailState = clearThumbnailState.evaluate();
    QVERIFY2(!clearThumbnailState.hasError(), qPrintable(clearThumbnailState.error().toString()));
    QCOMPARE(clearedThumbnailState.toString(), QStringLiteral("0,true"));

    QQmlExpression transformableSelection(engine.rootContext(),
                                          object.data(),
                                          QStringLiteral("hasTransformableSelectedDrawableObject();"));
    const QVariant transformableSelectionResult = transformableSelection.evaluate();
    QVERIFY2(!transformableSelection.hasError(), qPrintable(transformableSelection.error().toString()));
    QCOMPARE(transformableSelectionResult.toBool(), false);

    QQmlExpression beginBlankLayerTransform(engine.rootContext(),
                                            object.data(),
                                            QStringLiteral("beginDrawableObjectTransform(10, 10);"));
    const QVariant beginBlankLayerTransformResult = beginBlankLayerTransform.evaluate();
    QVERIFY2(!beginBlankLayerTransform.hasError(), qPrintable(beginBlankLayerTransform.error().toString()));
    QCOMPARE(beginBlankLayerTransformResult.toBool(), false);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), -1);

    QQmlExpression deleteLayer(engine.rootContext(),
                               object.data(),
                               QStringLiteral("deleteLayerByKey(\"object-2\");"));
    const QVariant deleteResult = deleteLayer.evaluate();
    QVERIFY2(!deleteLayer.hasError(), qPrintable(deleteLayer.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);
    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    QCOMPARE(objects.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Layer 1"));
}

void tst_DrawingSurfaceItem::savesRasterLayerItemsAsIndependentCanvasLayers()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    viewModel.setBrushSize(10);
    viewModel.setBrushColor(QColor(QStringLiteral("#1976d2")));

    DrawingSurfaceItem baseItem;
    baseItem.setWidth(48);
    baseItem.setHeight(36);
    baseItem.setDocumentViewModel(&viewModel);

    DrawingSurfaceItem layerItem;
    layerItem.setWidth(48);
    layerItem.setHeight(36);
    layerItem.setDocumentViewModel(&viewModel);

    layerItem.beginStroke(18, 18, 1.0, false);
    layerItem.endStroke(18, 18, 1.0, false);
    QTRY_COMPARE(layerItem.strokeCount(), 1);

    QVariantMap layerObject;
    layerObject.insert(QStringLiteral("id"), 1);
    layerObject.insert(QStringLiteral("type"), QStringLiteral("layer"));
    layerObject.insert(QStringLiteral("name"), QStringLiteral("Layer 1"));
    layerObject.insert(QStringLiteral("x"), 0);
    layerObject.insert(QStringLiteral("y"), 0);
    layerObject.insert(QStringLiteral("width"), 48);
    layerObject.insert(QStringLiteral("height"), 36);
    layerObject.insert(QStringLiteral("opacity"), 1);
    layerObject.insert(QStringLiteral("visible"), true);

    QVariantMap rasterLayerDescriptor;
    rasterLayerDescriptor.insert(QStringLiteral("objectId"), 1);
    rasterLayerDescriptor.insert(QStringLiteral("item"),
                                 QVariant::fromValue(static_cast<QObject *>(&layerItem)));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString layeredPath = dir.filePath(QStringLiteral("layered-output.png"));
    QVERIFY(baseItem.saveToFileWithObjectsAndRasterLayers(layeredPath, {layerObject}, {rasterLayerDescriptor}));
    const QImage layered(layeredPath);
    QVERIFY(!layered.isNull());
    QCOMPARE(layered.size(), QSize(48, 36));
    QVERIFY(layered.pixelColor(18, 18).alpha() > 0);
    QVERIFY(layered.pixelColor(18, 18).blue() > 120);

    const QString baseOnlyPath = dir.filePath(QStringLiteral("base-only-output.png"));
    QVERIFY(baseItem.saveToFileWithObjectsAndRasterLayers(baseOnlyPath, {}, {}));
    const QImage baseOnly(baseOnlyPath);
    QVERIFY(!baseOnly.isNull());
    QCOMPARE(baseOnly.pixelColor(18, 18).rgba(), QColor(Qt::white).rgba());
}

void tst_DrawingSurfaceItem::deletingRasterLayerRemovesItsPaintFromQmlComposite()
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
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));
    initialProperties.insert(QStringLiteral("brushColor"), QColor(QStringLiteral("#1976d2")));
    initialProperties.insert(QStringLiteral("brushSize"), 10);
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("brush"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));

    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 1);

    QTRY_VERIFY([&]() {
        QQmlExpression layerReady(engine.rootContext(),
                                  object.data(),
                                  QStringLiteral("rasterLayerItemById(1) !== null && rasterLayerItemById(1).width > 1"));
        const QVariant result = layerReady.evaluate();
        return !layerReady.hasError() && result.toBool();
    }());

    QQmlExpression paintLayer(engine.rootContext(),
                              object.data(),
                              QStringLiteral("var rasterSurface = rasterLayerItemById(1);"
                                             "rasterSurface.beginStroke(24, 24, 1.0, false);"
                                             "rasterSurface.endStroke(24, 24, 1.0, false);"));
    paintLayer.evaluate();
    QVERIFY2(!paintLayer.hasError(), qPrintable(paintLayer.error().toString()));

    QTRY_VERIFY([&]() {
        QQmlExpression strokeCommitted(engine.rootContext(),
                                       object.data(),
                                       QStringLiteral("rasterLayerItemById(1) !== null && rasterLayerItemById(1).strokeCount === 1"));
        const QVariant result = strokeCommitted.evaluate();
        return !strokeCommitted.hasError() && result.toBool();
    }());

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString beforeDeletePath = dir.filePath(QStringLiteral("before-delete.png"));
    QQmlExpression saveBeforeDelete(engine.rootContext(),
                                    object.data(),
                                    QStringLiteral("saveToFile(\"%1\");").arg(QUrl::fromLocalFile(beforeDeletePath).toString()));
    const QVariant saveBeforeResult = saveBeforeDelete.evaluate();
    QVERIFY2(!saveBeforeDelete.hasError(), qPrintable(saveBeforeDelete.error().toString()));
    QCOMPARE(saveBeforeResult.toBool(), true);

    const QImage beforeDelete(beforeDeletePath);
    QVERIFY(!beforeDelete.isNull());
    QVERIFY(beforeDelete.pixelColor(24, 24).alpha() > 0);

    QQmlExpression deleteLayer(engine.rootContext(),
                               object.data(),
                               QStringLiteral("deleteLayerByKey(\"object-1\");"));
    const QVariant deleteResult = deleteLayer.evaluate();
    QVERIFY2(!deleteLayer.hasError(), qPrintable(deleteLayer.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);

    const QString afterDeletePath = dir.filePath(QStringLiteral("after-delete.png"));
    QQmlExpression saveAfterDelete(engine.rootContext(),
                                   object.data(),
                                   QStringLiteral("saveToFile(\"%1\");").arg(QUrl::fromLocalFile(afterDeletePath).toString()));
    const QVariant saveAfterResult = saveAfterDelete.evaluate();
    QVERIFY2(!saveAfterDelete.hasError(), qPrintable(saveAfterDelete.error().toString()));
    QCOMPARE(saveAfterResult.toBool(), true);

    const QImage afterDelete(afterDeletePath);
    QVERIFY(!afterDelete.isNull());
    QCOMPARE(afterDelete.pixelColor(24, 24).rgba(), QColor(Qt::white).rgba());
}

void tst_DrawingSurfaceItem::addsManyRasterLayersWithoutSnapshotChurn()
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
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    QQmlExpression addLayers(engine.rootContext(),
                             object.data(),
                             QStringLiteral("for (let index = 0; index < 32; ++index) { addEmptyLayer(); } true;"));
    const QVariant addLayersResult = addLayers.evaluate();
    QVERIFY2(!addLayers.hasError(), qPrintable(addLayers.error().toString()));
    QCOMPARE(addLayersResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 33);
    QCOMPARE(objects.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Layer 1"));
    QCOMPARE(objects.last().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Layer 33"));
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 33);

    QQmlExpression visualModelCount(engine.rootContext(),
                                    object.data(),
                                    QStringLiteral("drawableObjectVisualModelCount();"));
    const QVariant visualModelCountResult = visualModelCount.evaluate();
    QVERIFY2(!visualModelCount.hasError(), qPrintable(visualModelCount.error().toString()));
    QCOMPARE(visualModelCountResult.toInt(), 33);

    QTRY_VERIFY([&]() {
        QQmlExpression rasterLayerItemCount(engine.rootContext(),
                                            object.data(),
                                            QStringLiteral("Object.keys(rasterLayerItems).length;"));
        const QVariant result = rasterLayerItemCount.evaluate();
        return !rasterLayerItemCount.hasError() && result.toInt() == 33;
    }());

    QQmlExpression snapshotCount(engine.rootContext(),
                                 object.data(),
                                 QStringLiteral("Object.keys(rasterLayerSnapshotSources).length;"));
    const QVariant snapshotCountResult = snapshotCount.evaluate();
    QVERIFY2(!snapshotCount.hasError(), qPrintable(snapshotCount.error().toString()));
    QCOMPARE(snapshotCountResult.toInt(), 0);

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 34);
    QCOMPARE(rows.first().toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Layer 33"));
    QCOMPARE(rows.first().toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.last().toMap().value(QStringLiteral("key")).toString(), QStringLiteral("raster-canvas"));
}

void tst_DrawingSurfaceItem::renamesLayerRowsAndDrawableObjectMetadata()
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
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    QQmlExpression canRenameLayer(engine.rootContext(),
                                  object.data(),
                                  QStringLiteral("canRenameLayerByKey(\"object-1\");"));
    const QVariant canRenameResult = canRenameLayer.evaluate();
    QVERIFY2(!canRenameLayer.hasError(), qPrintable(canRenameLayer.error().toString()));
    QCOMPARE(canRenameResult.toBool(), true);

    QQmlExpression rasterRenameCheck(engine.rootContext(),
                                     object.data(),
                                     QStringLiteral("canRenameLayerByKey(\"raster-canvas\");"));
    const QVariant rasterRenameCheckResult = rasterRenameCheck.evaluate();
    QVERIFY2(!rasterRenameCheck.hasError(), qPrintable(rasterRenameCheck.error().toString()));
    QCOMPARE(rasterRenameCheckResult.toBool(), false);

    QQmlExpression renameLayer(engine.rootContext(),
                               object.data(),
                               QStringLiteral("renameLayerByKey(\"object-1\", \"  Ink pass  \");"));
    const QVariant renameResult = renameLayer.evaluate();
    QVERIFY2(!renameLayer.hasError(), qPrintable(renameLayer.error().toString()));
    QCOMPARE(renameResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    QCOMPARE(objects.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Ink pass"));

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.first().toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Ink pass"));

    QQmlExpression layerName(engine.rootContext(),
                             object.data(),
                             QStringLiteral("layerNameByKey(\"object-1\");"));
    const QVariant layerNameResult = layerName.evaluate();
    QVERIFY2(!layerName.hasError(), qPrintable(layerName.error().toString()));
    QCOMPARE(layerNameResult.toString(), QStringLiteral("Ink pass"));

    QQmlExpression rejectBlankRename(engine.rootContext(),
                                     object.data(),
                                     QStringLiteral("renameLayerByKey(\"object-1\", \"   \");"));
    const QVariant rejectBlankRenameResult = rejectBlankRename.evaluate();
    QVERIFY2(!rejectBlankRename.hasError(), qPrintable(rejectBlankRename.error().toString()));
    QCOMPARE(rejectBlankRenameResult.toBool(), false);

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Ink pass"));
}

void tst_DrawingSurfaceItem::layersExposeHierarchyRowsAndReorderDrawableObjects()
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
    initialProperties.insert(QStringLiteral("width"), 500);
    initialProperties.insert(QStringLiteral("height"), 360);
    initialProperties.insert(QStringLiteral("documentViewModel"),
                             QVariant::fromValue(static_cast<QObject *>(&viewModel)));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    QQmlExpression appendObjects(engine.rootContext(),
                                 object.data(),
                                 QStringLiteral("appendDrawableObject({ id: 2, type: \"shape\", x: 10, y: 20, width: 30, height: 28, shapeKind: \"rectangle\", color: \"#1976d2\" });"
                                                "appendDrawableObject({ id: 3, type: \"text\", x: 40, y: 50, width: 120, height: 32, text: \"Label\", fontPixelSize: 18, color: \"#111111\" });"));
    appendObjects.evaluate();
    QVERIFY2(!appendObjects.hasError(), qPrintable(appendObjects.error().toString()));

    QVariantList rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.size(), 4);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("objectId")).toInt(), 3);
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Label"));
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("selected")).toBool(), true);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("objectId")).toInt(), 2);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Rectangle"));
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("draggable")).toBool(), true);
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("objectId")).toInt(), 1);
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("label")).toString(), QStringLiteral("Layer 1"));
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("raster-canvas"));
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("draggable")).toBool(), false);

    QQmlExpression selectLayer(engine.rootContext(),
                               object.data(),
                               QStringLiteral("activateLayerByKey(\"object-2\");"));
    const QVariant selectResult = selectLayer.evaluate();
    QVERIFY2(!selectLayer.hasError(), qPrintable(selectLayer.error().toString()));
    QCOMPARE(selectResult.toBool(), true);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), 2);

    QQmlExpression reorderLayers(engine.rootContext(),
                                 object.data(),
                                 QStringLiteral("applyLayerHierarchyOrder(["
                                                "{ key: \"object-2\", objectId: 2, layerKind: \"object\", depth: 0 },"
                                                "{ key: \"object-3\", objectId: 3, layerKind: \"object\", depth: 0 },"
                                                "{ key: \"object-1\", objectId: 1, layerKind: \"object\", depth: 0 },"
                                                "{ key: \"raster-canvas\", objectId: -1, layerKind: \"raster\", depth: 0 }"
                                                "]);"));
    const QVariant reorderResult = reorderLayers.evaluate();
    QVERIFY2(!reorderLayers.hasError(), qPrintable(reorderLayers.error().toString()));
    QCOMPARE(reorderResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 3);
    QCOMPARE(objects.at(0).toMap().value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(objects.at(1).toMap().value(QStringLiteral("id")).toInt(), 3);
    QCOMPARE(objects.at(2).toMap().value(QStringLiteral("id")).toInt(), 2);

    rows = rootItem->property("layerHierarchyRows").toList();
    QCOMPARE(rows.at(0).toMap().value(QStringLiteral("objectId")).toInt(), 2);
    QCOMPARE(rows.at(1).toMap().value(QStringLiteral("objectId")).toInt(), 3);
    QCOMPARE(rows.at(2).toMap().value(QStringLiteral("objectId")).toInt(), 1);
    QCOMPARE(rows.at(3).toMap().value(QStringLiteral("key")).toString(), QStringLiteral("raster-canvas"));

    QQmlExpression deleteLayer(engine.rootContext(),
                               object.data(),
                               QStringLiteral("deleteLayerByKey(\"object-2\");"));
    const QVariant deleteResult = deleteLayer.evaluate();
    QVERIFY2(!deleteLayer.hasError(), qPrintable(deleteLayer.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 2);
    QCOMPARE(objects.at(0).toMap().value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(objects.at(1).toMap().value(QStringLiteral("id")).toInt(), 3);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), -1);
}

void tst_DrawingSurfaceItem::drawsAndSavesStroke()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(128);
    item.setHeight(96);
    item.setDocumentViewModel(&viewModel);

    QSignalSpy contentChanged(&item, &DrawingSurfaceItem::rasterContentChanged);
    item.beginStroke(10, 10, 1.0, false);
    QVERIFY(item.appendStrokePoint(40, 40, 1.0, false));
    item.endStroke(60, 48, 1.0, false);

    QTRY_COMPARE(item.strokeCount(), 1);
    QTRY_VERIFY(contentChanged.count() > 0);

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
        QVERIFY(item.commitShape(x, y, 30, 28, shapeKinds.at(index), QColor(QStringLiteral("#1976d2"))));
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
    const QColor filledRectangleCenter = saved.pixelColor(25, 26);
    QVERIFY(filledRectangleCenter.alpha() > 0);
    QVERIFY(filledRectangleCenter.blue() > 120);
    QVERIFY(filledRectangleCenter.red() < 80);
    QVERIFY(filledRectangleCenter.green() > 70);
}

void tst_DrawingSurfaceItem::commitsSpeechBubbleTailsAsIntegratedSolidShapes()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(180);
    item.setHeight(96);
    item.setDocumentViewModel(&viewModel);

    const QColor shapeColor(QStringLiteral("#1976d2"));
    QVERIFY(item.commitShape(16, 16, 64, 56, QStringLiteral("rectanglebubble"), shapeColor));
    QVERIFY(item.commitShape(96, 16, 64, 56, QStringLiteral("ellipsebubble"), shapeColor));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("speech-bubble-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(180, 96));

    QVERIFY(isBlueShapePixel(saved.pixelColor(36, 62)));
    QVERIFY(isBlueShapePixel(saved.pixelColor(116, 59)));
    QVERIFY(isBlueShapePixel(saved.pixelColor(108, 68)));
}

void tst_DrawingSurfaceItem::fillsContiguousRasterRegion()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    viewModel.setBrushSize(5);
    viewModel.setBrushColor(QColor(QStringLiteral("#101010")));
    DrawingSurfaceItem item;
    item.setWidth(96);
    item.setHeight(64);
    item.setDocumentViewModel(&viewModel);

    item.beginStroke(48, 0, 1.0, false);
    QVERIFY(item.appendStrokePoint(48, 63, 1.0, false));
    item.endStroke(48, 63, 1.0, false);
    QTRY_COMPARE(item.strokeCount(), 1);

    const QColor fillColor(QStringLiteral("#43a047"));
    QVERIFY(item.fillAt(12, 12, fillColor));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString outputPath = dir.filePath(QStringLiteral("fill-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(96, 64));
    QCOMPARE(saved.pixelColor(12, 12).rgba(), fillColor.rgba());
    QCOMPARE(saved.pixelColor(84, 12).alpha(), 0);
    QVERIFY(saved.pixelColor(48, 32).alpha() > 0);
}

void tst_DrawingSurfaceItem::cachesLayerBitmapThumbnails()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    QVERIFY(item.cacheGrabbedThumbnailSource(nullptr).isEmpty());
    item.setWidth(96);
    item.setHeight(64);
    item.setDocumentViewModel(&viewModel);

    QSignalSpy contentChanged(&item, &DrawingSurfaceItem::rasterContentChanged);
    QVERIFY(item.commitShape(12, 10, 36, 28, QStringLiteral("rectangle"), QColor(QStringLiteral("#1976d2"))));
    QVERIFY(contentChanged.count() > 0);

    const QString rasterThumbnailSource = item.cacheRasterThumbnailSource(32, 32);
    QVERIFY(!rasterThumbnailSource.isEmpty());
    const QString rasterThumbnailPath = QUrl(rasterThumbnailSource).toLocalFile();
    QVERIFY(QFileInfo::exists(rasterThumbnailPath));
    const QImage rasterThumbnail(rasterThumbnailPath);
    QVERIFY(!rasterThumbnail.isNull());
    QCOMPARE(rasterThumbnail.size(), QSize(32, 32));

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 8);
    shapeObject.insert(QStringLiteral("y"), 6);
    shapeObject.insert(QStringLiteral("width"), 40);
    shapeObject.insert(QStringLiteral("height"), 30);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("ellipse"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    const QString objectThumbnailSource = item.cacheDrawableObjectThumbnailSource(shapeObject, 32, 32);
    QVERIFY(!objectThumbnailSource.isEmpty());
    const QString objectThumbnailPath = QUrl(objectThumbnailSource).toLocalFile();
    QVERIFY(QFileInfo::exists(objectThumbnailPath));
    const QImage objectThumbnail(objectThumbnailPath);
    QVERIFY(!objectThumbnail.isNull());
    QCOMPARE(objectThumbnail.size(), QSize(32, 32));

    bool hasObjectColorPixel = false;
    for (int y = 0; y < objectThumbnail.height() && !hasObjectColorPixel; ++y) {
        for (int x = 0; x < objectThumbnail.width(); ++x) {
            const QColor pixel = objectThumbnail.pixelColor(x, y);
            if (pixel.alpha() > 0 && pixel.blue() > 120 && pixel.red() < 80 && pixel.green() > 70) {
                hasObjectColorPixel = true;
                break;
            }
        }
    }
    QVERIFY(hasObjectColorPixel);
}

void tst_DrawingSurfaceItem::savesCompositeDrawableObjectsWithoutFlatteningRaster()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(120);
    item.setHeight(90);
    item.setDocumentViewModel(&viewModel);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 16);
    shapeObject.insert(QStringLiteral("y"), 18);
    shapeObject.insert(QStringLiteral("width"), 48);
    shapeObject.insert(QStringLiteral("height"), 36);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    const QColor imageObjectColor(QStringLiteral("#f4511e"));
    QImage imageObjectRaster(18, 14, QImage::Format_ARGB32);
    imageObjectRaster.fill(imageObjectColor);
    const QString imageObjectPath = dir.filePath(QStringLiteral("image-object.png"));
    QVERIFY(imageObjectRaster.save(imageObjectPath));

    QVariantMap imageObject;
    imageObject.insert(QStringLiteral("id"), 2);
    imageObject.insert(QStringLiteral("type"), QStringLiteral("image"));
    imageObject.insert(QStringLiteral("x"), 72);
    imageObject.insert(QStringLiteral("y"), 24);
    imageObject.insert(QStringLiteral("width"), 18);
    imageObject.insert(QStringLiteral("height"), 14);
    imageObject.insert(QStringLiteral("source"), QUrl::fromLocalFile(imageObjectPath).toString());

    QVariantList objects;
    objects.append(shapeObject);
    objects.append(imageObject);

    const QString compositePath = dir.filePath(QStringLiteral("composite-output.png"));
    QVERIFY(item.saveToFileWithObjects(compositePath, objects));

    const QImage composite(compositePath);
    QVERIFY(!composite.isNull());
    QCOMPARE(composite.size(), QSize(120, 90));

    bool hasShapeColorPixel = false;
    for (int y = 0; y < composite.height() && !hasShapeColorPixel; ++y) {
        for (int x = 0; x < composite.width(); ++x) {
            const QColor pixel = composite.pixelColor(x, y);
            if (pixel.alpha() > 0 && pixel.blue() > 120 && pixel.red() < 80 && pixel.green() > 70) {
                hasShapeColorPixel = true;
                break;
            }
        }
    }
    QVERIFY(hasShapeColorPixel);
    const QColor filledShapeCenter = composite.pixelColor(40, 36);
    QVERIFY(filledShapeCenter.alpha() > 0);
    QVERIFY(filledShapeCenter.blue() > 120);
    QVERIFY(filledShapeCenter.red() < 80);
    QVERIFY(filledShapeCenter.green() > 70);
    QCOMPARE(composite.pixelColor(76, 28).rgba(), imageObjectColor.rgba());

    const QString rasterOnlyPath = dir.filePath(QStringLiteral("raster-only-output.png"));
    QVERIFY(item.saveToFile(rasterOnlyPath));
    const QImage rasterOnly(rasterOnlyPath);
    QVERIFY(!rasterOnly.isNull());
    QVERIFY(rasterOnly.pixelColor(16, 18).alpha() == 0);
    QVERIFY(rasterOnly.pixelColor(76, 28).alpha() == 0);
}

void tst_DrawingSurfaceItem::savesBlankCanvasAsOpaquePsdBackground()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(8);
    item.setHeight(6);
    item.setDocumentViewModel(&viewModel);
    item.newCanvas();

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("blank-canvas.psd"));
    QVERIFY(item.saveToFile(psdPath));

    const PsdImportedDocument importedDocument = PsdImageReader::readDocument(psdPath);
    QVERIFY(importedDocument.isValid());
    QCOMPARE(importedDocument.canvasSize, QSize(8, 6));
    QVERIFY(!importedDocument.mergedImage.isNull());
    QCOMPARE(importedDocument.layers.size(), 1);
    QCOMPARE(importedDocument.layers.at(0).name, QStringLiteral("Background"));
    QCOMPARE(importedDocument.layers.at(0).bounds, QRect(0, 0, 8, 6));

    const QColor expectedCanvasBackground(Qt::white);
    QCOMPARE(importedDocument.mergedImage.pixelColor(0, 0).rgba(), expectedCanvasBackground.rgba());
    QCOMPARE(importedDocument.mergedImage.pixelColor(7, 5).rgba(), expectedCanvasBackground.rgba());
    QCOMPARE(importedDocument.layers.at(0).image.pixelColor(0, 0).rgba(), expectedCanvasBackground.rgba());
    QCOMPARE(importedDocument.layers.at(0).image.pixelColor(7, 5).rgba(), expectedCanvasBackground.rgba());
}

void tst_DrawingSurfaceItem::savesPsdLayerRecordsInBottomToTopOrder()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(16);
    item.setHeight(12);
    item.setDocumentViewModel(&viewModel);

    QVariantList objects;
    for (int layerIndex = 1; layerIndex <= 4; ++layerIndex) {
        QVariantMap layerObject;
        layerObject.insert(QStringLiteral("id"), layerIndex);
        layerObject.insert(QStringLiteral("type"), QStringLiteral("layer"));
        layerObject.insert(QStringLiteral("name"), QStringLiteral("Layer %1").arg(layerIndex));
        layerObject.insert(QStringLiteral("x"), 0);
        layerObject.insert(QStringLiteral("y"), 0);
        layerObject.insert(QStringLiteral("width"), 16);
        layerObject.insert(QStringLiteral("height"), 12);
        objects.append(layerObject);
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("ordered-layers.psd"));
    QVERIFY(item.saveToFileWithObjects(psdPath, objects));

    QFile psdFile(psdPath);
    QVERIFY(psdFile.open(QIODevice::ReadOnly));
    QCOMPARE(psdLayerRecordNames(psdFile.readAll()),
             QStringList({QStringLiteral("Background"),
                          QStringLiteral("Layer 1"),
                          QStringLiteral("Layer 2"),
                          QStringLiteral("Layer 3"),
                          QStringLiteral("Layer 4")}));

    const PsdImportedDocument importedDocument = PsdImageReader::readDocument(psdPath);
    QVERIFY(importedDocument.isValid());
    QCOMPARE(importedDocument.layers.size(), 5);
    QCOMPARE(importedDocument.layers.at(0).name, QStringLiteral("Background"));
    QCOMPARE(importedDocument.layers.at(1).name, QStringLiteral("Layer 1"));
    QCOMPARE(importedDocument.layers.at(2).name, QStringLiteral("Layer 2"));
    QCOMPARE(importedDocument.layers.at(3).name, QStringLiteral("Layer 3"));
    QCOMPARE(importedDocument.layers.at(4).name, QStringLiteral("Layer 4"));
}

void tst_DrawingSurfaceItem::savesCompositeDrawableObjectsAsLayeredPsdWithMetadata()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(20);
    item.setHeight(12);
    item.setDocumentViewModel(&viewModel);

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 2);
    shapeObject.insert(QStringLiteral("y"), 3);
    shapeObject.insert(QStringLiteral("width"), 10);
    shapeObject.insert(QStringLiteral("height"), 6);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("composite-output.psd"));
    QVERIFY(item.saveToFileWithObjects(psdPath, {shapeObject}));

    QFile psdFile(psdPath);
    QVERIFY(psdFile.open(QIODevice::ReadOnly));
    const QByteArray psd = psdFile.readAll();
    QVERIFY(psd.size() > 40);
    QCOMPARE(psd.mid(0, 4), QByteArray("8BPS"));
    QCOMPARE(readUInt16(psd, 4), 1);
    QCOMPARE(readUInt16(psd, 12), 4);
    QCOMPARE(readUInt32(psd, 14), 12U);
    QCOMPARE(readUInt32(psd, 18), 20U);
    QCOMPARE(readUInt16(psd, 22), 8);
    QCOMPARE(readUInt16(psd, 24), 3);
    QCOMPARE(readUInt32(psd, 26), 0U);

    const qsizetype resourcesOffset = imageResourcesLengthOffset(psd);
    QVERIFY(resourcesOffset + 4 < psd.size());
    const quint32 imageResourcesLength = readUInt32(psd, static_cast<int>(resourcesOffset));
    QVERIFY(imageResourcesLength > 0);
    QVERIFY(psd.contains("VincentLayerManifestBase64"));
    QVERIFY(psd.contains("VincentLayerCount"));
    QVERIFY(psd.contains("base64-json"));

    const qsizetype layerOffset = layerMaskLengthOffset(psd);
    QVERIFY(layerOffset + 10 < psd.size());
    const quint32 layerMaskLength = readUInt32(psd, static_cast<int>(layerOffset));
    QVERIFY(layerMaskLength > 0);
    const quint32 layerInfoLength = readUInt32(psd, static_cast<int>(layerOffset + 4));
    QVERIFY(layerInfoLength > 0);
    QCOMPARE(readUInt16(psd, static_cast<int>(layerOffset + 8)), 2);
    QVERIFY(psd.contains("Background"));
    QVERIFY(psd.contains("Rectangle"));

    const PsdImportedDocument importedDocument = PsdImageReader::readDocument(psdPath);
    QVERIFY(importedDocument.isValid());
    QCOMPARE(importedDocument.canvasSize, QSize(20, 12));
    QCOMPARE(importedDocument.bitsPerChannel, 8);
    QVERIFY(!importedDocument.xmpMetadata.isEmpty());
    QCOMPARE(importedDocument.vincentManifest.value(QStringLiteral("compatibilityVersion")).toInt(), 1);
    QCOMPARE(importedDocument.vincentManifest.value(QStringLiteral("layers")).toList().size(), 2);
    QCOMPARE(importedDocument.layers.size(), 2);
    QCOMPARE(importedDocument.layers.at(0).name, QStringLiteral("Background"));
    QCOMPARE(importedDocument.layers.at(0).bounds, QRect(0, 0, 20, 12));
    QCOMPARE(importedDocument.layers.at(1).name, QStringLiteral("Rectangle"));
    QCOMPARE(importedDocument.layers.at(1).bounds, QRect(2, 3, 10, 6));
    QCOMPARE(importedDocument.layers.at(1).blendModeKey, QStringLiteral("norm"));
    QCOMPARE(importedDocument.layers.at(1).opacity, 255);
    QVERIFY(importedDocument.layers.at(1).visible);
    QCOMPARE(importedDocument.layers.at(1).image.size(), QSize(20, 12));
    QVERIFY(isBlueShapePixel(importedDocument.layers.at(1).image.pixelColor(4, 4)));
}

void tst_DrawingSurfaceItem::opensLayeredPsdThroughPsdSdkReader()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(24);
    item.setHeight(16);
    item.setDocumentViewModel(&viewModel);

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 4);
    shapeObject.insert(QStringLiteral("y"), 5);
    shapeObject.insert(QStringLiteral("width"), 12);
    shapeObject.insert(QStringLiteral("height"), 8);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("roundtrip.psd"));
    QVERIFY(item.saveToFileWithObjects(psdPath, {shapeObject}));

    DrawingSurfaceItem openedItem;
    openedItem.setWidth(24);
    openedItem.setHeight(16);
    openedItem.setDocumentViewModel(&viewModel);
    QVERIFY(openedItem.openRaster(psdPath));
    QCOMPARE(openedItem.width(), 24.0);
    QCOMPARE(openedItem.height(), 16.0);

    const QString outputPath = dir.filePath(QStringLiteral("roundtrip-output.png"));
    QVERIFY(openedItem.saveToFile(outputPath));
    const QImage opened(outputPath);
    QVERIFY(!opened.isNull());
    QCOMPARE(opened.size(), QSize(24, 16));
    QVERIFY(isBlueShapePixel(opened.pixelColor(8, 8)));
}

void tst_DrawingSurfaceItem::createsPsdDrawablePreviewForQmlImage()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(32);
    item.setHeight(20);
    item.setDocumentViewModel(&viewModel);

    QVariantMap shapeObject;
    shapeObject.insert(QStringLiteral("id"), 1);
    shapeObject.insert(QStringLiteral("type"), QStringLiteral("shape"));
    shapeObject.insert(QStringLiteral("x"), 0);
    shapeObject.insert(QStringLiteral("y"), 0);
    shapeObject.insert(QStringLiteral("width"), 32);
    shapeObject.insert(QStringLiteral("height"), 20);
    shapeObject.insert(QStringLiteral("shapeKind"), QStringLiteral("rectangle"));
    shapeObject.insert(QStringLiteral("color"), QStringLiteral("#1976d2"));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString psdPath = dir.filePath(QStringLiteral("preview-source.psd"));
    QVERIFY(item.saveToFileWithObjects(psdPath, {shapeObject}));

    const QVariantMap object = item.imageObjectForFile(psdPath, 128, 128);
    QCOMPARE(object.value(QStringLiteral("sourceFormat")).toString(), QStringLiteral("psd"));
    QCOMPARE(object.value(QStringLiteral("originalSource")).toString(), QUrl::fromLocalFile(psdPath).toString());
    QCOMPARE(object.value(QStringLiteral("width")).toInt(), 32);
    QCOMPARE(object.value(QStringLiteral("height")).toInt(), 20);

    const QUrl previewUrl(object.value(QStringLiteral("source")).toString());
    QVERIFY(previewUrl.isLocalFile());
    const QImage preview(previewUrl.toLocalFile());
    QVERIFY(!preview.isNull());
    QCOMPARE(preview.size(), QSize(32, 20));
    QVERIFY(isBlueShapePixel(preview.pixelColor(4, 4)));
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

void tst_DrawingSurfaceItem::opensLargeRasterWithinCurrentCanvasBounds()
{
    PaletteUtils paletteUtils;
    CanvasDocumentViewModel viewModel(&paletteUtils);
    DrawingSurfaceItem item;
    item.setWidth(320);
    item.setHeight(180);
    item.setDocumentViewModel(&viewModel);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage image(1200, 600, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#26c6da")));
    const QString inputPath = dir.filePath(QStringLiteral("large-background.png"));
    QVERIFY(image.save(inputPath));

    QVERIFY(item.openRaster(inputPath));
    QVERIFY(item.hasBackground());
    QCOMPARE(item.backgroundSource(), QUrl::fromLocalFile(inputPath).toString());
    QCOMPARE(item.width(), 320.0);
    QCOMPARE(item.height(), 160.0);
    QCOMPARE(viewModel.canvasWidth(), 320);
    QCOMPARE(viewModel.canvasHeight(), 160);

    const QString outputPath = dir.filePath(QStringLiteral("large-background-output.png"));
    QVERIFY(item.saveToFile(outputPath));
    const QImage saved(outputPath);
    QVERIFY(!saved.isNull());
    QCOMPARE(saved.size(), QSize(320, 160));
}

void tst_DrawingSurfaceItem::opensRasterThroughQmlAsDrawableImageObjectWithinWorkspaceBounds()
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
    initialProperties.insert(QStringLiteral("toolMode"), QStringLiteral("move"));

    QScopedPointer<QObject> object(component.createWithInitialProperties(initialProperties));
    QVERIFY2(!object.isNull(), qPrintable(qmlErrorsToString(component.errors())));
    auto *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    DrawingSurfaceItem *canvasItem = findDrawingSurfaceItem(rootItem);
    QVERIFY(canvasItem);
    const QSize workspaceSize = workspaceCanvasSize(720, 480);
    QTRY_COMPARE(canvasItem->width(), static_cast<qreal>(workspaceSize.width()));
    QTRY_COMPARE(canvasItem->height(), static_cast<qreal>(workspaceSize.height()));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QImage image(1600, 1200, QImage::Format_ARGB32);
    image.fill(QColor(QStringLiteral("#26c6da")));
    const QString inputPath = dir.filePath(QStringLiteral("oversized-open.png"));
    QVERIFY(image.save(inputPath));
    engine.rootContext()->setContextProperty(QStringLiteral("testOpenImageUrl"),
                                             QUrl::fromLocalFile(inputPath).toString());

    QQmlExpression openImage(engine.rootContext(),
                             object.data(),
                             QStringLiteral("openRaster(testOpenImageUrl);"));
    const QVariant openResult = openImage.evaluate();
    QVERIFY2(!openImage.hasError(), qPrintable(openImage.error().toString()));
    QCOMPARE(openResult.toBool(), true);

    QVERIFY(canvasItem->width() < rootItem->width());
    QVERIFY(canvasItem->height() < rootItem->height());
    QCOMPARE(canvasItem->width(), static_cast<qreal>(workspaceSize.width()));
    QCOMPARE(canvasItem->height(), static_cast<qreal>(workspaceSize.height()));

    const QSize expectedOpenedSize = image.size().scaled(workspaceSize, Qt::KeepAspectRatio);
    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    QVariantMap imageObject = objects.first().toMap();
    QCOMPARE(imageObject.value(QStringLiteral("type")).toString(), QStringLiteral("image"));
    QCOMPARE(imageObject.value(QStringLiteral("source")).toString(), QUrl::fromLocalFile(inputPath).toString());
    QCOMPARE(imageObject.value(QStringLiteral("width")).toInt(), expectedOpenedSize.width());
    QCOMPARE(imageObject.value(QStringLiteral("height")).toInt(), expectedOpenedSize.height());
    QVERIFY(imageObject.value(QStringLiteral("width")).toInt() <= workspaceSize.width());
    QVERIFY(imageObject.value(QStringLiteral("height")).toInt() <= workspaceSize.height());
    QCOMPARE(imageObject.value(QStringLiteral("x")).toInt(), qRound((workspaceSize.width() - expectedOpenedSize.width()) / 2.0));
    QCOMPARE(imageObject.value(QStringLiteral("y")).toInt(), qRound((workspaceSize.height() - expectedOpenedSize.height()) / 2.0));
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), imageObject.value(QStringLiteral("id")).toInt());
    QCOMPARE(viewModel.canvasWidth(), workspaceSize.width());
    QCOMPARE(viewModel.canvasHeight(), workspaceSize.height());

    const qreal startX = imageObject.value(QStringLiteral("x")).toReal();
    const qreal startY = imageObject.value(QStringLiteral("y")).toReal();
    QQmlExpression moveImage(engine.rootContext(),
                             object.data(),
                             QStringLiteral("const imageObject = drawableObjects[0];"
                                            "beginDrawableObjectTransform(imageObject.x + 4, imageObject.y + 4);"
                                            "updateDrawableObjectTransform(imageObject.x + 24, imageObject.y + 4);"
                                            "commitDrawableObjectTransform();"));
    moveImage.evaluate();
    QVERIFY2(!moveImage.hasError(), qPrintable(moveImage.error().toString()));

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    imageObject = objects.first().toMap();
    QCOMPARE(imageObject.value(QStringLiteral("x")).toReal(), startX + 20.0);
    QCOMPARE(imageObject.value(QStringLiteral("y")).toReal(), startY);
}

QTEST_MAIN(tst_DrawingSurfaceItem)

#include "tst_drawingsurfaceitem.moc"
