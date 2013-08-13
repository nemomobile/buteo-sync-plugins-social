/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebooknotificationsyncadaptor.h"
#include "syncservice.h"
#include "trace.h"
#include "constants_p.h"
#include <QUrlQuery>
#include <QtCore/QJsonArray>

//nemo-qml-plugins/notifications
#include <notification.h>

FacebookNotificationSyncAdaptor::FacebookNotificationSyncAdaptor(SyncService *syncService, QObject *parent)
    : FacebookDataTypeSyncAdaptor(syncService, SyncService::Notifications, parent)
{
    setInitialActive(true);
}

FacebookNotificationSyncAdaptor::~FacebookNotificationSyncAdaptor()
{
}

void FacebookNotificationSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // purge all data from our database
        removeAllData(QLatin1String("facebook"),
                SyncService::dataType(SyncService::Notifications),
                QString::number(pid));
    }
}

void FacebookNotificationSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    requestNotifications(accountId, accessToken);
}

void FacebookNotificationSyncAdaptor::requestNotifications(int accountId, const QString &accessToken, const QString &until, const QString &pagingToken)
{
    // TODO: continuation requests need these two.  if exists, also set limit = 5000.
    // if not set, set "since" to the timestamp value.
    Q_UNUSED(until);
    Q_UNUSED(pagingToken);

    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("include_read")), QString(QLatin1String("false"))));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    QUrl url(QLatin1String("https://graph.facebook.com/me/notifications"));
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
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request notifications from Facebook account with id %1"))
                .arg(accountId));
    }
}

void FacebookNotificationSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QDateTime lastSync = lastSyncTimestamp(QLatin1String("facebook"), SyncService::dataType(SyncService::Notifications), QString::number(accountId));
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QJsonObject parsed = FacebookDataTypeSyncAdaptor::parseReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("summary"))) {
        QJsonArray data = parsed.value(QLatin1String("data")).toArray();

        int notificationCount = data.size();
        Notification *notification = existingNemoNotification(accountId);
        if (notificationCount > 0) {
            // Only publish a notification if one doesn't exist or the published notification count is different
            if (notification == 0 || notification->itemCount() != notificationCount) {
                if (notification == 0) {
                    notification = new Notification;
                    notification->setCategory("x-nemo.social.facebook.notification");
                    notification->setHintValue("x-nemo.sociald.account-id", accountId);
                }

                //: The title of the Facebook Notifications device notification
                //% "You have %1 new Facebook notification(s)!"
                QString title = qtTrId("sociald_facebook_posts-notification_title").arg(notificationCount);
                notification->setSummary(title);
                notification->setBody(QString());
                notification->setPreviewSummary(title);
                notification->setPreviewBody(QString());
                notification->setItemCount(notificationCount);
                notification->setTimestamp(QDateTime::currentDateTime());
                notification->publish();
            }
        } else if (notification != 0) {
            // Destroy any existing notification if there should be no notifications
            notification->close();
        }
        delete notification;
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse notification data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}

Notification *FacebookNotificationSyncAdaptor::existingNemoNotification(int accountId)
{
    foreach (QObject *object, Notification::notifications()) {
        Notification *notification = static_cast<Notification *>(object);
        if (notification->category() == "x-nemo.social.facebook.notification" && notification->hintValue("x-nemo.sociald.account-id").toInt() == accountId) {
            return notification;
        }
    }
    return 0;
}
