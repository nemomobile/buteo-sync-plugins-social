/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebookpostsyncadaptor.h"
#include "syncservice.h"
#include "trace.h"
#include "constants_p.h"

#include <QtCore/QPair>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

//QtMobility
#include <QtContacts/QContactManager>
#include <QtContacts/QContactFetchHint>
#include <QtContacts/QContactFetchRequest>
#include <QtContacts/QContact>
#include <QtContacts/QContactName>
#include <QtContacts/QContactNickname>
#include <QtContacts/QContactPresence>
#include <QtContacts/QContactAvatar>

#include "eventfeedhelper_p.h"

//meegotouchevents/meventfeed
#include <meventfeed.h>

#define SOCIALD_FACEBOOK_POSTS_ID_PREFIX QLatin1String("facebook-posts-")
#define SOCIALD_FACEBOOK_POSTS_GROUPNAME QLatin1String("facebook")
#define QTCONTACTS_SQLITE_AVATAR_METADATA QLatin1String("AvatarMetadata")
#define FACEBOOK_AVATAR QLatin1String("https://graph.facebook.com/%1/picture?width=200&height=200")


static const char *FQL_QUERY = "{"\
        "\"query1\":\"SELECT post_id,viewer_id,actor_id,target_id,message,type,attachment,"\
                     "description,created_time,updated_time,comment_info,like_info,parent_post_id "\
                     "FROM stream WHERE filter_key IN (SELECT filter_key FROM stream_filter "\
                     "WHERE uid = me() AND type = 'newsfeed') AND created_time > %1 "\
                     "ORDER BY created_time DESC LIMIT %2,%3\","\
        "\"query2\": \"SELECT uid,name FROM user WHERE uid in (SELECT actor_id FROM #query1)\","\
        "\"query3\": \"SELECT page_id,name FROM page WHERE page_id in (SELECT actor_id FROM #query1)\","\
        "\"query4\": \"SELECT gid,name FROM group WHERE gid in (SELECT actor_id FROM #query1)\","\
        "\"query5\": \"SELECT eid,name FROM event WHERE eid in (SELECT actor_id FROM #query1)\"}";

#define QUERY_QUERY1 QLatin1String("query1")
#define QUERY_QUERY2 QLatin1String("query2")
#define QUERY_QUERY3 QLatin1String("query3")
#define QUERY_QUERY4 QLatin1String("query4")
#define QUERY_QUERY5 QLatin1String("query5")
#define QUERY_DATA_KEY QLatin1String("data")
#define QUERY_NAME_KEY QLatin1String("name")
#define QUERY_RESULT_KEY QLatin1String("fql_result_set")
#define QUERY_UID_KEY QLatin1String("uid")
#define QUERY_PAGEID_KEY QLatin1String("page_id")
#define QUERY_GID_KEY QLatin1String("gid")
#define QUERY_EID_KEY QLatin1String("eid")
static const int QUERY_SIZE = 1000;

static QContactManager *aggregatingContactManager(QObject *parent)
{
    QContactManager *retn = new QContactManager(
            QString::fromLatin1("org.nemomobile.contacts.sqlite"),
            QMap<QString, QString>(),
            parent);
    if (retn->managerName() != QLatin1String("org.nemomobile.contacts.sqlite")) {
        // the manager specified is not the aggregating manager we depend on.
        delete retn;
        return 0;
    }

    return retn;
}

// currently, we integrate with the device events feed via libeventfeed / meegotouchevents' meventfeed.

