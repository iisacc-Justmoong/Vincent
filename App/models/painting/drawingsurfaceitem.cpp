#include "drawingsurfaceitem.h"

#include "../brush/brushstrokebuilder.h"
#include "../file/canvasfileadapter.h"
#include "paintingdocumentmodel.h"
#include "../../canvasbackend.h"

#include <QPainter>
#include <QtGlobal>

namespace {

constexpr int kMaxUndoSteps = 64;

QString normalizedToolMode(const QString &toolMode)
{
    return toolMode == QStringLiteral("eraser") ? QStringLiteral("eraser") : QStringLiteral("brush");
}

} // namespace

DrawingSurfaceItem::DrawingSurfaceItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_brushBuilder(new BrushStrokeBuilder())
    , m_fileAdapter(new CanvasFileAdapter())
    , m_paintingModel(new PaintingDocumentModel(this))
{
    setAntialiasing(true);
    setOpaquePainting(true);
    setFillColor(Qt::transparent);

    connect(m_paintingModel, &PaintingDocumentModel::canUndoChanged, this, &DrawingSurfaceItem::canUndoChanged);
    connect(m_paintingModel, &PaintingDocumentModel::canRedoChanged, this, &DrawingSurfaceItem::canRedoChanged);
    connect(m_paintingModel, &PaintingDocumentModel::strokeCountChanged, this, [this]() {
        emit strokeCountChanged();
        update();
    });
    connect(m_paintingModel, &PaintingDocumentModel::backgroundChanged, this, [this]() {
        emit backgroundChanged();
        update();
    });
}

DrawingSurfaceItem::~DrawingSurfaceItem()
{
    delete m_brushBuilder;
    delete m_fileAdapter;
}

void DrawingSurfaceItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    m_paintingModel->render(painter, *m_brushBuilder, m_brushSize);
}

QColor DrawingSurfaceItem::brushColor() const
{
    return m_brushColor;
}

qreal DrawingSurfaceItem::brushSize() const
{
    return m_brushSize;
}

QString DrawingSurfaceItem::toolMode() const
{
    return m_toolMode;
}

QObject *DrawingSurfaceItem::documentViewModel() const
{
    return m_documentViewModel;
}

QString DrawingSurfaceItem::viewId() const
{
    return m_viewId;
}

QString DrawingSurfaceItem::backgroundSource() const
{
    return m_paintingModel->backgroundSource();
}

bool DrawingSurfaceItem::hasBackground() const
{
    return m_paintingModel->hasBackground();
}

int DrawingSurfaceItem::strokeCount() const
{
    return m_paintingModel->strokeCount();
}

bool DrawingSurfaceItem::canUndo() const
{
    return m_paintingModel->canUndo();
}

bool DrawingSurfaceItem::canRedo() const
{
    return m_paintingModel->canRedo();
}

void DrawingSurfaceItem::setBrushColor(const QColor &brushColor)
{
    if (m_brushColor == brushColor) {
        return;
    }
    m_brushColor = brushColor;
    emit brushColorChanged();
}

void DrawingSurfaceItem::setBrushSize(qreal brushSize)
{
    const qreal boundedSize = qBound<qreal>(1.0, brushSize, 48.0);
    if (qFuzzyCompare(m_brushSize, boundedSize)) {
        return;
    }
    m_brushSize = boundedSize;
    emit brushSizeChanged();
}

void DrawingSurfaceItem::setToolMode(const QString &toolMode)
{
    const QString normalizedTool = normalizedToolMode(toolMode);
    if (m_toolMode == normalizedTool) {
        return;
    }
    m_toolMode = normalizedTool;
    emit toolModeChanged();
}

void DrawingSurfaceItem::setDocumentViewModel(QObject *documentViewModel)
{
    if (m_documentViewModel == documentViewModel) {
        return;
    }
    m_documentViewModel = documentViewModel;
    syncWithDocumentViewModel();
    syncDocumentCanvasSize();
    emit documentViewModelChanged();
}

void DrawingSurfaceItem::setViewId(const QString &viewId)
{
    if (m_viewId == viewId) {
        return;
    }
    m_viewId = viewId;
    emit viewIdChanged();
}

void DrawingSurfaceItem::newCanvas()
{
    if (!canMutateDocument()) {
        return;
    }
    m_paintingModel->pushUndoState(qMax(1, qRound(width())), qMax(1, qRound(height())), kMaxUndoSteps);
    syncDocumentCanvasSize();
    m_paintingModel->clear();
}

