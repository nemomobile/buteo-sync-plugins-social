/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "vkcontactimagedownloader.h"

#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>

static const char *IMAGE_DOWNLOADER_TOKEN_KEY = "token";
static const char *IMAGE_DOWNLOADER_IDENTIFIER_KEY = "identifier";

VKContactImageDownloader::VKContactImageDownloader()
    : AbstractImageDownloader()
{
}

QString VKContactImageDownloader::staticOutputFile(const QString &identifier, const QUrl &url)
{
    return makeOutputFile(SocialSyncInterface::VK, SocialSyncInterface::Contacts, identifier, url.toString());
}

QNetworkReply * VKContactImageDownloader::createReply(const QString &url,
                                                      const QVariantMap &metadata)
{
    Q_D(AbstractImageDownloader);

    // XXX TODO: this might not be needed.
    QString accessToken = metadata.value(IMAGE_DOWNLOADER_TOKEN_KEY).toString();
    QUrl requestUrl(url);
    QUrlQuery urlQuery;
    urlQuery.addQueryItem("access_token", accessToken);
    requestUrl.setQuery(urlQuery);

    QNetworkRequest request(requestUrl);
    return d->networkAccessManager->get(request);
}

QString VKContactImageDownloader::outputFile(const QString &url, const QVariantMap &data) const
{
    return staticOutputFile(data.value(IMAGE_DOWNLOADER_IDENTIFIER_KEY).toString(), url);
}
