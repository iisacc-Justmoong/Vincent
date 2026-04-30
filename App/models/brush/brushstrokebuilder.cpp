#include "brushstrokebuilder.h"

#include "brushengine.h"

#include <QPainter>

BrushStrokeBuilder::BrushStrokeBuilder()
    : m_engine(new BrushEngine())
{
}

BrushStrokeBuilder::~BrushStrokeBuilder()
{
    delete m_engine;
}

QVariantMap BrushStrokeBuilder::beginStroke(const QString &toolMode,
                                            const QColor &brushColor,
                                            qreal brushSize,
                                            qreal pointX,
                                            qreal pointY,
                                            qreal rawPressure,
                                            bool pressureSensitive) const
{
    QVariantMap stroke = {
        {QStringLiteral("color"), currentStrokeColor(toolMode, brushColor)},
        {QStringLiteral("size"), brushSize},
        {QStringLiteral("erase"), toolMode == QStringLiteral("eraser")},
        {QStringLiteral("pressureSensitive"), pressureSensitive}
    };
    stroke.insert(QStringLiteral("points"), QVariantList{createStrokePoint(stroke,
                                                                            brushSize,
                                                                            pointX,
                                                                            pointY,
                                                                            rawPressure,
                                                                            pressureSensitive)});
    return stroke;
}

bool BrushStrokeBuilder::appendPoint(QVariantMap &stroke,
                                     qreal pointX,
                                     qreal pointY,
                                     qreal rawPressure,
                                     bool pressureSensitive) const
{
    QVariantList points = stroke.value(QStringLiteral("points")).toList();
    const qreal baseSize = stroke.value(QStringLiteral("size")).toReal();
    const QVariantMap nextPoint = createStrokePoint(stroke,
                                                    baseSize,
                                                    pointX,
                                                    pointY,
                                                    rawPressure,
                                                    pressureSensitive);
    if (points.isEmpty()) {
        points.push_back(nextPoint);
        stroke.insert(QStringLiteral("points"), points);
        return true;
    }

    const QVariantMap lastPoint = points.constLast().toMap();
    if (!m_engine->shouldAppendPoint(lastPoint.value(QStringLiteral("x")).toReal(),
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
    stroke.insert(QStringLiteral("points"), points);
    return true;
}

void BrushStrokeBuilder::drawStroke(QPainter *painter, const QVariantMap &stroke, qreal fallbackBrushSize) const
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

    const qreal baseSize = stroke.value(QStringLiteral("size"), fallbackBrushSize).toReal();
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
        const int stamps = m_engine->stampCount(previousPoint.value(QStringLiteral("x")).toReal(),
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

QString BrushStrokeBuilder::currentStrokeColor(const QString &toolMode, const QColor &brushColor) const
{
    return toolMode == QStringLiteral("eraser")
        ? QStringLiteral("#000000")
        : brushColor.name(QColor::HexRgb);
}

qreal BrushStrokeBuilder::strokePointSize(const QVariantMap &point, qreal fallbackSize) const
{
    return point.contains(QStringLiteral("size")) ? point.value(QStringLiteral("size")).toReal() : fallbackSize;
}

qreal BrushStrokeBuilder::strokePointOpacity(const QVariantMap &point) const
{
    if (point.contains(QStringLiteral("opacity"))) {
        return point.value(QStringLiteral("opacity")).toReal();
    }
    if (point.contains(QStringLiteral("pressure"))) {
        return point.value(QStringLiteral("pressure")).toReal();
    }
    return 1.0;
}

QVariantMap BrushStrokeBuilder::createStrokePoint(const QVariantMap &stroke,
                                                  qreal fallbackBrushSize,
                                                  qreal pointX,
                                                  qreal pointY,
                                                  qreal rawPressure,
                                                  bool pressureSensitive) const
{
    const qreal baseSize = stroke.value(QStringLiteral("size"), fallbackBrushSize).toReal();
    qreal pointSize = m_engine->sampleSize(baseSize, rawPressure, pressureSensitive);
    qreal pointOpacity = m_engine->resolvedOpacity(rawPressure, pressureSensitive);

    const QVariantList points = stroke.value(QStringLiteral("points")).toList();
    if (!points.isEmpty()) {
        const QVariantMap lastPoint = points.constLast().toMap();
        pointSize = m_engine->smoothedSampleSize(strokePointSize(lastPoint, baseSize), pointSize);
        pointOpacity = m_engine->smoothedSampleOpacity(strokePointOpacity(lastPoint), pointOpacity);
    }

    return {
        {QStringLiteral("x"), pointX},
        {QStringLiteral("y"), pointY},
        {QStringLiteral("pressure"), m_engine->resolvedPressure(rawPressure, pressureSensitive)},
        {QStringLiteral("size"), pointSize},
        {QStringLiteral("opacity"), pointOpacity}
    };
}

void BrushStrokeBuilder::drawStamp(QPainter *painter, qreal pointX, qreal pointY, qreal diameter, qreal opacity) const
{
    if (diameter <= 0.0 || opacity <= 0.0) {
        return;
    }

    painter->setOpacity(opacity);
    painter->drawEllipse(QPointF(pointX, pointY), diameter / 2.0, diameter / 2.0);
    painter->setOpacity(1.0);
}
