/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebookimagesyncadaptor.h"
#include "syncservice.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QVariantMap>
#include <QtCore/QByteArray>
#include <QtCore/QUrlQuery>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtSql/QSqlRecord>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

// Update the following version if database schema changes e.g. new
// fields are added to the existing tables.
// It will make old tables dropped and creates new ones.

// Currently, we integrate with the device image gallery via saving thumbnails to the
// ~/.config/sociald/images directory, and filling the ~/.config/sociald/images/facebook.db
// with appropriate data.

// TODO: there is still issues with multiaccount, if an user adds two times the same
// account, it might have some problems, like data being removed while it shouldn't.
FacebookImageSyncAdaptor::FacebookImageSyncAdaptor(SyncService *syncService, QObject *parent)
    : FacebookDataTypeSyncAdaptor(syncService, SyncService::Images, parent)
{
    m_db.initDatabase();
    setInitialActive(m_db.isValid());
}


FacebookImageSyncAdaptor::~FacebookImageSyncAdaptor()
{
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
        m_db.purgeAccount(pid);
    }
}

void FacebookImageSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    // XXX TODO: use a sync queue.  One for accounts + one for albums.
    // Finish all images from a single album, etc on down.
    // That way we don't request anything "out of order" which can screw up Facebook's paging etc stuff.
    // Downside: much slower, since more (network) IO bound than previously.
    requestData(accountId, accessToken, QString(), QString(), QString());
}

void FacebookImageSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId)
    // Remove albums
    m_db.removeAlbums(m_cachedAlbums.keys());

    // Remove images
    m_db.removeImages(m_removedImages);

    m_db.write();
}

void FacebookImageSyncAdaptor::requestData(int accountId,
                                           const QString &accessToken,
                                           const QString &continuationUrl,
                                           const QString &fbUserId,
                                           const QString &fbAlbumId)
{
    QUrl url;
    if (!continuationUrl.isEmpty()) {
        // fetch the next page.
        url = QUrl(continuationUrl);
    } else {
        // build the request, depending on whether we're fetching albums or images.
        if (fbAlbumId.isEmpty()) {
            // fetching all albums from the me user.
            url = QUrl(QLatin1String("https://graph.facebook.com/me/albums"));
        } else {
            // fetching images from a particular album.
            url = QUrl(QString(QLatin1String("https://graph.facebook.com/%1/photos")).arg(fbAlbumId));
        }
    }

    QList<QPair<QString, QString> > queryItems;
    QUrlQuery query(url);
    if (!url.toString().contains("access_token")) {
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    }
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("limit")), QString(QLatin1String("2000"))));
    query.setQueryItems(queryItems);
    url.setQuery(query);

    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(url));
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
            connect(reply, SIGNAL(finished()), this, SLOT(imagesFinishedHandler()));
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
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString fbUserId = reply->property("fbUserId").toString();
    QString fbAlbumId = reply->property("fbAlbumId").toString();
    QString continuationUrl = reply->property("continuationUrl").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (isError || !ok || !parsed.contains(QLatin1String("data"))) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to read albums response for Facebook account with id %1"))
                .arg(accountId));
        clearRemovalDetectionLists(); // don't perform server-side removal detection during this sync run.
        decrementSemaphore(accountId);
        return;
    }

    QJsonArray data = parsed.value(QLatin1String("data")).toArray();
    if (data.size() == 0) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("Facebook account with id %1 has no albums"))
                .arg(accountId));
        decrementSemaphore(accountId);
        return;
    }

    // read the albums information
    for (int i = 0; i < data.size(); ++i) {
        QJsonObject albumObject = data.at(i).toObject();
        if (albumObject.isEmpty()) {
            continue;
        }

        QString albumId = albumObject.value(QLatin1String("id")).toString();
        QString userId = albumObject.value(QLatin1String("from")).toObject().value(QLatin1String("id")).toString();
        if (!userId.isEmpty() && userId != fbUserId) {
            // probably because the fbUserId hasn't been filled yet.
            fbUserId = userId;
            m_db.syncAccount(accountId, fbUserId);
        }
        if (!albumId.isEmpty() && albumId != fbAlbumId) {
            // probably because the fbAlbumId hasn't been filled yet.
            fbAlbumId = albumId;
        }

        QString albumName = albumObject.value(QLatin1String("name")).toString();
        QString createdTimeStr = albumObject.value(QLatin1String("created_time")).toString();
        QString updatedTimeStr = albumObject.value(QLatin1String("updated_time")).toString();
        int imageCount = static_cast<int>(albumObject.value(QLatin1String("count")).toDouble());


        // check to see whether we need to sync (any changes since last sync)
        // Note that we also check if the image count is the same, since, when
        // removing an image, the updatedTime is not changed
        QDateTime createdTime = QDateTime::fromString(createdTimeStr, Qt::ISODate);
        QDateTime updatedTime = QDateTime::fromString(updatedTimeStr, Qt::ISODate);

        // Removal detection
        m_cachedAlbums.remove(fbAlbumId);

        const FacebookAlbum::ConstPtr &dbAlbum = m_cachedAlbums.value(fbAlbumId);
        if (!dbAlbum.isNull() && (dbAlbum->updatedTime() >= updatedTime
                                  && dbAlbum->imageCount() == imageCount)) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("album with id %1 by user %2 from Facebook account with id %3 doesn't need sync"))
                    .arg(albumId).arg(userId).arg(accountId));
            continue;
        }

        // We need to sync. We save the album entry, and request the images for the album.
        // When saving the album, we might need to add a new user
        possiblyAddNewUser(userId, accountId, accessToken);

        // We then save the album
        m_db.addAlbum(albumId, userId, createdTime, updatedTime, albumName, imageCount);
        // TODO: After successfully added an album, we should begin a new query to get the image
        // information (based on cover image id).
        requestData(accountId, accessToken, QString(), fbUserId, fbAlbumId);

    }

    // Perform a continuation request if required.
    QJsonObject paging = parsed.value(QLatin1String("paging")).toObject();
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


