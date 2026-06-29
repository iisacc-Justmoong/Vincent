#pragma once

#include <QImage>
#include <QString>

class PsdImageReader
{
public:
    [[nodiscard]] static bool canReadPath(const QString &filePath);
    [[nodiscard]] static QImage readMergedImage(const QString &filePath);
};
