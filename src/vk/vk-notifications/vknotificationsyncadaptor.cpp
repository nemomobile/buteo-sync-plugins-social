/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "vknotificationsyncadaptor.h"
#include "trace.h"

#include <QUrlQuery>
#include <QDebug>

//static const int OLD_NOTIFICATION_LIMIT_IN_DAYS = 21;

static QMap<QString,int> getNotificationTypes()
{
    QMap<QString,int> types;
    types[QStringLiteral("follow")] = int(VKNotification::Follow);
    types[QStringLiteral("friend_accepted")] = int(VKNotification::FriendRequestAccepted);
    types[QStringLiteral("mention")] = int(VKNotification::Mention);
    types[QStringLiteral("mention_comments")] = int(VKNotification::MentionComments);
    types[QStringLiteral("wall")] = int(VKNotification::WallPost);
    types[QStringLiteral("comment_post")] = int(VKNotification::CommentPost);
    types[QStringLiteral("comment_photo")] = int(VKNotification::CommentPhoto);
    types[QStringLiteral("comment_video")] = int(VKNotification::CommentVideo);
    types[QStringLiteral("reply_comment")] = int(VKNotification::ReplyComment);
    types[QStringLiteral("reply_comment_photo")] = int(VKNotification::ReplyCommentPhoto);
    types[QStringLiteral("reply_comment_video")] = int(VKNotification::ReplyCommentVideo);
    types[QStringLiteral("reply_topic")] = int(VKNotification::ReplyTopic);
    types[QStringLiteral("like_post")] = int(VKNotification::LikePost);
    types[QStringLiteral("like_omment")] = int(VKNotification::LikeComment);
    types[QStringLiteral("like_photo")] = int(VKNotification::LikePhoto);
    types[QStringLiteral("like_video")] = int(VKNotification::LikeVideo);
    types[QStringLiteral("like_comment_photo")] = int(VKNotification::LikeCommentPhoto);
    types[QStringLiteral("like_comment_video")] = int(VKNotification::LikeCommentVideo);
    types[QStringLiteral("like_comment_topic")] = int(VKNotification::LikeCommentTopic);
    types[QStringLiteral("copy_post")] = int(VKNotification::CopyPost);
    types[QStringLiteral("copy_photo")] = int(VKNotification::CopyPhoto);
    types[QStringLiteral("copy_video")] = int(VKNotification::CopyVideo);
    return types;
}

static QMap<QString,int> notificationTypes = getNotificationTypes();

VKNotificationSyncAdaptor::VKNotificationSyncAdaptor(QObject *parent)
    : VKDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Notifications, parent)
{
    setInitialActive(true);
}

VKNotificationSyncAdaptor::~VKNotificationSyncAdaptor()
{
}

QString VKNotificationSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("vk-microblog");
}

void VKNotificationSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    Q_UNUSED(purgeIds);
//    if (purgeIds.size()) {
//        foreach (int accountIdentifier, purgeIds) {
//            m_db.removeNotifications(accountIdentifier);
//        }
//        m_db.sync();
//        m_db.wait();
//    }
}

void VKNotificationSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    requestNotifications(accountId, accessToken);
}

void VKNotificationSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId);
//    m_db.purgeOldNotifications(OLD_NOTIFICATION_LIMIT_IN_DAYS);
//    m_db.sync();
//    m_db.wait();
}

void VKNotificationSyncAdaptor::requestNotifications(int accountId, const QString &accessToken, const QString &until, const QString &pagingToken)
{
    Q_UNUSED(until);
    Q_UNUSED(pagingToken);
    // TODO: continuation requests need these two.  if exists, also set limit = 5000.
    // if not set, set "since" to the timestamp value.
    Q_UNUSED(until);
    Q_UNUSED(pagingToken);

    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));

    QUrl url(QStringLiteral("https://api.vk.com/method/notifications.get"));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(url));

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
        setupReplyTimeout(accountId, reply);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QStringLiteral("error: unable to request home posts from VK account with id %1"))
                .arg(accountId));
    }
}

//void VKNotificationSyncAdaptor::saveVKPostFromObject(int accountId, const QJsonObject &post, const QList<UserProfile> &userProfiles)
//{

//}

void VKNotificationSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
//    int sinceSpan = m_accountSyncProfile
//                  ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("7")).toInt()
//                  : 7;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);

    qWarning() << "data:" << replyData.constData();

    if (!isError && ok && parsed.contains(QLatin1String("response"))) {
        QJsonArray items = parsed.value(QLatin1String("items")).toArray();


        /*

           {"response":
                {"items": [1, {"type":"follow","date":1399264153,"feedback":[{"owner_id":"253028092"}]} ],
                "profiles":[{"uid":253028092,"first_name":"Bea","last_name":"Lam","sex":1,"screen_name":"id253028092",
"photo":"http:\/\/vk.com\/images\/camera_50.gif","photo_medium_rec":"http:\/\/vk.com\/images\/camera_100.gif","online":1}],
"groups":[],"new_from":"#0","last_viewed":0,"new_offset":0}}
         */
        foreach (const QJsonValue &entry, items) {
            QJsonObject object = entry.toObject();
            qWarning() << "Parsed:" << object.toVariantMap();

            if (!object.isEmpty()) {

            }


//            QDateTime createdTime = QDateTime::fromString(object.value(QLatin1String("created_time")).toString(), Qt::ISODate);
//            createdTime.setTimeSpec(Qt::UTC);
//            QDateTime updatedTime = QDateTime::fromString(object.value(QLatin1String("updated_time")).toString(), Qt::ISODate);
//            updatedTime.setTimeSpec(Qt::UTC);

//            if (createdTime.daysTo(QDateTime::currentDateTime()) > sinceSpan
//                    && updatedTime.daysTo(QDateTime::currentDateTime()) > sinceSpan) {
//                TRACE(SOCIALD_DEBUG,
//                        QString(QLatin1String("notification for account %1 is more than %2 days old:\n    %3 - %4 - %5"))
//                        .arg(accountId)
//                        .arg(sinceSpan)
//                        .arg(createdTime.toString(Qt::ISODate))
//                        .arg(updatedTime.toString(Qt::ISODate))
//                        .arg(object.value(QLatin1String("title")).toString()));
//                continue;
//            }

//            QJsonObject sender = object.value(QLatin1String("from")).toObject();
//            QJsonObject receiver = object.value(QLatin1String("to")).toObject();
//            QJsonObject application = object.value(QLatin1String("application")).toObject();
//            QJsonObject notificationObject = object.value(QLatin1String("object")).toObject();

//            m_db.addVKNotification(object.value(QLatin1String("id")).toString(),
//                                         sender.value(QLatin1String("id")).toString(),
//                                         receiver.value(QLatin1String("id")).toString(),
//                                         createdTime,
//                                         updatedTime,
//                                         object.value(QLatin1String("title")).toString(),
//                                         object.value(QLatin1String("link")).toString(),
//                                         application.value(QLatin1String("id")).toString(),
//                                         notificationObject.value(QLatin1String("id")).toString(),
//                                         object.value(QLatin1String("unread")).toDouble() != 0,
//                                         accountId,
//                                         clientId());
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse notification data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}
