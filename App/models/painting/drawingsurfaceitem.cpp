#include "drawingsurfaceitem.h"

#include "../../brushengine.h"
#include "../../canvasbackend.h"
#include "../../rasterdocumentio.h"

#include <QFileInfo>
#include <QImageReader>
#include <QMetaProperty>
#include <QPainter>
#include <QPainterPath>
#include <QSignalBlocker>
#include <QUrl>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>

namespace {

constexpr int kMaxUndoSteps = 64;

QString normalizedToolMode(const QString &toolMode)
{
    return toolMode == QStringLiteral("eraser") ? QStringLiteral("eraser") : QStringLiteral("brush");
}

QVariant deepCloneVariant(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) {
        return value;
    }
    if (value.metaType().id() == QMetaType::QVariantMap) {
        QVariantMap clone;
        const QVariantMap map = value.toMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            clone.insert(it.key(), deepCloneVariant(it.value()));
        }
        return clone;
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        QVariantList clone;
        const QVariantList list = value.toList();
        clone.reserve(list.size());
        for (const QVariant &entry : list) {
            clone.push_back(deepCloneVariant(entry));
        }
        return clone;
    }
    return value;
}

} // namespace

DrawingSurfaceItem::DrawingSurfaceItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_brushEngine(new BrushEngine(this))
    , m_canvasBackend(new CanvasBackend(this))
    , m_rasterDocumentIO(new RasterDocumentIO(this))
{
    setAntialiasing(true);
    setOpaquePainting(true);
    setFillColor(Qt::transparent);

    connect(m_canvasBackend, &CanvasBackend::canUndoChanged, this, &DrawingSurfaceItem::canUndoChanged);
    connect(m_canvasBackend, &CanvasBackend::canRedoChanged, this, &DrawingSurfaceItem::canRedoChanged);
}

void DrawingSurfaceItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->fillRect(boundingRect(), Qt::white);

    if (!m_backgroundImage.isNull() && !m_backgroundPlacement.isEmpty()) {
        painter->drawImage(m_backgroundPlacement, m_backgroundImage);
    }

    for (const QVariant &strokeVar : m_strokes) {
        drawStroke(painter, strokeVar.toMap());
    }
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
    return m_backgroundSource;
}

bool DrawingSurfaceItem::hasBackground() const
{
    return !m_backgroundSource.isEmpty();
}

int DrawingSurfaceItem::strokeCount() const
{
    return m_strokes.size();
}

bool DrawingSurfaceItem::canUndo() const
{
    return m_canvasBackend->canUndo();
}

bool DrawingSurfaceItem::canRedo() const
{
    return m_canvasBackend->canRedo();
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
    if (!m_currentStroke.isEmpty()) {
        m_currentStroke.clear();
    }
    emit toolModeChanged();
}

void DrawingSurfaceItem::setDocumentViewModel(QObject *documentViewModel)
{
    if (m_documentViewModel == documentViewModel) {
        return;
    }

    disconnectDocumentViewModel();
    m_documentViewModel = documentViewModel;
    connectDocumentViewModel();
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

    pushUndoState();
    syncDocumentCanvasSize();
    clearDocumentState();
}

void DrawingSurfaceItem::clearCanvas()
{
    if (!canMutateDocument()) {
        return;
    }

    pushUndoState();
    clearDocumentState();
}

bool DrawingSurfaceItem::openRaster(const QString &fileUrl)
{
    if (!canMutateDocument()) {
        return false;
    }

    const QVariantMap openResult = m_rasterDocumentIO->loadRasterDocument(fileUrl);
    if (!openResult.value(QStringLiteral("ok")).toBool()) {
        return false;
    }

    pushUndoState();
    m_currentStroke.clear();

    const QString sourceUrl = openResult.value(QStringLiteral("source")).toString();
    if (!loadBackgroundImage(sourceUrl)) {
        return false;
    }

    const int documentWidth = openResult.value(QStringLiteral("width"), qMax(1, qRound(width()))).toInt();
    const int documentHeight = openResult.value(QStringLiteral("height"), qMax(1, qRound(height()))).toInt();
    const QVariantMap fit = m_canvasBackend->documentFitTransform(qMax(1, qRound(width())),
                                                                  qMax(1, qRound(height())),
                                                                  documentWidth,
                                                                  documentHeight);

    m_backgroundSource = sourceUrl;
    m_backgroundPlacement = QRectF(fit.value(QStringLiteral("offsetX")).toReal(),
                                   fit.value(QStringLiteral("offsetY")).toReal(),
                                   documentWidth * fit.value(QStringLiteral("scale")).toReal(),
                                   documentHeight * fit.value(QStringLiteral("scale")).toReal());
    emit backgroundChanged();
    update();
    return true;
}

