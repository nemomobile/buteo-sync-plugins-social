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

//QtMobility
#include <QtContacts/QContactManager>
#include <QtContacts/QContactFetchHint>
#include <QtContacts/QContactFetchRequest>
#include <QtContacts/QContact>
#include <QtContacts/QContactName>
#include <QtContacts/QContactNickname>
#include <QtContacts/QContactPresence>
#include <QtContacts/QContactAvatar>

//meegotouchevents/meventfeed
#include <meventfeed.h>

// sailfish-components-accounts-qt5
#include <sailfishkeyprovider.h>

#define SOCIALD_TWITTER_POSTS_ID_PREFIX QLatin1String("twitter-posts-")
#define SOCIALD_TWITTER_POSTS_GROUPNAME QLatin1String("twitter")
#define QTCONTACTS_SQLITE_AVATAR_METADATA QLatin1String("AvatarMetadata")

static QString storedSecret(const char *key)
{
    char *cKey = NULL;
    int success = SailfishKeyProvider_storedKey("twitter", "twitter-sync", key, &cKey);
    if (success != 0) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("Twitter sync: could not retrieve key from SailfishKeyProvider")));
        free(cKey);
        return QString();
    }

    QString retn = QLatin1String(cKey);
    free(cKey);
    return retn;
}

// currently, we integrate with the device events feed via libeventfeed / meegotouchevents' meventfeed.

TwitterHomeTimelineSyncAdaptor::TwitterHomeTimelineSyncAdaptor(SyncService *syncService, QObject *parent)
    : TwitterDataTypeSyncAdaptor(syncService, SyncService::Posts, parent)
    , m_contactFetchRequest(new QContactFetchRequest(this))
    , m_eventFeed(MEventFeed::instance())
{
    if (!m_eventFeed) {
        m_enabled = false;
        return; // can't sync to the local device's event feed, so not enabled.
    }

    // fetch all contacts.  We detect which contact a event came from.
    // XXX TODO: we really shouldn't do this, we should do it on demand instead
    // of holding the contacts in memory.
    if (m_contactFetchRequest) {
        QContactFetchHint cfh;
        cfh.setOptimizationHints(QContactFetchHint::NoRelationships | QContactFetchHint::NoActionPreferences | QContactFetchHint::NoBinaryBlobs);
        cfh.setDetailTypesHint(QList<QContactDetail::DetailType>()
                               << QContactDetail::TypeAvatar
                               << QContactDetail::TypeName
                               << QContactDetail::TypeNickname
                               << QContactDetail::TypePresence);
        m_contactFetchRequest->setFetchHint(cfh);
        m_contactFetchRequest->setManager(&m_contactManager);
        connect(m_contactFetchRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)), this, SLOT(contactFetchStateChangedHandler(QContactAbstractRequest::State)));
        m_contactFetchRequest->start();
    }

    // can sync, enabled
    m_enabled = true;
    m_status = SocialNetworkSyncAdaptor::Inactive;
}

TwitterHomeTimelineSyncAdaptor::~TwitterHomeTimelineSyncAdaptor()
{
}

void TwitterHomeTimelineSyncAdaptor::sync(const QString &dataType)
{
    // refresh local cache of contacts.
    // we do this asynchronous request in parallel to the sync code below
    // since the network request round-trip times should far exceed the
    // local database fetch.  If not, then the current sync run will
    // still work, but the "post is from which contact" detection
    // will be using slightly stale data.
    if (m_contactFetchRequest &&
            (m_contactFetchRequest->state() == QContactAbstractRequest::InactiveState ||
             m_contactFetchRequest->state() == QContactAbstractRequest::FinishedState)) {
        m_contactFetchRequest->start();
    }

    // call superclass impl.
    TwitterDataTypeSyncAdaptor::sync(dataType);
}

void TwitterHomeTimelineSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // first, purge all data from nemo events
        QStringList purgeDataIds = syncedDatumLocalIdentifiers(QLatin1String("twitter"),
                SyncService::dataType(SyncService::Posts),
                QString::number(pid));

        bool ok = true;
        int prefixLen = QString(SOCIALD_TWITTER_POSTS_ID_PREFIX).size();
        foreach (const QString &pdi, purgeDataIds) {
            QString eventIdStr = pdi.mid(prefixLen); // pdi is of form: "twitter-posts-EVENTID"
            qlonglong eventId = eventIdStr.toLongLong(&ok);
            if (ok) {
                m_eventFeed->removeItem(eventId);
            } else {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to convert event id string to int: %1"))
                        .arg(pdi));
            }
        }

        // second, purge all data from our database
        removeAllData(QLatin1String("twitter"),
                SyncService::dataType(SyncService::Posts),
                QString::number(pid)); // XXX TODO: use fb id instead of QString::number(accountId)
    }
}

