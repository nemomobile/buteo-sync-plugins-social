/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKIMAGESYNCADAPTOR_H
#define FACEBOOKIMAGESYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtSql/QSqlDatabase>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <socialcache/facebookimagesdatabase.h>

class SyncService;

class FacebookImageSyncAdaptor
        : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookImageSyncAdaptor(SyncService *syncService, QObject *parent);
    ~FacebookImageSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestData(int accountId, const QString &accessToken, const QString &continuationUrl,
                     const QString &fbUserId, const QString &fbAlbumId);
    bool haveAlreadyCachedImage(const QString &fbImageId, const QString &imageUrl);
    void possiblyAddNewUser(const QString &fbUserId, int accountId, const QString &accessToken);


private Q_SLOTS:
    void albumsFinishedHandler();
    void imagesFinishedHandler();
    void userFinishedHandler();

private:
    // for server-side removal detection.
    void initRemovalDetectionLists();
    void clearRemovalDetectionLists();
    void checkRemovedImages(const QString &fbAlbumId);
    QMap<QString, FacebookAlbum::ConstPtr> m_cachedAlbums;
    QMap<QString, QSet<QString> > m_serverImageIds;
    QStringList m_removedImages;

    FacebookImagesDatabase m_db;
};

#endif // FACEBOOKIMAGESYNCADAPTOR_H