bool DrawingSurfaceItem::saveToFile(const QString &fileUrl) const
{
    const QString path = toLocalPath(fileUrl);
    if (path.isEmpty()) {
        return false;
    }

    QImage image = renderToImage();
    if (image.isNull()) {
        return false;
    }

    return image.save(path);
}

void DrawingSurfaceItem::undo()
{
    if (!canMutateDocument() || !m_canvasBackend->canUndo()) {
        return;
    }

    const QVariantMap snapshot = m_canvasBackend->undo(captureSnapshot(), kMaxUndoSteps);
    if (!snapshot.isEmpty()) {
        applySnapshot(snapshot);
    }
}

void DrawingSurfaceItem::redo()
{
    if (!canMutateDocument() || !m_canvasBackend->canRedo()) {
        return;
    }

    const QVariantMap snapshot = m_canvasBackend->redo(captureSnapshot(), kMaxUndoSteps);
    if (!snapshot.isEmpty()) {
        applySnapshot(snapshot);
    }
}

void DrawingSurfaceItem::beginStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (!canMutateDocument()) {
        return;
    }
    if (m_toolMode != QStringLiteral("brush") && m_toolMode != QStringLiteral("eraser")) {
        return;
    }

    pushUndoState();
    m_currentStroke = {
        {QStringLiteral("color"), currentStrokeColor()},
        {QStringLiteral("size"), m_brushSize},
        {QStringLiteral("points"), QVariantList{createStrokePoint(pointX, pointY, rawPressure, pressureSensitive)}},
        {QStringLiteral("erase"), m_toolMode == QStringLiteral("eraser")},
        {QStringLiteral("pressureSensitive"), pressureSensitive}
    };
    m_strokes.push_back(cloneVariantMap(m_currentStroke));
    emit strokeCountChanged();
    update();
}

bool DrawingSurfaceItem::appendStrokePoint(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (m_currentStroke.isEmpty()) {
        return false;
    }

    QVariantList points = m_currentStroke.value(QStringLiteral("points")).toList();
    const QVariantMap nextPoint = createStrokePoint(pointX, pointY, rawPressure, pressureSensitive);
    if (points.isEmpty()) {
        points.push_back(nextPoint);
        m_currentStroke.insert(QStringLiteral("points"), points);
        m_strokes.back() = cloneVariantMap(m_currentStroke);
        update();
        return true;
    }

    const QVariantMap lastPoint = points.constLast().toMap();
    const qreal baseSize = m_currentStroke.value(QStringLiteral("size"), m_brushSize).toReal();
    if (!m_brushEngine->shouldAppendPoint(lastPoint.value(QStringLiteral("x")).toReal(),
                                          lastPoint.value(QStringLiteral("y")).toReal(),
                                          strokePointSize(lastPoint, baseSize),
                                          strokePointOpacity(lastPoint),
                                          nextPoint.value(QStringLiteral("x")).toReal(),
                                          nextPoint.value(QStringLiteral("y")).toReal(),
                                          nextPoint.value(QStringLiteral("size")).toReal(),
                                          nextPoint.value(QStringLiteral("opacity")).toReal(),
                                          baseSize)) {
        return false;
    }

    points.push_back(nextPoint);
    m_currentStroke.insert(QStringLiteral("points"), points);
    m_strokes.back() = cloneVariantMap(m_currentStroke);
    update();
    return true;
}

void DrawingSurfaceItem::endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    if (m_currentStroke.isEmpty()) {
        return;
    }

    appendStrokePoint(pointX, pointY, rawPressure, pressureSensitive);
    m_currentStroke.clear();
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
        m_canvasBackend->clearHistory();
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

    m_canvasBackend->clearHistory();
    update();
}

void DrawingSurfaceItem::syncDocumentCanvasSize()
{
    if (!m_documentViewModel) {
        return;
    }

    const int canvasWidth = qMax(1, qRound(width()));
    const int canvasHeight = qMax(1, qRound(height()));
    m_documentViewModel->setProperty("canvasWidth", canvasWidth);
    m_documentViewModel->setProperty("canvasHeight", canvasHeight);
}

QVariantMap DrawingSurfaceItem::captureBackgroundSnapshot() const
{
    if (m_backgroundSource.isEmpty()) {
        return {};
    }

    return {
        {QStringLiteral("source"), m_backgroundSource},
        {QStringLiteral("x"), m_backgroundPlacement.x()},
        {QStringLiteral("y"), m_backgroundPlacement.y()},
        {QStringLiteral("width"), m_backgroundPlacement.width()},
        {QStringLiteral("height"), m_backgroundPlacement.height()}
    };
}

