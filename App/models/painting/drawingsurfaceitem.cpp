#include "drawingsurfaceitem.h"

#include "drawingdocumentcontroller.h"
#include "drawingstrokecontroller.h"
#include "paintingdocumentmodel.h"
#include "../brush/brushstrokebuilder.h"
#include "../canvas/canvasviewmodelbridge.h"
#include "../file/canvasfileadapter.h"

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
    , m_viewModelBridge(new CanvasViewModelBridge())
    , m_brushBuilder(new BrushStrokeBuilder())
    , m_fileAdapter(new CanvasFileAdapter())
    , m_paintingModel(new PaintingDocumentModel(this))
    , m_documentController(new DrawingDocumentController(m_paintingModel, m_fileAdapter, m_brushBuilder))
    , m_strokeController(new DrawingStrokeController(m_brushBuilder, m_paintingModel))
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
    delete m_strokeController;
    delete m_documentController;
    delete m_fileAdapter;
    delete m_brushBuilder;
    delete m_viewModelBridge;
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
    return m_viewModelBridge->documentViewModel();
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
    if (m_viewModelBridge->documentViewModel() == documentViewModel) {
        return;
    }

    m_viewModelBridge->setDocumentViewModel(documentViewModel);
    m_viewModelBridge->syncToolState(m_brushColor, m_brushSize, m_toolMode);
    m_viewModelBridge->syncCanvasSize(width(), height());
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
    m_viewModelBridge->syncCanvasSize(width(), height());
    m_documentController->newCanvas(canvasSize(), kMaxUndoSteps);
}

void DrawingSurfaceItem::clearCanvas()
{
    if (!canMutateDocument()) {
        return;
    }
    m_documentController->clearCanvas(canvasSize(), kMaxUndoSteps);
}

bool DrawingSurfaceItem::openRaster(const QString &fileUrl)
{
    return canMutateDocument() && m_documentController->openRaster(fileUrl, canvasSize(), kMaxUndoSteps);
}

bool DrawingSurfaceItem::saveToFile(const QString &fileUrl) const
{
    return m_documentController->saveToFile(fileUrl, canvasSize(), m_brushSize);
}

void DrawingSurfaceItem::undo()
{
    if (!canMutateDocument()) {
        return;
    }
    m_documentController->undo(canvasSize(), kMaxUndoSteps);
}

void DrawingSurfaceItem::redo()
{
    if (!canMutateDocument()) {
        return;
    }
    m_documentController->redo(canvasSize(), kMaxUndoSteps);
}

void DrawingSurfaceItem::beginStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (!canMutateDocument()) {
        return;
    }
    if (m_toolMode != QStringLiteral("brush") && m_toolMode != QStringLiteral("eraser")) {
        return;
    }

    m_strokeController->beginStroke(m_toolMode,
                                    m_brushColor,
                                    m_brushSize,
                                    pointX,
                                    pointY,
                                    rawPressure,
                                    pressureSensitive,
                                    canvasSize().width(),
                                    canvasSize().height(),
                                    kMaxUndoSteps);
}

bool DrawingSurfaceItem::appendStrokePoint(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    const bool appended = m_strokeController->appendStrokePoint(pointX, pointY, rawPressure, pressureSensitive);
    if (appended) {
        update();
    }
    return appended;
}

void DrawingSurfaceItem::endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    m_strokeController->endStroke(pointX, pointY, rawPressure, pressureSensitive);
}

void DrawingSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() == oldGeometry.size()) {
        return;
    }
    m_viewModelBridge->syncCanvasSize(newGeometry.width(), newGeometry.height());
    update();
}

QSize DrawingSurfaceItem::canvasSize() const
{
    return QSize(qMax(1, qRound(width())), qMax(1, qRound(height())));
}

bool DrawingSurfaceItem::canMutateDocument() const
{
    return m_viewModelBridge->canMutateDocument();
}
