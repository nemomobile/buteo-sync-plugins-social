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

#include "vkimagesyncadaptor.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QVariantMap>
#include <QtCore/QByteArray>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#define VK_IMAGES_MAX_COUNT 1000 /* maximum images returned per request */

// Currently, we integrate with the device image gallery via saving thumbnails to the
// ~/.local/share/system/privileged/Images directory, and filling the
// ~/.local/share/system/privileged/Images/vk.db with appropriate data
// via libsocialcache.

VKImageSyncAdaptor::VKImageSyncAdaptor(QObject *parent)
    : VKDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Images, parent)
    , m_syncError(false)
{
    setInitialActive(m_db.isValid());
}

VKImageSyncAdaptor::~VKImageSyncAdaptor()
{
}

QString VKImageSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("vk-images");
}

void VKImageSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    // call superclass impl.
    VKDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void VKImageSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    m_db.purgeAccount(oldId);
    m_db.commit();
    m_db.wait();
}

void VKImageSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    requestData(accountId, accessToken, QString(), QString(), QString());
}

void VKImageSyncAdaptor::finalize(int accountId)
{
    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, won't commit database changes");
    } else if (m_syncError) {
        SOCIALD_LOG_INFO("sync error, won't commit database changes");
    } else {
        // Determine album delta.
        QHash<QString, QSet<QString> > deletedAlbumIds; // user to deleted album ids
        QHash<QString, QSet<QString> > emptiedAlbumIds; // user to empty album ids
        QList<VKAlbum::ConstPtr> deletedAlbums;
        QList<VKAlbum::ConstPtr> addedAlbums;
        QList<VKAlbum::ConstPtr> modifiedAlbums;
        QList<VKAlbum::ConstPtr> unmodifiedAlbums;

        // first, find the albums which are new and need to be added to the db.
        bool found = false;
        QList<VKAlbum::ConstPtr> accountAlbums = m_db.albums(accountId, QString());
        Q_FOREACH (const VKAlbum::ConstPtr &receivedAlbum, m_receivedAlbums) {
            found = false;
            Q_FOREACH (const VKAlbum::ConstPtr &album, accountAlbums) {
                if (album->id() == receivedAlbum->id() && album->ownerId() == receivedAlbum->ownerId()) {
                    found = true;
                }
            }
            if (!found) {
                addedAlbums.append(receivedAlbum);
            }
        }

        // then, find the albums which need to be removed or updated in the db.
        Q_FOREACH (const VKAlbum::ConstPtr &album, accountAlbums) {
            found = false;
            Q_FOREACH (const VKAlbum::ConstPtr &receivedAlbum, m_receivedAlbums) {
                if (album->id() == receivedAlbum->id() && album->ownerId() == receivedAlbum->ownerId()) {
                    found = true;
                    if (*album != *receivedAlbum) {
                        modifiedAlbums.append(receivedAlbum);
                    } else {
                        unmodifiedAlbums.append(receivedAlbum);
                    }
                }
            }
            if (!found) {
                deletedAlbums.append(album);
                deletedAlbumIds[album->ownerId()].insert(album->id());
            }
        }

        // and find the albums which are empty server side.
        // these will be unmodified but the photos will need to be removed.
        Q_FOREACH (const VKAlbum::ConstPtr &album, m_emptyAlbums) {
            emptiedAlbumIds[album->ownerId()].insert(album->id());
        }

        // Determine photo delta.
        QList<VKImage::ConstPtr> deletedPhotos;
        QList<VKImage::ConstPtr> addedPhotos;
        QList<VKImage::ConstPtr> modifiedPhotos;
        QList<VKImage::ConstPtr> unmodifiedPhotos;

        // first, find the photos which need to be removed or updated in the db.
        QList<VKImage::ConstPtr> accountPhotos = m_db.images(accountId, QString(), QString());
        Q_FOREACH (const VKImage::ConstPtr &photo, accountPhotos) {
            if (deletedAlbumIds[photo->ownerId()].contains(photo->albumId())) {
                // the entire album has been deleted.  every photo in it needs to be deleted.
                deletedPhotos.append(photo);
            } else if (emptiedAlbumIds[photo->ownerId()].contains(photo->albumId())) {
                // the album has been emptied server-side.  every photo in it needs to be deleted.
                deletedPhotos.append(photo);
            } else if (!m_requestedPhotosForOwnerAndAlbum.contains(QStringLiteral("%1:%2").arg(photo->ownerId()).arg(photo->albumId()))) {
                // this album wasn't modified, so we didn't request photos from it.
                // that is, every photo in it is unchanged.
                unmodifiedPhotos.append(photo);
            } else {
                // this album was modified, so we need to perform delta detection.
                found = false;
                Q_FOREACH (const VKImage::ConstPtr &receivedPhoto, m_receivedPhotos) {
                    if (photo->id() == receivedPhoto->id() && photo->ownerId() == receivedPhoto->ownerId()) {
                        found = true;
                        if (*photo != *receivedPhoto) {
                            modifiedPhotos.append(receivedPhoto);
                        } else {
                            unmodifiedPhotos.append(receivedPhoto);
                        }
                    }
                }
                if (!found) {
                    deletedPhotos.append(photo);
                }
            }
        }

        // then find the photos which are new and need to be added to the db.
        Q_FOREACH (const VKImage::ConstPtr &receivedPhoto, m_receivedPhotos) {
            found = false;
            Q_FOREACH (const VKImage::ConstPtr &photo, accountPhotos) {
                if (photo->id() == receivedPhoto->id() && photo->ownerId() == receivedPhoto->ownerId()) {
                    found = true;
                }
            }
            if (!found) {
                addedPhotos.append(receivedPhoto);
            }
        }

        LOG_DEBUG("Have finished Images sync for VK account:" << accountId);
        LOG_DEBUG("   with Users  added:  " << m_receivedUsers.size());
        LOG_DEBUG("   with Albums A/M/R/U:" << addedAlbums.size() << "/" << modifiedAlbums.size() << "/" << deletedAlbums.size() << "/" << unmodifiedAlbums.size());
        LOG_DEBUG("   with Photos A/M/R/U:" << addedPhotos.size() << "/" << modifiedPhotos.size() << "/" << deletedPhotos.size() << "/" << unmodifiedPhotos.size());

        // write changes to database.
        Q_FOREACH (const VKUser::ConstPtr &user, m_receivedUsers) { m_db.addUser(user); }
        m_db.addAlbums(addedAlbums+modifiedAlbums);
        m_db.removeAlbums(deletedAlbums);
        m_db.addImages(addedPhotos + modifiedPhotos);
        m_db.removeImages(deletedPhotos);

        // and commit the changes to disk.
        m_db.commit();
        m_db.wait();
    }
}

