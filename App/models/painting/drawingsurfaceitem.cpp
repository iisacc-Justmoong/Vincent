#include "drawingsurfaceitem.h"

#include "../canvas/canvasviewmodelbridge.h"

#include <QAbstractTextDocumentLayout>
#include <QDir>
#include <QEvent>
#include <QFont>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QTemporaryFile>
#include <QTextDocument>
#include <QTextOption>
#include <QUrl>
#include <QVariantMap>
#include <QVector>
#include <QtMath>
#include <QtGlobal>

namespace {

constexpr qreal minimumTextFontPixelSize = 8.0;
constexpr qreal maximumTextFontPixelSize = 144.0;
constexpr qreal minimumTextBoxWidth = 8.0;
constexpr qreal minimumShapeDimension = 2.0;
constexpr qreal minimumShapeStrokeWidth = 1.0;
constexpr qreal maximumShapeStrokeWidth = 96.0;

QString localFileSource(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    if (url.isValid() && url.isLocalFile()) {
        return url.toString();
    }
    return QUrl::fromLocalFile(fileUrl).toString();
}

QString localFilePath(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    if (url.isValid() && url.isLocalFile()) {
        return url.toLocalFile();
    }
    return fileUrl;
}

bool isTabletEvent(QEvent::Type type)
{
    return type == QEvent::TabletPress
        || type == QEvent::TabletMove
        || type == QEvent::TabletRelease;
}

QImage transparentCanvasImage(const QSize &size)
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

QSize fittedOpenedRasterSize(const QSize &imageSize, const QSize &maximumSize)
{
    if (imageSize.isEmpty()) {
        return {};
    }

    const QSize boundedMaximum(qMax(1, maximumSize.width()), qMax(1, maximumSize.height()));
    if (imageSize.width() <= boundedMaximum.width() && imageSize.height() <= boundedMaximum.height()) {
        return imageSize;
    }

    const QSize fitted = imageSize.scaled(boundedMaximum, Qt::KeepAspectRatio);
    return QSize(qMax(1, fitted.width()), qMax(1, fitted.height()));
}

QString normalizedShapeKind(const QString &shapeKind)
{
    const QString normalized = shapeKind.trimmed().toLower();
    if (normalized == QStringLiteral("triagle")) {
        return QStringLiteral("triangle");
    }
    if (normalized == QStringLiteral("ellipse")
        || normalized == QStringLiteral("triangle")
        || normalized == QStringLiteral("diamond")
        || normalized == QStringLiteral("star")
        || normalized == QStringLiteral("rectanglebubble")
        || normalized == QStringLiteral("ellipsebubble")) {
        return normalized;
    }
    return QStringLiteral("rectangle");
}

QPainterPath starPath(const QRectF &rect)
{
    QPainterPath path;
    const QPointF center = rect.center();
    const qreal outerRadiusX = rect.width() / 2.0;
    const qreal outerRadiusY = rect.height() / 2.0;
    const qreal innerRadiusX = outerRadiusX * 0.45;
    const qreal innerRadiusY = outerRadiusY * 0.45;

    for (int index = 0; index < 10; ++index) {
        const bool outerPoint = index % 2 == 0;
        const qreal radiusX = outerPoint ? outerRadiusX : innerRadiusX;
        const qreal radiusY = outerPoint ? outerRadiusY : innerRadiusY;
        const qreal angle = -M_PI_2 + index * M_PI / 5.0;
        const QPointF point(center.x() + qCos(angle) * radiusX,
                            center.y() + qSin(angle) * radiusY);
        if (index == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }
    path.closeSubpath();
    return path;
}

QPainterPath speechBubblePath(const QRectF &rect, bool ellipseBody)
{
    QPainterPath path;
    const qreal tailHeight = qBound<qreal>(4.0, rect.height() * 0.22, rect.height() * 0.35);
    QRectF bodyRect = rect;
    bodyRect.setBottom(qMax(rect.top() + minimumShapeDimension, rect.bottom() - tailHeight));
    if (bodyRect.height() < minimumShapeDimension * 2.0) {
        bodyRect = rect;
    }

    if (ellipseBody) {
        path.addEllipse(bodyRect);
    } else {
        const qreal radius = qMin(bodyRect.width(), bodyRect.height()) * 0.12;
        path.addRoundedRect(bodyRect, radius, radius);
    }

    if (bodyRect.bottom() < rect.bottom()) {
        QPainterPath tailPath;
        tailPath.moveTo(bodyRect.left() + bodyRect.width() * 0.26, bodyRect.bottom());
        tailPath.lineTo(rect.left() + rect.width() * 0.18, rect.bottom());
        tailPath.lineTo(bodyRect.left() + bodyRect.width() * 0.44, bodyRect.bottom());
        tailPath.closeSubpath();
        path.addPath(tailPath);
    }

    return path;
}

QPainterPath shapePath(const QRectF &rect, const QString &shapeKind)
{
    QPainterPath path;
    const QString kind = normalizedShapeKind(shapeKind);

    if (kind == QStringLiteral("ellipse")) {
        path.addEllipse(rect);
        return path;
    }
    if (kind == QStringLiteral("triangle")) {
        path.moveTo(rect.center().x(), rect.top());
        path.lineTo(rect.right(), rect.bottom());
        path.lineTo(rect.left(), rect.bottom());
        path.closeSubpath();
        return path;
    }
    if (kind == QStringLiteral("diamond")) {
        path.moveTo(rect.center().x(), rect.top());
        path.lineTo(rect.right(), rect.center().y());
        path.lineTo(rect.center().x(), rect.bottom());
        path.lineTo(rect.left(), rect.center().y());
        path.closeSubpath();
        return path;
    }
    if (kind == QStringLiteral("star")) {
        return starPath(rect);
    }
    if (kind == QStringLiteral("rectanglebubble")) {
        return speechBubblePath(rect, false);
    }
    if (kind == QStringLiteral("ellipsebubble")) {
        return speechBubblePath(rect, true);
    }

    path.addRect(rect);
    return path;
}

void drawTextObject(QPainter &painter, const QVariantMap &object)
{
    const QString text = object.value(QStringLiteral("text")).toString();
    if (text.trimmed().isEmpty()) {
        return;
    }

    QRectF textRect(object.value(QStringLiteral("x")).toReal(),
                    object.value(QStringLiteral("y")).toReal(),
                    qMax<qreal>(minimumTextBoxWidth, object.value(QStringLiteral("width")).toReal()),
                    qMax<qreal>(minimumTextFontPixelSize, object.value(QStringLiteral("height")).toReal()));
    if (textRect.isEmpty()) {
        return;
    }

    const int fontPixelSize = qRound(qBound<qreal>(minimumTextFontPixelSize,
                                                  object.value(QStringLiteral("fontPixelSize")).toReal(),
                                                  maximumTextFontPixelSize));
    QFont font;
    font.setPixelSize(fontPixelSize);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    QTextDocument textDocument;
    textDocument.setDocumentMargin(0);
    textDocument.setDefaultFont(font);
    textDocument.setDefaultTextOption(textOption);
    textDocument.setPlainText(text);
    textDocument.setTextWidth(textRect.width());

    painter.save();
    painter.setClipRect(textRect);
    painter.translate(textRect.topLeft());

    QAbstractTextDocumentLayout::PaintContext paintContext;
    const QColor color(object.value(QStringLiteral("color")).toString());
    paintContext.palette.setColor(QPalette::Text, color.isValid() ? color : QColor(QStringLiteral("#1a1a1a")));
    textDocument.documentLayout()->draw(&painter, paintContext);
    painter.restore();
}

void drawShapeObject(QPainter &painter, const QVariantMap &object)
{
    QRectF shapeRect(object.value(QStringLiteral("x")).toReal(),
                     object.value(QStringLiteral("y")).toReal(),
                     object.value(QStringLiteral("width")).toReal(),
                     object.value(QStringLiteral("height")).toReal());
    shapeRect = shapeRect.normalized();
    if (shapeRect.width() < minimumShapeDimension || shapeRect.height() < minimumShapeDimension) {
        return;
    }

    const qreal boundedStrokeWidth = qBound<qreal>(minimumShapeStrokeWidth,
                                                   object.value(QStringLiteral("strokeWidth")).toReal(),
                                                   maximumShapeStrokeWidth);
    const qreal horizontalInset = qMin(boundedStrokeWidth / 2.0,
                                      qMax<qreal>(0.0, shapeRect.width() / 2.0 - 0.5));
    const qreal verticalInset = qMin(boundedStrokeWidth / 2.0,
                                    qMax<qreal>(0.0, shapeRect.height() / 2.0 - 0.5));
    const QRectF strokedRect = shapeRect.adjusted(horizontalInset,
                                                  verticalInset,
                                                  -horizontalInset,
                                                  -verticalInset);
    if (strokedRect.width() < minimumShapeDimension || strokedRect.height() < minimumShapeDimension) {
        return;
    }

    const QColor color(object.value(QStringLiteral("color")).toString());
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color.isValid() ? color : QColor(QStringLiteral("#1a1a1a")),
                        boundedStrokeWidth,
                        Qt::SolidLine,
                        Qt::RoundCap,
                        Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shapePath(strokedRect, object.value(QStringLiteral("shapeKind")).toString()));
    painter.restore();
}

void drawImageObject(QPainter &painter, const QVariantMap &object)
{
    const QImage image(localFilePath(object.value(QStringLiteral("source")).toString()));
    if (image.isNull()) {
        return;
    }

    QRectF imageRect(object.value(QStringLiteral("x")).toReal(),
                     object.value(QStringLiteral("y")).toReal(),
                     object.value(QStringLiteral("width")).toReal(),
                     object.value(QStringLiteral("height")).toReal());
    imageRect = imageRect.normalized();
    if (imageRect.width() < 1.0 || imageRect.height() < 1.0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(imageRect, image);
    painter.restore();
}

void drawObject(QPainter &painter, const QVariant &objectValue)
{
    const QVariantMap object = objectValue.toMap();
    const QString type = object.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("text")) {
        drawTextObject(painter, object);
        return;
    }
    if (type == QStringLiteral("shape")) {
        drawShapeObject(painter, object);
        return;
    }
    if (type == QStringLiteral("image")) {
        drawImageObject(painter, object);
    }
}

} // namespace