void TwitterHomeTimelineSyncAdaptor::beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret)
{
    requestMe(accountId, oauthToken, oauthTokenSecret);
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
    QNetworkReply *reply = m_qnam->get(nreq);
    
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

void TwitterHomeTimelineSyncAdaptor::requestPosts(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &sinceTweetId, const QString &fromUserId)
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
    QNetworkReply *reply = m_qnam->get(nreq);
    
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
    QVariantMap parsed = TwitterDataTypeSyncAdaptor::parseReplyData(replyData, &ok).toMap();
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
    QString accessToken = reply->property("accessToken").toString();
    QString selfUserId = reply->property("selfUserId").toString();
    QDateTime lastSync = lastSyncTimestamp(QLatin1String("twitter"), SyncService::dataType(SyncService::Posts), QString::number(accountId));
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariant parsed = TwitterDataTypeSyncAdaptor::parseReplyData(replyData, &ok);
    if (ok && parsed.type() == QVariant::List) {
        QVariantList data = parsed.toList();
        if (!data.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no feed posts received for account %1"))
                    .arg(accountId));
            decrementSemaphore(accountId);
            return;
        }

        bool needMorePages = true;
        bool postedNew = false;
        for (int i = 0; i < data.size(); ++i) {
            // these are the fields we eventually need to fill out:
            QString eventTitle;
            QString eventBody;
            QStringList eventImageList;
            QDateTime eventTimestamp;
            QString eventFooter;
            bool eventIsVideo;
            QString eventUrl;

            QString retweeter;

            // grab the data from the current post
            QVariantMap currData = data.at(i).toMap();

            if (currData.contains(QLatin1String("retweeted_status"))) {
                retweeter = currData.value(QLatin1String("user")).toMap().value("name").toString();
                currData = currData.value(QLatin1String("retweeted_status")).toMap();
            }

            QDateTime createdTime = parseTwitterDateTime(currData.value(QLatin1String("created_at")).toString());
            QString postId = currData.value(QLatin1String("id_str")).toString();
            QString text = currData.value(QLatin1String("text")).toString();
            QVariantMap dataUser = currData.value(QLatin1String("user")).toMap();
            QString userId = dataUser.value("id_str").toString();
            QString userName = dataUser.value("name").toString();
            QString screenName = dataUser.value("screen_name").toString();
            QString icon = dataUser.value(QLatin1String("profile_image_url")).toString();

            QVariantList mediaList = currData.value(QLatin1String("entities")).toMap().value(QLatin1String("media")).toList();
            if (!mediaList.isEmpty()) {
                foreach (QVariant mediaVariant, mediaList) {
                    QVariantMap mediaObject = mediaVariant.toMap();
                    if (mediaObject.contains(QLatin1String("media_url_https"))) {
                        eventImageList.append(mediaObject.value(QLatin1String("media_url_https")).toString());
                    }
                }
            }

            eventTitle = userName;
            eventBody = text;
            eventTimestamp = createdTime;
            eventIsVideo = false; // XXX TODO: Twitter Vine posts?
            eventUrl = QLatin1String("https://twitter.com/") + screenName + QLatin1String("/status/") + postId;

            // check to see if we need to post it to the events feed
            if (lastSync.isValid() && createdTime < lastSync) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("event for account %1 came after last sync:"))
                        .arg(accountId) << "    " << createdTime << ":" << eventBody);
                needMorePages = false; // don't fetch more pages of results.
                break;                 // all subsequent events will be even older.
            } else if (createdTime.daysTo(QDateTime::currentDateTime()) > 7) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("event for account %1 is more than a week old:\n"))
                        .arg(accountId) << "    " << createdTime << ":" << eventBody);
                needMorePages = false; // don't fetch more pages of results.
                break;                 // all subsequent events will be even older.
            } else if (haveAlreadyPostedEvent(postId, eventBody, createdTime)) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("event for account %1 has already been posted:\n"))
                        .arg(accountId) << "    " << createdTime << ":" << eventBody);
            } else {
                QVariantMap metaData;
                metaData.insert("accountId", accountId);
                metaData.insert("consumerKey", storedSecret("consumer_key"));
                metaData.insert("consumerSecret", storedSecret("consumer_secret"));
                metaData.insert("nodeId", postId);
                metaData.insert("retweeter", retweeter);
                metaData.insert("profilePicture", m_accountProfileImage.value(accountId));


                // publish the post to the events feed.
                qlonglong eventId = m_eventFeed->addItem(
                        icon,
                        //QLatin1String("icon-s-service-twitter"),
                        eventTitle,
                        eventBody,
                        eventImageList,
                        createdTime,
                        eventFooter,
                        eventIsVideo,
                        eventUrl,
                        SOCIALD_TWITTER_POSTS_GROUPNAME, // sourceName
                        QLatin1String("Twitter"), // sourceDisplayName // XXX TODO: per-account?
                        metaData);
                if (eventId == 0) {
                    // failed.
                    TRACE(SOCIALD_ERROR,
                            QString(QLatin1String("error: failed to publish post/feed event: %1"))
                            .arg(eventBody));
                } else {
                    // and store the fact that we have synced it to the events feed.
                    markSyncedDatum(QString(QLatin1String("twitter-posts-%1")).arg(QString::number(eventId)),
                                    QLatin1String("twitter"), SyncService::dataType(SyncService::Posts),
                                    QString::number(accountId), createdTime, QDateTime::currentDateTime(),
                                    postId);
                }

                // if we didn't post anything new, we don't try to fetch more.
                postedNew = true;
            }
        }

        if (needMorePages && postedNew) {
            // XXX TODO: paging?
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

void TwitterHomeTimelineSyncAdaptor::contactFetchStateChangedHandler(QContactAbstractRequest::State newState)
{
    // update our local cache of contacts.
    if (m_contactFetchRequest && newState == QContactAbstractRequest::FinishedState) {
        m_contacts = m_contactFetchRequest->contacts();
        m_selfContact = m_contactManager.contact(m_contactManager.selfContactId());
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("finished refreshing local cache of contacts, have %1"))
                .arg(m_contacts.size()));
    }
}

