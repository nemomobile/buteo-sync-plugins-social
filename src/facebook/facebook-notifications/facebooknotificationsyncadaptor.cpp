/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebooknotificationsyncadaptor.h"
#include "trace.h"

#include <QUrlQuery>
#include <QDebug>

static const int OLD_NOTIFICATION_LIMIT_IN_DAYS = 21;

FacebookNotificationSyncAdaptor::FacebookNotificationSyncAdaptor(QObject *parent)
    : FacebookDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Notifications, parent)
{
    setInitialActive(true);
}

FacebookNotificationSyncAdaptor::~FacebookNotificationSyncAdaptor()
{
}

QString FacebookNotificationSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("facebook-microblog");
}

void FacebookNotificationSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    if (purgeIds.size()) {
        foreach (int accountIdentifier, purgeIds) {
            m_db.removeNotifications(accountIdentifier);
        }
        m_db.sync();
        m_db.wait();
    }
}

void FacebookNotificationSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    requestNotifications(accountId, accessToken);
}

void FacebookNotificationSyncAdaptor::finalize(int accountId)
{
    Q_UNUSED(accountId)
    m_db.purgeOldNotifications(OLD_NOTIFICATION_LIMIT_IN_DAYS);
    m_db.sync();
    m_db.wait();
}

void FacebookNotificationSyncAdaptor::requestNotifications(int accountId, const QString &accessToken, const QString &until, const QString &pagingToken)
{
    // TODO: continuation requests need these two.  if exists, also set limit = 5000.
    // if not set, set "since" to the timestamp value.
    Q_UNUSED(until);
    Q_UNUSED(pagingToken);

    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("include_read")), QString(QLatin1String("true"))));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("locale")), QLocale::system().name()));
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
        setupReplyTimeout(accountId, reply);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request notifications from Facebook account with id %1"))
                .arg(accountId));
    }
}

void FacebookNotificationSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
    int sinceSpan = m_accountSyncProfile
                  ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("7")).toInt()
                  : 7;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok && parsed.contains(QLatin1String("summary"))) {
        QJsonArray data = parsed.value(QLatin1String("data")).toArray();

        foreach (const QJsonValue &entry, data) {
            QJsonObject object = entry.toObject();
            QDateTime createdTime = QDateTime::fromString(object.value(QLatin1String("created_time")).toString(), Qt::ISODate);
            createdTime.setTimeSpec(Qt::UTC);
            QDateTime updatedTime = QDateTime::fromString(object.value(QLatin1String("updated_time")).toString(), Qt::ISODate);
            updatedTime.setTimeSpec(Qt::UTC);

            if (createdTime.daysTo(QDateTime::currentDateTime()) > sinceSpan
                    && updatedTime.daysTo(QDateTime::currentDateTime()) > sinceSpan) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("notification for account %1 is more than %2 days old:\n    %3 - %4 - %5"))
                        .arg(accountId)
                        .arg(sinceSpan)
                        .arg(createdTime.toString(Qt::ISODate))
                        .arg(updatedTime.toString(Qt::ISODate))
                        .arg(object.value(QLatin1String("title")).toString()));
                continue;
            }

            QJsonObject sender = object.value(QLatin1String("from")).toObject();
            QJsonObject receiver = object.value(QLatin1String("to")).toObject();
            QJsonObject application = object.value(QLatin1String("application")).toObject();
            QJsonObject notificationObject = object.value(QLatin1String("object")).toObject();

            m_db.addFacebookNotification(object.value(QLatin1String("id")).toString(),
                                         sender.value(QLatin1String("id")).toString(),
                                         receiver.value(QLatin1String("id")).toString(),
                                         createdTime,
                                         updatedTime,
                                         object.value(QLatin1String("title")).toString(),
                                         object.value(QLatin1String("link")).toString(),
                                         application.value(QLatin1String("id")).toString(),
                                         notificationObject.value(QLatin1String("id")).toString(),
                                         object.value(QLatin1String("unread")).toDouble() != 0,
                                         accountId,
                                         clientId());
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