void DrawingSurfaceItem::applyBackgroundSnapshot(const QVariantMap &snapshot)
{
    const QString nextSource = snapshot.value(QStringLiteral("source")).toString();
    m_backgroundSource = nextSource;
    m_backgroundPlacement = QRectF(snapshot.value(QStringLiteral("x")).toReal(),
                                   snapshot.value(QStringLiteral("y")).toReal(),
                                   snapshot.value(QStringLiteral("width")).toReal(),
                                   snapshot.value(QStringLiteral("height")).toReal());
    if (m_backgroundSource.isEmpty()) {
        m_backgroundImage = {};
    } else {
        const bool loaded = loadBackgroundImage(m_backgroundSource);
        Q_UNUSED(loaded)
    }
    emit backgroundChanged();
}

QVariantMap DrawingSurfaceItem::captureSnapshot() const
{
    return m_canvasBackend->captureSnapshot(qRound(width()),
                                            qRound(height()),
                                            cloneVariantList(m_strokes),
                                            captureBackgroundSnapshot());
}

void DrawingSurfaceItem::applySnapshot(const QVariantMap &snapshot)
{
    const int previousStrokeCount = m_strokes.size();
    m_strokes = cloneVariantList(snapshot.value(QStringLiteral("strokes")).toList());
    m_currentStroke.clear();
    applyBackgroundSnapshot(snapshot.value(QStringLiteral("background")).toMap());
    if (previousStrokeCount != m_strokes.size()) {
        emit strokeCountChanged();
    }
    update();
}

void DrawingSurfaceItem::pushUndoState()
{
    m_canvasBackend->pushUndoState(captureSnapshot(), kMaxUndoSteps);
}

void DrawingSurfaceItem::clearDocumentState()
{
    const int previousStrokeCount = m_strokes.size();
    m_strokes.clear();
    m_currentStroke.clear();
    m_backgroundSource.clear();
    m_backgroundPlacement = {};
    m_backgroundImage = {};
    if (previousStrokeCount != 0) {
        emit strokeCountChanged();
    }
    emit backgroundChanged();
    update();
}

QString DrawingSurfaceItem::currentStrokeColor() const
{
    return m_toolMode == QStringLiteral("eraser")
        ? QStringLiteral("#000000")
        : m_brushColor.name(QColor::HexRgb);
}

qreal DrawingSurfaceItem::strokePointSize(const QVariantMap &point, qreal fallbackSize) const
{
    return point.contains(QStringLiteral("size")) ? point.value(QStringLiteral("size")).toReal() : fallbackSize;
}

qreal DrawingSurfaceItem::strokePointOpacity(const QVariantMap &point) const
{
    if (point.contains(QStringLiteral("opacity"))) {
        return point.value(QStringLiteral("opacity")).toReal();
    }
    if (point.contains(QStringLiteral("pressure"))) {
        return point.value(QStringLiteral("pressure")).toReal();
    }
    return 1.0;
}

QVariantMap DrawingSurfaceItem::createStrokePoint(qreal pointX,
                                                  qreal pointY,
                                                  qreal rawPressure,
                                                  bool pressureSensitive) const
{
    const qreal baseSize = !m_currentStroke.isEmpty()
        ? m_currentStroke.value(QStringLiteral("size"), m_brushSize).toReal()
        : m_brushSize;
    qreal pointSize = m_brushEngine->sampleSize(baseSize, rawPressure, pressureSensitive);
    qreal pointOpacity = m_brushEngine->resolvedOpacity(rawPressure, pressureSensitive);

    const QVariantList points = m_currentStroke.value(QStringLiteral("points")).toList();
    if (!points.isEmpty()) {
        const QVariantMap lastPoint = points.constLast().toMap();
        const qreal previousSize = strokePointSize(lastPoint, baseSize);
        const qreal previousOpacity = strokePointOpacity(lastPoint);
        pointSize = m_brushEngine->smoothedSampleSize(previousSize, pointSize);
        pointOpacity = m_brushEngine->smoothedSampleOpacity(previousOpacity, pointOpacity);
    }

    return {
        {QStringLiteral("x"), pointX},
        {QStringLiteral("y"), pointY},
        {QStringLiteral("pressure"), m_brushEngine->resolvedPressure(rawPressure, pressureSensitive)},
        {QStringLiteral("size"), pointSize},
        {QStringLiteral("opacity"), pointOpacity}
    };
}

