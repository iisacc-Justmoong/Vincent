#include "paintingdocumentmodel.h"

#include "paintingbackgroundstate.h"
#include "paintinghistorycontroller.h"
#include "paintingrenderer.h"
#include "../brush/brushstrokebuilder.h"

PaintingDocumentModel::PaintingDocumentModel(QObject *parent)
    : QObject(parent)
    , m_backgroundState(new PaintingBackgroundState())
    , m_historyController(new PaintingHistoryController(this))
    , m_renderer(new PaintingRenderer())
{
    connect(m_historyController, &PaintingHistoryController::canUndoChanged, this, &PaintingDocumentModel::canUndoChanged);
    connect(m_historyController, &PaintingHistoryController::canRedoChanged, this, &PaintingDocumentModel::canRedoChanged);
}

PaintingDocumentModel::~PaintingDocumentModel()
{
    delete m_renderer;
    delete m_backgroundState;
}

bool PaintingDocumentModel::canUndo() const
{
    return m_historyController->canUndo();
}

bool PaintingDocumentModel::canRedo() const
{
    return m_historyController->canRedo();
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
    return m_backgroundState->source();
}

bool PaintingDocumentModel::hasBackground() const
{
    return m_backgroundState->hasBackground();
}

const QImage &PaintingDocumentModel::backgroundImage() const
{
    return m_backgroundState->image();
}

QRectF PaintingDocumentModel::backgroundPlacement() const
{
    return m_backgroundState->placement();
}

void PaintingDocumentModel::pushUndoState(int canvasWidth, int canvasHeight, int maxUndoSteps)
{
    m_historyController->pushUndoState(canvasWidth,
                                       canvasHeight,
                                       cloneList(m_strokes),
                                       m_backgroundState->snapshot(),
                                       maxUndoSteps);
}

void PaintingDocumentModel::clear()
{
    const int previousStrokeCount = m_strokes.size();
    m_strokes.clear();
    m_currentStroke.clear();
    m_backgroundState->clear();
    if (previousStrokeCount != 0) {
        emit strokeCountChanged();
    }
    emit backgroundChanged();
}

void PaintingDocumentModel::setBackground(const QString &sourceUrl, const QImage &image, const QRectF &placement)
{
    m_backgroundState->setBackground(sourceUrl, image, placement);
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
    applySnapshot(m_historyController->undo(canvasWidth,
                                            canvasHeight,
                                            cloneList(m_strokes),
                                            m_backgroundState->snapshot(),
                                            maxUndoSteps));
}

void PaintingDocumentModel::redo(int canvasWidth, int canvasHeight, int maxUndoSteps)
{
    if (!canRedo()) {
        return;
    }
    applySnapshot(m_historyController->redo(canvasWidth,
                                            canvasHeight,
                                            cloneList(m_strokes),
                                            m_backgroundState->snapshot(),
                                            maxUndoSteps));
}

void PaintingDocumentModel::render(QPainter *painter,
                                   const BrushStrokeBuilder &brushBuilder,
                                   qreal fallbackBrushSize) const
{
    m_renderer->render(painter,
                       m_strokes,
                       m_backgroundState->image(),
                       m_backgroundState->placement(),
                       brushBuilder,
                       fallbackBrushSize);
}

QImage PaintingDocumentModel::renderToImage(const QSize &imageSize,
                                            const BrushStrokeBuilder &brushBuilder,
                                            qreal fallbackBrushSize) const
{
    return m_renderer->renderToImage(imageSize,
                                     m_strokes,
                                     m_backgroundState->image(),
                                     m_backgroundState->placement(),
                                     brushBuilder,
                                     fallbackBrushSize);
}

void PaintingDocumentModel::applySnapshot(const QVariantMap &snapshot)
{
    const int previousStrokeCount = m_strokes.size();
    m_strokes = cloneList(snapshot.value(QStringLiteral("strokes")).toList());
    m_currentStroke.clear();
    m_backgroundState->applySnapshot(snapshot.value(QStringLiteral("background")).toMap());
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
