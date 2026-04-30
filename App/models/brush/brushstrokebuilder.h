#pragma once

#include <QColor>
#include <QVariantMap>

class QPainter;

class BrushEngine;

class BrushStrokeBuilder
{
public:
    BrushStrokeBuilder();
    ~BrushStrokeBuilder();

    [[nodiscard]] QVariantMap beginStroke(const QString &toolMode,
                                          const QColor &brushColor,
                                          qreal brushSize,
                                          qreal pointX,
                                          qreal pointY,
                                          qreal rawPressure,
                                          bool pressureSensitive) const;
    [[nodiscard]] bool appendPoint(QVariantMap &stroke,
                                   qreal pointX,
                                   qreal pointY,
                                   qreal rawPressure,
                                   bool pressureSensitive) const;
    void drawStroke(QPainter *painter, const QVariantMap &stroke, qreal fallbackBrushSize) const;

private:
    [[nodiscard]] QString currentStrokeColor(const QString &toolMode, const QColor &brushColor) const;
    [[nodiscard]] qreal strokePointSize(const QVariantMap &point, qreal fallbackSize) const;
    [[nodiscard]] qreal strokePointOpacity(const QVariantMap &point) const;
    [[nodiscard]] QVariantMap createStrokePoint(const QVariantMap &stroke,
                                                qreal fallbackBrushSize,
                                                qreal pointX,
                                                qreal pointY,
                                                qreal rawPressure,
                                                bool pressureSensitive) const;
    void drawStamp(QPainter *painter, qreal pointX, qreal pointY, qreal diameter, qreal opacity) const;

    BrushEngine *m_engine = nullptr;
};
