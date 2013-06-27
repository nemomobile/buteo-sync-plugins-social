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

//nemo-qml-plugins/notifications
#include <notification.h>

//meegotouchevents/meventfeed
#include <meventfeed.h>

#define SOCIALD_FACEBOOK_NOTIFICATIONS_ID_PREFIX QLatin1String("facebook-notifications-")
#define SOCIALD_FACEBOOK_NOTIFICATIONS_GROUPNAME QLatin1String("sociald-sync-facebook-notifications")
#define QTCONTACTS_SQLITE_AVATAR_METADATA QLatin1String("AvatarMetadata")

// currently, we integrate with the device notifications via nemo-qml-plugin-notification

FacebookNotificationSyncAdaptor::FacebookNotificationSyncAdaptor(SyncService *syncService, QObject *parent)
    : FacebookDataTypeSyncAdaptor(syncService, SyncService::Notifications, parent)
    , m_contactFetchRequest(new QContactFetchRequest(this))
    , m_eventFeed(MEventFeed::instance())
{
    if (!m_eventFeed) {
        m_enabled = false;
        return; // can't sync to the local device's event feed, so not enabled.
    }

    // can sync, enabled
    m_enabled = true;
    m_status = SocialNetworkSyncAdaptor::Inactive;

    // fetch all contacts.  We detect which contact a notification came from.
    // XXX TODO: we really shouldn't do this, we should do it on demand instead
    // of holding the contacts in memory.  If qtcontacts-tracker doesn't observe
    // our fetch hint, it could return an obscenely large amount of data...
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
}

FacebookNotificationSyncAdaptor::~FacebookNotificationSyncAdaptor()
{
}

void FacebookNotificationSyncAdaptor::sync(const QString &dataType)
{
    // refresh local cache of contacts.
    // we do this asynchronous request in parallel to the sync code below
    // since the network request round-trip times should far exceed the
    // local database fetch.  If not, then the current sync run will
    // still work, but the "notifications is from which contact" detection
    // will be using slightly stale data.
    if (m_contactFetchRequest &&
            (m_contactFetchRequest->state() == QContactAbstractRequest::InactiveState ||
             m_contactFetchRequest->state() == QContactAbstractRequest::FinishedState)) {
        m_contactFetchRequest->start();
    }

    // clear our count of notifications synced per account for this sync run
    m_notificationsCount.clear();

    // call superclass impl.
    FacebookDataTypeSyncAdaptor::sync(dataType);
}

void FacebookNotificationSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // first, purge all data from nemo notifications
        QStringList purgeDataIds = syncedDatumLocalIdentifiers(QLatin1String("facebook"),
                SyncService::dataType(SyncService::Notifications),
                QString::number(pid));

        bool ok = true;
        int prefixSize = QString(SOCIALD_FACEBOOK_NOTIFICATIONS_ID_PREFIX).size();
        foreach (const QString &pdi, purgeDataIds) {
            QString notifEventIdStr = pdi.mid(prefixSize); // pdi is of form: "facebook-notifications-NOTIFICATIONID"
            qlonglong notificationEventId = notifEventIdStr.toLongLong(&ok);
            if (ok) {
                m_eventFeed->removeItem(notificationEventId);
                // XXX TODO: remove the notification from the notifications feed also?  How?
            } else {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to convert notification id string to int: %1"))
                        .arg(pdi));
            }
        }

        // second, purge all data from our database
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
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("include_read")), QString(QLatin1String("true"))));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    QUrl url(QLatin1String("https://graph.facebook.com/me/notifications"));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = m_qnam->get(QNetworkRequest(url));

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
    QVariantMap parsed = FacebookDataTypeSyncAdaptor::parseReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("summary"))) {
        // we expect "data" and "summary"
        QVariantList data = parsed.value(QLatin1String("data")).toList();
        QVariantMap summary = parsed.value(QLatin1String("summary")).toMap();
        QVariantMap paging = parsed.value(QLatin1String("paging")).toMap(); // may not exist.

        if (!data.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no notifications received for account %1"))
                    .arg(accountId));
            decrementSemaphore(accountId);
            return;
        }

        bool needMorePages = true;
        bool postedNew = false;
        for (int i = 0; i < data.size(); ++i) {
            QVariantMap currData = data.at(i).toMap();
            QDateTime createdTime = QDateTime::fromString(currData.value(QLatin1String("created_time")).toString(), Qt::ISODate);
            QString title = currData.value(QLatin1String("title")).toString();
            QString link = currData.value(QLatin1String("link")).toString();
            QString notificationId = currData.value(QLatin1String("id")).toString();
            QVariantMap from = currData.value(QLatin1String("from")).toMap();

            // check to see if we need to post it to the notifications feed
            if (lastSync.isValid() && createdTime < lastSync) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("notification for account %1 came after last sync:"))
                        .arg(accountId) << "    " << createdTime << ":" << title);
                needMorePages = false; // don't fetch more pages of results.
                break;                 // all subsequent notifications will be even older.
            } else if (createdTime.daysTo(QDateTime::currentDateTime()) > 7) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("notification for account %1 is more than a week old:\n"))
                        .arg(accountId) << "    " << createdTime << ":" << title);
                needMorePages = false; // don't fetch more pages of results.
                break;                 // all subsequent notifications will be even older.
            } else if (haveAlreadyPostedNotification(notificationId, title, createdTime)) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("notification for account %1 has already been posted:\n"))
                        .arg(accountId) << "    " << createdTime << ":" << title);
            } else {
                // attempt to scrape the contact name from the notification title.
                // XXX TODO: use fbid to look up the contact directly, instead of heuristic detection.
                QString nameString;
                if (!from.isEmpty() && !from.value(QLatin1String("name")).toString().isEmpty()) {
                    // grab the name string from the "from" data in the notification.
                    nameString = from.value(QLatin1String("name")).toString();
                } else {
                    // fallback: scrape the name from the notification title.
                    int indexOfFirstSpace = title.indexOf(' ');
                    int indexOfSecondSpace = title.indexOf(' ', indexOfFirstSpace);
                    nameString = title.mid(0, indexOfSecondSpace);
                }
                QString avatar = QLatin1String("icon-s-service-facebook"); // default.
                QContact matchingContact = findMatchingContact(nameString);
                if (matchingContact != QContact()) {
                    QContactDisplayLabel displayLabel = matchingContact.detail<QContactDisplayLabel>();
                    QContactName contactName = matchingContact.detail<QContactName>();
                    QString originalNameString = nameString;
                    if (!displayLabel.label().isEmpty()) {
                        nameString = displayLabel.label();
                    } else if (!contactName.value<QString>(QContactName__FieldCustomLabel).isEmpty()) {
                        nameString = contactName.value<QString>(QContactName__FieldCustomLabel);
                    }

                    QList<QContactAvatar> allAvatars = matchingContact.details<QContactAvatar>();
                    bool foundFacebookPicture = false;
                    foreach (const QContactAvatar &avat, allAvatars) {
                        // TODO: avat.value(QTCONTACTS_SQLITE_AVATAR_METADATA) == QLatin1String("picture")
                        if (!avat.imageUrl().toString().isEmpty()) {
                            // found avatar synced from Facebook sociald sync adaptor
                            avatar = avat.imageUrl().toString();
                            foundFacebookPicture = true;
                            break;
                        }
                    }
                    if (!foundFacebookPicture && !matchingContact.detail<QContactAvatar>().imageUrl().toString().isEmpty()) {
                        // fallback.
                        avatar = matchingContact.detail<QContactAvatar>().imageUrl().toString();
                    }

                    TRACE(SOCIALD_DEBUG,
                            QString(QLatin1String("heuristically matched %1 as %2 with avatar %3"))
                            .arg(originalNameString).arg(nameString).arg(avatar));
                }

                // Set timespec to make conversion to local time to work properly
                createdTime.setTimeSpec(Qt::UTC);

                //: The title text displayed for Facebook notifications in the events view
                //% "New Facebook notification!"
                QString NOTIFICATION_CATEGORY_TRANSLATED_TEXT = qtTrId("qtn_social_notifications_new_facebook");

                // publish the notification to the events feed.
                qlonglong eventId = m_eventFeed->addItem(
                        QLatin1String("icon-s-service-facebook"),
                        NOTIFICATION_CATEGORY_TRANSLATED_TEXT,
                        title,
                        QStringList(),
                        createdTime,
                        QString(),
                        false,
                        link,
                        SOCIALD_FACEBOOK_NOTIFICATIONS_GROUPNAME, // sourceName
                        QLatin1String("Facebook Notifications")); // sourceDisplayName // XXX TODO: per-account?

                if (eventId == 0) {
                    // failed.
                    TRACE(SOCIALD_ERROR,
                            QString(QLatin1String("error: failed to publish notification: %1"))
                            .arg(title));
                } else {
                    // increment our count of notifications synced for this account
                    int nCount = m_notificationsCount.value(accountId) + 1;
                    m_notificationsCount.insert(accountId, nCount);

                    // and store the fact that we have synced it to the notifications feed.
                    markSyncedDatum(QString(QLatin1String("facebook-notifications-%1")).arg(QString::number(eventId)),
                                    QLatin1String("facebook"), SyncService::dataType(SyncService::Notifications),
                                    QString::number(accountId), createdTime, QDateTime::currentDateTime(),
                                    notificationId); // XXX TODO: instead of QString::number(accountId) use fb user id.
                }

                // if we didn't post anything new, we don't try to fetch more.
                postedNew = true;
            }
        }

        // paging if we need to retrieve more notifications
        if (postedNew && needMorePages && (paging.contains("previous") || paging.contains("next"))) {
            QString until;
            QString pagingToken;
            // The FB api is terrible, and so we don't know in advance which paging url
            // to use (as it will change depending on whether the current request was
            // a first request, or itself a paging request).
            QUrl prevUrl(paging.value("previous").toString());
            QUrl nextUrl(paging.value("next").toString());
            QUrlQuery prevUrlQuery(prevUrl);
            if (prevUrlQuery.hasQueryItem(QLatin1String("until"))) {
                until = prevUrlQuery.queryItemValue(QLatin1String("until"));
                pagingToken = prevUrlQuery.queryItemValue(QLatin1String("__paging_token"));
            } else {
                QUrlQuery nextUrlQuery(nextUrl);
                until = nextUrlQuery.queryItemValue(QLatin1String("until"));
                pagingToken = nextUrlQuery.queryItemValue(QLatin1String("__paging_token"));
            }
            // request the next page of results.
            requestNotifications(accountId, accessToken, until, pagingToken);
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

void FacebookNotificationSyncAdaptor::contactFetchStateChangedHandler(QContactAbstractRequest::State newState)
{
    // update our local cache of contacts.
    if (m_contactFetchRequest && newState == QContactAbstractRequest::FinishedState) {
        m_contacts = m_contactFetchRequest->contacts();
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("finished refreshing local cache of contacts, have %1"))
                .arg(m_contacts.size()));
    }
}

