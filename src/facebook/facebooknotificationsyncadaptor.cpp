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

#include "facebooknotificationsyncadaptor.h"
#include "syncservice.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtSql/QSqlQuery>

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <qjson/parser.h>
#else
#include <QJsonDocument>
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

//libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Service>
#include <Accounts/Account>
#include <Accounts/AccountService>
#include <Accounts/AuthData>

//libsignon-qt
#include <SignOn/Identity>
#include <SignOn/SessionData>
#include <SignOn/AuthSession>

//meegotouchevents/meventfeed
#include <meventfeed.h>

Q_DECLARE_METATYPE(SignOn::Identity*)

#define SOCIALD_FACEBOOK_NOTIFICATIONS_GROUPNAME QLatin1String("sociald-sync-facebook-notifications")

FacebookNotificationSyncAdaptor::FacebookNotificationSyncAdaptor(SyncService *parent)
    : SocialNetworkSyncAdaptor(parent)
    , m_contactFetchRequest(new QContactFetchRequest(this))
    , m_accountManager(new Accounts::Manager(QLatin1String("sync"), this))
    , m_qnam(new QNetworkAccessManager(this))
    , m_eventFeed(MEventFeed::instance())
{
    if (!m_eventFeed) {
        m_enabled = false;
        return; // can't sync to the local device's event feed, so not enabled.
    }

    // can sync, enabled
    m_enabled = true;

    // fetch all contacts.  We detect which contact a notification came from.
    if (m_contactFetchRequest) {
        QContactFetchHint cfh;
        cfh.setOptimizationHints(QContactFetchHint::NoRelationships | QContactFetchHint::NoActionPreferences);
        cfh.setDetailDefinitionsHint(QStringList()
                << QContactAvatar::DefinitionName
                << QContactName::DefinitionName
                << QContactNickname::DefinitionName
                << QContactPresence::DefinitionName);
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
    if (dataType != SyncService::dataType(SyncService::Notifications)) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: facebook notification sync adaptor was asked to sync %1"))
                .arg(dataType));
        return;
    }

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

    // three stage process.
    // 1) if an account has been removed, we need to purge the notifications we retrieved with it
    // 2) if an account has been added, we need to pull notifications for the account
    // 3) for existing accounts, pull new notifications for the existing account
    // Currently, we integrate with the device notifications via libeventfeed

    QList<int> newIds, purgeIds, updateIds;
    checkAccounts(&newIds, &purgeIds, &updateIds);
    purgeNotifications(purgeIds);
    updateNotifications(newIds);
    updateNotifications(updateIds);

    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("successfully triggered sync of notifications: %1 purged, %2 new, %3 updated accounts"))
            .arg(purgeIds.size()).arg(newIds.size()).arg(updateIds.size()));
}

void FacebookNotificationSyncAdaptor::checkAccounts(QList<int> *newIds, QList<int> *purgeIds, QList<int> *updateIds)
{
    QList<int> knownIds;
    QStringList knownIdStrings = accountIdsWithSyncTimestamp(QLatin1String("facebook"), SyncService::dataType(SyncService::Notifications));
    foreach (const QString &kis, knownIdStrings) {
        // XXX TODO: instead of QString::number(accountId) use fb user id.
        bool ok = true;
        int intId = kis.toInt(&ok);
        if (ok) {
            knownIds.append(intId);
        } else {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: unable to convert known id string to int: %1"))
                    .arg(kis));
        }
    }

    Accounts::AccountIdList currentIds = m_accountManager->accountList();
    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("have found %1 accounts which support a sync service; determining old/new/update sets..."))
            .arg(currentIds.size()));
    for (int i = 0; i < currentIds.size(); ++i) {
        int currId = currentIds.at(i);
        Accounts::Account *act = m_accountManager->account(currId);
        if (!act || act->providerName() != QLatin1String("facebook")) {
            continue; // not a facebook account.  Ignore it.
        }

        if (knownIds.contains(currId)) {
            knownIds.removeOne(currId);
            updateIds->append(currId);
        } else {
            newIds->append(currId);
        }
    }

    // anything left in knownIds must belong to an old, removed account.
    for (int i = 0; i < knownIds.size(); ++i) {
        purgeIds->append(knownIds.at(i));
    }
}

void FacebookNotificationSyncAdaptor::purgeNotifications(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // first, purge all data from libeventfeed
        QStringList purgeDataIds = syncedDatumLocalIdentifiers(QLatin1String("facebook"),
                SyncService::dataType(SyncService::Notifications),
                QString::number(pid)); // XXX TODO: use fb id instead of QString::number(accountId)

        bool ok = true;
        foreach (const QString &pdi, purgeDataIds) {
            QString eventIdStr = pdi.mid(23); // pdi is of form: "facebook-notifications-EVENTID"
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
                SyncService::dataType(SyncService::Notifications),
                QString::number(pid)); // XXX TODO: use fb id instead of QString::number(accountId)
    }
}

