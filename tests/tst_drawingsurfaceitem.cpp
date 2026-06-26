#include <QFileInfo>
#include <QImage>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>
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
    void constrainsShapeDragWithShiftModifier();
    void movesAndResizesDrawableObjects();
    void deletesSelectedDrawableObject();
    void drawsAndSavesStroke();
    void erasesCommittedStrokePixels();
    void commitsTextToRasterCanvas();
    void commitsShapeToRasterCanvas();
    void fillsContiguousRasterRegion();
    void savesCompositeDrawableObjectsWithoutFlatteningRaster();
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
                              QStringLiteral("appendDrawableObject({ id: 1, type: \"shape\", x: 10, y: 20, width: 30, height: 28, shapeKind: \"rectangle\", color: \"#1976d2\", strokeWidth: 3 });"
                                             "beginDrawableObjectTransform(20, 30); updateDrawableObjectTransform(40, 60); commitDrawableObjectTransform();"));
    moveObject.evaluate();
    QVERIFY2(!moveObject.hasError(), qPrintable(moveObject.error().toString()));

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    QVariantMap movedObject = objects.first().toMap();
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
    QCOMPARE(objects.size(), 1);
    const QVariantMap resizedObject = objects.first().toMap();
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
                                QStringLiteral("appendDrawableObject({ id: 1, type: \"shape\", x: 10, y: 20, width: 30, height: 28, shapeKind: \"rectangle\", color: \"#1976d2\", strokeWidth: 3 });"
                                               "appendDrawableObject({ id: 2, type: \"text\", x: 40, y: 50, width: 120, height: 32, text: \"Label\", fontPixelSize: 18, color: \"#111111\" });"
                                               "deleteSelectedDrawableObject();"));
    const QVariant deleteResult = deleteObject.evaluate();
    QVERIFY2(!deleteObject.hasError(), qPrintable(deleteObject.error().toString()));
    QCOMPARE(deleteResult.toBool(), true);

    QVariantList objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
    const QVariantMap remainingObject = objects.first().toMap();
    QCOMPARE(remainingObject.value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(rootItem->property("selectedDrawableObjectId").toInt(), -1);

    QQmlExpression deleteWithoutSelection(engine.rootContext(),
                                          object.data(),
                                          QStringLiteral("deleteSelectedDrawableObject();"));
    const QVariant deleteWithoutSelectionResult = deleteWithoutSelection.evaluate();
    QVERIFY2(!deleteWithoutSelection.hasError(), qPrintable(deleteWithoutSelection.error().toString()));
    QCOMPARE(deleteWithoutSelectionResult.toBool(), false);

    objects = rootItem->property("drawableObjects").toList();
    QCOMPARE(objects.size(), 1);
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
    shapeObject.insert(QStringLiteral("strokeWidth"), 5);

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
    QCOMPARE(composite.pixelColor(76, 28).rgba(), imageObjectColor.rgba());

    const QString rasterOnlyPath = dir.filePath(QStringLiteral("raster-only-output.png"));
    QVERIFY(item.saveToFile(rasterOnlyPath));
    const QImage rasterOnly(rasterOnlyPath);
    QVERIFY(!rasterOnly.isNull());
    QVERIFY(rasterOnly.pixelColor(16, 18).alpha() == 0);
    QVERIFY(rasterOnly.pixelColor(76, 28).alpha() == 0);
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
