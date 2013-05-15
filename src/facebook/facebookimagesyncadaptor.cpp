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

#include "facebookimagesyncadaptor.h"
#include "facebooksyncadaptor.h"
#include "syncservice.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QVariantMap>
#include <QtCore/QByteArray>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

// Currently, we integrate with the device image gallery via saving thumbnails to the
// ~/.config/sociald/images directory, and filling the ~/.config/sociald/images/facebook.db
// with appropriate data.

FacebookImageSyncAdaptor::FacebookImageSyncAdaptor(SyncService *parent, FacebookSyncAdaptor *fbsa)
    : FacebookDataTypeSyncAdaptor(parent, fbsa, SyncService::Images)
{
    m_enabled = false;

    // we create a database at SOCIALD_DATABASE_DIR/Images/facebook.db
    // the Jolla Gallery application will open this database for fb album support.
    if (!QFile::exists(QString("%1/%2/%3")
                .arg(QLatin1String(SOCIALD_DATABASE_DIR))
                .arg(SyncService::dataType(m_dataType))
                .arg(QLatin1String("facebook.db")))) {
        QDir dir(QString("%1/%2").arg(QLatin1String(SOCIALD_DATABASE_DIR)).arg(SyncService::dataType(m_dataType)));
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        QString absolutePath = dir.absoluteFilePath(QLatin1String("facebook.db"));
        QFile dbfile(absolutePath);
        if (!dbfile.open(QIODevice::ReadWrite)) {
            TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to create Facebook image database %1 - Facebook image sync will be inactive"))
                .arg(absolutePath));
            return;
        }
        dbfile.close();
    }

    // open the database in which we store our synced image information
    m_imgdb = QSqlDatabase::addDatabase("QSQLITE", QString(QLatin1String("sociald/facebook/%1")).arg(SyncService::dataType(m_dataType)));
    m_imgdb.setDatabaseName(QString("%1/%2/%3").arg(QLatin1String(SOCIALD_DATABASE_DIR)).arg(SyncService::dataType(m_dataType)).arg(QLatin1String("facebook.db")));
    if (!m_imgdb.open()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to open Facebook image database %1 - Facebook image sync will be inactive"))
            .arg(QLatin1String(SOCIALD_DATABASE_NAME)));
        return;
    }

    // create the facebook image db tables
    // photos = fbPhotoId, fbAlbumId, fbUserId, createdTime, updatedTime, photoName, width, height, thumbnailUrl, imageUrl, thumbnailFile, imageFile
    // albums = fbAlbumId, fbUserId, createdTime, updatedTime, albumName, photoCount, thumbnailUrl, imageUrl, thumbnailFile, imageFile
    // users = fbUserId, updatedTime, userName, thumbnailUrl, imageUrl, thumbnailFile, imageFile
    QSqlQuery query(m_imgdb);
    query.prepare( "CREATE TABLE IF NOT EXISTS photos ("
                   "fbPhotoId VARCHAR(50) UNIQUE PRIMARY KEY,"
                   "fbAlbumId VARCHAR(50),"
                   "fbUserId VARCHAR(50),"
                   "createdTime VARCHAR(30),"
                   "updatedTime VARCHAR(30),"
                   "photoName VARCHAR(100),"
                   "width INTEGER,"
                   "height INTEGER,"
                   "thumbnailUrl VARCHAR(100),"
                   "imageUrl VARCHAR(100),"
                   "thumbnailFile VARCHAR(100),"
                   "imageFile VARCHAR(100))");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to create photos table: %1 - Facebook image sync will be inactive"))
            .arg(query.lastError().text()));
        m_imgdb.close();
        return;
    }

    query.prepare( "CREATE TABLE IF NOT EXISTS albums ("
                   "fbAlbumId VARCHAR(50) UNIQUE PRIMARY KEY,"
                   "fbUserId VARCHAR(50),"
                   "createdTime VARCHAR(30),"
                   "updatedTime VARCHAR(30),"
                   "albumName VARCHAR(100),"
                   "photoCount INTEGER,"
                   "thumbnailUrl VARCHAR(100),"
                   "imageUrl VARCHAR(100),"
                   "thumbnailFile VARCHAR(100),"
                   "imageFile VARCHAR(100))");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to create albums table: %1 - Facebook image sync will be inactive"))
            .arg(query.lastError().text()));
        m_imgdb.close();
        return;
    }

    query.prepare( "CREATE TABLE IF NOT EXISTS users ("
                   "fbUserId VARCHAR(50) UNIQUE PRIMARY KEY,"
                   "updatedTime VARCHAR(30),"
                   "userName VARCHAR(100),"
                   "thumbnailUrl VARCHAR(100),"
                   "imageUrl VARCHAR(100),"
                   "thumbnailFile VARCHAR(100),"
                   "imageFile VARCHAR(100))");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to create users table: %1 - Facebook image sync will be inactive"))
            .arg(query.lastError().text()));
        m_imgdb.close();
        return;
    }

    query.prepare( "CREATE TABLE IF NOT EXISTS accounts ("
                   "accountId INTEGER UNIQUE PRIMARY KEY,"
                   "fbUserId VARCHAR(50))");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: unable to create accounts table: %1 - Facebook image sync will be inactive"))
            .arg(query.lastError().text()));
        m_imgdb.close();
        return;
    }

    // we we were able to open the database, we can sync
    m_enabled = true;
    m_status = SocialNetworkSyncAdaptor::Inactive;
}

FacebookImageSyncAdaptor::~FacebookImageSyncAdaptor()
{
    m_imgdb.close();
}

void FacebookImageSyncAdaptor::sync(const QString &dataType)
{
    // get ready for sync
    initRemovalDetectionLists();

    // call superclass impl.
    FacebookDataTypeSyncAdaptor::sync(dataType);
}

void FacebookImageSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // first, purge the data from our database + our cache directory
        purgeAccount(pid);

        // second, purge all data from the sociald main database
        removeAllData(QLatin1String("facebook"),
                SyncService::dataType(SyncService::Images),
                QString::number(pid)); // XXX TODO: use fb id instead of QString::number(accountId)
    }
}

void FacebookImageSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    // XXX TODO: use a sync queue.  One for accounts + one for albums.
    // Finish all photos from a single album, etc on down.
    // That way we don't request anything "out of order" which can screw up Facebook's paging etc stuff.
    // Downside: much slower, since more (network) IO bound than previously.
    requestData(accountId, accessToken, QString(), QString(), QString());
}

void FacebookImageSyncAdaptor::requestData(int accountId, const QString &accessToken, const QString &continuationUrl, const QString &fbUserId, const QString &fbAlbumId)
{
    QUrl url;
    if (!continuationUrl.isEmpty()) {
        // fetch the next page.
        url = QUrl(continuationUrl);
    } else {
        // build the request, depending on whether we're fetching albums or photos.
        if (fbAlbumId.isEmpty()) {
            // fetching all albums from the me user.
            url = QUrl(QLatin1String("https://graph.facebook.com/me/albums"));
        } else {
            // fetching photos from a particular album.
            url = QUrl(QString(QLatin1String("https://graph.facebook.com/%1/photos")).arg(fbAlbumId));
        }
    }

    if (!url.toString().contains("access_token")) {
        QList<QPair<QString, QString> > queryItems;
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
        url.setQueryItems(queryItems);
    }

    QNetworkReply *reply = m_fbsa->m_qnam->get(QNetworkRequest(url));
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("fbUserId", fbUserId);
        reply->setProperty("fbAlbumId", fbAlbumId);
        reply->setProperty("continuationUrl", continuationUrl);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        if (fbAlbumId.isEmpty()) {
            connect(reply, SIGNAL(finished()), this, SLOT(albumsFinishedHandler()));
        } else {
            connect(reply, SIGNAL(finished()), this, SLOT(photosFinishedHandler()));
        }

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request data from Facebook account with id %1"))
                .arg(accountId));
        clearRemovalDetectionLists(); // don't perform server-side removal detection during this sync run.
    }
}

void FacebookImageSyncAdaptor::albumsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString fbUserId = reply->property("fbUserId").toString();
    QString fbAlbumId = reply->property("fbAlbumId").toString();
    QString continuationUrl = reply->property("continuationUrl").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariantMap parsed = FacebookDataTypeSyncAdaptor::parseReplyData(replyData, &ok);
    if (!ok || !parsed.contains(QLatin1String("data"))) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to read albums response for Facebook account with id %1"))
                .arg(accountId));
        clearRemovalDetectionLists(); // don't perform server-side removal detection during this sync run.
        decrementSemaphore(accountId);
        return;
    }

    QList<QVariant> data = parsed.value(QLatin1String("data")).toList();
    if (data.size() == 0) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("Facebook account with id %1 has no albums"))
                .arg(accountId));
        decrementSemaphore(accountId);
        return;
    }

    // read the albums information
    for (int i = 0; i < data.size(); ++i) {
        QVariantMap album = data.at(i).toMap();
        if (album.isEmpty()) {
            continue;
        }

        QString albumId = album.value(QLatin1String("id")).toString();
        QString userId = album.value(QLatin1String("from")).toMap().value(QLatin1String("id")).toString();
        if (!userId.isEmpty() && userId != fbUserId) {
            // probably because the fbUserId hasn't been filled yet.
            fbUserId = userId;
            updateAccountsTable(accountId, fbUserId);
        }
        if (!albumId.isEmpty() && albumId != fbAlbumId) {
            // probably because the fbAlbumId hasn't been filled yet.
            fbAlbumId = albumId;
        }

        if (!m_serverAlbumIds.contains(albumId)) {
            // for removal detection.  Don't remove this one from cache, it still exists on the server.
            m_serverAlbumIds.append(albumId);
        }

        QDateTime lastSync = lastSyncTimestamp(QLatin1String("facebook"), SyncService::dataType(SyncService::Images), QString::number(accountId));
        QString userName = album.value(QLatin1String("from")).toMap().value(QLatin1String("name")).toString();
        QString albumName = album.value(QLatin1String("name")).toString();
        QString photoCountStr = album.value(QLatin1String("count")).toString();
        QString createdTimeStr = album.value(QLatin1String("created_time")).toString();
        QString updatedTimeStr = album.value(QLatin1String("updated_time")).toString();
        QString coverPhotoId = album.value(QLatin1String("cover_photo")).toString();

        // check to see whether we need to sync (any changes since last sync)
        QDateTime updatedTime = QDateTime::fromString(updatedTimeStr, Qt::ISODate);
        if (haveAlreadyCachedAlbum(fbAlbumId, updatedTime)) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("album with id %1 by user %2 from Facebook account with id %3 doesn't need sync"))
                    .arg(albumId).arg(userId).arg(accountId));
            // it hasn't been modified, so none of its photos have been removed.
            QStringList serverPhotoIdsInAlbum = photosInAlbum(fbAlbumId);
            foreach (const QString &pid, serverPhotoIdsInAlbum) {
                if (!m_serverPhotoIds.contains(pid)) {
                    m_serverPhotoIds.append(pid);
                }
            }
            continue;
        }

        // we need to sync.  See if we need to save the album entry, and request the photos for the album.
        possiblyAddNewAlbum(albumId, userId, userName, createdTimeStr, updatedTimeStr, albumName,
                            photoCountStr, coverPhotoId, accountId, accessToken);
        requestData(accountId, accessToken, QString(), fbUserId, fbAlbumId);
    }

    // perform a continuation request if required.
    QVariantMap paging = parsed.value(QLatin1String("paging")).toMap();
    QString nextUrl = paging.value(QLatin1String("next")).toString();
    if (!nextUrl.isEmpty() && nextUrl != continuationUrl) {
        // note: we check equality because fb can return spurious paging data...
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("performing continuation request for more albums for Facebook account with id %1: %2"))
                .arg(accountId).arg(nextUrl));
        requestData(accountId, accessToken, nextUrl, fbUserId, QString());
    }

    // Finally, reduce our semaphore.
    decrementSemaphore(accountId);
}