void FacebookNotificationSyncAdaptor::updateNotifications(const QList<int> &updateIds)
{
    foreach (int uid, updateIds) {
        Accounts::Account *act = m_accountManager->account(uid);
        if (!act) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: existing account with id %1 couldn't be retrieved"))
                    .arg(uid));
            continue;
        }

        // grab out a valid identity for the sync service.
        Accounts::ServiceList enabledSrvs = act->enabledServices();
        if (!enabledSrvs.size()) {
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("account with id %1 has no enabled sync service"))
                    .arg(uid));
            continue;
        }

        quint32 identityId = 0;
        Accounts::AccountService *asrv = 0;
        for (int i = 0; i < enabledSrvs.size(); ++i) {
            asrv = new Accounts::AccountService(act, enabledSrvs.at(i));
            if (!asrv) {
                continue;
            }
            identityId = asrv->authData().credentialsId();
            if (identityId != 0) {
                break;
            }

            asrv->deleteLater();
            asrv = 0;
        }

        if (identityId == 0) {
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("account with id %1 has no valid credentials"))
                    .arg(uid));
            continue;
        }

        SignOn::Identity *ident = SignOn::Identity::existingIdentity(identityId);
        if (!ident) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved"))
                    .arg(uid));
            continue;
        }

        // sign in - we need the access token to perform requests.
        // set UiPolicy to NO_USER_INTERACTION because we don't want
        // to show any UI if we don't already have a token.
        Accounts::AuthData authData(asrv->authData());
        asrv->deleteLater();
        SignOn::AuthSession *session = ident->createSession(authData.method());
        QVariantMap sessionData = authData.parameters();
        sessionData.insert(QLatin1String("UiPolicy"), SignOn::NoUserInteractionPolicy);
        connect(session, SIGNAL(error(SignOn::Error)), this, SLOT(signOnError(SignOn::Error)));
        connect(session, SIGNAL(response(SignOn::SessionData)), this, SLOT(signOnResponse(SignOn::SessionData)));
        QVariant identVar = QVariant::fromValue<SignOn::Identity*>(ident);
        session->setProperty("ident", identVar);
        session->setProperty("accountId", uid);
        session->process(sessionData, authData.mechanism());
    }
}

void FacebookNotificationSyncAdaptor::signOnError(const SignOn::Error &err)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession *>(sender());
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved:"))
            .arg(session->property("accountId").toInt()) << err.message());
    SignOn::Identity *ident = session->property("ident").value<SignOn::Identity*>();
    ident->destroySession(session); // XXX: is this safe?  Does it deleteLater()?
    ident->deleteLater();
}

void FacebookNotificationSyncAdaptor::signOnResponse(const SignOn::SessionData &sdata)
{
    QVariantMap data;
    QStringList sdpns = sdata.propertyNames();
    foreach (const QString &sdpn, sdpns) {
        data.insert(sdpn, sdata.getProperty(sdpn));
    }    

    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession *>(sender());
    int accountId = static_cast<int>(session->property("accountId").toUInt());

    if (data.contains(QLatin1String("AccessToken"))) {
        QString accessToken = data.value(QLatin1String("AccessToken")).toString();
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("signon response for account %1 contained access token %2"))
                .arg(accountId).arg(accessToken));
        requestNotifications(accountId, accessToken);
    } else {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("signon response for account with id %1 contained no access token"))
                .arg(accountId));
    }

    SignOn::Identity *ident = session->property("ident").value<SignOn::Identity*>();
    ident->destroySession(session); // XXX: is this safe?  Does it deleteLater()?
    ident->deleteLater();
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
    url.setQueryItems(queryItems);
    QNetworkReply *reply = m_qnam->get(QNetworkRequest(url));
    
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(finishedHandler()));
    } else {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to request notifications from Facebook account with id %1"))
                .arg(accountId));
    }
}

void FacebookNotificationSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
{
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: notification request with account %1 experienced error: %2"))
            .arg(sender()->property("accountId").toInt()).arg(err)); // incomprehensible enum value, but doesn't matter to users.
}

void FacebookNotificationSyncAdaptor::sslErrorsHandler(const QList<QSslError> &errs)
{
    QString sslerrs;
    foreach (const QSslError &e, errs) {
        sslerrs += e.errorString() + "; ";
    }
    if (errs.size() > 0) {
        sslerrs.chop(2);
    }
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: notification request with account %1 experienced ssl errors: %2"))
            .arg(sender()->property("accountId").toInt()).arg(sslerrs));
}

