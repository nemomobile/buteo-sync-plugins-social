/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ** This program/library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation.
 **
 ** This program/library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this program/library; if not, write to the Free
 ** Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 ** 02110-1301 USA
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
