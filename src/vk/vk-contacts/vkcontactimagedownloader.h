/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef VKCONTACTIMAGEDOWNLOADER_H
#define VKCONTACTIMAGEDOWNLOADER_H

#include <socialcache/abstractimagedownloader.h>
#include <socialcache/abstractimagedownloader_p.h>

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QUrl>

class QNetworkReply;
class VKContactImageDownloader: public AbstractImageDownloader
{
    Q_OBJECT

public:
    explicit VKContactImageDownloader();
    static QString staticOutputFile(const QString &identifier, const QUrl &url);
protected:
    QNetworkReply * createReply(const QString &url, const QVariantMap &metadata);
    // This is a reimplemented method, used by AbstractImageDownloader
    QString outputFile(const QString &url, const QVariantMap &data) const;
private:
    Q_DECLARE_PRIVATE(AbstractImageDownloader)
};

#endif // VKCONTACTIMAGEDOWNLOADER_H