void VKImageSyncAdaptor::requestData(int accountId,
                                     const QString &accessToken,
                                     const QString &continuationUrl,
                                     const QString &vkUserId,
                                     const QString &vkAlbumId)
{
    if (syncAborted()) {
        SOCIALD_LOG_DEBUG("skipping data request due to sync abort");
        m_syncError = true;
        return;
    }

    QUrl url;
    if (!continuationUrl.isEmpty()) {
        // fetch the next page.
        url = QUrl(continuationUrl);
    } else {
        // build the request, depending on whether we're fetching albums or images.
        if (vkAlbumId.isEmpty()) {
            // fetching all albums from the user.
            url = QUrl(QStringLiteral("https://api.vk.com/method/photos.getAlbums"));
        } else {
            // fetching images from a particular album.
            url = QUrl(QStringLiteral("https://api.vk.com/method/photos.get"));
        }
    }

    // if the url already contains query part (in which case it is continuationUrl), don't overwrite it.
    if (!url.hasQuery()) {
        QList<QPair<QString, QString> > queryItems;
        QUrlQuery query(url);
        queryItems.append(QPair<QString, QString>(QStringLiteral("access_token"), accessToken));
        if (vkAlbumId.isEmpty()) {
            queryItems.append(QPair<QString, QString>(QStringLiteral("need_system"), QStringLiteral("1")));
            queryItems.append(QPair<QString, QString>(QStringLiteral("need_covers"), QStringLiteral("1")));
        } else {
            queryItems.append(QPair<QString, QString>(QStringLiteral("album_id"), vkAlbumId));
            queryItems.append(QPair<QString, QString>(QStringLiteral("extended"), QStringLiteral("1")));
            queryItems.append(QPair<QString, QString>(QStringLiteral("photo_sizes"), QStringLiteral("1")));
            queryItems.append(QPair<QString, QString>(QStringLiteral("count"), QString::number(VK_IMAGES_MAX_COUNT)));
        }
        queryItems.append(QPair<QString, QString>(QStringLiteral("v"), QStringLiteral("5.33"))); // version
        query.setQueryItems(queryItems);
        url.setQuery(query);
    }

    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("vkUserId", vkUserId);               // only valid for photos request
        reply->setProperty("vkAlbumId", vkAlbumId);             // only valid for photos request
        reply->setProperty("continuationUrl", continuationUrl); // only valid for photos request
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        if (vkAlbumId.isEmpty()) {
            SOCIALD_LOG_DEBUG("Requesting albums for VK account:" << accountId << ":" << url.toString());
            connect(reply, SIGNAL(finished()), this, SLOT(albumsFinishedHandler()));
        } else {
            SOCIALD_LOG_DEBUG("Requesting photos from album:" << vkAlbumId << "for VK account:" << accountId << ":" << url.toString());
            connect(reply, SIGNAL(finished()), this, SLOT(imagesFinishedHandler()));
        }

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request data from VK account with id" << accountId);
        m_syncError = true;
    }
}