DrawingSurfaceItem::DrawingSurfaceItem(QQuickItem *parent)
    : CanvasAdapter(parent)
    , m_viewModelBridge(new CanvasViewModelBridge())
{
    connect(this, &CanvasAdapter::undoRedoChanged, this, &DrawingSurfaceItem::emitUndoRedoSignals);
}

DrawingSurfaceItem::~DrawingSurfaceItem()
{
    delete m_viewModelBridge;
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
    return m_backgroundSource;
}

bool DrawingSurfaceItem::hasBackground() const
{
    return m_hasBackground;
}

void DrawingSurfaceItem::setDocumentViewModel(QObject *documentViewModel)
{
    if (m_viewModelBridge->documentViewModel() == documentViewModel) {
        return;
    }

    m_viewModelBridge->setDocumentViewModel(documentViewModel);
    CanvasBrushConfig nextBrushConfig = brushConfig();
    QString nextToolMode = toolMode();
    m_viewModelBridge->syncToolState(nextBrushConfig, nextToolMode);
    setBrushConfig(nextBrushConfig);
    setToolMode(nextToolMode);
    syncCanvasSize();
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

    syncCanvasSize();
    const bool created = CanvasAdapter::newCanvas(canvasSize().width(), canvasSize().height());
    if (created && m_hasBackground) {
        m_hasBackground = false;
        m_backgroundSource.clear();
        emit backgroundChanged();
    }
}

