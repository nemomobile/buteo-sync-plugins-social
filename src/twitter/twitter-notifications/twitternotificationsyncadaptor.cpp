/****************************************************************************
 **
 ** Copyright (C) 2013-2015 Jolla Ltd.
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

#include "twitternotificationsyncadaptor.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QUrlQuery>
#include <QtCore/QJsonValue>

//nemo-qml-plugins/notifications
#include <notification.h>

// currently, we integrate with the device notifications via nemo-qml-plugin-notification

#define OPEN_BROWSER_ACTION(openUrlArgs)    \
    Notification::remoteAction(             \
        "default",                          \
        "",                                 \
        "org.sailfishos.browser",           \
        "/",                                \
        "org.sailfishos.browser",           \
        "openUrl",                          \
        QVariantList() << openUrlArgs       \
    )

TwitterNotificationSyncAdaptor::TwitterNotificationSyncAdaptor(QObject *parent)
    : TwitterDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Notifications, parent)
{
    setInitialActive(m_db.isValid());
}

TwitterNotificationSyncAdaptor::~TwitterNotificationSyncAdaptor()
{
}

QString TwitterNotificationSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("twitter-microblog");
}

void TwitterNotificationSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    Notification *notification = findNotification(oldId, Mention);
    if (notification) {
        notification->close();
        notification->deleteLater();
    }

    notification = findNotification(oldId, Retweet);
    if (notification) {
        notification->close();
        notification->deleteLater();
    }

    notification = findNotification(oldId, Follower);
    if (notification) {
        notification->close();
        notification->deleteLater();
    }

    m_db.setFollowerIds(oldId, QSet<QString>());
    m_db.setRetweetedTweetCounts(oldId, QHash<QString, int>());
    m_db.sync();
    m_db.wait();
}

void TwitterNotificationSyncAdaptor::beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret)
{
    m_lastSyncTimestamp = lastSyncTimestamp(QLatin1String("twitter"),
                                            SocialNetworkSyncAdaptor::dataTypeName(SocialNetworkSyncAdaptor::Notifications),
                                            accountId);
    SOCIALD_LOG_DEBUG("last sync of Twitter notifications was at:" << m_lastSyncTimestamp.toString(Qt::ISODate));
    requestNotifications(accountId, oauthToken, oauthTokenSecret);
}

void TwitterNotificationSyncAdaptor::finalize(int accountId)
{
    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, won't commit notification database changes for Twitter account:" << accountId);
    } else {
        m_db.sync();
        m_db.wait();
    }
}

void TwitterNotificationSyncAdaptor::requestNotifications(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &sinceTweetId, const QString &followersCursor)
{
    if (followersCursor.isEmpty()) {
        // request mentions
        QList<QPair<QString, QString> > queryItems;
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("count")), QString(QLatin1String("50"))));
        if (!sinceTweetId.isEmpty()) {
            queryItems.append(QPair<QString, QString>(QString(QLatin1String("since_id")), sinceTweetId));
        }
        QString baseUrl(QLatin1String("https://api.twitter.com/1.1/statuses/mentions_timeline.json"));
        QUrl url(baseUrl);
        QUrlQuery query(url);
        query.setQueryItems(queryItems);
        url.setQuery(query);

        QNetworkRequest mentionsRequest(url);
        mentionsRequest.setRawHeader("Authorization", authorizationHeader(
                                     accountId, oauthToken, oauthTokenSecret,
                                     QLatin1String("GET"), baseUrl, queryItems).toLatin1());

        QNetworkReply *mreply = m_networkAccessManager->get(mentionsRequest);

        if (mreply) {
            mreply->setProperty("accountId", accountId);
            mreply->setProperty("oauthToken", oauthToken);
            mreply->setProperty("oauthTokenSecret", oauthTokenSecret);
            connect(mreply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
            connect(mreply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
            connect(mreply, SIGNAL(finished()), this, SLOT(finishedMentionsHandler()));

            // we're requesting data.  Increment the semaphore so that we know we're still busy.
            incrementSemaphore(accountId);
            setupReplyTimeout(accountId, mreply);
        } else {
            SOCIALD_LOG_ERROR("unable to request mention timeline notifications from Twitter account with id" << accountId);
        }

        // request retweets
        queryItems.clear();
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("count")), QString(QLatin1String("40"))));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("trim_user")), QString(QLatin1String("false"))));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("include_entities")), QString(QLatin1String("false"))));
        if (!sinceTweetId.isEmpty()) {
            queryItems.append(QPair<QString, QString>(QString(QLatin1String("since_id")), sinceTweetId));
        }
        baseUrl = QString(QLatin1String("https://api.twitter.com/1.1/statuses/retweets_of_me.json"));
        url = QUrl(baseUrl);
        QUrlQuery retweetsQuery(url);
        retweetsQuery.setQueryItems(queryItems);
        url.setQuery(retweetsQuery);

        QNetworkRequest retweetsRequest(url);
        retweetsRequest.setRawHeader("Authorization", authorizationHeader(
                                     accountId, oauthToken, oauthTokenSecret,
                                     QLatin1String("GET"), baseUrl, queryItems).toLatin1());

        QNetworkReply *rreply = m_networkAccessManager->get(retweetsRequest);

        if (rreply) {
            rreply->setProperty("accountId", accountId);
            rreply->setProperty("oauthToken", oauthToken);
            rreply->setProperty("oauthTokenSecret", oauthTokenSecret);
            connect(rreply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
            connect(rreply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
            connect(rreply, SIGNAL(finished()), this, SLOT(finishedRetweetsHandler()));

            // we're requesting data.  Increment the semaphore so that we know we're still busy.
            incrementSemaphore(accountId);
            setupReplyTimeout(accountId, rreply);
        } else {
            SOCIALD_LOG_ERROR("unable to request retweet notifications from Twitter account with id" << accountId);
        }
    }

    // request followers
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("count")), QString(QLatin1String("5000"))));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("stringify_ids")), QString(QLatin1String("true"))));
    if (!followersCursor.isEmpty()) {
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("cursor")), followersCursor));
    }
    QString baseUrl(QLatin1String("https://api.twitter.com/1.1/followers/ids.json"));
    QUrl url(baseUrl);
    QUrlQuery followersQuery(url);
    followersQuery.setQueryItems(queryItems);
    url.setQuery(followersQuery);

    QNetworkRequest followersRequest(url);
    followersRequest.setRawHeader("Authorization", authorizationHeader(
                                  accountId, oauthToken, oauthTokenSecret,
                                  QLatin1String("GET"), baseUrl, queryItems).toLatin1());

    QNetworkReply *freply = m_networkAccessManager->get(followersRequest);

    if (freply) {
        freply->setProperty("accountId", accountId);
        freply->setProperty("oauthToken", oauthToken);
        freply->setProperty("oauthTokenSecret", oauthTokenSecret);
        connect(freply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(freply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(freply, SIGNAL(finished()), this, SLOT(finishedFollowersHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, freply);
    } else {
        SOCIALD_LOG_ERROR("unable to request followers from Twitter account with id" << accountId);
    }
}

void TwitterNotificationSyncAdaptor::finishedMentionsHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();

    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, ignoring request response");
        decrementSemaphore(accountId);
        return;
    }

    bool ok = false;
    QJsonArray tweets = parseJsonArrayReplyData(replyData, &ok);
    if (ok) {
        if (!tweets.size()) {
            SOCIALD_LOG_DEBUG("no mentions received for account" << accountId);
            decrementSemaphore(accountId);
            return;
        }

        int mentionsCount = 0;
        QString body;
        QString summary;
        QDateTime timestamp;
        QString link;
        foreach (const QJsonValue &tweetValue, tweets) {
            QJsonObject tweet = tweetValue.toObject();
            QDateTime createdTime = parseTwitterDateTime(tweet.value(QLatin1String("created_at")).toString());
            QString mentionId = tweet.value(QLatin1String("id_str")).toString();
            QString text = tweet.value(QLatin1String("text")).toString();
            QJsonObject user = tweet.value(QLatin1String("user")).toObject();
            QString userName = user.value(QLatin1String("name")).toString();
            QString userScreenName = user.value(QLatin1String("screen_name")).toString();

            // check to see if we need to post it to the notifications feed
            int sinceSpan = m_accountSyncProfile
                          ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("7")).toInt()
                          : 7;
            if (m_lastSyncTimestamp.isValid() && createdTime < m_lastSyncTimestamp) {
                SOCIALD_LOG_DEBUG("mention notification for account" << accountId << "came after last sync:" << createdTime << ":" << text);
                break; // all subsequent notifications will be even older.
            } else if (createdTime.daysTo(QDateTime::currentDateTimeUtc()) > sinceSpan) {
                SOCIALD_LOG_DEBUG("mention for account" << accountId << "is more than" << sinceSpan << "days old:" << createdTime << ":" << text);
            } else {
                //: Label telling the user that they have N new Twitter mentions.  Do NOT include N.  e.g. "New Mention" / "New Mentions"
                //% "%n New Mentions"
                summary = qtTrId("qtn_social_notifications_twitter_new_mentions");
                //: Label telling the user that someone mentioned them.  e.g: "John Smith mentioned you in a Tweet"
                //% "%1 mentioned you in a Tweet"
                body = qtTrId("qtn_social_notifications_twitter_mentioned_you").arg(userName);
                timestamp = createdTime;
                link = QLatin1String("https://mobile.twitter.com/") + userScreenName + QLatin1String("/status/") + mentionId;
                mentionsCount ++;
            }
        }

        if (mentionsCount > 0) {
            // Search if we already have a notification
            Notification *notification = createNotification(accountId, Mention);

            // Set properties of the notification
            notification->setItemCount(notification->itemCount() + mentionsCount);
            QStringList openUrlArgs;
            if (notification->itemCount() == 1) {
                notification->setTimestamp(timestamp);
                notification->setSummary(summary);
                notification->setBody(body);
                openUrlArgs << link;
            } else {
                notification->setTimestamp(QDateTime::currentDateTimeUtc());
                notification->setSummary(qtTrId("qtn_social_notifications_twitter_new_mentions")); // EE defined above.
                //: The number of tweets (n) by other people which mention this user. Include n.
                //% "You received %n mentions"
                notification->setBody(qtTrId("qtn_social_notifications_twitter_n_mentions_include_n", notification->itemCount()));
                openUrlArgs << QLatin1String("https://mobile.twitter.com/i/connect");
            }
            notification->setRemoteAction(OPEN_BROWSER_ACTION(openUrlArgs));
            notification->publish();
            if (notification->replacesId() == 0) {
                // failed.
                SOCIALD_LOG_ERROR("failed to publish mention notification:" <<  body);
            }
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse mention notification data from request with account" << accountId << "," <<
                          "got:" << QString::fromLatin1(replyData.constData()));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

void TwitterNotificationSyncAdaptor::finishedRetweetsHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();

    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, ignoring request response");
        decrementSemaphore(accountId);
        return;
    }

    bool ok = false;
    QJsonArray tweets = parseJsonArrayReplyData(replyData, &ok);
    if (ok) {
        if (!tweets.size()) {
            SOCIALD_LOG_DEBUG("no retweets received for account" << accountId);
            decrementSemaphore(accountId);
            return;
        }

        QString selfUserScreenName;
        QHash<QString, int> dbRetweetCounts = m_db.retweetedTweetCounts(accountId);
        QHash<QString, int> retweetCounts;
        QStringList newlyRetweetedTweets;
        int delta = 0;
        foreach (const QJsonValue &tweetValue, tweets) {
            QJsonObject tweet = tweetValue.toObject();
            selfUserScreenName = tweet.value(QLatin1String("user")).toObject().value(QLatin1String("screen_name")).toString();
            QString retweetId = tweet.value(QLatin1String("id_str")).toString();
            int retweetsCount = tweet.value(QLatin1String("retweet_count")).toInt();
            retweetCounts.insert(retweetId, retweetsCount);
            if (!dbRetweetCounts.contains(retweetId) || dbRetweetCounts.value(retweetId) < retweetsCount) {
                delta += retweetsCount - dbRetweetCounts.value(retweetId);
                newlyRetweetedTweets.append(retweetId);
            }
        }

        m_db.setRetweetedTweetCounts(accountId, retweetCounts); // won't get committed until finalize();
        if (newlyRetweetedTweets.size() > 0) {
            // Search if we already have a notification
            Notification *notification = createNotification(accountId, Retweet);

            // Set properties of the notification
            QStringList openUrlArgs;
            notification->setItemCount(newlyRetweetedTweets.size());
            notification->setTimestamp(QDateTime::currentDateTimeUtc());
            //: Label telling the user that they have N new Twitter retweets.  Do NOT include N.  e.g. "New Retweet" / "New Retweets"
            //% "%n New Retweets"
            notification->setSummary(qtTrId("qtn_social_notifications_twitter_new_retweets"));
            if (newlyRetweetedTweets.size() == 1) {
                //: This label tells the user how many times (n) a single Tweet has been retweeted.  Include n.
                //% "Your Tweet has been retweeted %n times"
                notification->setBody(qtTrId("qtn_social_notifications_twitter_1_n_retweets_include_n", delta));
                openUrlArgs << QLatin1String("https://mobile.twitter.com/") + selfUserScreenName + QLatin1String("/status/") + newlyRetweetedTweets.first();
            } else {
                //: This label tells the user how many (n) times multiple (m) Tweets have been retweeted.  Include n.  e.g. "Your Tweets have been retweeted 6 times".
                //% "Your Tweets have been retweeted %n times"
                notification->setBody(qtTrId("qtn_social_notifications_twitter_m_n_retweets_include_n", delta));
                openUrlArgs << QLatin1String("https://mobile.twitter.com/i/connect");
            }
            notification->setRemoteAction(OPEN_BROWSER_ACTION(openUrlArgs));
            notification->publish();
            if (notification->replacesId() == 0) {
                // failed.
                SOCIALD_LOG_ERROR("failed to publish retweet notification:" <<  newlyRetweetedTweets << delta);
            }
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse retweet notification data from request with account" << accountId << "," <<
                          "got:" << QString::fromLatin1(replyData.constData()));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

void TwitterNotificationSyncAdaptor::finishedFollowersHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString oauthToken = reply->property("oauthToken").toString();
    QString oauthTokenSecret = reply->property("oauthTokenSecret").toString();

    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, ignoring request response");
        decrementSemaphore(accountId);
        return;
    }

    bool ok = false;
    QJsonObject response = parseJsonObjectReplyData(replyData, &ok);
    if (ok && response.contains("ids")) {
        QJsonArray ids = response.value("ids").toArray();
        while (ids.size()) {
            m_followerIds.insert(ids.takeAt(0).toString());
        }

        // if next_cursor exists, we have more followers we need to request.
        if (response.contains("next_cursor_str") && response.value("next_cursor_str").toString() != QStringLiteral("0")) {
            requestNotifications(accountId, oauthToken, oauthTokenSecret, QString(), response.value("next_cursor_str").toString());
        } else {
            // finished requesting all followers.  now calculate the delta to database data.
            QSet<QString> dbFollowerIds = m_db.followerIds(accountId);
            QSet<QString> differenceSet = m_followerIds;
            QList<QString> newFollowers = differenceSet.subtract(dbFollowerIds).toList();
            bool needMultipleNotification = false;
            if (newFollowers.size() == 0) {
                // no new followers.  No need to raise a notification.
            } else if (newFollowers.size() == 1) {
                // exactly one new follower.  Possibly need to request detailed information.
                Notification *notification = findNotification(accountId, Follower);
                if (notification && notification->itemCount()) {
                    // don't need to request detailed information, as we have multiple new followers
                    // ie, exactly one since last sync + N from the sync before.
                    needMultipleNotification = true;
                } else {
                    // do need to request detailed information.
                    QList<QPair<QString, QString> > queryItems;
                    queryItems.append(QPair<QString, QString>(QString(QLatin1String("user_id")), newFollowers.first()));
                    QString baseUrl(QLatin1String("https://api.twitter.com/1.1/users/show.json"));
                    QUrl url(baseUrl);
                    QUrlQuery query(url);
                    query.setQueryItems(queryItems);
                    url.setQuery(query);

                    QNetworkRequest showRequest(url);
                    showRequest.setRawHeader("Authorization", authorizationHeader(
                                             accountId, oauthToken, oauthTokenSecret,
                                             QLatin1String("GET"), baseUrl, queryItems).toLatin1());

                    QNetworkReply *sreply = m_networkAccessManager->get(showRequest);

                    if (sreply) {
                        sreply->setProperty("accountId", accountId);
                        sreply->setProperty("oauthToken", oauthToken);
                        sreply->setProperty("oauthTokenSecret", oauthTokenSecret);
                        connect(sreply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
                        connect(sreply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
                        connect(sreply, SIGNAL(finished()), this, SLOT(finishedUserShowHandler()));

                        // we're requesting data.  Increment the semaphore so that we know we're still busy.
                        incrementSemaphore(accountId);
                        setupReplyTimeout(accountId, sreply);
                    } else {
                        SOCIALD_LOG_ERROR("unable to request user information from Twitter account with id" << accountId);
                    }
                }
            } else {
                // multiple new followers.
                needMultipleNotification = true;
            }

            if (needMultipleNotification) {
                Notification *notification = createNotification(accountId, Follower);
                notification->setItemCount(notification->itemCount() + newFollowers.size());
                notification->setTimestamp(QDateTime::currentDateTimeUtc());
                //: Label telling the user that they have N new Twitter followers.  Do NOT include N.  e.g. "New Follower" / "New Followers"
                //% "%n New Followers"
                notification->setSummary(qtTrId("qtn_social_notifications_twitter_new_followers"));
                //: The number of new followers (n) the user has on Twitter.  Include n.  e.g. "You have 5 new followers".
                //% "You have %n new followers"
                notification->setBody(qtTrId("qtn_social_notifications_n_followers_include_n", notification->itemCount()));
                QStringList openUrlArgs;
                openUrlArgs << QLatin1String("https://mobile.twitter.com/i/connect");
                notification->setRemoteAction(OPEN_BROWSER_ACTION(openUrlArgs));
                notification->publish();
                if (notification->replacesId() == 0) {
                    // failed.
                    SOCIALD_LOG_ERROR("failed to publish followers notification:" <<  notification->itemCount());
                }
            }

            // now update our database.  Note that this doesn't get synced until finalize().
            m_db.setFollowerIds(accountId, m_followerIds);
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse mention notification data from request with account" << accountId << "," <<
                          "got:" << QString::fromLatin1(replyData.constData()));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

void TwitterNotificationSyncAdaptor::finishedUserShowHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();

    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, ignoring request response");
        decrementSemaphore(accountId);
        return;
    }

    bool ok = false;
    QJsonObject response = parseJsonObjectReplyData(replyData, &ok);
    if (ok && (response.contains("name") || response.contains("screen_name"))) {
        QString name = response.value("name").toString();
        QString screenName = response.value("screen_name").toString();
        Notification *notification = createNotification(accountId, Retweet);
        notification->setItemCount(1);
        notification->setTimestamp(QDateTime::currentDateTimeUtc());
        notification->setSummary(qtTrId("qtn_social_notifications_twitter_new_followers")); // EE defined above.
        //: Text telling the user that another user has followed them, e.g.: "John Smith Followed you".
        //% "%1 followed you"
        notification->setBody(qtTrId("qtn_social_notifications_twitter_followed_you").arg(name.isEmpty() ? screenName : name));
        QStringList openUrlArgs;
        openUrlArgs << QLatin1String("https://mobile.twitter.com/i/connect");
        notification->setRemoteAction(OPEN_BROWSER_ACTION(openUrlArgs));
        notification->publish();
        if (notification->replacesId() == 0) {
            // failed.
            SOCIALD_LOG_ERROR("failed to publish follower notification:" <<  name << screenName);
        }
    } else {
        SOCIALD_LOG_ERROR("unable to parse user information response:" << replyData);
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

Notification *TwitterNotificationSyncAdaptor::createNotification(int accountId, TwitterNotificationType ntype)
{
    Notification *notification = findNotification(accountId, ntype);
    if (notification) {
        return notification;
    }

    notification = new Notification(this);
    //% "Twitter"
    notification->setAppName(qtTrId("qtn_social_notifications_twitter"));
    notification->setHintValue("x-nemo.sociald.account-id", accountId);
    if (ntype == TwitterNotificationSyncAdaptor::Mention) {
        notification->setCategory(QLatin1String("x-nemo.social.twitter.mention"));
    } else if (ntype == TwitterNotificationSyncAdaptor::Retweet) {
        notification->setCategory(QLatin1String("x-nemo.social.twitter.retweet"));
    } else {
        notification->setCategory(QLatin1String("x-nemo.social.twitter.follower"));
    }

    return notification;
}

Notification * TwitterNotificationSyncAdaptor::findNotification(int accountId, TwitterNotificationType ntype)
{
    QString ntypeCategory;
    if (ntype == TwitterNotificationSyncAdaptor::Mention) {
        ntypeCategory = QLatin1String("x-nemo.social.twitter.mention");
    } else if (ntype == TwitterNotificationSyncAdaptor::Retweet) {
        ntypeCategory = QLatin1String("x-nemo.social.twitter.retweet");
    } else {
        ntypeCategory = QLatin1String("x-nemo.social.twitter.follower");
    }

    Notification *notification = 0;
    QList<QObject *> notifications = Notification::notifications();
    foreach (QObject *object, notifications) {
        Notification *castedNotification = static_cast<Notification *>(object);
        if (castedNotification->category() == ntypeCategory
            && castedNotification->hintValue("x-nemo.sociald.account-id").toInt() == accountId) {
            notification = castedNotification;
            break;
        }
    }

    if (notification) {
        notifications.removeAll(notification);
    }

    qDeleteAll(notifications);

    return notification;
}
