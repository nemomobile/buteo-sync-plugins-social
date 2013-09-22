/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "twitterhometimelinesyncadaptor.h"
#include "syncservice.h"
#include "trace.h"
#include "constants_p.h"

#include <QtCore/QPair>
#include <QtCore/QJsonValue>

#include "eventfeedhelper_p.h"

TwitterHomeTimelineSyncAdaptor::TwitterHomeTimelineSyncAdaptor(SyncService *syncService, QObject *parent)
    : TwitterDataTypeSyncAdaptor(syncService, SyncService::Posts, parent)
{
    m_db.initDatabase();
    setInitialActive(m_db.isValid());
}

TwitterHomeTimelineSyncAdaptor::~TwitterHomeTimelineSyncAdaptor()
{
}

void TwitterHomeTimelineSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    /*
     *foreach (int accountIdentifier, purgeIds) {
        bool ok;
        QStringList localIdentifiers = removeAllData(serviceName(), SyncService::dataType(dataType),
                                                     QString::number(accountIdentifier), &ok);
        if (!ok) {
            continue;
        }


        int prefixLength = QString(SOCIALD_TWITTER_POSTS_ID_PREFIX).size();
        // Remove entries in the event feed
        foreach (const QString &localIdentifier, localIdentifiers) {
            QString eventIdString = localIdentifier.mid(prefixLength);
            qlonglong eventId = eventIdString.toLongLong(&ok);
            if (ok) {
                MEventFeed::instance()->removeItem(eventId);
            } else {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to remove event %1"))
                        .arg(eventIdString));
            }
        }
    }*/
    // TODO ^
}

void TwitterHomeTimelineSyncAdaptor::beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret)
{
    requestMe(accountId, oauthToken, oauthTokenSecret);
}

void TwitterHomeTimelineSyncAdaptor::finalize()
{
    m_db.write();
}

void TwitterHomeTimelineSyncAdaptor::requestMe(int accountId, const QString &oauthToken, const QString &oauthTokenSecret)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("skip_status")), QString(QLatin1String("true"))));
    QString baseUrl = QLatin1String("https://api.twitter.com/1.1/account/verify_credentials.json");
    QUrl url(baseUrl);
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);

    QNetworkRequest nreq(url);
    nreq.setRawHeader("Authorization", authorizationHeader(
            accountId, oauthToken, oauthTokenSecret,
            QLatin1String("GET"), baseUrl, queryItems).toLatin1());
    QNetworkReply *reply = networkAccessManager->get(nreq);
    
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("oauthToken", oauthToken);
        reply->setProperty("oauthTokenSecret", oauthTokenSecret);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedMeHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request user verification from Twitter account with id %1"))
                .arg(accountId));
    }
}

void TwitterHomeTimelineSyncAdaptor::requestPosts(int accountId, const QString &oauthToken,
                                                  const QString &oauthTokenSecret,
                                                  const QString &sinceTweetId, const QString &fromUserId)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("count")), QString(QLatin1String("50"))));
    if (!sinceTweetId.isEmpty()) {
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("since_id")), sinceTweetId));
    }
    if (!fromUserId.isEmpty()) {
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("user_id")), fromUserId));
    }
    QString baseUrl = QLatin1String("https://api.twitter.com/1.1/statuses/home_timeline.json");
    QUrl url(baseUrl);
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);

    QNetworkRequest nreq(url);
    nreq.setRawHeader("Authorization", authorizationHeader(
            accountId, oauthToken, oauthTokenSecret,
            QLatin1String("GET"), baseUrl, queryItems).toLatin1());
    QNetworkReply *reply = networkAccessManager->get(nreq);
    
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("oauthToken", oauthToken);
        reply->setProperty("oauthTokenSecret", oauthTokenSecret);
        reply->setProperty("selfUserId", fromUserId);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedPostsHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request user timeline posts from Twitter account with id %1"))
                .arg(accountId));
    }
}

