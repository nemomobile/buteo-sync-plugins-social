/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "googlecontactimagedownloader.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>

static const char *IMAGE_DOWNLOADER_TOKEN_KEY = "url";
static const char *IMAGE_DOWNLOADER_IDENTIFIER_KEY = "identifier";

GoogleContactImageDownloader::GoogleContactImageDownloader()
    : AbstractImageDownloader()
{
}

QString GoogleContactImageDownloader::staticOutputFile(const QString &identifier, const QUrl &url)
{
    return makeOutputFile(SocialSyncInterface::Google, SocialSyncInterface::Contacts, identifier, url.toString());
}

QNetworkReply * GoogleContactImageDownloader::createReply(const QString &url,
                                                          const QVariantMap &metadata)
{
    Q_D(AbstractImageDownloader);

    QString accessToken = metadata.value(IMAGE_DOWNLOADER_TOKEN_KEY).toString();
    QNetworkRequest request(url);
    request.setRawHeader("GData-Version", "3.0");
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ") + accessToken).toUtf8());
    return d->networkAccessManager->get(request);
}

QString GoogleContactImageDownloader::outputFile(const QString &url, const QVariantMap &data) const
{
    return staticOutputFile(data.value(IMAGE_DOWNLOADER_IDENTIFIER_KEY).toString(), url);
}
