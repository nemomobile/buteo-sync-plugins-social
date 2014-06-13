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

#ifndef GOOGLECONTACTIMAGEDOWNLOADER_H
#define GOOGLECONTACTIMAGEDOWNLOADER_H

#include <socialcache/abstractimagedownloader.h>
#include <socialcache/abstractimagedownloader_p.h>

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QUrl>

class QNetworkReply;
class GoogleContactImageDownloader: public AbstractImageDownloader
{
    Q_OBJECT

public:
    explicit GoogleContactImageDownloader();
    static QString staticOutputFile(const QString &identifier, const QUrl &url);
protected:
    QNetworkReply * createReply(const QString &url, const QVariantMap &metadata);
    // This is a reimplemented method, used by AbstractImageDownloader
    QString outputFile(const QString &url, const QVariantMap &data) const;
private:
    Q_DECLARE_PRIVATE(AbstractImageDownloader)
};

#endif // GOOGLECONTACTIMAGEDOWNLOADER_H