void FacebookImageSyncAdaptor::photosFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString fbUserId = reply->property("fbUserId").toString();
    QString fbAlbumId = reply->property("fbAlbumId").toString();
    QString continuationUrl = reply->property("continuationUrl").toString();
    QDateTime lastSync = lastSyncTimestamp(QLatin1String("facebook"), SyncService::dataType(SyncService::Images), QString::number(accountId));
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariantMap parsed = parseReplyData(replyData, &ok);
    if (!ok || !parsed.contains(QLatin1String("data"))) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to read photos response for Facebook account with id %1"))
                .arg(accountId));
        clearRemovalDetectionLists(); // don't perform server-side removal detection during this sync run.
        decrementSemaphore(accountId);
        return;
    }

    QList<QVariant> data = parsed.value(QLatin1String("data")).toList();
    if (data.size() == 0) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("Album with id %1 from Facebook account with id %2 has no photos"))
                .arg(fbAlbumId).arg(accountId));
        decrementSemaphore(accountId);
        return;
    }

    // read the photos information
    for (int i = 0; i < data.size(); ++i) {
        QVariantMap photo = data.at(i).toMap();
        if (photo.isEmpty()) {
            continue;
        }

        QString photoId = photo.value(QLatin1String("id")).toString();
        QString thumbnailUrl = photo.value(QLatin1String("picture")).toString();
        QString imageSrcUrl = photo.value(QLatin1String("source")).toString();
        QString createdTimeStr = photo.value(QLatin1String("created_time")).toString();
        QString updatedTimeStr = photo.value(QLatin1String("updated_time")).toString();
        QString photoName = photo.value(QLatin1String("name")).toString();

        // Find the correct thumbnail size. The fallback will be the "picture" which usually
        // is too small so this is sort of best guess what sizes FB might returns. We can't
        // also hardcode the exact sizes here, because we can't be sure that certains sizes
        // will stay for ever.
        QVariantList images = photo.value(QLatin1String("images")).toList();
        for (int j = 0; j < images.size(); j++) {
            QVariantMap image = images.at(j).toMap();
            qulonglong width = image.value(QLatin1String("width")).toULongLong();
            qulonglong height= image.value(QLatin1String("height")).toULongLong();
            if (160 <= width && width <= 350 &&
                160 <= height && height <= 350) {
                thumbnailUrl = image.value(QLatin1String("source")).toString();
                break;
            }
        }


        bool ok = false;
        int width = photo.value(QLatin1String("width")).toString().toInt(&ok);
        int height = photo.value(QLatin1String("height")).toString().toInt(&ok);

        if (!m_serverPhotoIds.contains(photoId)) {
            // for removal detection.  Don't remove this one from cache, it still exists on the server.
            m_serverPhotoIds.append(photoId);
        }

        // we need to sync.  Write to the database.
        if (haveAlreadyCachedImage(photoId, imageSrcUrl, accountId)) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("updating previously cached photo %1: %2"))
                    .arg(photoId).arg(imageSrcUrl));
        } else {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("caching new photo %1: %2"))
                    .arg(photoId).arg(imageSrcUrl));
        }

        cacheImage(photoId, fbAlbumId, fbUserId, createdTimeStr, updatedTimeStr, thumbnailUrl, imageSrcUrl, photoName, width, height);
    }

    // perform a continuation request if required.
    QVariantMap paging = parsed.value(QLatin1String("paging")).toMap();
    QString nextUrl = paging.value(QLatin1String("next")).toString();
    if (!nextUrl.isEmpty() && nextUrl != continuationUrl) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("performing continuation request for more photos for Facebook account with id %1: %2"))
                .arg(accountId).arg(nextUrl));
        requestData(accountId, accessToken, nextUrl, fbUserId, fbAlbumId);
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

bool FacebookImageSyncAdaptor::haveAlreadyCachedAlbum(const QString &fbAlbumId, const QDateTime &updatedTime)
{
    QSqlQuery query(m_imgdb);
    query.prepare("SELECT fbAlbumId, updatedTime FROM albums WHERE fbAlbumId = :fbaid");
    query.bindValue(":fbaid", fbAlbumId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QLatin1String("error reading from albums table:") << query.lastError());
        return false; // failed query, let's assume that it needs update.
    }

    if (!query.next()) {
        return false; // no such fb album.  we need to cache it.
    }

    QString utstr = query.value(1).toString();
    QDateTime dbUpdatedTime = QDateTime::fromString(utstr, Qt::ISODate);
    if (dbUpdatedTime < updatedTime) {
        return false; // it has been updated since last cache.
    }

    // we've already cached this album and it's up-to-date.  Ignore it.
    return true;
}

