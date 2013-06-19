/*
 * Copyright (C) 2013 Jolla Ltd. <chris.adams@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

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

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
//QtMobility
#include <qmobilityglobal.h>
#endif

#include <QtContacts/QContactManager>
#include <QtContacts/QContactAbstractRequest>
#include <QtContacts/QContactFetchRequest>
#include <QtContacts/QContactFetchHint>
#include <QtContacts/QContact>

class SyncService;
class FacebookSyncAdaptor;

class FacebookImageSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookImageSyncAdaptor(SyncService *parent, FacebookSyncAdaptor *fbsa);
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
                             const QString &photoCountStr, const QString &coverPhotoId,
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
