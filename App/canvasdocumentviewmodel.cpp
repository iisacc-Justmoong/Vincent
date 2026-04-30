#include "canvasdocumentviewmodel.h"

#include "paletteutils.h"

#include <QStringList>
#include <QVariantMap>
#include <QtGlobal>

namespace {

QVariantMap paletteEntry(const QString &name, const QString &color)
{
    return {
        {QStringLiteral("name"), name},
        {QStringLiteral("color"), color}
    };
}

} // namespace

CanvasDocumentViewModel::CanvasDocumentViewModel(PaletteUtils *paletteUtils, QObject *parent)
    : QObject(parent)
    , m_palette(buildDefaultPalette(paletteUtils))
    , m_brushColor(QStringLiteral("#1a1a1a"))
{
}

QVariantList CanvasDocumentViewModel::palette() const
{
    return m_palette;
}

QColor CanvasDocumentViewModel::brushColor() const
{
    return m_brushColor;
}

qreal CanvasDocumentViewModel::brushSize() const
{
    return m_brushSize;
}

QString CanvasDocumentViewModel::toolMode() const
{
    return m_toolMode;
}

int CanvasDocumentViewModel::canvasWidth() const
{
    return m_canvasWidth;
}

int CanvasDocumentViewModel::canvasHeight() const
{
    return m_canvasHeight;
}

void CanvasDocumentViewModel::setBrushColor(const QColor &brushColor)
{
    if (m_brushColor == brushColor) {
        return;
    }

    m_brushColor = brushColor;
    emit brushColorChanged();
}

void CanvasDocumentViewModel::setBrushSize(qreal brushSize)
{
    const qreal boundedSize = qBound<qreal>(1.0, brushSize, 48.0);
    if (qFuzzyCompare(m_brushSize, boundedSize)) {
        return;
    }

    m_brushSize = boundedSize;
    emit brushSizeChanged();
}

void CanvasDocumentViewModel::setToolMode(const QString &toolMode)
{
    static const QStringList allowedTools{
        QStringLiteral("brush"),
        QStringLiteral("eraser")
    };

    const QString normalizedTool = allowedTools.contains(toolMode)
        ? toolMode
        : QStringLiteral("brush");
    if (m_toolMode == normalizedTool) {
        return;
    }

    m_toolMode = normalizedTool;
    emit toolModeChanged();
}

void CanvasDocumentViewModel::setCanvasWidth(int canvasWidth)
{
    const int boundedWidth = qMax(1, canvasWidth);
    if (m_canvasWidth == boundedWidth) {
        return;
    }

    m_canvasWidth = boundedWidth;
    emit canvasWidthChanged();
}

void CanvasDocumentViewModel::setCanvasHeight(int canvasHeight)
{
    const int boundedHeight = qMax(1, canvasHeight);
    if (m_canvasHeight == boundedHeight) {
        return;
    }

    m_canvasHeight = boundedHeight;
    emit canvasHeightChanged();
}

QVariantList CanvasDocumentViewModel::buildDefaultPalette(PaletteUtils *paletteUtils) const
{
    const QVariantList primary = {
        paletteEntry(QStringLiteral("Ink Black"), QStringLiteral("#1a1a1a")),
        paletteEntry(QStringLiteral("Signal Red"), QStringLiteral("#e53935")),
        paletteEntry(QStringLiteral("Amber"), QStringLiteral("#fb8c00")),
        paletteEntry(QStringLiteral("Sun Yellow"), QStringLiteral("#fdd835")),
        paletteEntry(QStringLiteral("Leaf Green"), QStringLiteral("#43a047")),
        paletteEntry(QStringLiteral("Sky Blue"), QStringLiteral("#1e88e5")),
        paletteEntry(QStringLiteral("Violet"), QStringLiteral("#5e35b1")),
        paletteEntry(QStringLiteral("Clay"), QStringLiteral("#8d6e63")),
        paletteEntry(QStringLiteral("Pure White"), QStringLiteral("#ffffff")),
        paletteEntry(QStringLiteral("Pitch Black"), QStringLiteral("#000000"))
    };

    const QVariantList extended = {
        paletteEntry(QStringLiteral("Coral"), QStringLiteral("#ff7043")),
        paletteEntry(QStringLiteral("Rose"), QStringLiteral("#f06292")),
        paletteEntry(QStringLiteral("Lilac"), QStringLiteral("#ba68c8")),
        paletteEntry(QStringLiteral("Cerulean"), QStringLiteral("#0091ea")),
        paletteEntry(QStringLiteral("Seafoam"), QStringLiteral("#26c6da")),
        paletteEntry(QStringLiteral("Forest"), QStringLiteral("#2e7d32")),
        paletteEntry(QStringLiteral("Olive"), QStringLiteral("#827717")),
        paletteEntry(QStringLiteral("Burnt Sienna"), QStringLiteral("#d84315")),
        paletteEntry(QStringLiteral("Slate"), QStringLiteral("#546e7a"))
    };

    if (paletteUtils) {
        return paletteUtils->buildDefaultPalette(primary, extended);
    }

    QVariantList fallback = primary;
    fallback.append(extended);
    return fallback;
}