void FacebookNotificationSyncAdaptor::finishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return; // weird error.
    }

    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QDateTime lastSync = lastSyncTimestamp(QLatin1String("facebook"), SyncService::dataType(SyncService::Notifications), QString::number(accountId));
    QByteArray replyData = reply->readAll();
    disconnect(reply);
    reply->deleteLater();

    bool ok = false;
    QVariantMap parsed = parseReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("summary"))) {
        // we expect "data" and "summary"
        QVariantList data = parsed.value(QLatin1String("data")).toList();
        QVariantMap summary = parsed.value(QLatin1String("summary")).toMap();
        QVariantMap paging = parsed.value(QLatin1String("paging")).toMap(); // may not exist.

        if (!data.size()) {
            TRACE(SOCIALD_DEBUG,
                    QString(QLatin1String("no notifications received for account %1"))
                    .arg(accountId));
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

            // check to see if we need to post it to the event feed
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
                // TODO: use fbid to look up the contact directly, instead of heuristic detection.
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
                QString avatar = QLatin1String("image://theme/icon-s-service-facebook"); // default.
                QContact matchingContact = findMatchingContact(nameString);
                if (matchingContact != QContact()) {
                    QString originalNameString = nameString;
                    if (!matchingContact.displayLabel().isEmpty()) {
                        nameString = matchingContact.displayLabel();
                    } else if (!matchingContact.detail<QContactName>().customLabel().isEmpty()) {
                        nameString = matchingContact.detail<QContactName>().customLabel();
                    }
                    if (!matchingContact.detail<QContactAvatar>().imageUrl().toString().isEmpty()) {
                        avatar = matchingContact.detail<QContactAvatar>().imageUrl().toString();
                    }

                    TRACE(SOCIALD_DEBUG,
                            QString(QLatin1String("heuristically matched %1 as %2 with avatar %3"))
                            .arg(originalNameString).arg(nameString).arg(avatar));
                }

                // post the notification to the event feed.
                qlonglong localId = m_eventFeed->addItem(
                        avatar,                                   // icon
                        nameString,                               // title
                        title,                                    // body
                        QStringList(),                            // imageList
                        createdTime,                              // timestamp
                        QString(),                                // footer
                        false,                                    // isVideo
                        link,                                     // url
                        SOCIALD_FACEBOOK_NOTIFICATIONS_GROUPNAME, // sourceName
                        QLatin1String("Facebook"));               // sourceDisplayName // XXX TODO: per-account?

                // and store the fact that we have synced it to the event feed.
                markSyncedDatum(QString(QLatin1String("facebook-notifications-%1")).arg(QString::number(localId)),
                                QLatin1String("facebook"), SyncService::dataType(SyncService::Notifications),
                                QString::number(accountId), createdTime, QDateTime::currentDateTime(),
                                notificationId); // XXX TODO: instead of QString::number(accountId) use fb user id.

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
            if (prevUrl.hasQueryItem(QLatin1String("until"))) {
                until = prevUrl.queryItemValue(QLatin1String("until"));
                pagingToken = prevUrl.queryItemValue(QLatin1String("__paging_token"));
            } else {
                until = nextUrl.queryItemValue(QLatin1String("until"));
                pagingToken = nextUrl.queryItemValue(QLatin1String("__paging_token"));
            }

            // request the next page of results.
            requestNotifications(accountId, accessToken, until, pagingToken);
        } else {
            // finished sync for this account.
            updateLastSyncTimestamp(QLatin1String("facebook"),
                                    SyncService::dataType(SyncService::Notifications),
                                    QString::number(accountId),
                                    QDateTime::currentDateTime());
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("finished sync of facebook notifications for account %1"))
                    .arg(accountId));
        }
    } else {
        // error occurred during request.
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: unable to parse notification data from request with account %1; got: %2"))
                .arg(accountId).arg(QString::fromLatin1(replyData.constData())));
    }
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
            if (n.customLabel() == nameString ||
                    (firstAndLast.size() == 2 &&
                     n.firstName() == firstAndLast.at(0) &&
                     n.lastName() == firstAndLast.at(1))) {
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

    // this isn't a "hard error" since we can still post the notification event
    // but it is a "soft error" since we _should_ have the contact in our db.
    TRACE(SOCIALD_INFORMATION,
            QString(QLatin1String("unable to find matching contact with name: %1"))
            .arg(nameString));

    return QContact();
}

bool FacebookNotificationSyncAdaptor::haveAlreadyPostedNotification(const QString &notificationId, const QString &title, const QDateTime &createdTime)
{
    // TODO: also read notifications from libeventfeed and check that someone else hasn't posted it using the title/createdTime.
    Q_UNUSED(title);
    Q_UNUSED(createdTime);

    return (whenSyncedDatum(QLatin1String("facebook"), notificationId).isValid());
}

QVariantMap FacebookNotificationSyncAdaptor::parseReplyData(const QByteArray &replyData, bool *ok)
{
    QVariant parsed;

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    QJson::Parser jsonParser;
    parsed = jsonParser.parse(replyData, ok);
#else
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyData);
    *ok = !doc.isEmpty();
    parsed = doc.toVariant();
#endif

    if (*ok && parsed.type() == QVariant::Map) {
        return parsed.toMap();
    }
    *ok = false;
    return QVariantMap();
}