bool FacebookImageSyncAdaptor::haveAlreadyCachedImage(const QString &fbPhotoId, const QString &imageUrl, int accountId)
{
    QStringList row = queryDatabaseRow(fbPhotoId);
    bool socialdSynced = whenSyncedDatum(QLatin1String("facebook"), QString::number(accountId)).isValid();
    bool imagedbSynced = row.size();

    if (socialdSynced != imagedbSynced) {
        // sociald's central sync timestamp db has different information to
        // the facebook image sync adaptor's local db... You know a sync
        // adaptor has problems when it doesn't stay in sync with itself.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: sociald.db and Image/facebook.db are out of sync!"
                                      "\n   fbPhotoId: %1\n   socialdSynced: %2\n   imagedbSynced: %3"))
                .arg(fbPhotoId).arg(socialdSynced).arg(imagedbSynced));
    }

    if (imagedbSynced) {
        QString dbImageUrl = row.at(6);
        if (dbImageUrl != imageUrl) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: Image/facebook.db has outdated data!"
                                          "\n   fbPhotoId: %1\n   cached image url: %2\n   new image url: %3"))
                    .arg(fbPhotoId).arg(dbImageUrl).arg(imageUrl));
        }
    }

    return socialdSynced || imagedbSynced;
}

void FacebookImageSyncAdaptor::possiblyAddNewAlbum(const QString &fbAlbumId, const QString &fbUserId,
                                                   const QString &fbUserName, const QString &createdTime,
                                                   const QString &updatedTime, const QString &albumName,
                                                   const QString &photoCountStr, const QString &coverPhotoId,
                                                   int accountId, const QString &accessToken)
{
    // first, check to see whether we need to add a new user to the users table
    possiblyAddNewUser(fbUserId, fbUserName, accountId, accessToken);

    // then, check to see whether we need to add a new album to the albums table
    QSqlQuery query(m_imgdb);
    query.prepare("SELECT fbAlbumId FROM albums WHERE fbAlbumId = :fbaid");
    query.bindValue(":fbaid", fbAlbumId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QLatin1String("error reading from albums table:") << query.lastError());
        return;
    }

    bool ok = false;
    int photoCount = photoCountStr.toInt(&ok);

    if (query.next()) {
        // already exists.  Don't need to add it.  But, can update the updated time + album name (might have changed).
        QSqlQuery insertQuery(m_imgdb);
        insertQuery.prepare("UPDATE albums SET updatedTime = :ut, albumName = :an, photoCount = :pc WHERE fbAlbumId = :fbaid");
        insertQuery.bindValue(":ut", updatedTime);
        insertQuery.bindValue(":an", albumName);
        insertQuery.bindValue(":pc", photoCount);
        insertQuery.bindValue(":fbaid", fbAlbumId);
        if (!insertQuery.exec()) {
            TRACE(SOCIALD_ERROR, QLatin1String("error updating albums table:") << query.lastError());
            return;
        }
    } else {
        // new album.  Add it to the table.
        QSqlQuery insertQuery(m_imgdb);
        insertQuery.prepare("INSERT INTO albums (fbAlbumId, fbUserId, createdTime, updatedTime, albumName, photoCount, thumbnailUrl, imageUrl, thumbnailFile, imageFile) VALUES (:fbaid, :fbuid, :ct, :ut, :an, :pc, :tu, :iu, :tf, :if)");
        insertQuery.bindValue(":fbaid", fbAlbumId);
        insertQuery.bindValue(":fbuid", fbUserId);
        insertQuery.bindValue(":ct", createdTime);
        insertQuery.bindValue(":ut", updatedTime);
        insertQuery.bindValue(":an", albumName);
        insertQuery.bindValue(":pc", photoCount);
        insertQuery.bindValue(":tu", QString());
        insertQuery.bindValue(":iu", QString());
        insertQuery.bindValue(":tf", QString());
        insertQuery.bindValue(":if", QString());
        if (!insertQuery.exec()) {
            TRACE(SOCIALD_ERROR, QLatin1String("error inserting album:")
                                 << fbAlbumId << QLatin1String("(")
                                 << albumName << QLatin1String(") into albums table:")
                                 << query.lastError());
            return;
        }

        // successfully added album.  begin a new query to get the image information (based on cover photo id).
        // XXX TODO.
    }
}

void FacebookImageSyncAdaptor::possiblyAddNewUser(const QString &fbUserId, const QString &fbUserName, int accountId, const QString &accessToken)
{
    // check to see whether the user exists in the db.
    QSqlQuery query(m_imgdb);
    query.prepare("SELECT fbUserId FROM users WHERE fbUserId = :fbuid");
    query.bindValue(":fbuid", fbUserId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QLatin1String("error reading from users table:") << query.lastError());
        return;
    }

    if (query.next()) {
        // already exists.  Don't need to add it.
    } else {
        // need to add it.
        QSqlQuery addQuery(m_imgdb);// fbUserId, updatedTime, userName, thumbnailUrl, imageUrl, thumbnailFile, imageFile
        addQuery.prepare("INSERT INTO users (fbUserId, updatedTime, userName, thumbnailUrl, imageUrl, thumbnailFile, imageFile) VALUES (:fbuid, :ut, :un, :tu, :iu, :tf, :if)");
        addQuery.bindValue(":fbuid", fbUserId);
        addQuery.bindValue(":ut", QString());
        addQuery.bindValue(":un", fbUserName);
        addQuery.bindValue(":tu", QString());
        addQuery.bindValue(":iu", QString());
        addQuery.bindValue(":tf", QString());
        addQuery.bindValue(":if", QString());
        if (!addQuery.exec()) {
            TRACE(SOCIALD_ERROR, QLatin1String("error writing to users table:") << query.lastError());
            return;
        }

        // successfully added user.  begin a new query to get more information: me?fields=updated_time,name,picture,cover
        QUrl url(QLatin1String("https://graph.facebook.com/me"));
        QList<QPair<QString, QString> > queryItems;
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("fields")), QLatin1String("id,updated_time,name,picture,cover")));
        url.setQueryItems(queryItems);
        QNetworkReply *reply = m_fbsa->m_qnam->get(QNetworkRequest(url));
        if (reply) {
            reply->setProperty("accountId", accountId);
            reply->setProperty("accessToken", accessToken);
            connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
            connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
            connect(reply, SIGNAL(finished()), this, SLOT(userFinishedHandler()));
        }
    }
}

