/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Bea Lam <bea.lam@jollamobile.com>
 **
 ****************************************************************************/

#include "vkpostsyncadaptor.h"
#include "trace.h"
#include "constants_p.h"

#include <QtCore/QPair>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>

#define SOCIALD_VK_POSTS_ID_PREFIX QStringLiteral("vk-posts-")
#define SOCIALD_VK_POSTS_GROUPNAME QStringLiteral("vk")

VKPostSyncAdaptor::VKPostSyncAdaptor(QObject *parent)
    : VKDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Posts, parent)
{
    setInitialActive(m_db.isValid());
}

VKPostSyncAdaptor::~VKPostSyncAdaptor()
{
}

QString VKPostSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("vk-microblog");
}

void VKPostSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    // call superclass impl.
    VKDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void VKPostSyncAdaptor::purgeDataForOldAccounts(const QList<int> &oldIds, SocialNetworkSyncAdaptor::PurgeMode mode)
{
    Q_UNUSED(mode);
    if (oldIds.size()) {
        foreach (int accountIdentifier, oldIds) {
            m_db.removePosts(accountIdentifier);
        }
        m_db.commit();
        m_db.wait();
    }
}

void VKPostSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    m_db.removePosts(accountId); // always purge all posts for the account, prior to syncing most recent.
    requestPosts(accountId, accessToken);
}

void VKPostSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId)
    m_db.commit();
    m_db.wait();
}

void VKPostSyncAdaptor::requestPosts(int accountId, const QString &accessToken)
{
    QList<QPair<QString, QString> > queryItems;

    queryItems.append(QPair<QString, QString>(QString(QStringLiteral("access_token")), accessToken));
    queryItems.append(QPair<QString, QString>(QString(QStringLiteral("extended")), QStringLiteral("1")));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("v")), QStringLiteral("5.21")));    // version

    QUrl url(QStringLiteral("https://api.vk.com/method/wall.get"));
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
                QString(QStringLiteral("error: unable to request home posts from VK account with id %1"))
                .arg(accountId));
    }
}

