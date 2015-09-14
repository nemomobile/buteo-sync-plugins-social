/****************************************************************************
 **
 ** Copyright (C) 2015 Jolla Ltd.
 ** Contact: Antti Seppälä <antti.seppala@jollamobile.com>
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

#ifndef ONEDRIVEIMAGESYNCADAPTOR_H
#define ONEDRIVEIMAGESYNCADAPTOR_H

#include "onedrivedatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtSql/QSqlDatabase>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <socialcache/onedriveimagesdatabase.h>
#include <socialcache/socialimagesdatabase.h>

class OneDriveImageSyncAdaptor
        : public OneDriveDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    OneDriveImageSyncAdaptor(QObject *parent);
    ~OneDriveImageSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId);

protected: // implementing OneDriveDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void queryCameraRoll(int accountId, const QString &accessToken);
    void requestAlbums(int accountId, const QString &accessToken);
    void requestImages(int accountId, const QString &accessToken,
                       const QString &albumId, const QString &userId,
                       int imageCount = 0, const QString &nextRound = QString());
    void possiblyAddNewUser(const QString &fbUserId, int accountId, const QString &accessToken);

private Q_SLOTS:
    void cameraRollFinishedHandler();
    void albumsFinishedHandler();
    void imagesFinishedHandler();
    void userFinishedHandler();

private:
    struct AlbumData {
        AlbumData();
        AlbumData(QString albumId, QString userId, QDateTime createdTime,
                  QDateTime updatedTime, QString albumName, int imageCount);
        AlbumData(const AlbumData &other);

        QString albumId;
        QString userId;
        QDateTime createdTime;
        QDateTime updatedTime;
        QString albumName;
        int imageCount;
    };

private:
    // for server-side removal detection.
    bool initRemovalDetectionLists(int accountId);
    void clearRemovalDetectionLists();
    void checkRemovedImages(const QString &fbAlbumId);
    QMap<QString, OneDriveAlbum::ConstPtr> m_cachedAlbums;
    QMap<QString, QSet<QString> > m_serverImageIds;
    QStringList m_removedImages;

    OneDriveImagesDatabase m_db;

    // image variants with different dimentions
    class ImageSource {
    public:
        ImageSource(int width, int height, const QString &sourceUrl) : width(width), height(height), sourceUrl(sourceUrl) {}
        bool operator<(const ImageSource &other) const { return this->width < other.width; }
        int width;
        int height;
        QString sourceUrl;
    };
    bool determineOptimalDimensions();
    int m_optimalThumbnailWidth;
    int m_optimalImageWidth;
    SocialImagesDatabase m_imageCacheDb;
    QMap<QString, AlbumData> m_albumData;
};

#endif // ONEDRIVEIMAGESYNCADAPTOR_H
