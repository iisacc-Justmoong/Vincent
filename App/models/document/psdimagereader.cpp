#include "psdimagereader.h"

#include <PsdPch.h>
#include <PsdColorMode.h>
#include <PsdDocument.h>
#include <PsdFile.h>
#include <PsdImageDataSection.h>
#include <PsdMallocAllocator.h>
#include <PsdParseDocument.h>
#include <PsdParseImageDataSection.h>
#include <PsdPlanarImage.h>

#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QtGlobal>

#include <limits>

namespace {

class QFilePsdFile final : public psd::File
{
public:
    QFilePsdFile(psd::Allocator *allocator, const QString &filePath)
        : psd::File(allocator)
        , m_file(filePath)
    {
        m_file.open(QIODevice::ReadOnly);
    }

    [[nodiscard]] bool isOpen() const
    {
        return m_file.isOpen();
    }

private:
    struct ReadResult {
        bool ok = false;
    };

    bool DoOpenRead(const wchar_t *filename) override
    {
        if (m_file.isOpen()) {
            return true;
        }
        m_file.setFileName(QString::fromWCharArray(filename));
        return m_file.open(QIODevice::ReadOnly);
    }

    bool DoOpenWrite(const wchar_t *) override
    {
        return false;
    }

    bool DoClose() override
    {
        m_file.close();
        return true;
    }

    psd::File::ReadOperation DoRead(void *buffer, uint32_t count, uint64_t position) override
    {
        auto *result = new ReadResult;
        if (!m_file.isOpen()
                || position > static_cast<uint64_t>(std::numeric_limits<qint64>::max())
                || count > static_cast<uint32_t>(std::numeric_limits<qint64>::max())) {
            return result;
        }

        result->ok = m_file.seek(static_cast<qint64>(position))
            && m_file.read(static_cast<char *>(buffer), static_cast<qint64>(count)) == static_cast<qint64>(count);
        return result;
    }

    bool DoWaitForRead(psd::File::ReadOperation &operation) override
    {
        auto *result = static_cast<ReadResult *>(operation);
        const bool ok = result && result->ok;
        delete result;
        operation = nullptr;
        return ok;
    }

    psd::File::WriteOperation DoWrite(const void *, uint32_t, uint64_t) override
    {
        return nullptr;
    }

    bool DoWaitForWrite(psd::File::WriteOperation &) override
    {
        return false;
    }

    uint64_t DoGetSize() const override
    {
        return m_file.isOpen() ? static_cast<uint64_t>(m_file.size()) : 0;
    }

    QFile m_file;
};

[[nodiscard]] bool isSupportedMergedDocument(const psd::Document &document, const psd::ImageDataSection &imageData)
{
    return document.width > 0
        && document.height > 0
        && document.bitsPerChannel == 8
        && document.colorMode == psd::colorMode::RGB
        && imageData.imageCount >= 3;
}

[[nodiscard]] QImage imageFromMergedData(const psd::Document &document, const psd::ImageDataSection &imageData)
{
    if (!isSupportedMergedDocument(document, imageData)) {
        return {};
    }

    const int width = static_cast<int>(document.width);
    const int height = static_cast<int>(document.height);
    const auto *red = static_cast<const uchar *>(imageData.images[0].data);
    const auto *green = static_cast<const uchar *>(imageData.images[1].data);
    const auto *blue = static_cast<const uchar *>(imageData.images[2].data);
    const auto *alpha = imageData.imageCount >= 4 ? static_cast<const uchar *>(imageData.images[3].data) : nullptr;
    if (!red || !green || !blue) {
        return {};
    }

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        return {};
    }

    for (int y = 0; y < height; ++y) {
        uchar *line = image.scanLine(y);
        const qsizetype rowOffset = static_cast<qsizetype>(y) * width;
        for (int x = 0; x < width; ++x) {
            const qsizetype channelIndex = rowOffset + x;
            const int pixelIndex = x * 4;
            line[pixelIndex] = red[channelIndex];
            line[pixelIndex + 1] = green[channelIndex];
            line[pixelIndex + 2] = blue[channelIndex];
            line[pixelIndex + 3] = alpha ? alpha[channelIndex] : 255;
        }
    }

    return image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

} // namespace

bool PsdImageReader::canReadPath(const QString &filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("psd"), Qt::CaseInsensitive) == 0;
}

QImage PsdImageReader::readMergedImage(const QString &filePath)
{
    if (!canReadPath(filePath)) {
        return {};
    }

    psd::MallocAllocator allocator;
    QFilePsdFile file(&allocator, filePath);
    if (!file.isOpen()) {
        return {};
    }

    psd::Document *document = psd::CreateDocument(&file, &allocator);
    const auto destroyDocument = qScopeGuard([&] {
        psd::DestroyDocument(document, &allocator);
    });
    if (!document || document->imageDataSection.length == 0) {
        return {};
    }

    psd::ImageDataSection *imageData = psd::ParseImageDataSection(document, &file, &allocator);
    const auto destroyImageData = qScopeGuard([&] {
        psd::DestroyImageDataSection(imageData, &allocator);
    });
    if (!imageData) {
        return {};
    }

    return imageFromMergedData(*document, *imageData);
}