void VKPostSyncAdaptor::finishedPostsHandler()
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

    qWarning() << "\t received raw:" << QString::fromLatin1(replyData.constData());

    if (!isError && ok && parsed.contains(QStringLiteral("response"))) {
        QJsonObject responseObj = parsed.value(QStringLiteral("response")).toObject();
        QJsonArray items = responseObj.value(QLatin1String("items")).toArray();
        if (!items.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no feed posts received for account %1"))
                    .arg(accountId));
            decrementSemaphore(accountId);
            return;
        }

        QJsonArray profileValues = responseObj.value(QStringLiteral("profiles")).toArray();
        QList<UserProfile> userProfiles;
        foreach (const QJsonValue &entry, profileValues) {
            userProfiles << UserProfile::fromJsonObject(entry.toObject());
        }

        foreach (const QJsonValue &entry, items) {
            QJsonObject object = entry.toObject();
            if (!object.isEmpty()) {
                saveVKPostFromObject(accountId, object, userProfiles);
            }
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QStringLiteral("error: unable to parse event feed data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

void VKPostSyncAdaptor::saveVKPostFromObject(int accountId, const QJsonObject &post, const QList<UserProfile> &userProfiles)
{
    VKPostsDatabase::Post newPost;
    newPost.fromId = int(post.value(QStringLiteral("from_id")).toDouble());
    newPost.toId = int(post.value(QStringLiteral("to_id")).toDouble());
    newPost.postType = post.value(QStringLiteral("post_type")).toString();
    newPost.replyOwnerId = int(post.value(QStringLiteral("reply_owner_id")).toDouble());
    newPost.replyPostId = int(post.value(QStringLiteral("reply_post_id")).toDouble());
    newPost.signerId = int(post.value(QStringLiteral("signer_id")).toDouble());
    newPost.friendsOnly = int(post.value(QStringLiteral("friends_only")).toDouble()) == 1;

    QJsonObject commentInfo = post.value(QStringLiteral("comments")).toObject();
    VKPostsDatabase::Comments comments;
    comments.count = int(commentInfo.value(QStringLiteral("count")).toDouble());
    comments.userCanComment = int(commentInfo.value(QStringLiteral("count")).toDouble()) == 1;
    newPost.comments = comments;

    QJsonObject likeInfo = post.value(QStringLiteral("likes")).toObject();
    VKPostsDatabase::Likes likes;
    likes.count = int(likeInfo.value(QStringLiteral("count")).toDouble());
    likes.userLikes = int(likeInfo.value(QStringLiteral("user_likes")).toDouble()) == 1;
    likes.userCanLike = int(likeInfo.value(QStringLiteral("can_like")).toDouble()) == 1;
    likes.userCanPublish = int(likeInfo.value(QStringLiteral("can_publish")).toDouble()) == 1;
    newPost.likes = likes;

    QJsonObject repostInfo = post.value(QStringLiteral("reposts")).toObject();
    VKPostsDatabase::Reposts reposts;
    reposts.count = int(repostInfo.value(QStringLiteral("count")).toDouble());
    reposts.userReposted = int(repostInfo.value(QStringLiteral("user_reposted")).toDouble()) == 1;
    newPost.reposts = reposts;

    QJsonObject postSourceInfo = post.value(QStringLiteral("post_source")).toObject();
    VKPostsDatabase::PostSource postSource;
    postSource.type = postSourceInfo.value(QStringLiteral("type")).toString();
    postSource.data = postSourceInfo.value(QStringLiteral("data")).toString();
    newPost.postSource = postSource;

    // XXX test this
    QList<QPair<QString, SocialPostImage::ImageType> > images;
    QJsonArray attachmentsInfo = post.value(QStringLiteral("attachments")).toArray();
    Q_FOREACH (const QJsonValue &attValue, attachmentsInfo) {
        QJsonObject attObject = attValue.toObject();
        QString type = attObject.value(QStringLiteral("type")).toString();
        QJsonObject attValue = attObject.value(type).toObject();
        if (type == QStringLiteral("photo")
                || type == QStringLiteral("posted_photo")
                || type == QStringLiteral("graffiti")) {
            QString src = attValue.value(QStringLiteral("photo_75")).toString();
            if (!src.isEmpty()) {
                images.append(qMakePair(src, SocialPostImage::Photo));
            }
        }
    }

    VKPostsDatabase::GeoLocation geo;
    QJsonObject geoInfo = post.value(QStringLiteral("geo")).toObject();
    if (!geoInfo.isEmpty()) {
        geo.placeId = int(geoInfo.value(QStringLiteral("place_id")).toDouble());
        geo.title = geoInfo.value(QStringLiteral("title")).toString();
        geo.type = geoInfo.value(QStringLiteral("type")).toString();
        geo.countryId = int(geoInfo.value(QStringLiteral("country_id")).toDouble());
        geo.cityId = int(geoInfo.value(QStringLiteral("city_id")).toDouble());
        geo.address = geoInfo.value(QStringLiteral("address")).toString();
        geo.showMap = int(geoInfo.value(QStringLiteral("showmap")).toDouble()) == 1;   // type????
        newPost.geo = geo;
    }

    VKPostsDatabase::CopyPost copyPost;
    copyPost.createdTime = parseVKDateTime(post.value(QStringLiteral("copy_post_date")));
    copyPost.type = post.value(QStringLiteral("copy_post_type")).toString();
    copyPost.ownerId = int(post.value(QStringLiteral("copy_owner_id")).toDouble());
    copyPost.postId = int(post.value(QStringLiteral("copy_post_id")).toDouble());
    copyPost.text = post.value(QStringLiteral("copy_text")).toString();
    newPost.copyPost = copyPost;

    UserProfile user = findProfile(userProfiles, newPost.fromId);
    QString identifier = QString::number(post.value(QStringLiteral("id")).toDouble());
    QDateTime createdTime = parseVKDateTime(post.value(QStringLiteral("date")));
    QString body = post.value(QStringLiteral("text")).toString();

    qWarning() << "+++++++++++++++ adding" << identifier << createdTime << body << accountId << user.name() << user.icon;

    m_db.addVKPost(identifier, createdTime, body, newPost, images, user.name(), user.icon, accountId);
}
