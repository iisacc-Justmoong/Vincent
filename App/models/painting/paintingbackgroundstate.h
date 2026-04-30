#pragma once

#include <QImage>
#include <QRectF>
#include <QString>
#include <QVariantMap>

class PaintingBackgroundState
{
public:
    void clear();
    void setBackground(const QString &sourceUrl, const QImage &image, const QRectF &placement);
    void applySnapshot(const QVariantMap &snapshot);
    [[nodiscard]] QVariantMap snapshot() const;

    [[nodiscard]] QString source() const;
    [[nodiscard]] bool hasBackground() const;
    [[nodiscard]] const QImage &image() const;
    [[nodiscard]] QRectF placement() const;

private:
    QString m_source;
    QRectF m_placement;
    QImage m_image;
};