void DrawingSurfaceItem::clearCanvas()
{
    if (!canMutateDocument()) {
        return;
    }
    m_paintingModel->pushUndoState(qMax(1, qRound(width())), qMax(1, qRound(height())), kMaxUndoSteps);
    m_paintingModel->clear();
}

bool DrawingSurfaceItem::openRaster(const QString &fileUrl)
{
    if (!canMutateDocument()) {
        return false;
    }

    const RasterLoadResult raster = m_fileAdapter->openRaster(fileUrl);
    if (!raster.ok) {
        return false;
    }

    m_paintingModel->pushUndoState(qMax(1, qRound(width())), qMax(1, qRound(height())), kMaxUndoSteps);
    const QVariantMap fit = CanvasBackend().documentFitTransform(qMax(1, qRound(width())),
                                                                 qMax(1, qRound(height())),
                                                                 raster.width,
                                                                 raster.height);
    m_paintingModel->setBackground(raster.sourceUrl,
                                   raster.image,
                                   QRectF(fit.value(QStringLiteral("offsetX")).toReal(),
                                          fit.value(QStringLiteral("offsetY")).toReal(),
                                          raster.width * fit.value(QStringLiteral("scale")).toReal(),
                                          raster.height * fit.value(QStringLiteral("scale")).toReal()));
    return true;
}

bool DrawingSurfaceItem::saveToFile(const QString &fileUrl) const
{
    return m_fileAdapter->saveImage(fileUrl,
                                    m_paintingModel->renderToImage(QSize(qMax(1, qRound(width())),
                                                                         qMax(1, qRound(height()))),
                                                                   *m_brushBuilder,
                                                                   m_brushSize));
}

void DrawingSurfaceItem::undo()
{
    if (!canMutateDocument()) {
        return;
    }
    m_paintingModel->undo(qMax(1, qRound(width())), qMax(1, qRound(height())), kMaxUndoSteps);
}

void DrawingSurfaceItem::redo()
{
    if (!canMutateDocument()) {
        return;
    }
    m_paintingModel->redo(qMax(1, qRound(width())), qMax(1, qRound(height())), kMaxUndoSteps);
}

void DrawingSurfaceItem::beginStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (!canMutateDocument()) {
        return;
    }
    if (m_toolMode != QStringLiteral("brush") && m_toolMode != QStringLiteral("eraser")) {
        return;
    }

    m_paintingModel->pushUndoState(qMax(1, qRound(width())), qMax(1, qRound(height())), kMaxUndoSteps);
    m_paintingModel->beginStroke(m_brushBuilder->beginStroke(m_toolMode,
                                                             m_brushColor,
                                                             m_brushSize,
                                                             pointX,
                                                             pointY,
                                                             rawPressure,
                                                             pressureSensitive));
}

bool DrawingSurfaceItem::appendStrokePoint(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    QVariantMap stroke = m_paintingModel->currentStroke();
    if (stroke.isEmpty()) {
        return false;
    }
    if (!m_brushBuilder->appendPoint(stroke, pointX, pointY, rawPressure, pressureSensitive)) {
        return false;
    }
    m_paintingModel->updateCurrentStroke(stroke);
    update();
    return true;
}

void DrawingSurfaceItem::endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (!appendStrokePoint(pointX, pointY, rawPressure, pressureSensitive)) {
        m_paintingModel->endStroke();
        return;
    }
    m_paintingModel->endStroke();
}

void DrawingSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() == oldGeometry.size()) {
        return;
    }
    syncDocumentCanvasSize();
    update();
}

bool DrawingSurfaceItem::canMutateDocument() const
{
    return !m_documentViewModel.isNull();
}

void DrawingSurfaceItem::syncWithDocumentViewModel()
{
    if (!m_documentViewModel) {
        return;
    }
    const QVariant brushColorValue = m_documentViewModel->property("brushColor");
    if (brushColorValue.isValid()) {
        setBrushColor(brushColorValue.value<QColor>());
    }
    const QVariant brushSizeValue = m_documentViewModel->property("brushSize");
    if (brushSizeValue.isValid()) {
        setBrushSize(brushSizeValue.toReal());
    }
    const QVariant toolModeValue = m_documentViewModel->property("toolMode");
    if (toolModeValue.isValid()) {
        setToolMode(toolModeValue.toString());
    }
}

void DrawingSurfaceItem::syncDocumentCanvasSize()
{
    if (!m_documentViewModel) {
        return;
    }
    m_documentViewModel->setProperty("canvasWidth", qMax(1, qRound(width())));
    m_documentViewModel->setProperty("canvasHeight", qMax(1, qRound(height())));
}
