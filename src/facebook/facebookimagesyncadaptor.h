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

#include <QtContacts/QContactManager>
#include <QtContacts/QContactAbstractRequest>
#include <QtContacts/QContactFetchRequest>
#include <QtContacts/QContactFetchHint>
#include <QtContacts/QContact>

class SyncService;

class FacebookImageSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookImageSyncAdaptor(SyncService *syncService, QObject *parent);
    ~FacebookImageSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestData(int accountId, const QString &accessToken, const QString &continuationUrl,
                     const QString &fbUserId, const QString &fbAlbumId);
    bool haveAlreadyCachedAlbum(const QString &fbAlbumId, const QDateTime &updatedTime);
    bool haveAlreadyCachedImage(const QString &fbPhotoId, const QString &imageUrl, int accountId);
    void cacheImage(const QString &fbPhotoId, const QString &fbAlbumId,
                    const QString &fbUserId, const QString &createdTime,
                    const QString &updatedTime, const QString &thumbnailUrl,
                    const QString &imageUrl, const QString &photoName,
                    int width, int height);
    void possiblyAddNewUser(const QString &fbUserId, const QString &fbUserName,
                            int accountId, const QString &accessToken);
    void possiblyAddNewAlbum(const QString &fbAlbumId, const QString &fbUserId,
                             const QString &fbUserName, const QString &createdTime,
                             const QString &updatedTime, const QString &albumName,
                             int photoCount, const QString &coverPhotoId,
                             int accountId, const QString &accessToken);

    QStringList queryDatabaseRow(const QString &fbPhotoId);
    void updateAccountsTable(int accountId, const QString &fbUserId);
    void purgeAccount(int accountId);

    int dbUserVersion();
    bool dbCreateTables();
    bool dbDropTables();


private Q_SLOTS:
    void albumsFinishedHandler();
    void photosFinishedHandler();
    void userFinishedHandler();

private:
    QSqlDatabase m_imgdb;

    // for server-side removal detection.
    void initRemovalDetectionLists();
    void clearRemovalDetectionLists();
    QStringList photosInAlbum(const QString &fbAlbumId);
    bool purgeAlbum(const QString &fbAlbumId);
    bool purgePhoto(const QString &fbPhotoId);
    void purgeDetectedRemovals();
    QStringList m_cachedAlbumIds;
    QStringList m_cachedPhotoIds;
    QStringList m_serverAlbumIds;
    QStringList m_serverPhotoIds;

    // for busy/inactive detection.
    void decrementSemaphore(int accountId);
    void incrementSemaphore(int accountId);
    QMap<int, int> m_accountSyncSemaphores;
};

#endif // FACEBOOKIMAGESYNCADAPTOR_H
