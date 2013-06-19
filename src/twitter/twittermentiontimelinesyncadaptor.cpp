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

#include "twittermentiontimelinesyncadaptor.h"
#include "twittersyncadaptor.h"
#include "syncservice.h"
#include "trace.h"

#include <QtCore/QPair>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QUrlQuery>
#endif

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

#define SOCIALD_TWITTER_MENTIONS_ID_PREFIX QLatin1String("twitter-mentions-")
#define SOCIALD_TWITTER_MENTIONS_GROUPNAME QLatin1String("sociald-sync-twitter-mentions")
#define QTCONTACTS_SQLITE_AVATAR_METADATA QLatin1String("AvatarMetadata")

// currently, we integrate with the device notifications via nemo-qml-plugin-notification

TwitterMentionTimelineSyncAdaptor::TwitterMentionTimelineSyncAdaptor(SyncService *parent, TwitterSyncAdaptor *fbsa)
    : TwitterDataTypeSyncAdaptor(parent, fbsa, SyncService::Notifications)
    , m_contactFetchRequest(new QContactFetchRequest(this))
{
    //: The text displayed for Twitter notifications on the lock screen
    //% "New Twitter notification!"
    QString NOTIFICATION_CATEGORY_TRANSLATED_TEXT = qtTrId("qtn_social_notifications_new_twitter");

    // can sync, enabled
    m_enabled = true;
    m_status = SocialNetworkSyncAdaptor::Inactive;

    // fetch all contacts.  We detect which contact a mention came from.
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
}

TwitterMentionTimelineSyncAdaptor::~TwitterMentionTimelineSyncAdaptor()
{
}

void TwitterMentionTimelineSyncAdaptor::sync(const QString &dataType)
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

    // call superclass impl.
    TwitterDataTypeSyncAdaptor::sync(dataType);
}

void TwitterMentionTimelineSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // first, purge all data from nemo notifications
        QStringList purgeDataIds = syncedDatumLocalIdentifiers(QLatin1String("twitter"),
                SyncService::dataType(SyncService::Notifications),
                QString::number(pid));

        bool ok = true;
        int prefixSize = QString(SOCIALD_TWITTER_MENTIONS_ID_PREFIX).size();
        foreach (const QString &pdi, purgeDataIds) {
            QString notifIdStr = pdi.mid(prefixSize); // pdi is of form: "twitter-mentions-NOTIFICATIONID"
            qlonglong notificationId = notifIdStr.toLongLong(&ok);
            if (ok) {
                TRACE(SOCIALD_INFORMATION,
                        QString(QLatin1String("TODO: purge notifications for deleted account %1: %2 = %3"))
                        .arg(pid).arg(pdi).arg(notificationId));
            } else {
                TRACE(SOCIALD_ERROR,
                        QString(QLatin1String("error: unable to convert notification id string to int: %1"))
                        .arg(pdi));
            }
        }

        // second, purge all data from our database
        removeAllData(QLatin1String("twitter"),
                SyncService::dataType(SyncService::Notifications),
                QString::number(pid));
    }
}

void TwitterMentionTimelineSyncAdaptor::beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret)
{
    requestNotifications(accountId, oauthToken, oauthTokenSecret);
}

void TwitterMentionTimelineSyncAdaptor::requestNotifications(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &sinceTweetId)
{
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("count")), QString(QLatin1String("50"))));
    if (!sinceTweetId.isEmpty()) {
        queryItems.append(QPair<QString, QString>(QString(QLatin1String("since_id")), sinceTweetId));
    }
    QString baseUrl = QLatin1String("https://api.twitter.com/1.1/statuses/mentions_timeline.json");
    QUrl url(baseUrl);
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    url.setQueryItems(queryItems);
#else
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
#endif

    QNetworkRequest nreq(url);
    nreq.setRawHeader("Authorization", authorizationHeader(
            accountId, oauthToken, oauthTokenSecret,
            QLatin1String("GET"), baseUrl, queryItems).toLatin1());
    QNetworkReply *reply = m_tsa->m_qnam->get(nreq);
    
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("oauthToken", oauthToken);
        reply->setProperty("oauthTokenSecret", oauthTokenSecret);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        incrementSemaphore(accountId);
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request mention timeline notifications from Twitter account with id %1"))
                .arg(accountId));
    }
}

void TwitterMentionTimelineSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    int accountId = reply->property("accountId").toInt();
    QString oauthToken = reply->property("oauthToken").toString();
    QString oauthTokenSecret = reply->property("oauthTokenSecret").toString();
    QDateTime lastSync = lastSyncTimestamp(QLatin1String("twitter"), SyncService::dataType(SyncService::Notifications), QString::number(accountId));
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariant parsed = TwitterDataTypeSyncAdaptor::parseReplyData(replyData, &ok);
    if (ok && parsed.type() == QVariant::List) {
        QVariantList data = parsed.toList();
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
            QDateTime createdTime = parseTwitterDateTime(currData.value(QLatin1String("created_at")).toString());
            QString mention_id = currData.value(QLatin1String("id_str")).toString();
            QString text = currData.value(QLatin1String("text")).toString();
            QVariantMap user = currData.value(QLatin1String("user")).toMap();
            QString user_id = user.value(QLatin1String("id_str")).toString();
            QString user_name = user.value(QLatin1String("name")).toString();
            QString user_screen_name = user.value(QLatin1String("screen_name")).toString();
            QString link = QLatin1String("https://twitter.com/") + user_screen_name + QLatin1String("/status/") + mention_id;

            // check to see if we need to post it to the notifications feed
            if (lastSync.isValid() && createdTime < lastSync) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("notification for account %1 came after last sync:"))
                        .arg(accountId) << "    " << createdTime << ":" << text);
                needMorePages = false; // don't fetch more pages of results.
                break;                 // all subsequent notifications will be even older.
            } else if (createdTime.daysTo(QDateTime::currentDateTime()) > 7) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("notification for account %1 is more than a week old:\n"))
                        .arg(accountId) << "    " << createdTime << ":" << text);
                needMorePages = false; // don't fetch more pages of results.
                break;                 // all subsequent notifications will be even older.
            } else if (haveAlreadyPostedNotification(mention_id, text, createdTime)) {
                TRACE(SOCIALD_DEBUG,
                        QString(QLatin1String("notification for account %1 has already been posted:\n"))
                        .arg(accountId) << "    " << createdTime << ":" << text);
            } else {
                // XXX TODO: use twitter user id to look up the contact directly, instead of heuristic detection.
                QString nameString = user_name;
                QString avatar = QLatin1String("icon-s-service-twitter"); // default.
                QContact matchingContact = findMatchingContact(nameString);
                if (matchingContact != QContact()) {
                    QString originalNameString = nameString;
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
                    if (!matchingContact.displayLabel().isEmpty()) {
                        nameString = matchingContact.displayLabel();
                    } else if (!matchingContact.detail<QContactName>().customLabel().isEmpty()) {
                        nameString = matchingContact.detail<QContactName>().customLabel();
                    }
#else
                    QContactName contactName = matchingContact.detail<QContactName>();
                    QString firstName = contactName.firstName();
                    if (!firstName.isEmpty()) {
                        nameString = firstName;
                    }

                    QString lastName = contactName.lastName();
                    if (!lastName.isEmpty()) {
                        nameString = (!firstName.isEmpty() ? (firstName + " ") : "") + lastName;
                    }
#endif

                    QList<QContactAvatar> allAvatars = matchingContact.details<QContactAvatar>();
                    bool foundTwitterProfileImage = false;
                    foreach (const QContactAvatar &avat, allAvatars) {
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
                        if (avat.value(QTCONTACTS_SQLITE_AVATAR_METADATA) == QLatin1String("profile")
                                && !avat.imageUrl().toString().isEmpty()) {
#else
                        if (!avat.imageUrl().toString().isEmpty()) {
#endif
                            // found avatar synced from Twitter sociald sync adaptor
                            avatar = avat.imageUrl().toString();
                            foundTwitterProfileImage = true;
                            break;
                        }
                    }
                    if (!foundTwitterProfileImage && !matchingContact.detail<QContactAvatar>().imageUrl().toString().isEmpty()) {
                        // fallback.
                        avatar = matchingContact.detail<QContactAvatar>().imageUrl().toString();
                    }

                    TRACE(SOCIALD_DEBUG,
                            QString(QLatin1String("heuristically matched %1 as %2 with avatar %3"))
                            .arg(originalNameString).arg(nameString).arg(avatar));
                }

                // post the notification to the notifications feed.
                Notification *notif = new Notification;
                notif->setCategory(QLatin1String("x-nemo.social.twitter.mention"));
                notif->setSummary(text);
                notif->setBody(text);
                notif->setPreviewSummary(nameString);
                notif->setPreviewBody(text);
                notif->setItemCount(1);
                notif->setTimestamp(createdTime);
                notif->setRemoteDBusCallServiceName("org.sailfishos.browser");
                notif->setRemoteDBusCallObjectPath("/");
                notif->setRemoteDBusCallInterface("org.sailfishos.browser");
                notif->setRemoteDBusCallMethodName("openUrl");
                QStringList openUrlArgs; openUrlArgs << link;
                notif->setRemoteDBusCallArguments(QVariantList() << openUrlArgs);
                notif->publish();
                qlonglong localId = (0 + notif->replacesId());
                if (localId == 0) {
                    // failed.
                    TRACE(SOCIALD_ERROR,
                            QString(QLatin1String("error: failed to publish notification: %1"))
                            .arg(text));
                } else {
                    // and store the fact that we have synced it to the notifications feed.
                    markSyncedDatum(QString(QLatin1String("twitter-notifications-%1")).arg(QString::number(localId)),
                                    QLatin1String("twitter"), SyncService::dataType(SyncService::Notifications),
                                    QString::number(accountId), createdTime, QDateTime::currentDateTime(),
                                    mention_id);
                }

                // if we didn't post anything new, we don't try to fetch more.
                postedNew = true;
                delete notif;
            }
        }

        if (postedNew && needMorePages) {
            // XXX TODO: paging?
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

void TwitterMentionTimelineSyncAdaptor::contactFetchStateChangedHandler(QContactAbstractRequest::State newState)
{
    // update our local cache of contacts.
    if (m_contactFetchRequest && newState == QContactAbstractRequest::FinishedState) {
        m_contacts = m_contactFetchRequest->contacts();
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("finished refreshing local cache of contacts, have %1"))
                .arg(m_contacts.size()));
    }
}