void FacebookImageSyncAdaptor::userFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QByteArray replyData = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariantMap parsed = parseReplyData(replyData, &ok);
    if (!ok || !parsed.contains(QLatin1String("id"))) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to read user response for Facebook account with id %1"))
                .arg(accountId));
        return;
    }

    QString fbUserId = parsed.value(QLatin1String("id")).toString();
    QString fbName = parsed.value(QLatin1String("name")).toString();
    QString updatedStr = parsed.value(QLatin1String("updated_time")).toString();
    QString fbPictureUrl = parsed.value(QLatin1String("picture")).toMap()
                           .value(QLatin1String("data")).toMap()
                           .value(QLatin1String("url")).toString();
    QString fbCoverUrl = parsed.value(QLatin1String("cover")).toMap()
                         .value(QLatin1String("source")).toString();

    QSqlQuery query(m_imgdb);
    query.prepare("UPDATE users SET updatedTime = :ut, userName = :un, thumbnailUrl = :tu, imageUrl = :iu WHERE fbUserId = :fbuid");
    query.bindValue(":ut", updatedStr);
    query.bindValue(":un", fbName);
    query.bindValue(":tu", fbPictureUrl);
    query.bindValue(":iu", fbCoverUrl);
    query.bindValue(":fbuid", fbUserId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to update user info for Facebook account with id %1:"))
                .arg(accountId) << query.lastError());
    }
}

void FacebookImageSyncAdaptor::cacheImage(const QString &fbPhotoId, const QString &fbAlbumId,
                                          const QString &fbUserId, const QString &createdTime,
                                          const QString &updatedTime, const QString &thumbnailUrl,
                                          const QString &imageUrl, const QString &photoName,
                                          int width, int height)
{
    // first, we request the row from the database
    QStringList row = queryDatabaseRow(fbPhotoId);

    // if it exists, we delete any thumbnailUrl / imageUrl associated with it.
    if (row.size()) {
        // check for changes.
        if (row.at(0) == fbPhotoId && row.at(1) == fbAlbumId && row.at(2) == fbUserId
                && row.at(3) == createdTime && row.at(4) == updatedTime && row.at(5) == thumbnailUrl
                && row.at(6) == imageUrl && row.at(7) == photoName && row.at(8) == QString::number(width)
                && row.at(9) == QString::number(height)) {
            // no change, already cached.
            return;
        }

        QString thumbFile = row.at(10);
        QString imageFile = row.at(11);

        if (!thumbFile.isEmpty() && QFile::exists(thumbFile)) {
            if (!QFile::remove(thumbFile)) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to remove stale thumbnail file %1 for photo %2"))
                        .arg(thumbFile).arg(fbPhotoId));
            }
        }

        if (!imageFile.isEmpty() && QFile::exists(imageFile)) {
            if (!QFile::remove(imageFile)) {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to remove stale image file %1 for photo %2"))
                        .arg(imageFile).arg(fbPhotoId));
            }
        }

        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("about to update cached photo in database:"
                                      "\n   fbPhotoId: %1"
                                      "\n   fbAlbumId: %2"
                                      "\n    fbUserId: %3"
                                      "\n     created: %4"
                                      "\n     updated: %5"
                                      "\n    thumbUrl: %6"
                                      "\n    imageUrl: %7"
                                      "\n   photoName: %8"
                                      "\n       width: %9"
                                      "\n      height: %10"))
                .arg(fbPhotoId).arg(fbAlbumId).arg(fbUserId).arg(createdTime)
                .arg(updatedTime).arg(thumbnailUrl).arg(imageUrl).arg(photoName)
                .arg(width).arg(height));

        // then we write the new / updated data.
        QSqlQuery query(m_imgdb);
        query.prepare("UPDATE photos SET fbAlbumId = :fbai, fbUserId = :fbui, createdTime = :ct, updatedTime = :ut, photoName = :pn, width = :wi, height = :hi, thumbnailUrl = :tu, imageUrl = :iu, thumbnailFile = :tf, imageFile = :if WHERE fbPhotoId = :fbpi");
        query.bindValue(":fbpi", fbPhotoId);
        query.bindValue(":fbai", fbAlbumId);
        query.bindValue(":fbui", fbUserId);
        query.bindValue(":ct", createdTime);
        query.bindValue(":ut", updatedTime);
        query.bindValue(":pn", photoName);
        query.bindValue(":wi", width);
        query.bindValue(":hi", height);
        query.bindValue(":tu", thumbnailUrl);
        query.bindValue(":iu", imageUrl);
        query.bindValue(":tf", QString());
        query.bindValue(":if", QString());
        bool success = query.exec();
        if (!success) {
            TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute cache image update: %1")).arg(query.lastError().text()));
        } else {
            TRACE(SOCIALD_DEBUG, QString(QLatin1String("successfully executed cache image update for fb photo: %1")).arg(fbPhotoId));
        }
    } else {
        // new row, insert.
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("about to insert cached photo into database:"
                                      "\n   fbPhotoId: %1"
                                      "\n   fbAlbumId: %2"
                                      "\n    fbUserId: %3"
                                      "\n     created: %4"
                                      "\n     updated: %5"
                                      "\n    thumbUrl: %6"
                                      "\n    imageUrl: %7"
                                      "\n   photoName: %8"
                                      "\n       width: %9"
                                      "\n      height: %10"))
                .arg(fbPhotoId).arg(fbAlbumId).arg(fbUserId).arg(createdTime)
                .arg(updatedTime).arg(thumbnailUrl).arg(imageUrl).arg(photoName)
                .arg(width).arg(height));

        // then we write the new / updated data.
        QSqlQuery query(m_imgdb);
        query.prepare("INSERT INTO photos (fbPhotoId, fbAlbumId, fbUserId, createdTime, updatedTime, photoName, width, height, thumbnailUrl, imageUrl, thumbnailFile, imageFile) VALUES (:fbpi, :fbai, :fbui, :ct, :ut, :pn, :wi, :hi, :tu, :iu, :tf, :if)");
        query.bindValue(":fbpi", fbPhotoId);
        query.bindValue(":fbai", fbAlbumId);
        query.bindValue(":fbui", fbUserId);
        query.bindValue(":ct", createdTime);
        query.bindValue(":ut", updatedTime);
        query.bindValue(":pn", photoName);
        query.bindValue(":wi", width);
        query.bindValue(":hi", height);
        query.bindValue(":tu", thumbnailUrl);
        query.bindValue(":iu", imageUrl);
        query.bindValue(":tf", QString());
        query.bindValue(":if", QString());
        bool success = query.exec();
        if (!success) {
            TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute cache image insert: %1")).arg(query.lastError().text()));
        } else {
            TRACE(SOCIALD_DEBUG, QString(QLatin1String("successfully executed cache image insert for fb photo: %1")).arg(fbPhotoId));
        }
    }
}

