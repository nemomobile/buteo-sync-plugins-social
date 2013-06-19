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

#include "facebookpostsyncadaptor.h"
#include "syncservice.h"
#include "trace.h"

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

#define SOCIALD_FACEBOOK_POSTS_ID_PREFIX QLatin1String("facebook-posts-")
#define SOCIALD_FACEBOOK_POSTS_GROUPNAME QLatin1String("sociald-sync-facebook-posts")
#define QTCONTACTS_SQLITE_AVATAR_METADATA QLatin1String("AvatarMetadata")

// currently, we integrate with the device events feed via libeventfeed / meegotouchevents' meventfeed.

FacebookPostSyncAdaptor::FacebookPostSyncAdaptor(SyncService *parent)
    : FacebookDataTypeSyncAdaptor(parent, SyncService::Posts)
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
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
        cfh.setDetailDefinitionsHint(QStringList()
                                     << QContactAvatar::DefinitionName
                                     << QContactName::DefinitionName
                                     << QContactNickname::DefinitionName
                                     << QContactPresence::DefinitionName);
#else
        cfh.setDetailTypesHint(QList<QContactDetail::DetailType>()
                               << QContactDetail::TypeAvatar
                               << QContactDetail::TypeName
                               << QContactDetail::TypeNickname
                               << QContactDetail::TypePresence);
#endif
        m_contactFetchRequest->setFetchHint(cfh);
        m_contactFetchRequest->setManager(&m_contactManager);
        connect(m_contactFetchRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)), this, SLOT(contactFetchStateChangedHandler(QContactAbstractRequest::State)));
        m_contactFetchRequest->start();
    }

    // can sync, enabled
    m_enabled = true;
    m_status = SocialNetworkSyncAdaptor::Inactive;
}

FacebookPostSyncAdaptor::~FacebookPostSyncAdaptor()
{
}

void FacebookPostSyncAdaptor::sync(const QString &dataType)
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
    FacebookDataTypeSyncAdaptor::sync(dataType);
}

void FacebookPostSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // first, purge all data from nemo events
        QStringList purgeDataIds = syncedDatumLocalIdentifiers(QLatin1String("facebook"),
                SyncService::dataType(SyncService::Posts),
                QString::number(pid));

        bool ok = true;
        int prefixLen = QString(SOCIALD_FACEBOOK_POSTS_ID_PREFIX).size();
        foreach (const QString &pdi, purgeDataIds) {
            QString eventIdStr = pdi.mid(prefixLen); // pdi is of form: "facebook-posts-EVENTID"
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
        removeAllData(QLatin1String("facebook"),
                SyncService::dataType(SyncService::Posts),
                QString::number(pid)); // XXX TODO: use fb id instead of QString::number(accountId)
    }
}

void FacebookPostSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    requestMe(accountId, accessToken);
}

void FacebookPostSyncAdaptor::requestMe(int accountId, const QString &accessToken)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("fields")), QLatin1String("id")));
    QUrl url(QLatin1String("https://graph.facebook.com/me"));
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    url.setQueryItems(queryItems);
#else
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
#endif
    QNetworkReply *reply = m_qnam->get(QNetworkRequest(url));
    
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedMeHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request feed posts from Facebook account with id %1"))
                .arg(accountId));
    }
}

void FacebookPostSyncAdaptor::requestPosts(int accountId, const QString &accessToken, const QString &until, const QString &pagingToken)
{
    // TODO: continuation requests need these two.  if exists, also set limit = 5000.
    // if not set, set "since" to the timestamp value.
    Q_UNUSED(until);
    Q_UNUSED(pagingToken);

    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    QUrl url(QLatin1String("https://graph.facebook.com/me/home"));
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    url.setQueryItems(queryItems);
#else
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
#endif
    QNetworkReply *reply = m_qnam->get(QNetworkRequest(url));
    
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedPostsHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request home posts from Facebook account with id %1"))
                .arg(accountId));
    }
}

void FacebookPostSyncAdaptor::finishedMeHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariantMap parsed = FacebookDataTypeSyncAdaptor::parseReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("id"))) {
        QString selfUserId = parsed.value(QLatin1String("id")).toString();
        if (!m_selfFbuids.contains(selfUserId)) {
            m_selfFbuids.append(selfUserId);
        }

        requestPosts(accountId, accessToken);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse self user id from me request for account %1"))
                .arg(accountId));
    }

    decrementSemaphore(accountId);
}

