#include "drawingsurfaceitem.h"

#include "../canvas/canvasviewmodelbridge.h"

#include <QAbstractTextDocumentLayout>
#include <QDir>
#include <QEvent>
#include <QFont>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QTemporaryFile>
#include <QTextDocument>
#include <QTextOption>
#include <QUrl>
#include <QtGlobal>

namespace {

constexpr qreal minimumTextFontPixelSize = 8.0;
constexpr qreal maximumTextFontPixelSize = 144.0;
constexpr qreal minimumTextBoxWidth = 8.0;

QString localFileSource(const QString &fileUrl)
{
    const QUrl url(fileUrl);
    if (url.isValid() && url.isLocalFile()) {
        return url.toString();
    }
    return QUrl::fromLocalFile(fileUrl).toString();
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

bool DrawingSurfaceItem::openRaster(const QString &fileUrl)
{
    if (!canMutateDocument()) {
        return false;
    }

    const bool opened = CanvasAdapter::openRaster(fileUrl);
    if (!opened) {
        return false;
    }

    m_backgroundSource = localFileSource(fileUrl);
    m_hasBackground = true;
    syncCanvasSize();
    emit backgroundChanged();
    return true;
}

bool DrawingSurfaceItem::saveToFile(const QString &fileUrl)
{
    return CanvasAdapter::saveToFile(fileUrl);
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

    QImage image;
    QTemporaryFile snapshotFile(QDir::tempPath() + QStringLiteral("/vincent-text-canvas-XXXXXX.png"));
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

bool DrawingSurfaceItem::event(QEvent *event)
{
    if (event && isTabletEvent(event->type()) && isTextToolActive()) {
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
    if (isTextToolActive()) {
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
    if (isTextToolActive()) {
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
    if (isTextToolActive()) {
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