void DrawingSurfaceItem::clearCanvas()
{
    newCanvas();
}

bool DrawingSurfaceItem::openRaster(const QString &fileUrl, qreal maximumCanvasWidth, qreal maximumCanvasHeight)
{
    if (!canMutateDocument()) {
        return false;
    }

    QImage image(localFilePath(fileUrl));
    if (image.isNull()) {
        return false;
    }

    const QSize maximumSize(
            maximumCanvasWidth > 0 ? qRound(maximumCanvasWidth) : canvasSize().width(),
            maximumCanvasHeight > 0 ? qRound(maximumCanvasHeight) : canvasSize().height());
    const QSize rasterSize = fittedOpenedRasterSize(image.size(), maximumSize);
    if (rasterSize.isEmpty()) {
        return false;
    }

    if (image.size() != rasterSize) {
        image = image.scaled(rasterSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    const bool opened = replaceRasterCanvas(image);
    if (!opened) {
        return false;
    }

    m_backgroundSource = localFileSource(fileUrl);
    m_hasBackground = true;
    syncCanvasSize();
    emit undoRedoChanged();
    emit backgroundChanged();
    return true;
}

QVariantMap DrawingSurfaceItem::imageObjectForFile(const QString &fileUrl,
                                                   qreal maximumObjectWidth,
                                                   qreal maximumObjectHeight) const
{
    const QImage image(localFilePath(fileUrl));
    if (image.isNull()) {
        return {};
    }

    const QSize maximumSize(
            maximumObjectWidth > 0 ? qRound(maximumObjectWidth) : canvasSize().width(),
            maximumObjectHeight > 0 ? qRound(maximumObjectHeight) : canvasSize().height());
    const QSize objectSize = fittedOpenedRasterSize(image.size(), maximumSize);
    if (objectSize.isEmpty()) {
        return {};
    }

    QVariantMap object;
    object.insert(QStringLiteral("source"), localFileSource(fileUrl));
    object.insert(QStringLiteral("width"), objectSize.width());
    object.insert(QStringLiteral("height"), objectSize.height());
    object.insert(QStringLiteral("originalWidth"), image.width());
    object.insert(QStringLiteral("originalHeight"), image.height());
    return object;
}

bool DrawingSurfaceItem::saveToFile(const QString &fileUrl)
{
    return CanvasAdapter::saveToFile(fileUrl);
}

bool DrawingSurfaceItem::saveToFileWithObjects(const QString &fileUrl, const QVariantList &objects)
{
    if (objects.isEmpty()) {
        return saveToFile(fileUrl);
    }

    syncCanvasSize();
    QImage image = currentRasterCanvasImage(canvasSize());
    if (image.isNull()) {
        return false;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    for (const QVariant &object : objects) {
        drawObject(painter, object);
    }
    painter.end();

    return image.save(localFilePath(fileUrl));
}

void DrawingSurfaceItem::undo()
{
    const bool applied = CanvasAdapter::undo();
    Q_UNUSED(applied)
}

void DrawingSurfaceItem::redo()
{
    const bool applied = CanvasAdapter::redo();
    Q_UNUSED(applied)
}

void DrawingSurfaceItem::resizeCanvasSurface(qreal canvasWidth, qreal canvasHeight)
{
    const qreal boundedWidth = qMax<qreal>(1.0, qRound(canvasWidth));
    const qreal boundedHeight = qMax<qreal>(1.0, qRound(canvasHeight));

    if (qFuzzyCompare(width(), boundedWidth) && qFuzzyCompare(height(), boundedHeight)) {
        syncCanvasSize();
        return;
    }

    m_isApplyingCanvasSurfaceSize = true;
    setWidth(boundedWidth);
    setHeight(boundedHeight);
    m_isApplyingCanvasSurfaceSize = false;

    syncCanvasSize();
}

void DrawingSurfaceItem::beginStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    Q_UNUSED(rawPressure)
    Q_UNUSED(pressureSensitive)

    if (!canMutateDocument()) {
        return;
    }

    QMouseEvent event = makeMouseEvent(QEvent::MouseButtonPress,
                                       pointX,
                                       pointY,
                                       Qt::LeftButton,
                                       Qt::LeftButton);
    CanvasAdapter::mousePressEvent(&event);
}

bool DrawingSurfaceItem::appendStrokePoint(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    Q_UNUSED(rawPressure)
    Q_UNUSED(pressureSensitive)

    if (!canMutateDocument()) {
        return false;
    }

    QMouseEvent event = makeMouseEvent(QEvent::MouseMove,
                                       pointX,
                                       pointY,
                                       Qt::NoButton,
                                       Qt::LeftButton);
    CanvasAdapter::mouseMoveEvent(&event);
    return true;
}

void DrawingSurfaceItem::endStroke(qreal pointX, qreal pointY, qreal rawPressure, bool pressureSensitive)
{
    Q_UNUSED(rawPressure)
    Q_UNUSED(pressureSensitive)

    if (!canMutateDocument()) {
        return;
    }

    QMouseEvent event = makeMouseEvent(QEvent::MouseButtonRelease,
                                       pointX,
                                       pointY,
                                       Qt::LeftButton,
                                       Qt::NoButton);
    CanvasAdapter::mouseReleaseEvent(&event);
}

bool DrawingSurfaceItem::commitText(qreal pointX,
                                    qreal pointY,
                                    qreal boxWidth,
                                    const QString &text,
                                    qreal fontPixelSize,
                                    const QColor &color)
{
    if (!canMutateDocument() || text.trimmed().isEmpty()) {
        return false;
    }

    syncCanvasSize();
    const QSize targetSize = canvasSize();
    if (targetSize.isEmpty()) {
        return false;
    }

    QImage image = currentRasterCanvasImage(targetSize);

    const qreal maxX = qMax<qreal>(0.0, image.width() - 1.0);
    const qreal maxY = qMax<qreal>(0.0, image.height() - 1.0);
    const qreal boundedX = qBound<qreal>(0.0, pointX, maxX);
    const qreal boundedY = qBound<qreal>(0.0, pointY, maxY);
    const qreal availableWidth = qMax<qreal>(1.0, image.width() - boundedX);
    const qreal textWidth = qBound<qreal>(minimumTextBoxWidth, boxWidth, availableWidth);
    const int boundedFontPixelSize = qRound(qBound<qreal>(minimumTextFontPixelSize,
                                                          fontPixelSize,
                                                          maximumTextFontPixelSize));

    QFont font;
    font.setPixelSize(boundedFontPixelSize);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    QTextDocument textDocument;
    textDocument.setDocumentMargin(0);
    textDocument.setDefaultFont(font);
    textDocument.setDefaultTextOption(textOption);
    textDocument.setPlainText(text);
    textDocument.setTextWidth(textWidth);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.translate(QPointF(boundedX, boundedY));

    QAbstractTextDocumentLayout::PaintContext paintContext;
    paintContext.palette.setColor(QPalette::Text, color.isValid() ? color : brushColor());
    textDocument.documentLayout()->draw(&painter, paintContext);
    painter.end();

    const bool committed = replaceRasterCanvas(image);
    if (committed) {
        emitUndoRedoSignals();
    }
    return committed;
}

bool DrawingSurfaceItem::commitShape(qreal pointX,
                                     qreal pointY,
                                     qreal boxWidth,
                                     qreal boxHeight,
                                     const QString &shapeKind,
                                     const QColor &color,
                                     qreal strokeWidth)
{
    if (!canMutateDocument()) {
        return false;
    }

    syncCanvasSize();
    const QSize targetSize = canvasSize();
    if (targetSize.isEmpty()) {
        return false;
    }

    QImage image = currentRasterCanvasImage(targetSize);
    QRectF shapeRect(QPointF(pointX, pointY), QSizeF(boxWidth, boxHeight));
    shapeRect = shapeRect.normalized().intersected(QRectF(0.0, 0.0, image.width(), image.height()));
    if (shapeRect.width() < minimumShapeDimension || shapeRect.height() < minimumShapeDimension) {
        return false;
    }

    const qreal boundedStrokeWidth = qBound<qreal>(minimumShapeStrokeWidth,
                                                   strokeWidth,
                                                   maximumShapeStrokeWidth);
    const qreal horizontalInset = qMin(boundedStrokeWidth / 2.0,
                                      qMax<qreal>(0.0, shapeRect.width() / 2.0 - 0.5));
    const qreal verticalInset = qMin(boundedStrokeWidth / 2.0,
                                    qMax<qreal>(0.0, shapeRect.height() / 2.0 - 0.5));
    const QRectF strokedRect = shapeRect.adjusted(horizontalInset,
                                                  verticalInset,
                                                  -horizontalInset,
                                                  -verticalInset);
    if (strokedRect.width() < minimumShapeDimension || strokedRect.height() < minimumShapeDimension) {
        return false;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color.isValid() ? color : brushColor(),
                        boundedStrokeWidth,
                        Qt::SolidLine,
                        Qt::RoundCap,
                        Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shapePath(strokedRect, shapeKind));
    painter.end();

    const bool committed = replaceRasterCanvas(image);
    if (committed) {
        emitUndoRedoSignals();
    }
    return committed;
}

bool DrawingSurfaceItem::fillAt(qreal pointX, qreal pointY, const QColor &color)
{
    if (!canMutateDocument()) {
        return false;
    }

    syncCanvasSize();
    const QSize targetSize = canvasSize();
    if (targetSize.isEmpty()) {
        return false;
    }

    QImage image = currentRasterCanvasImage(targetSize);
    if (image.isNull()) {
        return false;
    }

    const int seedX = qBound(0, static_cast<int>(qFloor(pointX)), image.width() - 1);
    const int seedY = qBound(0, static_cast<int>(qFloor(pointY)), image.height() - 1);
    const QColor targetColor = image.pixelColor(seedX, seedY);
    QColor replacementColor = color.isValid() ? color : brushColor();
    if (!replacementColor.isValid()) {
        replacementColor = QColor(Qt::transparent);
    }
    if (targetColor.rgba() == replacementColor.rgba()) {
        return false;
    }

    QVector<QPoint> pending;
    pending.reserve(qMin(image.width() * image.height(), 4096));
    pending.append(QPoint(seedX, seedY));

    while (!pending.isEmpty()) {
        const QPoint point = pending.takeLast();
        if (point.x() < 0 || point.x() >= image.width() || point.y() < 0 || point.y() >= image.height()) {
            continue;
        }
        if (image.pixelColor(point).rgba() != targetColor.rgba()) {
            continue;
        }

        image.setPixelColor(point, replacementColor);
        pending.append(QPoint(point.x() + 1, point.y()));
        pending.append(QPoint(point.x() - 1, point.y()));
        pending.append(QPoint(point.x(), point.y() + 1));
        pending.append(QPoint(point.x(), point.y() - 1));
    }

    const bool committed = replaceRasterCanvas(image);
    if (committed) {
        emitUndoRedoSignals();
    }
    return committed;
}

bool DrawingSurfaceItem::event(QEvent *event)
{
    if (event && isTabletEvent(event->type()) && isOverlayToolActive()) {
        event->accept();
        return true;
    }
    if (event && isTabletEvent(event->type()) && !canMutateDocument()) {
        event->accept();
        return true;
    }
    return CanvasAdapter::event(event);
}

void DrawingSurfaceItem::mousePressEvent(QMouseEvent *event)
{
    if (isOverlayToolActive()) {
        event->accept();
        return;
    }
    if (!canMutateDocument()) {
        event->accept();
        return;
    }
    CanvasAdapter::mousePressEvent(event);
}

void DrawingSurfaceItem::mouseMoveEvent(QMouseEvent *event)
{
    if (isOverlayToolActive()) {
        event->accept();
        return;
    }
    if (!canMutateDocument()) {
        event->accept();
        return;
    }
    CanvasAdapter::mouseMoveEvent(event);
}

void DrawingSurfaceItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (isOverlayToolActive()) {
        event->accept();
        return;
    }
    if (!canMutateDocument()) {
        event->accept();
        return;
    }
    CanvasAdapter::mouseReleaseEvent(event);
}

void DrawingSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    CanvasAdapter::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() == oldGeometry.size()) {
        return;
    }
    if (m_isApplyingCanvasSurfaceSize) {
        return;
    }
    syncCanvasSize();
}