void TwitterHomeTimelineSyncAdaptor::finishedMeHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        // shouldn't happen, but just in case.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: invalid reply in finished me - unable to decrement semaphore!")));
        return;
    }
    int accountId = reply->property("accountId").toInt();
    QString oauthToken = reply->property("oauthToken").toString();
    QString oauthTokenSecret = reply->property("oauthTokenSecret").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("id_str"))) {
        QString selfUserId = parsed.value(QLatin1String("id_str")).toString();
        QString selfScreenName = parsed.value(QLatin1String("screen_name")).toString();
        QString profileImage = parsed.value(QLatin1String("profile_image_url")).toString();
        if (!m_selfTuids.contains(selfUserId)) {
            m_selfTuids.append(selfUserId);
            m_selfTScreenNames.insert(selfUserId, selfScreenName);
            m_accountProfileImage.insert(accountId, profileImage);
        }

        requestPosts(accountId, oauthToken, oauthTokenSecret, QString(), QString());
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse self user id from me request for account %1, got:"))
                .arg(accountId) << replyData);
    }

    decrementSemaphore(accountId);
}

void TwitterHomeTimelineSyncAdaptor::finishedPostsHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        // shouldn't happen, but just in case
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: invalid reply in finished posts - unable to decrement semaphore!")));
        return;
    }

    int accountId = reply->property("accountId").toInt();
    QDateTime lastSync = lastSyncTimestamp(QLatin1String("twitter"),
                                           SyncService::dataType(SyncService::Posts),
                                           accountId);
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonArray tweets = parseJsonArrayReplyData(replyData, &ok);
    if (ok) {
        if (!tweets.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no feed posts received for account %1"))
                    .arg(accountId));
            decrementSemaphore(accountId);
            return;
        }

        QList<SyncedDatum> syncedData;

        foreach (const QJsonValue &tweetValue, tweets) {
            // these are the fields we eventually need to fill out:
            QList<QPair<QString, SocialPostImage::ImageType> > imageList;
            QString retweeter;

            // grab the data from the current post
            QJsonObject tweet = tweetValue.toObject();

            // Just to be sure to get the time of the current (re)tweet
            QDateTime eventTimestamp = parseTwitterDateTime(tweet.value(QLatin1String("created_at")).toString());

            // We should get data for the retweeted tweet instead of
            // getting the (often partial) retweeted tweet.
            if (tweet.contains(QLatin1String("retweeted_status"))) {
                retweeter = tweet.value(QLatin1String("user")).toObject().value("name").toString();
                tweet = tweet.value(QLatin1String("retweeted_status")).toObject();
            }

            QString postId = tweet.value(QLatin1String("id_str")).toString();
            QString body = tweet.value(QLatin1String("text")).toString();
            QJsonObject user = tweet.value(QLatin1String("user")).toObject();
            QString name = user.value("name").toString();
            QString screenName = user.value("screen_name").toString();
            QString icon = user.value(QLatin1String("profile_image_url")).toString();

            QJsonObject entities = tweet.value(QLatin1String("entities")).toObject();
            QJsonArray mediaList = entities.value(QLatin1String("media")).toArray();
            if (!mediaList.isEmpty()) {
                foreach (const QJsonValue &mediaValue, mediaList) {
                    QJsonObject mediaObject = mediaValue.toObject();
                    if (mediaObject.contains(QLatin1String("media_url_https"))) {
                        QString imageUrl = mediaObject.value(QLatin1String("media_url_https")).toString();
                        imageList.append(qMakePair<QString, SocialPostImage::ImageType>(imageUrl, SocialPostImage::Photo));
                    }
                }
            }

            QJsonArray urlList = entities.value(QLatin1String("urls")).toArray();
            foreach (const QJsonValue &urlValue, urlList) {
                // Right now, we use a slightly inefficient algorithm, that is error-proof
                // we just replace the old URL by the new one
                QJsonObject urlObject = urlValue.toObject();
                QString shortUrl = urlObject.value(QLatin1String("url")).toString();
                QString expandedUrl = urlObject.value(QLatin1String("expanded_url")).toString();
                body.replace(shortUrl, expandedUrl);
            }


            // check to see if we need to post it to the events feed
            if (lastSync.isValid() && eventTimestamp < lastSync) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("event for account %1 came after last sync:"))
                        .arg(accountId) << "    " << eventTimestamp << ":" << body);
                break;                 // all subsequent events will be even older.
            } else if (eventTimestamp.daysTo(QDateTime::currentDateTime()) > 7) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("event for account %1 is more than a week old:\n"))
                        .arg(accountId) << "    " << eventTimestamp << ":" << body);
                break;                 // all subsequent events will be even older.
            } else {
                m_db.addTwitterPost(postId, name, body, eventTimestamp, icon, imageList,
                                    screenName, retweeter, consumerKey(), consumerSecret(), accountId);
            }
        }
        markSyncedData(syncedData);
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse event feed data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}
