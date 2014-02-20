/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebookpostsyncadaptor.h"
#include "trace.h"
#include "constants_p.h"

#include <QtCore/QPair>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>

#include <QtContacts/QContactManager>
#include <QtContacts/QContact>
#include <QtContacts/QContactName>
#include <QtContacts/QContactNickname>
#include <QtContacts/QContactPresence>
#include <QtContacts/QContactAvatar>

#define SOCIALD_FACEBOOK_POSTS_ID_PREFIX QLatin1String("facebook-posts-")
#define SOCIALD_FACEBOOK_POSTS_GROUPNAME QLatin1String("facebook")
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
static const int QUERY_SIZE = 30;

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

FacebookPostSyncAdaptor::FacebookPostSyncAdaptor(QObject *parent)
    : FacebookDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Posts, parent)
    , m_contactManager(aggregatingContactManager(this))
{
    setInitialActive(false);
    if (!m_contactManager) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: no aggregating contact manager exists - Facebook posts sync will be inactive")));
        return;
    }

    m_selfContact = m_contactManager->contact(m_contactManager->selfContactId());
    setInitialActive(true);
}

FacebookPostSyncAdaptor::~FacebookPostSyncAdaptor()
{
    delete m_contactManager;
}

QString FacebookPostSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("facebook-microblog");
}

void FacebookPostSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    // call superclass impl.
    FacebookDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void FacebookPostSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    if (purgeIds.size()) {
        foreach (int accountIdentifier, purgeIds) {
            m_db.removePosts(accountIdentifier);
        }
        m_db.commit();
        m_db.wait();
    }
}

void FacebookPostSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    m_db.removePosts(accountId); // always purge all posts for the account, prior to syncing most recent.
    requestMe(accountId, accessToken);
}

void FacebookPostSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId)
    m_db.commit();
    m_db.wait();
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
        setupReplyTimeout(accountId, reply);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request feed posts from Facebook account with id %1"))
                .arg(accountId));
    }
}

void FacebookPostSyncAdaptor::requestPosts(int accountId, const QString &accessToken)
{
    // We query only the most recent QUERY_SIZE posts.  We set a long time limit.
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
        setupReplyTimeout(accountId, reply);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request home posts from Facebook account with id %1"))
                .arg(accountId));
    }
}