QSize DrawingSurfaceItem::canvasSize() const
{
    return QSize(qMax(1, qRound(width())), qMax(1, qRound(height())));
}

bool DrawingSurfaceItem::canMutateDocument() const
{
    return m_viewModelBridge->canMutateDocument();
}

bool DrawingSurfaceItem::isTextToolActive() const
{
    return toolMode() == QStringLiteral("text");
}

bool DrawingSurfaceItem::isShapeToolActive() const
{
    return toolMode() == QStringLiteral("shape");
}

bool DrawingSurfaceItem::isFillToolActive() const
{
    return toolMode() == QStringLiteral("fill");
}

bool DrawingSurfaceItem::isMoveToolActive() const
{
    return toolMode() == QStringLiteral("move");
}

bool DrawingSurfaceItem::isOverlayToolActive() const
{
    return isTextToolActive() || isShapeToolActive() || isFillToolActive() || isMoveToolActive();
}

QImage DrawingSurfaceItem::currentRasterCanvasImage(const QSize &targetSize)
{
    QImage image;
    QTemporaryFile snapshotFile(QDir::tempPath() + QStringLiteral("/vincent-canvas-XXXXXX.png"));
    snapshotFile.setAutoRemove(true);
    if (snapshotFile.open()) {
        const QString snapshotPath = snapshotFile.fileName();
        snapshotFile.close();
        if (saveRasterCanvasToFile(snapshotPath)) {
            image.load(snapshotPath);
        }
    }

    if (image.isNull()) {
        image = transparentCanvasImage(targetSize);
    }
    if (image.format() != QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    return image;
}

void DrawingSurfaceItem::syncCanvasSize()
{
    m_viewModelBridge->syncCanvasSize(width(), height());
}

void DrawingSurfaceItem::emitUndoRedoSignals()
{
    emit canUndoChanged();
    emit canRedoChanged();
}

QMouseEvent DrawingSurfaceItem::makeMouseEvent(QEvent::Type eventType,
                                               qreal pointX,
                                               qreal pointY,
                                               Qt::MouseButton button,
                                               Qt::MouseButtons buttons) const
{
    const QPointF position{pointX, pointY};
    return QMouseEvent{eventType,
                       position,
                       position,
                       position,
                       button,
                       buttons,
                       Qt::NoModifier};
}
