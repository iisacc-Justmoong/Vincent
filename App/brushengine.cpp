#include "brushengine.h"

#include <QtMath>

#include <algorithm>

namespace {

constexpr qreal kMinPressureScale = 0.22;
constexpr qreal kPressureCurveExponent = 0.72;
constexpr qreal kMinOpacityScale = 0.16;
constexpr qreal kOpacityCurveExponent = 0.9;
constexpr qreal kSizeCarryOver = 0.35;
constexpr qreal kOpacityCarryOver = 0.45;
constexpr qreal kMinimumSampleSize = 0.75;

qreal clampUnit(qreal value)
{
    return std::clamp(value, 0.0, 1.0);
}

qreal effectiveBaseSize(qreal baseSize)
{
    return std::max<qreal>(1.0, baseSize);
}

} // namespace

BrushEngine::BrushEngine(QObject *parent)
    : QObject(parent)
{
}

qreal BrushEngine::minPressureScale() const
{
    return kMinPressureScale;
}

qreal BrushEngine::pressureCurveExponent() const
{
    return kPressureCurveExponent;
}

qreal BrushEngine::minOpacityScale() const
{
    return kMinOpacityScale;
}

qreal BrushEngine::opacityCurveExponent() const
{
    return kOpacityCurveExponent;
}

qreal BrushEngine::sizeCarryOver() const
{
    return kSizeCarryOver;
}

qreal BrushEngine::opacityCarryOver() const
{
    return kOpacityCarryOver;
}

qreal BrushEngine::minimumSampleSize() const
{
    return kMinimumSampleSize;
}

qreal BrushEngine::resolvedPressure(qreal rawPressure, bool pressureSensitive) const
{
    if (!pressureSensitive) {
        return 1.0;
    }

    const qreal normalizedPressure = clampUnit(rawPressure);
    const qreal curvedPressure = qPow(normalizedPressure, kPressureCurveExponent);
    return kMinPressureScale + (1.0 - kMinPressureScale) * curvedPressure;
}

qreal BrushEngine::resolvedOpacity(qreal rawPressure, bool pressureSensitive) const
{
    if (!pressureSensitive) {
        return 1.0;
    }

    const qreal normalizedPressure = clampUnit(rawPressure);
    const qreal curvedPressure = qPow(normalizedPressure, kOpacityCurveExponent);
    return kMinOpacityScale + (1.0 - kMinOpacityScale) * curvedPressure;
}

qreal BrushEngine::sampleSize(qreal baseSize, qreal rawPressure, bool pressureSensitive) const
{
    const qreal resolved = resolvedPressure(rawPressure, pressureSensitive);
    return std::max(kMinimumSampleSize, effectiveBaseSize(baseSize) * resolved);
}

qreal BrushEngine::smoothedSampleSize(qreal previousSize, qreal currentSize) const
{
    if (previousSize <= 0.0) {
        return std::max(kMinimumSampleSize, currentSize);
    }

    return std::max(kMinimumSampleSize, (previousSize * kSizeCarryOver) + (currentSize * (1.0 - kSizeCarryOver)));
}

qreal BrushEngine::smoothedSampleOpacity(qreal previousOpacity, qreal currentOpacity) const
{
    if (previousOpacity <= 0.0) {
        return clampUnit(currentOpacity);
    }

    return clampUnit((previousOpacity * kOpacityCarryOver) + (currentOpacity * (1.0 - kOpacityCarryOver)));
}

bool BrushEngine::shouldAppendPoint(qreal lastX,
                                    qreal lastY,
                                    qreal lastSize,
                                    qreal lastOpacity,
                                    qreal nextX,
                                    qreal nextY,
                                    qreal nextSize,
                                    qreal nextOpacity,
                                    qreal baseSize) const
{
    const qreal dx = nextX - lastX;
    const qreal dy = nextY - lastY;
    const qreal distanceSquared = (dx * dx) + (dy * dy);

    const qreal referenceSize = std::max({kMinimumSampleSize, lastSize, nextSize, effectiveBaseSize(baseSize) * 0.25});
    const qreal minDistance = std::max<qreal>(0.5, referenceSize * 0.18);
    if (distanceSquared >= minDistance * minDistance) {
        return true;
    }

    const qreal minSizeDelta = std::max<qreal>(0.12, effectiveBaseSize(baseSize) * 0.045);
    if (qAbs(nextSize - lastSize) >= minSizeDelta) {
        return true;
    }

    constexpr qreal kMinOpacityDelta = 0.035;
    return qAbs(nextOpacity - lastOpacity) >= kMinOpacityDelta;
}

int BrushEngine::stampCount(qreal fromX,
                            qreal fromY,
                            qreal fromSize,
                            qreal toX,
                            qreal toY,
                            qreal toSize) const
{
    const qreal dx = toX - fromX;
    const qreal dy = toY - fromY;
    const qreal distance = qSqrt((dx * dx) + (dy * dy));
    if (distance <= 0.0) {
        return 1;
    }

    const qreal averageSize = std::max(kMinimumSampleSize, (fromSize + toSize) * 0.5);
    const qreal step = std::max<qreal>(0.35, averageSize * 0.22);
    return std::max(1, qCeil(distance / step));
}
