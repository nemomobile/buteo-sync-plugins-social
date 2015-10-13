/****************************************************************************
 **
 ** Copyright (C) 2015 Jolla Ltd.
 ** Contact: Jonni Rainisto <jonni.rainisto@jollamobile.com>
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

#include "dropboximagesyncadaptor.h"
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

#include <MGConfItem>

// Update the following version if database schema changes e.g. new
// fields are added to the existing tables.
// It will make old tables dropped and creates new ones.

// Currently, we integrate with the device image gallery via saving thumbnails to the
// ~/.config/sociald/images directory, and filling the ~/.config/sociald/images/dropbox.db
// with appropriate data.

DropboxImageSyncAdaptor::DropboxImageSyncAdaptor(QObject *parent)
    : DropboxDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Images, parent)
    , m_optimalThumbnailWidth(0)
    , m_optimalImageWidth(0)
{
    setInitialActive(m_db.isValid());
}

DropboxImageSyncAdaptor::~DropboxImageSyncAdaptor()
{
}

QString DropboxImageSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("dropbox-images");
}

void DropboxImageSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    // get ready for sync
    if (!determineOptimalDimensions()) {
        SOCIALD_LOG_ERROR("unable to determine optimal image dimensions, aborting");
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }
    if (!initRemovalDetectionLists(accountId)) {
        SOCIALD_LOG_ERROR("unable to initialized cached account list for account" << accountId);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    // call superclass impl.
    DropboxDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void DropboxImageSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    m_db.purgeAccount(oldId);
    m_db.commit();
    m_db.wait();

    // manage image cache. Gallery UI caches full size images
    // and maintains bindings between source and cached image in SocialImageDatabase.
    // purge cached images belonging to this account.
    purgeCachedImages(&m_imageCacheDb, oldId);
}

void DropboxImageSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    queryCameraRoll(accountId, accessToken);
    // requestAlbums(accountId, accessToken);
}

void DropboxImageSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId)

    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, won't commit database changes");
    } else {
        // Remove albums
        m_db.removeAlbums(m_cachedAlbums.keys());

        // Remove images
        m_db.removeImages(m_removedImages);

        m_db.commit();
        m_db.wait();

        // manage image cache. Gallery UI caches full size images
        // and maintains bindings between source and cached image in SocialImageDatabase.
        // purge cached images older than two weeks.
        purgeExpiredImages(&m_imageCacheDb, accountId);
    }
}

void DropboxImageSyncAdaptor::queryCameraRoll(int accountId, const QString &accessToken)
{
    QUrl url(QStringLiteral("%1/1/metadata/auto/Pictures").arg(api()));
    QNetworkRequest req;
    req.setUrl(url);
    req.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                     QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());

    QNetworkReply *reply = m_networkAccessManager->get(req);
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(cameraRollFinishedHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request data from Dropbox account with id" << accountId);
        clearRemovalDetectionLists(); // don't perform server-side removal detection during this sync run.
    }
}

void DropboxImageSyncAdaptor::cameraRollFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString continuationUrl = reply->property("continuationUrl").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);

    if (isError || !ok || parsed.contains("error")) {
        SOCIALD_LOG_ERROR("unable to read albums response for Dropbox account with id" << accountId);
        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            SOCIALD_LOG_DEBUG("Possibly" << reply->request().url().toString()
                              << "is not available on server because no photos have been uploaded yet");
        }
        clearRemovalDetectionLists(); // don't perform server-side removal detection during this sync run.
        decrementSemaphore(accountId);
        return;
    }

    QString albumId = "DropboxPictures-" + QString::number(accountId) ; // in future we might have multiple dropbox accounts
    QString albumName = "Pictures"; // TODO: where this is used, do we need to translate?
    QString createdTimeStr = parsed.value(QLatin1String("created_time")).toString();
    QString updatedTimeStr = parsed.value(QLatin1String("updated_time")).toString();
    int imageCount = static_cast<int>(parsed.value(QLatin1String("count")).toDouble());

    QString albumHash = parsed.value(QLatin1String("hash")).toString(); // This is used to check if anything has changed since last query.
    QJsonArray data = parsed.value(QLatin1String("contents")).toArray();
    if (data.size() == 0) {
        SOCIALD_LOG_DEBUG("Dropbox account with id" << accountId << "has no Pictures");
        checkRemovedImages(albumId);
        decrementSemaphore(accountId);
        return;
    }

    // read the albums information
    for (int i = 0; i < data.size(); ++i) {
        QJsonObject fileObject = data.at(i).toObject();
        if (fileObject.isEmpty() || !fileObject.value("mime_type").toString().startsWith("image")) {
            continue;
        }
        imageCount++;
    }

    QString userId = QString::number(accountId) ;
    QDateTime createdTime = QDateTime::fromString(createdTimeStr, Qt::ISODate);
    QDateTime updatedTime = QDateTime::fromString(updatedTimeStr, Qt::ISODate);

    const DropboxAlbum::ConstPtr &dbAlbum = m_cachedAlbums.value(albumId);
    m_cachedAlbums.remove(albumId);  // Removal detection

    if (!dbAlbum.isNull() && (dbAlbum->hash() == albumHash
                              && dbAlbum->imageCount() == imageCount)) {
        SOCIALD_LOG_DEBUG("album with id" << albumId << "by user" << userId <<
                          "from Dropbox account with id" << accountId << "doesn't need sync");
        decrementSemaphore(accountId);
        return;
    }

    possiblyAddNewUser(userId, accountId, accessToken);
    m_db.syncAccount(accountId, userId);
    m_db.addAlbum(albumId, userId, createdTime, updatedTime, albumName, imageCount, albumHash);

    for (int i = 0; i < data.size(); ++i) {
        QJsonObject fileObject = data.at(i).toObject();
        if (fileObject.isEmpty() || !fileObject.value("mime_type").toString().startsWith("image")) {
            continue;
        }

        bool thumbExists = fileObject.value("thumb_exists").toBool();
        QString thumbnailAPIUrl = content() + "/1/thumbnails/auto";
        QString fileAPIUrl = content() + "/1/files/auto";
        QString photoId = fileObject.value(QLatin1String("rev")).toString(); // does changing hash cause issues?
        QString thumbnailUrl = fileObject.value("thumb_exists").toBool() ? thumbnailAPIUrl + fileObject.value(QLatin1String("path")).toString() : "";
        QString imageSrcUrl = fileAPIUrl + fileObject.value(QLatin1String("path")).toString();
        QString createdTimeStr = fileObject.value(QLatin1String("client_mtime")).toString();
        QString updatedTimeStr = fileObject.value(QLatin1String("modified")).toString();
        QString photoName = fileObject.value(QLatin1String("path")).toString().split("/").last();
        int imageWidth = 0;
        int imageHeight = 0;

        // Find optimal thumbnail and image source urls based on dimensions.
        QList<ImageSource> imageSources;
        // https://content.dropboxapi.com/1/thumbnails/auto/
        // xs	32x32 s	64x64 m	128x128 l 640x480 xl 1024x768

        imageSources << ImageSource(static_cast<int>(32), static_cast<int>(32),
                                   thumbnailAPIUrl + fileObject.value(QLatin1String("path")).toString()+"?size=xs");
        imageSources << ImageSource(static_cast<int>(64), static_cast<int>(64),
                                   thumbnailAPIUrl + fileObject.value(QLatin1String("path")).toString()+"?size=s");
        imageSources << ImageSource(static_cast<int>(128), static_cast<int>(128),
                                   thumbnailAPIUrl + fileObject.value(QLatin1String("path")).toString()+"?size=m");
        imageSources << ImageSource(static_cast<int>(640), static_cast<int>(480),
                                   thumbnailAPIUrl + fileObject.value(QLatin1String("path")).toString()+"?size=l");
        imageSources << ImageSource(static_cast<int>(1024), static_cast<int>(768),
                                   thumbnailAPIUrl + fileObject.value(QLatin1String("path")).toString()+"?size=xl");
        // Dropbox does not have API to query original picture size, so we just lie it to be the best
        imageSources << ImageSource(static_cast<int>(1024), static_cast<int>(800),
                                   fileAPIUrl + fileObject.value(QLatin1String("path")).toString());

        bool foundOptimalImage = false, foundOptimalThumbnail = false;
        std::sort(imageSources.begin(), imageSources.end());
        Q_FOREACH (const ImageSource &img, imageSources) {
            if (thumbExists && !foundOptimalThumbnail && qMin(img.width, img.height) >= m_optimalThumbnailWidth) {
                foundOptimalThumbnail = true;
                thumbnailUrl = img.sourceUrl;
            }
            if (!foundOptimalImage && qMin(img.width, img.height) >= m_optimalImageWidth) {
                foundOptimalImage = true;
                imageWidth = img.width;
                imageHeight = img.height;
                imageSrcUrl = img.sourceUrl;
            }
        }
        if (!foundOptimalThumbnail) {
            // just choose the largest one.
            thumbnailUrl = imageSources.last().sourceUrl;
        }
        if (!foundOptimalImage) {
            // just choose the largest one. (size values are false)
            imageSrcUrl = imageSources.last().sourceUrl;
            imageWidth = imageSources.last().width;
            imageHeight = imageSources.last().height;
        }

        QDateTime createdTime = QDateTime::fromString(createdTimeStr, Qt::ISODate);
        QDateTime updatedTime = QDateTime::fromString(updatedTimeStr, Qt::ISODate);
        if (!m_serverImageIds[albumId].contains(photoId)) {
            m_serverImageIds[albumId].insert(photoId);
        }

        // check if we need to sync, and write to the database.
        if (haveAlreadyCachedImage(photoId, imageSrcUrl)) {
            SOCIALD_LOG_DEBUG("have previously cached photo" << photoId << ":" << imageSrcUrl);
        } else {
            SOCIALD_LOG_DEBUG("caching new photo" << photoId << ":" << imageSrcUrl << "->" << imageWidth << "x" << imageHeight);
            m_db.addImage(photoId, albumId, userId, createdTime, updatedTime,
                          photoName, imageWidth, imageHeight, thumbnailUrl, imageSrcUrl, accessToken);
        }
    }
    checkRemovedImages(albumId);
    // Finally, reduce our semaphore.
    decrementSemaphore(accountId);
}

bool DropboxImageSyncAdaptor::haveAlreadyCachedImage(const QString &imageId, const QString &imageUrl)
{
    DropboxImage::ConstPtr dbImage = m_db.image(imageId);
    bool imagedbSynced = !dbImage.isNull();

    if (!imagedbSynced) {
        return false;
    }

    QString dbImageUrl = dbImage->imageUrl();
    if (dbImageUrl != imageUrl) {
        SOCIALD_LOG_ERROR("Image/dropbox.db has outdated data!\n"
                          "   photoId:" << imageId << "\n"
                          "   cached image url:" << dbImageUrl << "\n"
                          "   new image url:" << imageUrl);
        return false;
    }

    return true;
}

void DropboxImageSyncAdaptor::possiblyAddNewUser(const QString &userId, int accountId,
                                                  const QString &accessToken)
{
    if (!m_db.user(userId).isNull()) {
        return;
    }

    // We need to add the user. We call Dropbox to get the informations that we
    // need and then add it to the database https://api.dropboxapi.com/1/account/info

    QUrl url(QStringLiteral("%1/1/account/info").arg(api()));
    QNetworkRequest req;
    req.setUrl(url);
    req.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                     QString(QLatin1String("Bearer ")).toUtf8() + accessToken.toUtf8());

    QNetworkReply *reply = m_networkAccessManager->get(req);
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)),
                this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(userFinishedHandler()));

        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    }
}

void DropboxImageSyncAdaptor::userFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QByteArray replyData = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!ok || !parsed.contains(QLatin1String("display_name"))) {
        SOCIALD_LOG_ERROR("unable to read user response for Dropbox account with id" << accountId);
        return;
    }

    // QString userId = parsed.value(QLatin1String("id")).toString();
    QString name = parsed.value(QLatin1String("display_name")).toString();

    m_db.addUser(QString::number(accountId), QDateTime::currentDateTime(), name);
    decrementSemaphore(accountId);
}

bool DropboxImageSyncAdaptor::initRemovalDetectionLists(int accountId)
{
    // This function should be called as part of the ::sync() preamble.
    // Clear our internal state variables which we use to track server-side deletions.
    // We have to do it this way, as results can be spread across multiple requests
    // if Dropbox returns results in paginated form.
    clearRemovalDetectionLists();

    bool ok = false;
    QMap<int,QString> accounts = m_db.accounts(&ok);
    if (!ok) {
        return false;
    }
    if (accounts.contains(accountId)) {
        QString userId = accounts.value(accountId);

        QStringList allAlbumIds = m_db.allAlbumIds();
        foreach (const QString& albumId, allAlbumIds) {
            DropboxAlbum::ConstPtr album = m_db.album(albumId);
            if (album->userId() == userId) {
                m_cachedAlbums.insert(albumId, album);
            }
        }
    }

    return true;
}

void DropboxImageSyncAdaptor::clearRemovalDetectionLists()
{
    m_cachedAlbums.clear();
    m_serverImageIds.clear();
    m_removedImages.clear();
}

void DropboxImageSyncAdaptor::checkRemovedImages(const QString &albumId)
{
    const QSet<QString> &serverImageIds = m_serverImageIds.value(albumId);
    QSet<QString> cachedImageIds = m_db.imageIds(albumId).toSet();

    foreach (const QString &imageId, serverImageIds) {
        cachedImageIds.remove(imageId);
    }

    m_removedImages.append(cachedImageIds.toList());
}

bool DropboxImageSyncAdaptor::determineOptimalDimensions()
{
    int width = 0, height = 0;
    const int defaultValue = 0;
    MGConfItem widthConf("/lipstick/screen/primary/width");
    if (widthConf.value(defaultValue).toInt() != defaultValue) {
        width = widthConf.value(defaultValue).toInt();
    }
    MGConfItem heightConf("/lipstick/screen/primary/height");
    if (heightConf.value(defaultValue).toInt() != defaultValue) {
        height = heightConf.value(defaultValue).toInt();
    }

    // we want to use the largest of these dimensions as the "optimal"
    int maxDimension = qMax(width, height);
    if (maxDimension % 3 == 0) {
        m_optimalThumbnailWidth = maxDimension / 3;
    } else {
        m_optimalThumbnailWidth = (maxDimension / 2);
    }
    m_optimalImageWidth = maxDimension;
    SOCIALD_LOG_DEBUG("Determined optimal image dimension:" << m_optimalImageWidth << ", thumbnail:" << m_optimalThumbnailWidth);
    return true;
}