void VKImageSyncAdaptor::albumsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);
    LOG_TRACE(QString::fromUtf8(replyData));

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (isError || !ok || !parsed.contains(QLatin1String("response"))) {
        SOCIALD_LOG_ERROR("unable to read albums response for VK account with id" << accountId);
        m_syncError = true;
        decrementSemaphore(accountId);
        return;
    }

    QJsonArray items = parsed.value(QLatin1String("response")).toObject().value(QLatin1String("items")).toArray();
    if (items.size() == 0) {
        SOCIALD_LOG_DEBUG("VK account with id" << accountId << "has no albums");
        decrementSemaphore(accountId);
        return;
    }

    // read the albums information
    for (int i = 0; i < items.size(); ++i) {
        QJsonObject albumObject = items.at(i).toObject();
        // ignore empty album objects.
        if (albumObject.isEmpty()) {
            continue;
        }

        // parse the album info.
        QString id = QString::number(albumObject.value("id").toDouble(), 'g', 13);
        QString ownerId = QString::number(albumObject.value("owner_id").toDouble(), 'g', 13);
        QString title = albumObject.value("title").toString();
        QString description = albumObject.value("description").toString();
        int created = albumObject.value("created").toInt();
        int updated = albumObject.value("updated").toInt();
        QString thumbSrc = albumObject.value("thumb_src").toString();
        int size = albumObject.value("size").toInt();
        m_receivedAlbums.append(VKAlbum::create(id, ownerId, title, description,
                                                thumbSrc, QString(), size, created, updated,
                                                accountId));

        // request the photos from this album if necessary
        int lastSyncTimestampForAlbum = 0;
        VKAlbum::ConstPtr dbAlbum = m_db.album(accountId, ownerId, id);
        if (dbAlbum) {
            lastSyncTimestampForAlbum = qMax(dbAlbum->created(), dbAlbum->updated());
        }
        if (created > lastSyncTimestampForAlbum || updated > lastSyncTimestampForAlbum) {
            SOCIALD_LOG_DEBUG("Need to request photos for album:" << id << title << "with timestamps:" <<
                              created << "+" << updated << ">" << lastSyncTimestampForAlbum);
            m_requestedPhotosForOwnerAndAlbum.insert(QStringLiteral("%1:%2").arg(ownerId).arg(id));
            requestData(accountId, accessToken, QString(), ownerId, id);
        } else {
            SOCIALD_LOG_DEBUG("No need to request photos for album:" << id << title << "with timestamps:" <<
                              created << "+" << updated << "<=" << lastSyncTimestampForAlbum);
        }

        // request the information for user who owns this album if necessary
        possiblyAddNewUser(ownerId, accountId, accessToken);
    }

    // Finally, reduce our semaphore.
    decrementSemaphore(accountId);
}

