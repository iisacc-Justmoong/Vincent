#pragma once

#include <QObject>
#include <QVariantMap>

class RasterDocumentIO : public QObject
{
    Q_OBJECT

public:
    explicit RasterDocumentIO(QObject *parent = nullptr);

    Q_INVOKABLE bool supportsRasterFile(const QString &fileUrl) const;
    Q_INVOKABLE QVariantMap loadRasterDocument(const QString &fileUrl) const;
};
