#pragma once

#include <QObject>

class BrushEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal minPressureScale READ minPressureScale CONSTANT)
    Q_PROPERTY(qreal pressureCurveExponent READ pressureCurveExponent CONSTANT)
    Q_PROPERTY(qreal minOpacityScale READ minOpacityScale CONSTANT)
    Q_PROPERTY(qreal opacityCurveExponent READ opacityCurveExponent CONSTANT)
    Q_PROPERTY(qreal sizeCarryOver READ sizeCarryOver CONSTANT)
    Q_PROPERTY(qreal opacityCarryOver READ opacityCarryOver CONSTANT)
    Q_PROPERTY(qreal minimumSampleSize READ minimumSampleSize CONSTANT)

public:
    explicit BrushEngine(QObject *parent = nullptr);

    [[nodiscard]] qreal minPressureScale() const;
    [[nodiscard]] qreal pressureCurveExponent() const;
    [[nodiscard]] qreal minOpacityScale() const;
    [[nodiscard]] qreal opacityCurveExponent() const;
    [[nodiscard]] qreal sizeCarryOver() const;
    [[nodiscard]] qreal opacityCarryOver() const;
    [[nodiscard]] qreal minimumSampleSize() const;

    Q_INVOKABLE qreal resolvedPressure(qreal rawPressure, bool pressureSensitive) const;
    Q_INVOKABLE qreal resolvedOpacity(qreal rawPressure, bool pressureSensitive) const;
    Q_INVOKABLE qreal sampleSize(qreal baseSize, qreal rawPressure, bool pressureSensitive) const;
    Q_INVOKABLE qreal smoothedSampleSize(qreal previousSize, qreal currentSize) const;
    Q_INVOKABLE qreal smoothedSampleOpacity(qreal previousOpacity, qreal currentOpacity) const;
    Q_INVOKABLE bool shouldAppendPoint(qreal lastX,
                                       qreal lastY,
                                       qreal lastSize,
                                       qreal lastOpacity,
                                       qreal nextX,
                                       qreal nextY,
                                       qreal nextSize,
                                       qreal nextOpacity,
                                       qreal baseSize) const;
    Q_INVOKABLE int stampCount(qreal fromX,
                               qreal fromY,
                               qreal fromSize,
                               qreal toX,
                               qreal toY,
                               qreal toSize) const;
};