void FacebookImageSyncAdaptor::imagesFinishedHandler()
{

    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString fbUserId = reply->property("fbUserId").toString();
    QString fbAlbumId = reply->property("fbAlbumId").toString();
    QString continuationUrl = reply->property("continuationUrl").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (isError || !ok || !parsed.contains(QLatin1String("data"))) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to read photos response for Facebook account with id %1"))
                .arg(accountId));
        clearRemovalDetectionLists(); // don't perform server-side removal detection during this sync run.
        decrementSemaphore(accountId);
        return;
    }

    QJsonArray data = parsed.value(QLatin1String("data")).toArray();
    if (data.size() == 0) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("Album with id %1 from Facebook account with id %2 has no photos"))
                .arg(fbAlbumId).arg(accountId));
        checkRemovedImages(fbAlbumId);
        decrementSemaphore(accountId);
        return;
    }

    // read the photos information
    foreach (const QJsonValue imageValue, data) {
        QJsonObject imageObject = imageValue.toObject();
        if (imageObject.isEmpty()) {
            continue;
        }

        QString photoId = imageObject.value(QLatin1String("id")).toString();
        QString thumbnailUrl = imageObject.value(QLatin1String("picture")).toString();
        QString imageSrcUrl = imageObject.value(QLatin1String("source")).toString();
        QString createdTimeStr = imageObject.value(QLatin1String("created_time")).toString();
        QString updatedTimeStr = imageObject.value(QLatin1String("updated_time")).toString();
        QString photoName = imageObject.value(QLatin1String("name")).toString();

        // Find the correct thumbnail size. The fallback will be the "picture" which usually
        // is too small so this is sort of best guess what sizes FB might returns. We can't
        // also hardcode the exact sizes here, because we can't be sure that certains sizes
        // will stay for ever.
        // TODO: we can use https://graph.facebook.com/object_id/picture?type=large
        QJsonArray images = imageObject.value(QLatin1String("images")).toArray();
        foreach (const QJsonValue &imageValue, images) {
            QJsonObject image = imageValue.toObject();
            int width = static_cast<int>(image.value(QLatin1String("width")).toDouble());
            int height= static_cast<int>(image.value(QLatin1String("height")).toDouble());
            if (160 <= width && width <= 350 &&
                160 <= height && height <= 350) {
                thumbnailUrl = image.value(QLatin1String("source")).toString();
                break;
            }
        }

        int width = static_cast<int>(imageObject.value(QLatin1String("width")).toDouble());
        int height = static_cast<int>(imageObject.value(QLatin1String("height")).toDouble());
        QDateTime createdTime = QDateTime::fromString(createdTimeStr, Qt::ISODate);
        QDateTime updatedTime = QDateTime::fromString(updatedTimeStr, Qt::ISODate);


        if (!m_serverImageIds[fbAlbumId].contains(photoId)) {
            m_serverImageIds[fbAlbumId].insert(photoId);
        }

        // we need to sync.  Write to the database.
        if (haveAlreadyCachedImage(photoId, imageSrcUrl)) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("updating previously cached photo %1: %2"))
                    .arg(photoId).arg(imageSrcUrl));
        } else {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("caching new photo %1: %2"))
                    .arg(photoId).arg(imageSrcUrl));
        }
        m_db.addImage(photoId, fbAlbumId, fbUserId, createdTime, updatedTime, photoName,
                      width, height, thumbnailUrl, imageSrcUrl);
    }
    // perform a continuation request if required.
    QJsonObject paging = parsed.value(QLatin1String("paging")).toObject();
    QString nextUrl = paging.value(QLatin1String("next")).toString();
    if (!nextUrl.isEmpty() && nextUrl != continuationUrl) {
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("performing continuation request for more photos for Facebook account with id %1: %2"))
                .arg(accountId).arg(nextUrl));
        requestData(accountId, accessToken, nextUrl, fbUserId, fbAlbumId);
    }

    // we're finished this request.  Decrement our busy semaphore.
    checkRemovedImages(fbAlbumId);
    decrementSemaphore(accountId);
}



