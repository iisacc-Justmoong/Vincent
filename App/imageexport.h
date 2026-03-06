#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class ImageExport : public QObject
{
    Q_OBJECT

public:
    explicit ImageExport(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap saveDocumentAsPsd(const QString &fileUrl,
                                              int canvasWidth,
                                              int canvasHeight,
                                              const QVariantList &layers,
                                              const QString &strokeDataUrl) const;
};