QContact FacebookNotificationSyncAdaptor::findMatchingContact(const QString &nameString) const
{
    // TODO: This heuristic detection could definitely be improved.
    // EG: instead of scraping the name string from the title, we
    // could get the actual fb id from the notification and then
    // look the contact up directly.  But we currently don't have
    // FB contact syncing done properly, so...
    if (nameString.isEmpty()) {
        return QContact();
    }

    QStringList firstAndLast = nameString.split(' '); // TODO: better detection of FN/LN

    foreach (const QContact &c, m_contacts) {
        QList<QContactName> names = c.details<QContactName>();
        foreach (const QContactName &n, names) {
            if (n.value<QString>(QContactName__FieldCustomLabel) == nameString ||
                    (firstAndLast.size() >= 2 &&
                     n.firstName() == firstAndLast.at(0) &&
                     n.lastName() == firstAndLast.at(firstAndLast.size()-1))) {
                return c;
            }
        }

        QList<QContactNickname> nicknames = c.details<QContactNickname>();
        foreach (const QContactNickname &n, nicknames) {
            if (n.nickname() == nameString) {
                return c;
            }
        }

        QList<QContactPresence> presences = c.details<QContactPresence>();
        foreach (const QContactPresence &p, presences) {
            if (p.nickname() == nameString) {
                return c;
            }
        }
    }

    // this isn't a "hard error" since we can still post the notification
    // but it is a "soft error" since we _should_ have the contact in our db.
    TRACE(SOCIALD_INFORMATION,
            QString(QLatin1String("unable to find matching contact with name: %1"))
            .arg(nameString));

    return QContact();
}

bool FacebookNotificationSyncAdaptor::haveAlreadyPostedNotification(const QString &notificationId, const QString &title, const QDateTime &createdTime)
{
    Q_UNUSED(title);
    Q_UNUSED(createdTime);

    return (whenSyncedDatum(QLatin1String("facebook"), notificationId).isValid());
}

void FacebookNotificationSyncAdaptor::incrementSemaphore(int accountId)
{
    int semaphoreValue = m_accountSyncSemaphores.value(accountId);
    semaphoreValue += 1;
    m_accountSyncSemaphores.insert(accountId, semaphoreValue);
    TRACE(SOCIALD_DEBUG, QString(QLatin1String("incremented busy semaphore for account %1 to %2")).arg(accountId).arg(semaphoreValue));

    if (m_status == SocialNetworkSyncAdaptor::Inactive) {
        changeStatus(SocialNetworkSyncAdaptor::Busy);
    }
}

void FacebookNotificationSyncAdaptor::decrementSemaphore(int accountId)
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
        // now post a notification to the notifications feed about the FB notifications.
        int notificationsCount = m_notificationsCount.value(accountId);
        m_notificationsCount.insert(accountId, 0);
        if (notificationsCount > 0) {
            //: The title of the Facebook Notifications device notification
            //% "You have %1 new Facebook notification(s)!"
            QString title = qtTrId("sociald_facebook_posts-notification_title").arg(notificationsCount);
            Notification *notif = new Notification;
            notif->setCategory(QLatin1String("x-nemo.social.facebook.notification"));
            notif->setSummary(title);
            notif->setBody(QString());
            notif->setPreviewSummary(title);
            notif->setPreviewBody(QString());
            notif->setItemCount(notificationsCount);
            notif->setTimestamp(QDateTime::currentDateTime());
            notif->publish();
            delete notif;
        }

        // finished all outstanding requests for Notifications sync for this account.
        // update the sync time for this user's Notifications in the global sociald database.
        updateLastSyncTimestamp(QLatin1String("facebook"),
                                SyncService::dataType(SyncService::Notifications),
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
            TRACE(SOCIALD_INFORMATION, QString(QLatin1String("Finished Facebook Notifications sync at: %1"))
                                       .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
            changeStatus(SocialNetworkSyncAdaptor::Inactive);
        }
    }
}
