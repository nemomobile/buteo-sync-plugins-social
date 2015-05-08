/****************************************************************************
 **
 ** Copyright (C) 2015 Jolla Ltd.
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

#ifndef VKIMAGESYNCADAPTOR_H
#define VKIMAGESYNCADAPTOR_H

#include "vkdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtSql/QSqlDatabase>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <socialcache/vkimagesdatabase.h>

class VKImageSyncAdaptor : public VKDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    VKImageSyncAdaptor(QObject *parent);
    ~VKImageSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId);

protected: // implementing VKDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestData(int accountId, const QString &accessToken, const QString &continuationUrl,
                     const QString &vkUserId, const QString &vkAlbumId);
    void possiblyAddNewUser(const QString &vkUserId, int accountId, const QString &accessToken);


private Q_SLOTS:
    void albumsFinishedHandler();
    void imagesFinishedHandler();
    void userFinishedHandler();

private:
    QList<VKAlbum> m_receivedAlbums;
    QList<VKImage> m_receivedPhotos;
    QList<VKUser> m_receivedUsers;
    QSet<QString> m_requestedUsers; // only want to request the user information once.
    QSet<QString> m_requestedPhotosForOwnerAndAlbum; // owner_id:album_id
    QList<VKAlbum> m_emptyAlbums;
    VKImagesDatabase m_db;
    bool m_syncError;
};

#endif // VKIMAGESYNCADAPTOR_H