QStringList FacebookImageSyncAdaptor::queryDatabaseRow(const QString &fbPhotoId)
{
    QStringList retn;
    QSqlQuery query(m_imgdb);
    query.prepare("SELECT fbAlbumId, fbUserId, createdTime, updatedTime, thumbnailUrl, imageUrl, photoName, width, height, thumbnailFile, imageFile"
                  " FROM photos"
                  " WHERE fbPhotoId = :fbpi");
    query.bindValue(":fbpi", fbPhotoId);
    bool success = query.exec();
    if (!success) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute image row query for photo %1: %2")).arg(fbPhotoId).arg(query.lastError().text()));
        return retn;
    }

    if (!query.next()) {
        // no data - looks like no such photo exists in our database - new photo.
    } else {
        retn.append(fbPhotoId);
        retn.append(query.value(0).toString());
        retn.append(query.value(1).toString());
        retn.append(query.value(2).toString());
        retn.append(query.value(3).toString());
        retn.append(query.value(4).toString());
        retn.append(query.value(5).toString());
        retn.append(query.value(6).toString());
        retn.append(query.value(7).toString());
        retn.append(QString::number(query.value(8).toInt()));
        retn.append(QString::number(query.value(9).toInt()));
        retn.append(query.value(10).toString());
        retn.append(query.value(11).toString());
    }

    TRACE(SOCIALD_DEBUG, QString(QLatin1String("successfully executed image row query for fb photo %1:")).arg(fbPhotoId) << retn);

    return retn;
}

void FacebookImageSyncAdaptor::updateAccountsTable(int accountId, const QString &fbUserId)
{
    QStringList allFbUserIds;
    QList<int> allAccountIds;

    QSqlQuery query(m_imgdb);
    query.prepare("SELECT accountId, fbUserId FROM accounts");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute accounts query for account %1: %2")).arg(accountId).arg(query.lastError().text()));
        return;
    }

    while (query.next()) {
        allAccountIds.append(query.value(0).toInt());
        allFbUserIds.append(query.value(1).toString());
    }

    for (int i = 0; i < allAccountIds.size(); ++i) {
        if (allAccountIds.at(i) == accountId) {
            // something exists in the db.  is it up to date?
            if (allFbUserIds.at(i) == fbUserId) {
                return; // yes, up to date.
            } else {
                // the user id has changed, somehow.
                query.prepare("UPDATE accounts SET fbUserId = :fbuid WHERE accountId = :aid");
                query.bindValue(":fbuid", fbUserId);
                query.bindValue(":aid", accountId);
                if (!query.exec()) {
                    TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to update accounts table for account %1: %2")).arg(accountId).arg(query.lastError().text()));
                } else {
                    TRACE(SOCIALD_INFORMATION, QString(QLatin1String("had to update fbUserId for account %1 from %2 to %3")).arg(accountId).arg(allFbUserIds.at(i)).arg(fbUserId));
                }
                return;
            }
        }
    }

    // no such account is listed in our table.  add it.
    query.prepare("INSERT INTO accounts (accountId, fbUserId) VALUES (:aid, :fbuid)");
    query.bindValue(":aid", accountId);
    query.bindValue(":fbuid", fbUserId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to insert into accounts table for account %1: %2")).arg(accountId).arg(query.lastError().text()));
    }

    TRACE(SOCIALD_INFORMATION, QString(QLatin1String("Successfully created account entry with fbUserId %1 for %2")).arg(fbUserId).arg(accountId));
}