void DrawingSurfaceItem::drawStroke(QPainter *painter, const QVariantMap &stroke) const
{
    const QVariantList points = stroke.value(QStringLiteral("points")).toList();
    if (points.isEmpty()) {
        return;
    }

    painter->save();
    painter->setCompositionMode(stroke.value(QStringLiteral("erase")).toBool()
                                    ? QPainter::CompositionMode_Clear
                                    : QPainter::CompositionMode_SourceOver);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(stroke.value(QStringLiteral("color")).toString()));

    const qreal baseSize = stroke.value(QStringLiteral("size"), m_brushSize).toReal();
    if (points.size() == 1) {
        const QVariantMap point = points.constFirst().toMap();
        drawStamp(painter,
                  point.value(QStringLiteral("x")).toReal(),
                  point.value(QStringLiteral("y")).toReal(),
                  strokePointSize(point, baseSize),
                  strokePointOpacity(point));
        painter->restore();
        return;
    }

    const QVariantMap firstPoint = points.constFirst().toMap();
    drawStamp(painter,
              firstPoint.value(QStringLiteral("x")).toReal(),
              firstPoint.value(QStringLiteral("y")).toReal(),
              strokePointSize(firstPoint, baseSize),
              strokePointOpacity(firstPoint));

    for (int index = 1; index < points.size(); ++index) {
        const QVariantMap previousPoint = points.at(index - 1).toMap();
        const QVariantMap currentPoint = points.at(index).toMap();
        const qreal previousSize = strokePointSize(previousPoint, baseSize);
        const qreal currentSize = strokePointSize(currentPoint, baseSize);
        const qreal previousOpacity = strokePointOpacity(previousPoint);
        const qreal currentOpacity = strokePointOpacity(currentPoint);
        const int stamps = m_brushEngine->stampCount(previousPoint.value(QStringLiteral("x")).toReal(),
                                                     previousPoint.value(QStringLiteral("y")).toReal(),
                                                     previousSize,
                                                     currentPoint.value(QStringLiteral("x")).toReal(),
                                                     currentPoint.value(QStringLiteral("y")).toReal(),
                                                     currentSize);
        for (int step = 1; step <= stamps; ++step) {
            const qreal t = static_cast<qreal>(step) / static_cast<qreal>(stamps);
            const qreal stampX = previousPoint.value(QStringLiteral("x")).toReal()
                + (currentPoint.value(QStringLiteral("x")).toReal() - previousPoint.value(QStringLiteral("x")).toReal()) * t;
            const qreal stampY = previousPoint.value(QStringLiteral("y")).toReal()
                + (currentPoint.value(QStringLiteral("y")).toReal() - previousPoint.value(QStringLiteral("y")).toReal()) * t;
            const qreal stampSize = previousSize + (currentSize - previousSize) * t;
            const qreal stampOpacity = previousOpacity + (currentOpacity - previousOpacity) * t;
            drawStamp(painter, stampX, stampY, stampSize, stampOpacity);
        }
    }

    painter->restore();
}

void DrawingSurfaceItem::drawStamp(QPainter *painter, qreal pointX, qreal pointY, qreal diameter, qreal opacity) const
{
    if (diameter <= 0.0 || opacity <= 0.0) {
        return;
    }

    painter->setOpacity(opacity);
    painter->drawEllipse(QPointF(pointX, pointY), diameter / 2.0, diameter / 2.0);
    painter->setOpacity(1.0);
}

QImage DrawingSurfaceItem::renderToImage() const
{
    const QSize imageSize(qMax(1, qRound(width())), qMax(1, qRound(height())));
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (!m_backgroundImage.isNull() && !m_backgroundPlacement.isEmpty()) {
        painter.drawImage(m_backgroundPlacement, m_backgroundImage);
    }
    for (const QVariant &strokeVar : m_strokes) {
        drawStroke(&painter, strokeVar.toMap());
    }
    painter.end();
    return image;
}

QString DrawingSurfaceItem::toLocalPath(const QString &fileUrl) const
{
    if (fileUrl.isEmpty()) {
        return {};
    }

    const QUrl url(fileUrl);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }
    if (url.isValid() && !url.scheme().isEmpty()) {
        return {};
    }
    return fileUrl;
}

bool DrawingSurfaceItem::loadBackgroundImage(const QString &sourceUrl)
{
    const QString localPath = toLocalPath(sourceUrl);
    if (localPath.isEmpty()) {
        return false;
    }

    QImageReader reader(localPath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return false;
    }

    m_backgroundImage = image;
    return true;
}

QVariantList DrawingSurfaceItem::cloneVariantList(const QVariantList &value)
{
    QVariantList clone;
    clone.reserve(value.size());
    for (const QVariant &entry : value) {
        clone.push_back(deepCloneVariant(entry));
    }
    return clone;
}

QVariantMap DrawingSurfaceItem::cloneVariantMap(const QVariantMap &value)
{
    QVariantMap clone;
    for (auto it = value.constBegin(); it != value.constEnd(); ++it) {
        clone.insert(it.key(), deepCloneVariant(it.value()));
    }
    return clone;
}

void DrawingSurfaceItem::connectDocumentViewModel()
{
}

void DrawingSurfaceItem::disconnectDocumentViewModel()
{
}