QContact TwitterMentionTimelineSyncAdaptor::findMatchingContact(const QString &nameString) const
{
    // TODO: This heuristic detection could definitely be improved.
    // EG: instead of scraping the name string from the title, we
    // could get the actual twitter id from the mention and then
    // look the contact up directly.  But we currently don't have
    // Twitter contact syncing done properly, so...
    if (nameString.isEmpty()) {
        return QContact();
    }

    QStringList firstAndLast = nameString.split(' '); // TODO: better detection of FN/LN

    foreach (const QContact &c, m_contacts) {
        QList<QContactName> names = c.details<QContactName>();
        foreach (const QContactName &n, names) {
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
            if (n.customLabel() == nameString ||
                    (firstAndLast.size() >= 2 &&
                     n.firstName() == firstAndLast.at(0) &&
                     n.lastName() == firstAndLast.at(firstAndLast.size()-1))) {
#else
            if (firstAndLast.size() >= 2 &&
                     n.firstName() == firstAndLast.at(0) &&
                     n.lastName() == firstAndLast.at(firstAndLast.size()-1)) {
#endif
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

bool TwitterMentionTimelineSyncAdaptor::haveAlreadyPostedNotification(const QString &mentionId, const QString &text, const QDateTime &createdTime)
{
    Q_UNUSED(text);
    Q_UNUSED(createdTime);

    return (whenSyncedDatum(QLatin1String("twitter"), mentionId).isValid());
}

void TwitterMentionTimelineSyncAdaptor::incrementSemaphore(int accountId)
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

void TwitterMentionTimelineSyncAdaptor::decrementSemaphore(int accountId)
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
        // finished all outstanding requests for Notifications sync for this account.
        // update the sync time for this user's Notifications in the global sociald database.
        updateLastSyncTimestamp(QLatin1String("twitter"),
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
            TRACE(SOCIALD_INFORMATION, QString(QLatin1String("Finished Twitter Notifications sync at: %1"))
                                       .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
            m_status = SocialNetworkSyncAdaptor::Inactive;
            emit statusChanged();
        }
    }
}
