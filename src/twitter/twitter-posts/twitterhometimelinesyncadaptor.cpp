/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "twitterhometimelinesyncadaptor.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QJsonValue>
#include <QtCore/QUrlQuery>

TwitterHomeTimelineSyncAdaptor::TwitterHomeTimelineSyncAdaptor(QObject *parent)
    : TwitterDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Posts, parent)
{
    setInitialActive(m_db.isValid());
}

TwitterHomeTimelineSyncAdaptor::~TwitterHomeTimelineSyncAdaptor()
{
}

void TwitterHomeTimelineSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    if (purgeIds.size()) {
        foreach (int accountIdentifier, purgeIds) {
            m_db.removePosts(accountIdentifier);
        }
        m_db.commit();
        m_db.wait();
    }
}

QString TwitterHomeTimelineSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("twitter-microblog");
}

void TwitterHomeTimelineSyncAdaptor::beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret)
{
    requestMe(accountId, oauthToken, oauthTokenSecret);
}

void TwitterHomeTimelineSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId)
    m_db.commit();
    m_db.wait();
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
        setupReplyTimeout(accountId, reply);
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
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("count")), QString(QLatin1String("10")))); // limit to 10 Tweets.
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
        setupReplyTimeout(accountId, reply);
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
    removeReplyTimeout(accountId, reply);

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
                                           SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Posts),
                                           accountId);
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

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

        m_db.removePosts(accountId); // purge old tweets.

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

            // Twitter does some HTML substitutions in their content
            // in JSON feeds, to prevent issues with JSONP formatting.
            body.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
            body.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
            body.replace(QStringLiteral("&amp;"), QStringLiteral("&"));

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


            // We always purge, so even if we've synced it in the past, we need it.
            // Check to see if we need to post it to the events feed
            int sinceSpan = m_accountSyncProfile
                          ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("7")).toInt()
                          : 7;
            if (eventTimestamp.daysTo(QDateTime::currentDateTime()) > sinceSpan) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("tweet for account %1 is more than %2 days old:\n    %3 - %4"))
                        .arg(accountId).arg(sinceSpan).arg(eventTimestamp.toString(Qt::ISODate)).arg(body));
            } else {
                m_db.addTwitterPost(postId, name, body, eventTimestamp, icon, imageList,
                                    screenName, retweeter, consumerKey(), consumerSecret(), accountId);
            }
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse event feed data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}