void VKImageSyncAdaptor::imagesFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QString vkUserId = reply->property("vkUserId").toString();
    QString vkAlbumId = reply->property("vkAlbumId").toString();
    QString continuationUrl = reply->property("continuationUrl").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);
    LOG_TRACE(QString::fromUtf8(replyData));

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (isError || !ok || !parsed.contains(QLatin1String("response"))) {
        SOCIALD_LOG_ERROR("unable to read photos response for VK account with id" << accountId);
        m_syncError = true;
        decrementSemaphore(accountId);
        return;
    }

    QJsonArray items = parsed.value(QLatin1String("response")).toObject().value(QLatin1String("items")).toArray();
    if (items.size() == 0) {
        SOCIALD_LOG_DEBUG("album with id" << vkAlbumId << "from VK account with id" << accountId << "has no photos");
        VKAlbum::Ptr emptyAlbum = VKAlbum::create(vkAlbumId, vkUserId, QString(), QString(), QString(), QString(), 0, 0, 0, accountId);
        m_emptyAlbums.append(emptyAlbum);
        decrementSemaphore(accountId);
        return;
    }

    // read the photos information
    int requestImagesCount = 0;
    foreach (const QJsonValue imageValue, items) {
        // ignore empty image objects
        QJsonObject imageObject = imageValue.toObject();
        if (imageObject.isEmpty()) {
            continue;
        }

        // parse image info.
        QString id = QString::number(imageObject.value("id").toDouble(), 'g', 13);
        QString text = imageObject.value("text").toString();
        int date = imageObject.value("date").toInt();
        int height = 0;
        int width = 0;
        QString src;
        QString thumbSrc;

        QJsonArray sizedPhotos = imageObject.value("sizes").toArray();
        for (int psi = 0; psi < sizedPhotos.size(); ++psi) {
            const QJsonObject &sizedImage(sizedPhotos[psi].toObject());
            int currHeight = sizedImage.value("height").toInt();
            if (currHeight > height) {
                height = currHeight;
                width = sizedImage.value("width").toInt();
                src = sizedImage.value("src").toString();
            }

            if (thumbSrc.isEmpty() && sizedImage.value("type").toString() == QStringLiteral("s")) {
                thumbSrc = sizedImage.value("src").toString();
            } else if (sizedImage.value("type").toString() == QStringLiteral("m")) {
                thumbSrc = sizedImage.value("src").toString();
            }
        }

        if (thumbSrc.isEmpty()) {
            thumbSrc = src;
        }

        // append the photo to our internal list.
        SOCIALD_LOG_DEBUG("have new photo:" << id << src << height << width << date);
        m_receivedPhotos.append(VKImage::create(id, vkAlbumId, vkUserId, text, thumbSrc,
                                                src, QString(), QString(),
                                                width, height, date, accountId));
        requestImagesCount += 1;
    }

    // perform a continuation request if required.   set offset in url + 1000 to current offset.
    if (requestImagesCount == VK_IMAGES_MAX_COUNT) {
        QUrl continuation = QUrl(continuationUrl);
        QUrlQuery queryItems(continuation);
        int offset = queryItems.hasQueryItem("offset")
                   ? queryItems.queryItemValue("offset").toInt() + VK_IMAGES_MAX_COUNT
                   : VK_IMAGES_MAX_COUNT;
        queryItems.removeAllQueryItems("offset");
        queryItems.addQueryItem("offset", QString::number(offset));
        continuation.setQuery(queryItems);
        SOCIALD_LOG_DEBUG("performing continuation request for album:" << vkAlbumId << ":" << continuation.toString());
        requestData(accountId, accessToken, continuation.toString(), vkUserId, vkAlbumId);
    }


    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

void VKImageSyncAdaptor::possiblyAddNewUser(const QString &vkUserId, int accountId, const QString &accessToken)
{
    QString dbUserId;
    VKUser::ConstPtr dbUser = m_db.user(accountId);
    if (dbUser) {
        dbUserId = dbUser->id();
    }
    if (m_requestedUsers.contains(vkUserId) || !dbUserId.isEmpty()) {
        return; // already requested or db already contains the user, no need to request.
    }

    // We need to add the user. We call VK to get the informations that we
    // need and then add it to the database
    m_requestedUsers.insert(vkUserId);
    QUrl url(QStringLiteral("https://api.vk.com/method/users.get"));
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QStringLiteral("access_token"), accessToken));
    queryItems.append(QPair<QString, QString>(QStringLiteral("fields"), QStringLiteral("id,photo_medium,first_name,last_name")));
    queryItems.append(QPair<QString, QString>(QStringLiteral("v"), QStringLiteral("5.33"))); // version
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));
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

void VKImageSyncAdaptor::userFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QByteArray replyData = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!ok || !parsed.contains(QLatin1String("response")) || !parsed.value(QLatin1String("response")).toArray().size()) {
        SOCIALD_LOG_ERROR("unable to read users.get response for VK account with id" << accountId);
        return;
    }

    QJsonObject userObject = parsed.value(QLatin1String("response")).toArray().first().toObject();
    QString id = QString::number(userObject.value(QLatin1String("id")).toDouble(), 'g', 13);
    QString firstName = userObject.value(QLatin1String("first_name")).toString();
    QString lastName = userObject.value(QLatin1String("last_name")).toString();
    QString photoSrc = userObject.value(QLatin1String("photo_medium")).toString();
    m_receivedUsers.append(VKUser::create(id, firstName, lastName, photoSrc, QString(), accountId));

    decrementSemaphore(accountId);
}