void FacebookImageSyncAdaptor::purgeAccount(int accountId)
{
    QStringList allFbUserIds;
    QList<int> allAccountIds;

    QSqlQuery query(m_imgdb);
    query.prepare("SELECT accountId, fbUserId FROM accounts");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute accounts query for account %1: %2")).arg(accountId).arg(query.lastError().text()));
        return;
    }

    while (query.next()) {
        allAccountIds.append(query.value(0).toInt());
        allFbUserIds.append(query.value(1).toString());
    }

    // find the fb user id associated.
    QString fbUserId;
    for (int i = 0; i < allAccountIds.size(); ++i) {
        if (allAccountIds.at(i) == accountId) {
            fbUserId = allFbUserIds.at(i);
            break;
        }
    }

    if (fbUserId.isEmpty()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: no valid fbUserId for account %1!")).arg(accountId));
        return;
    }

    // determine whether or not the fbUserId is represented multiple times
    bool isMultiple = false;
    for (int i = 0; i < allAccountIds.size(); ++i) {
        if (allFbUserIds.at(i) == fbUserId && allAccountIds.at(i) != accountId) {
            isMultiple = true;
        }
    }

    // now remove all required data from our database, plus associated thumbnail/image files.
    // first, delete the account from our accounts table
    query.prepare("DELETE FROM accounts WHERE accountId = :aid");
    query.bindValue(":aid", accountId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to delete account %1: %2")).arg(accountId).arg(query.lastError().text()));
        return;
    }

    // then, if we need to, delete the user / albums / photos.
    if (isMultiple) {
        TRACE(SOCIALD_INFORMATION, QString(QLatin1String("successully deleted account %1 but retaining cached data as user remains in cache")).arg(accountId));
        return;
    }

    // users
    query.prepare("DELETE FROM users WHERE fbUserId = :fbuid");
    query.bindValue(":fbuid", fbUserId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to delete users while deleting account %1: %2")).arg(accountId).arg(query.lastError().text()));
        return;
    }

    // albums
    query.prepare("DELETE FROM albums WHERE fbUserId = :fbuid");
    query.bindValue(":fbuid", fbUserId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to delete albums while deleting account %1: %2")).arg(accountId).arg(query.lastError().text()));
        return;
    }

    // remove images/thumbnails
    query.prepare("SELECT thumbnailFile, imageFile FROM photos WHERE fbUserId = :fbuid");
    query.bindValue(":fbuid", fbUserId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to query cache files while deleting account %1: %2")).arg(accountId).arg(query.lastError().text()));
        return;
    }

    while (query.next()) {
        QString thumb = query.value(0).toString();
        QString image = query.value(1).toString();
        if (!thumb.isEmpty() && QFile::exists(thumb)) {
            QFile::remove(thumb);
        }
        if (!image.isEmpty() && QFile::exists(image)) {
            QFile::remove(image);
        }
    }

    // photos
    query.prepare("DELETE FROM photos WHERE fbUserId = :fbuid");
    query.bindValue(":fbuid", fbUserId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to delete photos while deleting account %1: %2")).arg(accountId).arg(query.lastError().text()));
        return;
    }

    TRACE(SOCIALD_INFORMATION, QString(QLatin1String("successully deleted account %1 along with cached data")).arg(accountId));
}

bool FacebookImageSyncAdaptor::purgeAlbum(const QString &fbAlbumId)
{
    // first, grab ids of photos in this album, so we can remove them from the m_cachedPhotoIds list.
    QSqlQuery query(m_imgdb);
    query.prepare("SELECT DISTINCT fbPhotoId FROM photos WHERE fbAlbumId = :fbaid");
    query.bindValue(":fbaid", fbAlbumId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to query photo ids during album %1 purge: %2")).arg(fbAlbumId).arg(query.lastError().text()));
        return false;
    } else {
        while (query.next()) {
            QString pid = query.value(0).toString();
            if (m_cachedPhotoIds.contains(pid)) {
                m_cachedPhotoIds.removeAll(pid);
            }
        }
    }

    // then remove the album
    query.prepare("DELETE FROM albums WHERE fbAlbumId = :fbaid");
    query.bindValue(":fbaid", fbAlbumId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to delete album %1: %2")).arg(fbAlbumId).arg(query.lastError().text()));
        return false;
    }

    // remove images/thumbnails
    query.prepare("SELECT thumbnailFile, imageFile FROM photos WHERE fbAlbumId = :fbaid");
    query.bindValue(":fbaid", fbAlbumId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to query cache files while deleting album %1: %2")).arg(fbAlbumId).arg(query.lastError().text()));
        return false;
    }

    while (query.next()) {
        QString thumb = query.value(0).toString();
        QString image = query.value(1).toString();
        if (!thumb.isEmpty() && QFile::exists(thumb)) {
            QFile::remove(thumb);
        }
        if (!image.isEmpty() && QFile::exists(image)) {
            QFile::remove(image);
        }
    }

    // remove photos
    query.prepare("DELETE FROM photos WHERE fbAlbumId = :fbaid");
    query.bindValue(":fbaid", fbAlbumId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to delete photos while deleting album %1: %2")).arg(fbAlbumId).arg(query.lastError().text()));
        return false;
    }

    TRACE(SOCIALD_INFORMATION, QString(QLatin1String("successully deleted album %1 along with cached data")).arg(fbAlbumId));
    return true;
}

bool FacebookImageSyncAdaptor::purgePhoto(const QString &fbPhotoId)
{
    // remove images/thumbnails
    QSqlQuery query(m_imgdb);
    query.prepare("SELECT thumbnailFile, imageFile FROM photos WHERE fbPhotoId = :fbpid");
    query.bindValue(":fbpid", fbPhotoId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to query cache files while deleting photo %1: %2")).arg(fbPhotoId).arg(query.lastError().text()));
        return false;
    }

    while (query.next()) {
        QString thumb = query.value(0).toString();
        QString image = query.value(1).toString();
        if (!thumb.isEmpty() && QFile::exists(thumb)) {
            QFile::remove(thumb);
        }
        if (!image.isEmpty() && QFile::exists(image)) {
            QFile::remove(image);
        }
    }

    // remove photo
    query.prepare("DELETE FROM photos WHERE fbPhotoId = :fbpid");
    query.bindValue(":fbpid", fbPhotoId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to delete photo %1: %2")).arg(fbPhotoId).arg(query.lastError().text()));
        return false;
    }

    TRACE(SOCIALD_INFORMATION, QString(QLatin1String("successully deleted photo %1 along with cached data")).arg(fbPhotoId));
    return true;
}

