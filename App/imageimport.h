#pragma once

#include <QObject>
#include <QVariantMap>

class ImageImport : public QObject
{
    Q_OBJECT

public:
    explicit ImageImport(QObject *parent = nullptr);

    Q_INVOKABLE bool supportsImageFile(const QString &fileUrl) const;
    Q_INVOKABLE QVariantMap prepareImageImport(const QString &fileUrl);

    void setCacheDirectory(const QString &cacheDirectory);

private:
    QString m_cacheDirectory;
};