void FacebookPostSyncAdaptor::finishedMeHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok && parsed.contains(QLatin1String("id"))) {
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
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok && parsed.contains(QLatin1String("data"))) {
        QJsonArray data = parsed.value(QLatin1String("data")).toArray();

        if (!data.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no home posts received for account %1"))
                    .arg(accountId));
            decrementSemaphore(accountId);
            return;
        }

        // The FQL query will return 5 query results
        // The 1st one contains information about QUERY_SIZE entries of an user's home feed
        // The 4 others contains name that relates to the actor_id (the id of the entity
        // who post the posts listed in the 1st query)

        // We should create a hashmap from the last 4 queries to make retrieving of
        // name easier.
        QMap<QString, QString> actorNames;
        QJsonArray mainData;
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
        QList<QContact> qContacts = m_contactManager->contacts();
        QHash<QString, QContact> contactHash;
        foreach (QContact contact, qContacts) {
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
        // render in a nice way.
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
        // For this reason, we only cache posts whose "parent_post_id" is not the id of
        // another post we're caching.
        //
        // The "target_id" is the wall to which the post was posted. If the user has liked
        // someone else, posts on those walls will appear in the stream.

        // First, build up a list of "primary" posts, and a list of their ids.
        QList<QString> postObjectIds;
        QList<QJsonObject> postObjects;
        foreach (QJsonValue entry, mainData) {
            QJsonObject post = entry.toObject();
            QString currPostId = post.value(QLatin1String("post_id")).toVariant().toString();
            if (!postObjectIds.contains(currPostId)) {
                postObjectIds.append(currPostId);
                postObjects.append(post);
            }
        }

        // Second, convert the post into a cacheable post, and determine whether or
        // not we should cache it (based on our heuristics).
        foreach (const QJsonObject &post, postObjects) {
            // Any post with a parent_post_id will be discarded if that parent_post_id
            // is contained in the list of primary post ids, as we already cache that parent post,
            // unless the post_id is the same as the parent_post_id.
            QString postId = post.value(QLatin1String("post_id")).toVariant().toString();
            QString parentPostId = post.value(QLatin1String("parent_post_id")).toVariant().toString();
            if (!parentPostId.isEmpty() && postObjectIds.contains(parentPostId) && parentPostId != postId) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("Discarding post:\n%1because parent_post_id is already cached.\n"))
                        .arg(QString::fromUtf8(QJsonDocument::fromVariant(post.toVariantMap()).toJson())));
                continue;
            }

            // These are the fields we eventually need to fill out
            QString actorId = post.value(QLatin1String("actor_id")).toVariant().toString();
            QString name = actorNames.value(actorId);
            QString body;
            QList<QPair<QString, SocialPostImage::ImageType> > imageList;

            // Grab the data from the current post
            uint postType = post.value(QLatin1String("type")).toVariant().toString().toUInt();
            uint createdTimestamp = post.value(QLatin1String("created_time")).toVariant().toString().toUInt();
            QDateTime createdTime = QDateTime::fromTime_t(createdTimestamp);
            QString story = post.value(QLatin1String("description")).toString();
            QJsonObject attachment = post.value(QLatin1String("attachment")).toObject();
            QString message = post.value(QLatin1String("message")).toString();
            bool isMessagePost = !message.isEmpty();

            // We have to discard the post if it's a raw comment or like, as we
            // don't have any information which can be rendered in the feed.
            // XXX TODO: improve the query to fetch the actual post data, for
            // any stream item which is a like/comment on an actual post.
            QList<uint> likesPostTypes; likesPostTypes << 161 << 245 << 283 << 347;
            QList<uint> commentsPostTypes; commentsPostTypes << 257;
            if (!isMessagePost && (likesPostTypes.contains(postType) || commentsPostTypes.contains(postType))) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("Discarding post:\n%1because it's a like or comment.\n"))
                        .arg(QString::fromUtf8(QJsonDocument::fromVariant(post.toVariantMap()).toJson())));
                continue;
            }

            // Create the event body
            // We use the name of the attachment if the event body is empty
            body = isMessagePost ? message : story;
            if (body.isEmpty()) {
                body = attachment.value(QLatin1String("name")).toString();
            }

            // Create the event footer
            QJsonObject likeInfo = post.value(QLatin1String("like_info")).toObject();
            QJsonObject commentInfo = post.value(QLatin1String("comment_info")).toObject();
            bool allowLike = likeInfo.value(QLatin1String("can_like")).toBool();
            bool allowComment = commentInfo.value(QLatin1String("can_comment")).toBool();

            // Pass the description of the media to the subview via metadata
            QString attachmentName = attachment.value(QLatin1String("name")).toString();
            QString attachmentCaption = attachment.value(QLatin1String("caption")).toString();
            QString attachmentDescription = attachment.value(QLatin1String("description")).toString();
            QString attachmentUrl = attachment.value(QLatin1String("href")).toString();

            // If media was provided, we need to ensure that it's valid, else discard the post.
            if (attachment.keys().contains("media") && !attachment.value("media").isNull()) {
                bool wrongMediaFound = false;
                QJsonArray media = attachment.value("media").toArray();

                // If the media is empty (but exists) we discard
                if (media.isEmpty()) {
                    TRACE(SOCIALD_DEBUG,
                            QString(QLatin1String("Discarding post:\n%1because no valid media was found.\n"))
                            .arg(QString::fromUtf8(QJsonDocument::fromVariant(post.toVariantMap()).toJson())));
                    continue;
                }

                foreach (QJsonValue medium, media) {
                    QJsonObject mediumObject = medium.toObject();
                    SocialPostImage::ImageType type = SocialPostImage::Photo;
                    if (mediumObject.contains(QLatin1String("video"))) {
                        type = SocialPostImage::Video;
                    }
                    QString mediaUrlString = mediumObject.value(QLatin1String("src")).toString();

                    // Skip those media that do not have URL (someone went to an event)
                    if (mediaUrlString.isEmpty()) {
                        wrongMediaFound = true;
                        continue;
                    }

                    // Try to find a better image for this media
                    if (mediumObject.contains(QLatin1String("photo"))) {
                        QJsonArray imageArray = mediumObject.value(QLatin1String("photo")).toObject().value(QLatin1String("images")).toArray();
                        QString newImage = imageArray.last().toObject().value(QLatin1String("src")).toString();
                        if (!newImage.isEmpty()) {
                            mediaUrlString = newImage;
                        }
                    }

                    // Patch an issue with some applications using local path instead
                    // of absolute urls
                    if (!mediaUrlString.startsWith("http")) {
                        mediaUrlString.prepend("https://facebook.com/");
                    }

                    QString urlString = QUrl::fromEncoded(mediaUrlString.toLocal8Bit()).toString();
                    imageList.append(qMakePair<QString, SocialPostImage::ImageType>(urlString, type));
                }

                if (wrongMediaFound) {
                    TRACE(SOCIALD_DEBUG,
                            QString(QLatin1String("Discarding post:\n%1because media url was empty.\n"))
                            .arg(QString::fromUtf8(QJsonDocument::fromVariant(post.toVariantMap()).toJson())));
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
                QString icon;
                // Search the portrait in the contacts
                if (contactHash.contains(name)) {
                    QString pictureUrl;
                    QString fallbackUrl;
                    QContact contact = contactHash.value(name);
                    foreach (const QContactAvatar &avatar, contact.details<QContactAvatar>()) {
                        if (!avatar.imageUrl().isEmpty()) {
                            if (avatar.value(QContactAvatar__FieldAvatarMetadata).toString() == QLatin1String("picture")) {
                                pictureUrl = avatar.imageUrl().toString();
                            } else if (avatar.value(QContactAvatar__FieldAvatarMetadata).toString() != QLatin1String("cover")) {
                                // we don't want to use the cover image, as we don't cache it (yet)
                                // and it's a large size, heavy download.
                                fallbackUrl = avatar.imageUrl().toString();
                            }
                        }
                    }

                    if (!pictureUrl.isEmpty()) {
                        icon = pictureUrl;
                    } else if (!fallbackUrl.isEmpty()) {
                        icon = fallbackUrl;
                    }
                }

                // If we don't find the portrait in contacts, we download
                // the avatar from Facebook
                if (icon.isEmpty()) {
                    icon = QString(FACEBOOK_AVATAR).arg(actorId);
                }

                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("Adding post:\n%1into the Posts database as:\n%2, %3, %4, %5, %6, %7\n"))
                        .arg(QString::fromUtf8(QJsonDocument::fromVariant(post.toVariantMap()).toJson()))
                        .arg(postId)
                        .arg(icon)
                        .arg(name)
                        .arg(body)
                        .arg(attachmentCaption)
                        .arg(attachmentDescription));

                m_db.addFacebookPost(postId, name, body, createdTime, icon, imageList,
                                     attachmentName, attachmentCaption, attachmentDescription,
                                     attachmentUrl, allowLike, allowComment, clientId(), accountId);
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