void FacebookImageSyncAdaptor::initRemovalDetectionLists()
{
    // This function should be called as part of the ::sync() preamble.
    // Clear our internal state variables which we use to track server-side deletions.
    // We have to do it this way, as results can be spread across multiple requests
    // if Facebook returns results in paginated form.
    m_cachedAlbumIds = QStringList();
    m_cachedPhotoIds = QStringList();
    m_serverAlbumIds = QStringList();
    m_serverPhotoIds = QStringList();

    QSqlQuery query(m_imgdb);
    query.prepare("SELECT DISTINCT fbAlbumId FROM photos");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute album ids query: %1")).arg(query.lastError().text()));
        return;
    } else {
        while (query.next()) {
            m_cachedAlbumIds.append(query.value(0).toString());
        }
    }

    query.prepare("SELECT DISTINCT fbPhotoId FROM photos");
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute photo ids query: %1")).arg(query.lastError().text()));
        return;
    } else {
        while (query.next()) {
            m_cachedPhotoIds.append(query.value(0).toString());
        }
    }
}

void FacebookImageSyncAdaptor::clearRemovalDetectionLists()
{
    // This function should be called if a request errors out.
    // If the lists are empty, we won't purge anything.
    m_cachedAlbumIds = QStringList();
    m_cachedPhotoIds = QStringList();
}

QStringList FacebookImageSyncAdaptor::photosInAlbum(const QString &fbAlbumId)
{
    QStringList retn;
    QSqlQuery query(m_imgdb);
    query.prepare("SELECT DISTINCT fbPhotoId FROM photos WHERE fbAlbumId = :fbaid");
    query.bindValue(":fbaid", fbAlbumId);
    if (!query.exec()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: unable to execute photo ids query from album %1: %2")).arg(fbAlbumId).arg(query.lastError().text()));
        clearRemovalDetectionLists(); // in this case, we shouldn't clear synced data because it's probably still valid.
    } else {
        while (query.next()) {
            retn.append(query.value(0).toString());
        }
    }

    return retn;
}

void FacebookImageSyncAdaptor::purgeDetectedRemovals()
{
    // This function should be called once the synchronization process is completed.
    int expectedPurgeAlbumCount = 0; // can't just subtract the counts for this, as add+remove = 0.
    int actualPurgeAlbumCount = 0;
    foreach (const QString &cachedId, m_cachedAlbumIds) {
        if (!m_serverAlbumIds.contains(cachedId)) {
            expectedPurgeAlbumCount += 1;
            if (purgeAlbum(cachedId)) {
                actualPurgeAlbumCount += 1;
            }
        }
    }

    int expectedPurgePhotoCount = 0; // can't just subtract the counts for this, as add+remove = 0.
    int actualPurgePhotoCount = 0;
    foreach (const QString &cachedId, m_cachedPhotoIds) {
        if (!m_serverPhotoIds.contains(cachedId)) {
            expectedPurgePhotoCount += 1;
            if (purgePhoto(cachedId)) {
                actualPurgePhotoCount += 1;
            }
        }
    }

    if (expectedPurgeAlbumCount != actualPurgeAlbumCount
            || expectedPurgePhotoCount != actualPurgePhotoCount) {
        TRACE(SOCIALD_INFORMATION, QString(QLatin1String("unable to purge all albums or photos: expected to remove %1 and %2, removed %3 and %4 respectively"))
                                   .arg(expectedPurgeAlbumCount).arg(expectedPurgePhotoCount).arg(actualPurgeAlbumCount).arg(actualPurgePhotoCount));
    } else if (actualPurgeAlbumCount != 0 || actualPurgePhotoCount != 0) {
        TRACE(SOCIALD_DEBUG, QString(QLatin1String("successfully purged %1 albums and %2 photos")).arg(actualPurgeAlbumCount).arg(actualPurgePhotoCount));
    }
}

void FacebookImageSyncAdaptor::incrementSemaphore(int accountId)
{
    int semaphoreValue = m_accountSyncSemaphores.value(accountId);
    semaphoreValue += 1;
    m_accountSyncSemaphores.insert(accountId, semaphoreValue);
    TRACE(SOCIALD_DEBUG, QString(QLatin1String("incremented busy semaphore for account %1 to %2")).arg(accountId).arg(semaphoreValue));

    if (m_status == SocialNetworkSyncAdaptor::Inactive) {
        m_status = SocialNetworkSyncAdaptor::Busy;
        emit statusChanged();
    }
}

void FacebookImageSyncAdaptor::decrementSemaphore(int accountId)
{
    if (!m_accountSyncSemaphores.contains(accountId)) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: no such semaphore for account: %1")).arg(accountId));
        return;
    }

    int semaphoreValue = m_accountSyncSemaphores.value(accountId);
    semaphoreValue -= 1;
    TRACE(SOCIALD_DEBUG, QString(QLatin1String("decremented busy semaphore for account %1 to %2")).arg(accountId).arg(semaphoreValue));
    if (semaphoreValue < 0) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: busy semaphore is negative for account: %1")).arg(accountId));
        return;
    }
    m_accountSyncSemaphores.insert(accountId, semaphoreValue);

    if (semaphoreValue == 0) {
        // finished all outstanding requests for Image sync for this account.
        // update the sync time for this user's Images in the global sociald database.
        updateLastSyncTimestamp(QLatin1String("facebook"),
                                SyncService::dataType(SyncService::Images),
                                QString::number(accountId),
                                QDateTime::currentDateTime());

        // if all outstanding requests for all accounts have finished,
        // then update our status to Inactive / ready to handle more sync requests.
        bool allAreZero = true;
        QList<int> semaphores = m_accountSyncSemaphores.values();
        foreach (int sv, semaphores) {
            if (sv != 0) {
                allAreZero = false;
                break;
            }
        }

        if (allAreZero) {
            purgeDetectedRemovals(); // purge anything which has been deleted server-side.
            TRACE(SOCIALD_INFORMATION, QString(QLatin1String("Finished Facebook Images sync at: %1"))
                                       .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
            m_status = SocialNetworkSyncAdaptor::Inactive;
            emit statusChanged();
        }
    }
}