bool FacebookImageSyncAdaptor::haveAlreadyCachedImage(const QString &fbImageId, const QString &imageUrl)
{
    FacebookImage::ConstPtr dbImage = m_db.image(fbImageId);
    bool imagedbSynced = !dbImage.isNull();

    if (!imagedbSynced) {
        return false;
    }

    QString dbImageUrl = dbImage->imageUrl();
    if (dbImageUrl != imageUrl) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: Image/facebook.db has outdated data!"
                                      "\n   fbPhotoId: %1\n   cached image url: %2\n   new image url: %3"))
                .arg(fbImageId).arg(dbImageUrl).arg(imageUrl));
        return false;
    }

    return true;
}


void FacebookImageSyncAdaptor::possiblyAddNewUser(const QString &fbUserId, int accountId,
                                                  const QString &accessToken)
{
    if (!m_db.user(fbUserId).isNull()) {
        return;
    }

    // We need to add the user. We call Facebook to get the informations that we
    // need and then add it to the database
    // me?fields=updated_time,name,picture
    QUrl url(QLatin1String("https://graph.facebook.com/me"));
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("fields")),
                                              QLatin1String("id,updated_time,name,picture")));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(url));
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(userFinishedHandler()));
    }
}


void FacebookImageSyncAdaptor::userFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QByteArray replyData = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!ok || !parsed.contains(QLatin1String("id"))) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to read user response for Facebook account with id %1"))
                .arg(accountId));
        return;
    }

    QString fbUserId = parsed.value(QLatin1String("id")).toString();
    QString fbName = parsed.value(QLatin1String("name")).toString();
    QString updatedStr = parsed.value(QLatin1String("updated_time")).toString();

    m_db.addUser(fbUserId, QDateTime::fromString(updatedStr, Qt::ISODate), fbName);
}

void FacebookImageSyncAdaptor::initRemovalDetectionLists()
{
    // This function should be called as part of the ::sync() preamble.
    // Clear our internal state variables which we use to track server-side deletions.
    // We have to do it this way, as results can be spread across multiple requests
    // if Facebook returns results in paginated form.
    clearRemovalDetectionLists();

    QList<FacebookAlbum::ConstPtr> albums = m_db.albums();
    foreach (const FacebookAlbum::ConstPtr &album, albums) {
        m_cachedAlbums.insert(album->fbAlbumId(), album);
    }
}

void FacebookImageSyncAdaptor::clearRemovalDetectionLists()
{
    m_cachedAlbums.clear();
    m_serverImageIds.clear();
    m_removedImages.clear();
}

void FacebookImageSyncAdaptor::checkRemovedImages(const QString &fbAlbumId)
{
    const QSet<QString> &serverImageIds = m_serverImageIds.value(fbAlbumId);
    QSet<QString> cachedImageIds = m_db.imageIds(fbAlbumId).toSet();

    foreach (const QString &fbImageId, serverImageIds) {
        cachedImageIds.remove(fbImageId);
    }

    m_removedImages.append(cachedImageIds.toList());
}

// TODO v where is it used ? Defined but not used in the class
/*
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

}*/
