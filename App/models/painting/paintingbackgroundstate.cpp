#include "paintingbackgroundstate.h"

#include <QImageReader>
#include <QUrl>

void PaintingBackgroundState::clear()
{
    m_source.clear();
    m_placement = {};
    m_image = {};
}

void PaintingBackgroundState::setBackground(const QString &sourceUrl, const QImage &image, const QRectF &placement)
{
    m_source = sourceUrl;
    m_image = image;
    m_placement = placement;
}

void PaintingBackgroundState::applySnapshot(const QVariantMap &snapshot)
{
    m_source = snapshot.value(QStringLiteral("source")).toString();
    m_placement = QRectF(snapshot.value(QStringLiteral("x")).toReal(),
                         snapshot.value(QStringLiteral("y")).toReal(),
                         snapshot.value(QStringLiteral("width")).toReal(),
                         snapshot.value(QStringLiteral("height")).toReal());

    if (m_source.isEmpty()) {
        m_image = {};
        return;
    }

    const QUrl url(m_source);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : m_source;
    QImageReader reader(localPath);
    reader.setAutoTransform(true);
    m_image = reader.read();
}

QVariantMap PaintingBackgroundState::snapshot() const
{
    if (m_source.isEmpty()) {
        return {};
    }

    return {
        {QStringLiteral("source"), m_source},
        {QStringLiteral("x"), m_placement.x()},
        {QStringLiteral("y"), m_placement.y()},
        {QStringLiteral("width"), m_placement.width()},
        {QStringLiteral("height"), m_placement.height()}
    };
}

QString PaintingBackgroundState::source() const
{
    return m_source;
}

bool PaintingBackgroundState::hasBackground() const
{
    return !m_source.isEmpty();
}

const QImage &PaintingBackgroundState::image() const
{
    return m_image;
}

QRectF PaintingBackgroundState::placement() const
{
    return m_placement;
}