FacebookPostSyncAdaptor::FacebookPostSyncAdaptor(SyncService *syncService, QObject *parent)
    : FacebookDataTypeSyncAdaptor(syncService, SyncService::Posts, parent)
    , m_contactManager(aggregatingContactManager(this))
    , m_contactFetchRequest(new QContactFetchRequest(this))
{
    if (!MEventFeed::instance()) {
        setInitialActive(false);
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
        m_contactFetchRequest->setManager(m_contactManager);
        connect(m_contactFetchRequest, SIGNAL(stateChanged(QContactAbstractRequest::State)), this, SLOT(contactFetchStateChangedHandler(QContactAbstractRequest::State)));
    }

    // can sync, enabled
    setInitialActive(true);
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
    foreach (int accountIdentifier, purgeIds) {
        bool ok;
        QStringList localIdentifiers = removeAllData(serviceName(), SyncService::dataType(dataType),
                                                     QString::number(accountIdentifier), &ok);
        if (!ok) {
            continue;
        }


        int prefixLength = QString(SOCIALD_FACEBOOK_POSTS_ID_PREFIX).size();
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
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(url));
    
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

void FacebookPostSyncAdaptor::requestPosts(int accountId, const QString &accessToken)
{
    // No need to use paging, FB API will limit us, so we just query a huge number
    // with a time limit and that should do the work
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    uint timeLimit = QDateTime::currentDateTimeUtc().toTime_t();
    timeLimit -= 864000; // 10 days in seconds
    QString fqlQuery = QString(QLatin1String(FQL_QUERY)).arg(QString::number(timeLimit),
                                                             QString::number(0),
                                                             QString::number(QUERY_SIZE));
    queryItems.append(qMakePair<QString, QString>(QLatin1String("q"), fqlQuery));

    QUrl url(QLatin1String("https://graph.facebook.com/fql"));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(url));
    
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
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("id"))) {
        QString selfUserId = parsed.value(QLatin1String("id")).toString();
        if (!m_selfFacebookUserIds.contains(accountId)) {
            m_selfFacebookUserIds.insert(accountId, selfUserId);
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
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("data"))) {
        QJsonArray data = parsed.value(QLatin1String("data")).toArray();

        if (!data.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no home posts received for account %1"))
                    .arg(accountId));
            decrementSemaphore(accountId);
            return;
        }

        // The FQL query will return 5 query results
        // The 1st one contains information about 30 entries of an user's home feed
        // The 4 others contains name that relates to the actor_id (the id of the entity
        // who post the posts listed in the 1st query)

        // We should create a hashmap from the last 4 queries to make retrieving of
        // name easier.
        QMap<QString, QString> actorNames;
        QJsonArray mainData;
        QList<SyncedDatum> syncedData;
        int prefixLen = QString(SOCIALD_FACEBOOK_POSTS_ID_PREFIX).size();

        foreach (QJsonValue entry, data) {
            QJsonObject entryObject = entry.toObject();
            QString name = entryObject.value(QUERY_NAME_KEY).toString();
            if (name == QUERY_QUERY1) {
                mainData = entryObject.value(QUERY_RESULT_KEY).toArray();
            } else if (name == QUERY_QUERY2
                       || name == QUERY_QUERY3
                       || name == QUERY_QUERY4
                       || name == QUERY_QUERY5) {
                QString key;
                if (name == QUERY_QUERY2) {
                    key = QUERY_UID_KEY;
                } else if(name == QUERY_QUERY3) {
                    key = QUERY_PAGEID_KEY;
                } else if(name == QUERY_QUERY4) {
                    key = QUERY_GID_KEY;
                } else if(name == QUERY_QUERY5) {
                    key = QUERY_EID_KEY;
                }

                QJsonArray nameList = entryObject.value(QUERY_RESULT_KEY).toArray();
                foreach (QJsonValue name, nameList) {
                    QJsonObject nameObject = name.toObject();
                    // We need to cast to a variant and then to a string, because
                    // an int is handled as a double in C++ by QtJson, and
                    // might not be precise enough
                    actorNames.insert(nameObject.value(key).toVariant().toString(),
                                      nameObject.value(QUERY_NAME_KEY).toString());
                }
            }
        }

        // Create a hash map for contacts
        QHash<QString, QContact> contactHash;
        foreach (QContact contact, m_contacts) {
             QContactName contactName = contact.detail<QContactName>();
             QStringList nameList;
             nameList.append(contactName.firstName());
             if (!contactName.middleName().isEmpty()) {
                 nameList.append(contactName.middleName());
             }
             nameList.append(contactName.lastName());
             contactHash.insert(nameList.join(" "), contact);
        }

        // We are using FQL, instead of Graph API to retrieve Facebook event feeds
        // because FQL allows access to more data, and especially to multiple media
        // attached to a post.
        //
        // However, the biggest problem with Graph still appears in FQL (in a slightly
        // smaller scale). It is the problem about some posts carring rather
        // useless information. Sometimes, Facebook generates "stories", to notify
        // the user that something happened. These stories, like "Someone liked something"
        // or "A 'comment' on someone's wall" do not carry metadata, and are hard to
        // render in a nice way., so we should discard them.
        //
        // The problem is that sometimes, Facebook generates good stories as well,
        // like "Someone shared this link", with meaningful content.
        //
        // We will use an algorithm to filter out these bad stories. It is a guessed
        // algorithm and might have false positive and false negative, but in
        // general, it should provide a good experience.
        //
        // The idea is the following. If the user posted a message with a given
        // post, like if he/she wrote a status, or if he/she wrote a description of
        // a picture, then the post is a good one.
        // If it is Facebook that is in charge of writing a story, we have to be
        // more cautious. The meaningless stories do not come with data, like
        // a shared link with a caption / description, so we filter out those stories.
        //
        // We cannot rely on type nor on most of the attributes in the stream FQL table
        // because most of them are just undocumented, so we are basing our algorithm
        // on the displayed content.
        //
        // We also need to be careful about that "parent_post_id" property, because
        // some posts now seems to contain other posts like: "Someone commented on this comment",
        // containing the comment "Another one commented this post", containing the post etc.
        // Just to be secure, we discard all posts containing a parent_post_id.

        foreach (QJsonValue entry, mainData) {
            QJsonObject post = entry.toObject();
            // These are the fields we eventually need to fill out
            QString actorId = post.value(QLatin1String("actor_id")).toVariant().toString();
            QString title = actorNames.value(actorId);
            QString body;
            QStringList imageList;
            QString footer;
            bool isVideo = false;
            QString url; // What should we put here ? TODO


            // Grab the data from the current post
            uint createdTimestamp = post.value(QLatin1String("created_time")).toVariant().toString().toUInt();
            QDateTime createdTime = QDateTime::fromTime_t(createdTimestamp);


            QString postId = post.value(QLatin1String("post_id")).toVariant().toString();

            bool isMessagePost = false;

            // We discard posts with a target id (from now)
            if (!post.value(QLatin1String("target_id")).isNull()
                && post.value(QLatin1String("target_id")) != m_selfFacebookUserIds.value(accountId)) {
                continue;
            }

            // We discard posts with a parent_post_id
            if (!post.value(QLatin1String("parent_post_id")).isNull()) {
                continue;
            }

            // We keep posts with messages
            QString message = post.value(QLatin1String("message")).toString();
            isMessagePost = !message.isEmpty();

            QString story = post.value(QLatin1String("description")).toString();

            QJsonObject attachment = post.value(QLatin1String("attachment")).toObject();
            // If we don't have a message post, we will check if there is
            // a media  in the attachment. If not, we will trash the post
            // TODO: check if the media is valid
            if (!isMessagePost && (!attachment.contains(QLatin1String("media")))) {
                continue;
            }
            // Create the event body
            // We use the name of the attachment if the event body is empty
            body = isMessagePost ? message : story;
            if (body.isEmpty()) {
                body = attachment.value(QLatin1String("name")).toString();
            }

            // Create the event footer
            int likes = post.value(QLatin1String("like_info")).toObject().value(QLatin1String("like_count")).toVariant().toInt();
            int comments = post.value(QLatin1String("comment_info")).toObject().value(QLatin1String("comment_count")).toVariant().toInt();

            //% "%n likes"
            QString likesString = qtTrId("sociald_facebook_posts-n_likes", likes);
            //% "%n comments"
            QString commentsString = qtTrId("sociald_facebook_posts-n_comments", comments);

            footer = QString ("%1 \u2022 %2").arg(likesString, commentsString);

            // Pass the description of the media to the subview via metadata
            // (TODO: libeventfeed is lacking support of this, or maybe lipstick ?)
            QString attachmentName = attachment.value(QLatin1String("name")).toString();
            QString attachmentCaption = attachment.value(QLatin1String("caption")).toString();
            QString attachmentDescription = attachment.value(QLatin1String("description")).toString();

            if (!attachment.value("media").isNull()) {
                bool wrongMediaFound = false;
                QJsonArray media = attachment.value("media").toArray();

                // If the media is empty (but exists) we discard
                if (media.isEmpty()) {
                    continue;
                }

                foreach (QJsonValue medium, media) {
                    QJsonObject mediumObject = medium.toObject();
                    if (mediumObject.contains(QLatin1String("video"))) {
                        isVideo = true;
                    }
                    QString mediaUrlString = mediumObject.value(QLatin1String("src")).toString();

                    // Skip those media that do not have URL (someone went to an event)
                    if (mediaUrlString.isEmpty()) {
                        wrongMediaFound = true;
                        continue;
                    }

                    // Try to find a better image for this media
                    if (mediumObject.contains(QLatin1String("photo"))) {
                        QJsonArray imageList = mediumObject.value(QLatin1String("photo")).toObject().value(QLatin1String("images")).toArray();
                        QString newImage = imageList.last().toObject().value(QLatin1String("src")).toString();
                        if (!newImage.isEmpty()) {
                            mediaUrlString = newImage;
                        }
                    }

                    // Patch an issue with some applications using local path instead
                    // of absolute urls
                    if (!mediaUrlString.startsWith("http")) {
                        mediaUrlString.prepend("https://facebook.com/");
                    }

                    imageList.append(QUrl::fromEncoded(mediaUrlString.toLocal8Bit()).toString());
                }

                if (wrongMediaFound) {
                    continue;
                }
            }

            // check to see if we need to post it to the events feed
            if (createdTime.daysTo(QDateTime::currentDateTimeUtc()) > 7) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("event for account %1 is more than a week old:\n"))
                        .arg(accountId) << "    " << createdTime << ":" << body);
                break;                 // all subsequent events will be even older.
            } else {
                QVariantMap metaData;
                metaData.insert("nodeId", postId);
                metaData.insert("postAttachmentName", attachmentName);
                metaData.insert("postAttachmentCaption", attachmentCaption);
                metaData.insert("postAttachmentDescription", attachmentDescription);
                metaData.insert("clientId", clientId());

                QString icon;

                // Search the portrait in the contacts
                if (contactHash.contains(title)) {
                    QContact contact = contactHash.value(title);
                    QContactAvatar avatar = contact.detail<QContactAvatar>();
                    if (!avatar.imageUrl().isEmpty()) {
                        icon = avatar.imageUrl().toString();
                    }
                }

                // If we don't find the portrait in contacts, we download
                // the avatar from Facebook
                if (icon.isEmpty()) {
                    icon = QString(FACEBOOK_AVATAR).arg(actorId);
                }

                QString localIdentifier = syncedDatumLocalIdentifier(serviceName(), SyncService::dataType(dataType), postId).mid(prefixLen);

                QList<int> accountIds;
                if (!localIdentifier.isEmpty()) {
                    accountIds.append(syncedDatumAccountIds(QString(SOCIALD_FACEBOOK_POSTS_ID_PREFIX + localIdentifier)));
                }
                if (!accountIds.contains(accountId)) {
                    accountIds.append(accountId);
                }

                EventFeedHelper::manageEvent(icon, title, body, imageList, createdTime,
                                             footer, isVideo, url, serviceName(),
                                             // TODO: translate the string below
                                             QLatin1String("Facebook"), metaData, accountId,
                                             accountIds,
                                             // For Facebook, we do not need the map
                                             // of profile picture.
                                             QMap<int, QString>(), localIdentifier,
                                             SOCIALD_FACEBOOK_POSTS_ID_PREFIX,
                                             dataType, postId, syncedData);
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

void FacebookPostSyncAdaptor::contactFetchStateChangedHandler(QContactAbstractRequest::State newState)
{
    // update our local cache of contacts.
    if (m_contactFetchRequest && newState == QContactAbstractRequest::FinishedState) {
        m_contacts = m_contactFetchRequest->contacts();
        m_selfContact = m_contactManager->contact(m_contactManager->selfContactId());
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("finished refreshing local cache of contacts, have %1"))
                .arg(m_contacts.size()));
    }
}

bool FacebookPostSyncAdaptor::fromIsSelfContact(const QString &fromName, const QString &fromFbUid) const
{
    // XXX TODO: look this up from QtContacts database instead (saves one request round trip time)
    if (m_selfFacebookUserIds.values().contains(fromFbUid)) {
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

bool FacebookPostSyncAdaptor::haveAlreadyPostedEvent(const QString &postId, const QString &eventBody, const QDateTime &createdTime)
{
    Q_UNUSED(eventBody);
    Q_UNUSED(createdTime);

    QDateTime syncedDatum = whenSyncedDatum(QLatin1String("facebook"), postId);



    return (syncedDatum.isValid());
}
