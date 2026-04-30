#include "paintingdocumentmodel.h"

#include "../brush/brushstrokebuilder.h"
#include "../../canvasbackend.h"

#include <QImageReader>
#include <QPainter>
#include <QUrl>
#include <QtGlobal>

PaintingDocumentModel::PaintingDocumentModel(QObject *parent)
    : QObject(parent)
    , m_canvasBackend(new CanvasBackend(this))
{
    connect(m_canvasBackend, &CanvasBackend::canUndoChanged, this, &PaintingDocumentModel::canUndoChanged);
    connect(m_canvasBackend, &CanvasBackend::canRedoChanged, this, &PaintingDocumentModel::canRedoChanged);
}

bool PaintingDocumentModel::canUndo() const
{
    return m_canvasBackend->canUndo();
}

bool PaintingDocumentModel::canRedo() const
{
    return m_canvasBackend->canRedo();
}

int PaintingDocumentModel::strokeCount() const
{
    return m_strokes.size();
}

QVariantMap PaintingDocumentModel::currentStroke() const
{
    return cloneMap(m_currentStroke);
}

QString PaintingDocumentModel::backgroundSource() const
{
    return m_backgroundSource;
}

bool PaintingDocumentModel::hasBackground() const
{
    return !m_backgroundSource.isEmpty();
}

const QImage &PaintingDocumentModel::backgroundImage() const
{
    return m_backgroundImage;
}

QRectF PaintingDocumentModel::backgroundPlacement() const
{
    return m_backgroundPlacement;
}

void PaintingDocumentModel::pushUndoState(int canvasWidth, int canvasHeight, int maxUndoSteps)
{
    m_canvasBackend->pushUndoState(captureSnapshot(canvasWidth, canvasHeight), maxUndoSteps);
}

void PaintingDocumentModel::clear()
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
}

void PaintingDocumentModel::setBackground(const QString &sourceUrl, const QImage &image, const QRectF &placement)
{
    m_backgroundSource = sourceUrl;
    m_backgroundImage = image;
    m_backgroundPlacement = placement;
    emit backgroundChanged();
}

void PaintingDocumentModel::beginStroke(const QVariantMap &stroke)
{
    m_currentStroke = cloneMap(stroke);
    m_strokes.push_back(m_currentStroke);
    emit strokeCountChanged();
}

void PaintingDocumentModel::updateCurrentStroke(const QVariantMap &stroke)
{
    if (m_currentStroke.isEmpty() || m_strokes.isEmpty()) {
        return;
    }

    m_currentStroke = cloneMap(stroke);
    m_strokes.back() = m_currentStroke;
}

void PaintingDocumentModel::endStroke()
{
    m_currentStroke.clear();
}

void PaintingDocumentModel::undo(int canvasWidth, int canvasHeight, int maxUndoSteps)
{
    if (!canUndo()) {
        return;
    }
    applySnapshot(m_canvasBackend->undo(captureSnapshot(canvasWidth, canvasHeight), maxUndoSteps));
}

void PaintingDocumentModel::redo(int canvasWidth, int canvasHeight, int maxUndoSteps)
{
    if (!canRedo()) {
        return;
    }
    applySnapshot(m_canvasBackend->redo(captureSnapshot(canvasWidth, canvasHeight), maxUndoSteps));
}

void PaintingDocumentModel::render(QPainter *painter,
                                   const BrushStrokeBuilder &brushBuilder,
                                   qreal fallbackBrushSize) const
{
    painter->fillRect(QRectF(QPointF(0, 0), QSizeF(painter->device()->width(), painter->device()->height())), Qt::white);
    if (!m_backgroundImage.isNull() && !m_backgroundPlacement.isEmpty()) {
        painter->drawImage(m_backgroundPlacement, m_backgroundImage);
    }
    for (const QVariant &strokeVar : m_strokes) {
        brushBuilder.drawStroke(painter, strokeVar.toMap(), fallbackBrushSize);
    }
}

QImage PaintingDocumentModel::renderToImage(const QSize &imageSize,
                                            const BrushStrokeBuilder &brushBuilder,
                                            qreal fallbackBrushSize) const
{
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (!m_backgroundImage.isNull() && !m_backgroundPlacement.isEmpty()) {
        painter.drawImage(m_backgroundPlacement, m_backgroundImage);
    }
    for (const QVariant &strokeVar : m_strokes) {
        brushBuilder.drawStroke(&painter, strokeVar.toMap(), fallbackBrushSize);
    }
    painter.end();
    return image;
}

QVariantMap PaintingDocumentModel::captureSnapshot(int canvasWidth, int canvasHeight) const
{
    QVariantMap background;
    if (!m_backgroundSource.isEmpty()) {
        background = {
            {QStringLiteral("source"), m_backgroundSource},
            {QStringLiteral("x"), m_backgroundPlacement.x()},
            {QStringLiteral("y"), m_backgroundPlacement.y()},
            {QStringLiteral("width"), m_backgroundPlacement.width()},
            {QStringLiteral("height"), m_backgroundPlacement.height()}
        };
    }
    return m_canvasBackend->captureSnapshot(canvasWidth, canvasHeight, cloneList(m_strokes), background);
}

void PaintingDocumentModel::applySnapshot(const QVariantMap &snapshot)
{
    const int previousStrokeCount = m_strokes.size();
    m_strokes = cloneList(snapshot.value(QStringLiteral("strokes")).toList());
    m_currentStroke.clear();

    const QVariantMap background = snapshot.value(QStringLiteral("background")).toMap();
    m_backgroundSource = background.value(QStringLiteral("source")).toString();
    m_backgroundPlacement = QRectF(background.value(QStringLiteral("x")).toReal(),
                                   background.value(QStringLiteral("y")).toReal(),
                                   background.value(QStringLiteral("width")).toReal(),
                                   background.value(QStringLiteral("height")).toReal());
    if (m_backgroundSource.isEmpty()) {
        m_backgroundImage = {};
    } else {
        const QUrl url(m_backgroundSource);
        const QString localPath = url.isLocalFile() ? url.toLocalFile() : m_backgroundSource;
        QImageReader reader(localPath);
        reader.setAutoTransform(true);
        m_backgroundImage = reader.read();
    }

    if (previousStrokeCount != m_strokes.size()) {
        emit strokeCountChanged();
    }
    emit backgroundChanged();
}

QVariant PaintingDocumentModel::cloneDeep(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) {
        return value;
    }
    if (value.metaType().id() == QMetaType::QVariantMap) {
        return cloneMap(value.toMap());
    }
    if (value.metaType().id() == QMetaType::QVariantList) {
        return cloneList(value.toList());
    }
    return value;
}

QVariantMap PaintingDocumentModel::cloneMap(const QVariantMap &value)
{
    QVariantMap clone;
    for (auto it = value.constBegin(); it != value.constEnd(); ++it) {
        clone.insert(it.key(), cloneDeep(it.value()));
    }
    return clone;
}

QVariantList PaintingDocumentModel::cloneList(const QVariantList &value)
{
    QVariantList clone;
    clone.reserve(value.size());
    for (const QVariant &entry : value) {
        clone.push_back(cloneDeep(entry));
    }
    return clone;
}
