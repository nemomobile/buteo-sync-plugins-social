/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
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

#include "facebooknotificationsyncadaptor.h"
#include "trace.h"

#include <QUrlQuery>
#include <QDebug>

static const int OLD_NOTIFICATION_LIMIT_IN_DAYS = 21;
static const int NOTIFICATIONS_LIMIT = 30;

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

void FacebookNotificationSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    m_db.removeNotifications(oldId);
    m_db.sync();
    m_db.wait();
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
    // continuation requests require until+paging token.
    // if not set, set "since" to the timestamp value.
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("include_read")), QString(QLatin1String("true"))));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("locale")), QLocale::system().name()));
    QUrl url(graphAPI(QLatin1String("/me/notifications")));
    if (pagingToken.isEmpty()) {
        int sinceSpan = m_accountSyncProfile
                      ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("7")).toInt()
                      : 7;
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("since")),
                          QString::number(QDateTime::currentDateTime().addDays(-1 * sinceSpan).toTime_t())));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("limit")), QString::number(NOTIFICATIONS_LIMIT)));
    } else {
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("limit")), QString::number(NOTIFICATIONS_LIMIT)));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("until")), until));
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("__paging_token")), pagingToken));
    }

    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));

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
        SOCIALD_LOG_ERROR("unable to request notifications from Facebook account with id" << accountId);
    }
}

void FacebookNotificationSyncAdaptor::finishedHandler()
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
    int sinceSpan = m_accountSyncProfile
                  ? m_accountSyncProfile->key(Buteo::KEY_SYNC_SINCE_DAYS_PAST, QStringLiteral("7")).toInt()
                  : 7;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok && parsed.contains(QLatin1String("data"))) {
        QJsonArray data = parsed.value(QLatin1String("data")).toArray();

        bool needNextPage = false;
        bool seenOldNotification = false;
        foreach (const QJsonValue &entry, data) {
            QJsonObject object = entry.toObject();
            QDateTime createdTime = QDateTime::fromString(object.value(QLatin1String("created_time")).toString(), Qt::ISODate);
            createdTime.setTimeSpec(Qt::UTC);
            QDateTime updatedTime = QDateTime::fromString(object.value(QLatin1String("updated_time")).toString(), Qt::ISODate);
            updatedTime.setTimeSpec(Qt::UTC);

            if (createdTime.daysTo(QDateTime::currentDateTime()) > sinceSpan
                    && updatedTime.daysTo(QDateTime::currentDateTime()) > sinceSpan) {
                SOCIALD_LOG_DEBUG("notification for account" << accountId <<
                                  "is more than" << sinceSpan << "days old:\n" <<
                                  createdTime.toString(Qt::ISODate) << "-" <<
                                  updatedTime.toString(Qt::ISODate) << "-" <<
                                  object.value(QLatin1String("title")).toString());
                seenOldNotification = true;
                needNextPage = false;
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

            if (!seenOldNotification) {
                needNextPage = true;
            }
        }

        if (needNextPage && parsed.contains(QLatin1String("paging"))) {
            // we don't actually request the next page of results
            // since the sync schedule has such a small interval,
            // 30 notifications at a time should be plenty,
            // and we want to avoid performing spurious network activity.
            Q_UNUSED(accessToken)
            /*
            QString nextPage = parsed.value(QLatin1String("paging")).toObject().value(QLatin1String("next")).toString();
            QUrl nextPageUrl(nextPage);

            // instead of doing this, we could just pass the nextPageUrl directly to the requestNotifications function
            QUrlQuery npuQuery(nextPageUrl.query());
            QString until = npuQuery.queryItemValue(QStringLiteral("until"));
            QString pagingToken = npuQuery.queryItemValue(QStringLiteral("__paging_token"));

            if (!nextPage.isEmpty() && !until.isEmpty() && !pagingToken.isEmpty()) {
                SOCIALD_LOG_DEBUG("another page of notifications exists for account" << accountId << ":" << nextPage);
                requestNotifications(accountId, accessToken, until, pagingToken);
            }
            */
        }
    } else {
        // error occurred during request.
        SOCIALD_LOG_ERROR("unable to parse notification data from request with account" << accountId <<
                          "got:" << QString::fromLatin1(replyData.constData()));
    }

    // we're finished this request.  Decrement our busy semaphore.
    decrementSemaphore(accountId);
}