bool TwitterHomeTimelineSyncAdaptor::fromIsSelfContact(const QString &fromName, const QString &fromTwUid) const
{
    // XXX TODO: look this up from QtContacts database instead (saves one request round trip time)
    if (m_selfTuids.contains(fromTwUid)) {
        return true;
    }

    // fall back to heuristic matching.
    QStringList firstAndLast = fromName.split(' '); // TODO: better detection of FN/LN
    QContactName scn = m_selfContact.detail<QContactName>();
    if ((!fromName.isEmpty() && scn.value<QString>(QContactName__FieldCustomLabel) == fromName) ||
            (firstAndLast.size() >= 2 &&
             scn.firstName() == firstAndLast.at(0) &&
             scn.lastName() == firstAndLast.at(firstAndLast.size()-1))) {
        return true;
    }

    QList<QContactNickname> nicknames = m_selfContact.details<QContactNickname>();
    foreach (const QContactNickname &n, nicknames) {
        if (!fromName.isEmpty() && n.nickname() == fromName) {
            return true;
        }
    }

    QList<QContactPresence> presences = m_selfContact.details<QContactPresence>();
    foreach (const QContactPresence &p, presences) {
        if (!fromName.isEmpty() && p.nickname() == fromName) {
            return true;
        }
    }

    // not the self contact.
    return false;
}

bool TwitterHomeTimelineSyncAdaptor::haveAlreadyPostedEvent(const QString &postId, const QString &eventBody, const QDateTime &createdTime)
{
    Q_UNUSED(eventBody);
    Q_UNUSED(createdTime);

    return (whenSyncedDatum(QLatin1String("twitter"), postId).isValid());
}

void TwitterHomeTimelineSyncAdaptor::incrementSemaphore(int accountId)
{
    int semaphoreValue = m_accountSyncSemaphores.value(accountId);
    semaphoreValue += 1;
    m_accountSyncSemaphores.insert(accountId, semaphoreValue);
    TRACE(SOCIALD_DEBUG, QString(QLatin1String("incremented busy semaphore for account %1 to %2")).arg(accountId).arg(semaphoreValue));

    if (m_status == SocialNetworkSyncAdaptor::Inactive) {
        changeStatus(SocialNetworkSyncAdaptor::Busy);
    }
}

void TwitterHomeTimelineSyncAdaptor::decrementSemaphore(int accountId)
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
        // finished all outstanding requests for Posts sync for this account.
        // update the sync time for this user's Posts in the global sociald database.
        updateLastSyncTimestamp(QLatin1String("twitter"),
                                SyncService::dataType(SyncService::Posts),
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
            TRACE(SOCIALD_INFORMATION, QString(QLatin1String("Finished Twitter Posts sync at: %1"))
                                       .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
            changeStatus(SocialNetworkSyncAdaptor::Inactive);
        }
    }
}