void FacebookPostSyncAdaptor::finishedPostsHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QDateTime lastSync = lastSyncTimestamp(QLatin1String("facebook"), SyncService::dataType(SyncService::Posts), QString::number(accountId));
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariantMap parsed = FacebookDataTypeSyncAdaptor::parseReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("data"))) {
        // we expect "data" and possible "paging"
        QVariantList data = parsed.value(QLatin1String("data")).toList();
        QVariantMap paging = parsed.value(QLatin1String("paging")).toMap(); // may not exist.

        if (!data.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no home posts received for account %1"))
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

            // grab the data from the current post
            QVariantMap currData = data.at(i).toMap();
            QDateTime createdTime = QDateTime::fromString(currData.value(QLatin1String("created_time")).toString(), Qt::ISODate);
            QDateTime updatedTime = QDateTime::fromString(currData.value(QLatin1String("updated_time")).toString(), Qt::ISODate);
            createdTime.setTimeSpec(Qt::UTC);
            updatedTime.setTimeSpec(Qt::UTC);
            QString postId = currData.value(QLatin1String("id")).toString();
            QString postType = currData.value(QLatin1String("type")).toString();
            QString statusType = currData.value(QLatin1String("status_type")).toString(); // won't always exist.
            QVariantList actions = currData.value(QLatin1String("actions")).toList();
            QString actionLink;
            if (actions.size()) {
                actionLink = actions.at(0).toMap().value(QLatin1String("link")).toString();
            }

            // this is usually the current user, but can be another user if they've posted on your wall.
            QString fromName = currData.value(QLatin1String("from")).toMap().value("name").toString();
            QString fromFbUid = currData.value(QLatin1String("from")).toMap().value("id").toString();
            bool fromSelfContact = fromIsSelfContact(fromName, fromFbUid);

            if (postType == QLatin1String("photo") && statusType == QLatin1String("shared_story")) {
                // this is a photo shared by the current user
                QString message = currData.value(QLatin1String("message")).toString();
                QString link = currData.value(QLatin1String("link")).toString();
                QString picture = currData.value(QLatin1String("picture")).toString();

                // build the event fields
                if (fromSelfContact) {
                    //: Title of Facebook post in event feed where the device's user posted a photo
                    //% "You posted a photo"
                    eventTitle = qtTrId("sociald_facebook_posts-you_posted_photo");
                } else {
                    //: Title of Facebook post in event feed where a friend posted a photo
                    //% "%1 posted a photo"
                    eventTitle = qtTrId("sociald_facebook_posts-friend_posted_photo").arg(fromName);
                }
                eventBody = message; // XXX TODO: or should this be the link / source?  libeventfeed is ... bad.
                eventImageList << picture;
                eventTimestamp = createdTime;
                eventIsVideo = false;
                eventUrl = actionLink.isEmpty() ? link : actionLink;
            } else if (postType == QLatin1String("video") && statusType == QLatin1String("shared_story")) {
                // this is a video link shared by the current user
                QString message = currData.value(QLatin1String("message")).toString(); // the post message
                QString link = currData.value(QLatin1String("link")).toString(); // link (if any) included in the post (to the video)
                QString source = currData.value(QLatin1String("source")).toString(); // sourceurl embedded into the feed
                QString description = currData.value(QLatin1String("description")).toString(); // eg, the youtube description
                QString picture = currData.value(QLatin1String("picture")).toString();

                // build the event fields
                if (fromSelfContact) {
                    //: Title of Facebook post in event feed where the device's user posted a video
                    //% "You posted a video"
                    eventTitle = qtTrId("sociald_facebook_posts-you_posted_video");
                } else {
                    //: Title of Facebook post in event feed where a friend posted a video
                    //% "%1 posted a video"
                    eventTitle = qtTrId("sociald_facebook_posts-friend_posted_video").arg(fromName);
                }
                eventBody = message; // XXX TODO: or should this be the link / source?  libeventfeed is ... bad.
                eventImageList << picture;
                eventTimestamp = createdTime;
                eventIsVideo = true;
                eventUrl = actionLink.isEmpty() ? (source.isEmpty() ? link : source) : actionLink;
            } else if (postType == QLatin1String("link") && statusType == QLatin1String("shared_story")) {
                // this is a website/link that is shared by the user.
                QString message = currData.value(QLatin1String("message")).toString();
                QString link = currData.value(QLatin1String("link")).toString();
                QString picture = currData.value(QLatin1String("picture")).toString();

                // build the event fields
                if (fromSelfContact) {
                    //: Title of Facebook post in event feed where the device's user posted a link
                    //% "You posted a link"
                    eventTitle = qtTrId("sociald_facebook_posts-you_posted_link");
                } else {
                    //: Title of Facebook post in event feed where a friend posted a link
                    //% "%1 posted a link"
                    eventTitle = qtTrId("sociald_facebook_posts-friend_posted_link").arg(fromName);
                }
                eventBody = message; // XXX TODO: or should this be the link / source?  libeventfeed is ... bad.
                eventImageList << picture;
                eventTimestamp = createdTime;
                eventIsVideo = false;
                eventUrl = actionLink.isEmpty() ? link : actionLink;
            } else if (postType == QLatin1String("photo") && (statusType == QLatin1String("added_photos") || statusType == QLatin1String("mobile_status_update"))) {
                // this is a photo or album uploaded by the current user
                QString message = currData.value(QLatin1String("message")).toString();
                QString picture = currData.value(QLatin1String("picture")).toString();
                QString link = currData.value(QLatin1String("link")).toString();
                bool moreThanOnePhoto = false;
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
                QUrl linkUrl(link);
                QString relevantCount = linkUrl.queryItemValue(QLatin1String("relevant_count"));
#else
                QString relevantCount = QUrlQuery(link).queryItemValue(QLatin1String("relevant_count"));
#endif
                if (!relevantCount.isEmpty() && relevantCount.toInt() > 1) {
                    moreThanOnePhoto = true;
                }

                // build the event fields
                if (fromSelfContact) {
                    if (moreThanOnePhoto) {
                        //: Title of Facebook post in event feed where the device's user uploaded multiple photos
                        //% "You uploaded photos"
                        eventTitle = qtTrId("sociald_facebook_posts-you_uploaded_photos");
                    } else {
                        //: Title of Facebook post in event feed where the device's user uploaded a photo
                        //% "You uploaded a photo"
                        eventTitle = qtTrId("sociald_facebook_posts-you_uploaded_photo");
                    }
                } else {
                    if (moreThanOnePhoto) {
                        //: Title of Facebook post in event feed where a friend uploaded multiple photos
                        //% "%1 uploaded photos"
                        eventTitle = qtTrId("sociald_facebook_posts-friend_uploaded_photos").arg(fromName);
                    } else {
                        //: Title of Facebook post in event feed where a friend uploaded a photo (or album)
                        //% "%1 uploaded a photo"
                        eventTitle = qtTrId("sociald_facebook_posts-friend_uploaded_photo").arg(fromName);
                    }
                }
                eventBody = message; // XXX TODO: or should this be the link / source?  libeventfeed is ... bad.
                eventImageList << picture;
                eventTimestamp = createdTime;
                eventIsVideo = false;
                eventUrl = actionLink.isEmpty() ? link : actionLink;
            } else if (postType == QLatin1String("status") && statusType == QLatin1String("approved_friend")) {
                // this is an approved friend request
                QString story = currData.value(QLatin1String("story")).toString();

                // build the event fields
                //: Title of Facebook post in event feed where the device's user has a new friend
                //% "You have a new friend"
                eventTitle = qtTrId("sociald_facebook_posts-approved_friend");
                eventBody = story;
                eventTimestamp = createdTime;
                eventIsVideo = false;
                eventUrl = actionLink;
            } else if (postType == QLatin1String("status") && statusType == QLatin1String("wall_post")) {
                // this is a post on someone elses wall (eg, "happy birthday!")
                QString story = currData.value(QLatin1String("story")).toString();

                // build the event fields
                if (fromSelfContact) {
                    //: Title of Facebook post in event feed where the device's user posted on a friend's wall
                    //% "You posted on someone's wall"
                    eventTitle = qtTrId("sociald_facebook_posts-you_posted_wall");
                } else {
                    //: Title of Facebook post in event feed where a friend posted on the device's user's wall
                    //% "%1 posted on your wall"
                    eventTitle = qtTrId("sociald_facebook_posts-friend_posted_wall").arg(fromName);
                };
                eventBody = story;
                eventTimestamp = createdTime;
                eventIsVideo = false;
                eventUrl = actionLink;
            } else if (postType == QLatin1String("status") && statusType == QLatin1String("mobile_status_update")) {
                // this is a status posted by someone from a mobile phone
                QString story = currData.value(QLatin1String("story")).toString();
                QString message = currData.value(QLatin1String("message")).toString();

                // build the event fields
                if (fromSelfContact) {
                    //: Title of Facebook status in event feed where the device's user posted a mobile status update
                    //% "You posted a status update"
                    eventTitle = qtTrId("sociald_facebook_posts-you_posted_status");
                } else {
                    //: Title of Facebook status in event feed where a friend posted a mobile status update
                    //% "%1 posted a status update"
                    eventTitle = qtTrId("sociald_facebook_posts-friend_posted_status").arg(fromName);
                };
                eventBody = message.isEmpty() ? story : message;
                eventTimestamp = createdTime;
                eventIsVideo = false;
                eventUrl = actionLink;
            } else if (postType == QLatin1String("status") && statusType.isEmpty()) {
                // this is a comment/like on a status update, or on an application-specific post
                QString story = currData.value(QLatin1String("story")).toString();
                QString message = currData.value(QLatin1String("message")).toString();

                // build the event fields
                if (fromSelfContact) {
                    if (story == QString(QLatin1String("%1 likes a status.")).arg(fromName)) {
                        //: Title of Facebook post in event feed where the device's user liked a status
                        //% "You liked a status"
                        eventTitle = qtTrId("sociald_facebook_posts-you_liked_status");
                    } else if (story.startsWith(QString(QLatin1String("%1 posted a link ")).arg(fromName))) {
                        //: Title of Facebook post in event feed where the device's user posted a link on a friend's wall
                        //% "You posted a link"
                        eventTitle = qtTrId("sociald_facebook_posts-you_posted_link_wall");
                    } else { // it's probably a comment on another user's status
                        //: Title of Facebook post in event feed where the device's user commented on a status
                        //% "You added a comment"
                        eventTitle = qtTrId("sociald_facebook_posts-you_added_comment");
                    }
                } else {
                    eventTitle = story; // we can't build a meaningful title... use the story text.
                }

                eventBody = message.isEmpty() ? story : message;
                eventTimestamp = createdTime;
                eventIsVideo = false;
                eventUrl = actionLink; // this is empty for some things (eg likes on other peoples' statuses)
                                       // which is not very useful, but the Facebook graph api is terrible.
            } else if (postType == QLatin1String("link") && statusType.isEmpty()) {
                // this is a link someone else posted to your wall.
                QString message = currData.value(QLatin1String("message")).toString();
                QString link = currData.value(QLatin1String("link")).toString();
                QString picture = currData.value(QLatin1String("picture")).toString();

                // build the event fields
                if (fromSelfContact) { // this path shouldn't ever be hit, but I don't trust the FB API
                    //: Title of Facebook post in event feed where the device's user posted a link somewhere other than their own wall
                    //% "You posted a link"
                    eventTitle = qtTrId("sociald_facebook_posts-you_posted_link_other");
                } else {
                    //: Title of Facebook post in event feed where a friend posted a link on the device's user's wall
                    //% "%1 posted on your wall"
                    eventTitle = qtTrId("sociald_facebook_posts-friend_posted_link_other").arg(fromName);
                };
                eventBody = message;
                eventTimestamp = createdTime;
                eventImageList << picture;
                eventIsVideo = false;
                eventUrl = link;
            } else {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("unknown feed post type: %1, status_type: %2 for account: %3"))
                        .arg(postType).arg(statusType).arg(accountId));
                continue;
            }

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
                // publish the post to the events feed.
                qlonglong eventId = m_eventFeed->addItem(
                        QLatin1String("icon-s-service-facebook"),
                        eventTitle,
                        eventBody,
                        eventImageList,
                        createdTime,
                        eventFooter,
                        eventIsVideo,
                        eventUrl,
                        SOCIALD_FACEBOOK_POSTS_GROUPNAME, // sourceName
                        QLatin1String("Facebook"));       // sourceDisplayName // XXX TODO: per-account?
                if (eventId == 0) {
                    // failed.
                    TRACE(SOCIALD_ERROR,
                            QString(QLatin1String("error: failed to publish post/feed event: %1"))
                            .arg(eventBody));
                } else {
                    // and store the fact that we have synced it to the events feed.
                    markSyncedDatum(QString(QLatin1String("facebook-posts-%1")).arg(QString::number(eventId)),
                                    QLatin1String("facebook"), SyncService::dataType(SyncService::Posts),
                                    QString::number(accountId), createdTime, QDateTime::currentDateTime(),
                                    postId); // XXX TODO: instead of QString::number(accountId) use fb user id.
                }

                // if we didn't post anything new, we don't try to fetch more.
                postedNew = true;
            }
        }

        // paging if we need to retrieve more feed events
        if (postedNew && needMorePages && (paging.contains("previous") || paging.contains("next"))) {
            QString until;
            QString pagingToken;
            // The FB api is terrible, and so we don't know in advance which paging url
            // to use (as it will change depending on whether the current request was
            // a first request, or itself a paging request).
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
            QUrl prevUrl(paging.value("previous").toString());
            QUrl nextUrl(paging.value("next").toString());
            if (prevUrl.hasQueryItem(QLatin1String("until"))) {
                until = prevUrl.queryItemValue(QLatin1String("until"));
                pagingToken = prevUrl.queryItemValue(QLatin1String("__paging_token"));
            } else {
                until = nextUrl.queryItemValue(QLatin1String("until"));
                pagingToken = nextUrl.queryItemValue(QLatin1String("__paging_token"));
            }
#else
            QUrlQuery prevUrl(paging.value("previous").toString());
            QUrlQuery nextUrl(paging.value("next").toString());
            if (prevUrl.hasQueryItem(QLatin1String("until"))) {
                until = prevUrl.queryItemValue(QLatin1String("until"));
                pagingToken = prevUrl.queryItemValue(QLatin1String("__paging_token"));
            } else {
                until = nextUrl.queryItemValue(QLatin1String("until"));
                pagingToken = nextUrl.queryItemValue(QLatin1String("__paging_token"));
            }
#endif

            // request the next page of results.
            requestPosts(accountId, accessToken, until, pagingToken);
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

void FacebookPostSyncAdaptor::contactFetchStateChangedHandler(QContactAbstractRequest::State newState)
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

bool FacebookPostSyncAdaptor::fromIsSelfContact(const QString &fromName, const QString &fromFbUid) const
{
    // XXX TODO: look this up from QtContacts database instead (saves one request round trip time)
    if (m_selfFbuids.contains(fromFbUid)) {
        return true;
    }

    // fall back to heuristic matching.
    QStringList firstAndLast = fromName.split(' '); // TODO: better detection of FN/LN
    QContactName scn = m_selfContact.detail<QContactName>();
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    // TODO: no customLabel() method
    if ((!fromName.isEmpty() && scn.customLabel() == fromName) ||
            (firstAndLast.size() == 2 &&
             scn.firstName() == firstAndLast.at(0) &&
             scn.lastName() == firstAndLast.at(1))) {
        return true;
    }
#else
    if (firstAndLast.size() == 2 &&
            scn.firstName() == firstAndLast.at(0) &&
            scn.lastName() == firstAndLast.at(1)) {
        return true;
    }
#endif

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

bool FacebookPostSyncAdaptor::haveAlreadyPostedEvent(const QString &postId, const QString &eventBody, const QDateTime &createdTime)
{
    Q_UNUSED(eventBody);
    Q_UNUSED(createdTime);

    return (whenSyncedDatum(QLatin1String("facebook"), postId).isValid());
}

void FacebookPostSyncAdaptor::incrementSemaphore(int accountId)
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

void FacebookPostSyncAdaptor::decrementSemaphore(int accountId)
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
        updateLastSyncTimestamp(QLatin1String("facebook"),
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
            TRACE(SOCIALD_INFORMATION, QString(QLatin1String("Finished Facebook Posts sync at: %1"))
                                       .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
            m_status = SocialNetworkSyncAdaptor::Inactive;
            emit statusChanged();
        }
    }
}
