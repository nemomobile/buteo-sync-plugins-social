/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
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

#include "googletwowaycontactsyncadaptor.h"
#include "googlecontactstream.h"
#include "googlecontactatom.h"
#include "googlecontactimagedownloader.h"

#include "constants_p.h"
#include "trace.h"

#include <twowaycontactsyncadapter_impl.h>
#include <qtcontacts-extensions_manager_impl.h>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtGui/QImageReader>

#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactIntersectionFilter>
#include <QtContacts/QContact>
#include <QtContacts/QContactSyncTarget>
#include <QtContacts/QContactGuid>
#include <QtContacts/QContactName>
#include <QtContacts/QContactNickname>
#include <QtContacts/QContactAvatar>
#include <QtContacts/QContactUrl>
#include <QtContacts/QContactGender>
#include <QtContacts/QContactNote>
#include <QtContacts/QContactBirthday>
#include <QtContacts/QContactPhoneNumber>
#include <QtContacts/QContactEmailAddress>

#include <Accounts/Manager>
#include <Accounts/Account>

#define SOCIALD_GOOGLE_CONTACTS_SYNCTARGET QLatin1String("google")
#define SOCIALD_GOOGLE_MAX_CONTACT_ENTRY_RESULTS 50

static const char *IMAGE_DOWNLOADER_TOKEN_KEY = "url";
static const char *IMAGE_DOWNLOADER_ACCOUNT_ID_KEY = "account_id";
static const char *IMAGE_DOWNLOADER_IDENTIFIER_KEY = "identifier";

GoogleTwoWayContactSyncAdaptor::GoogleTwoWayContactSyncAdaptor(QObject *parent)
    : GoogleDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Contacts, parent)
    , QtContactsSqliteExtensions::TwoWayContactSyncAdapter(QStringLiteral("google"))
    , m_workerObject(new GoogleContactImageDownloader())
{
    connect(m_workerObject, &AbstractImageDownloader::imageDownloaded,
            this, &GoogleTwoWayContactSyncAdaptor::imageDownloaded);

    // can sync, enabled
    setInitialActive(true);
}

GoogleTwoWayContactSyncAdaptor::~GoogleTwoWayContactSyncAdaptor()
{
    delete m_workerObject;
}

QString GoogleTwoWayContactSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("google-contacts");
}

void GoogleTwoWayContactSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    m_apiRequestsRemaining[accountId] = 99; // assume we can make up to 99 requests per sync, before being throttled.

    // call superclass impl.
    GoogleDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void GoogleTwoWayContactSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        purgeAccount(pid);
    }
}

void GoogleTwoWayContactSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    Accounts::Account *account = accountManager->account(accountId);
    if (!account) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("unable to load Google account %1"))
              .arg(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    account->selectService(Accounts::Service());
    QString emailAddress = account->valueAsString(QStringLiteral("default_credentials_username"));
    if (emailAddress.isEmpty()) {
        emailAddress = account->valueAsString(QStringLiteral("name"));
    }
    account->deleteLater();
    if (emailAddress.isEmpty()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("unable to determine email address for Google account %1"))
              .arg(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    // clear our cache lists if necessary.
    m_localChanges[accountId].clear();
    m_remoteAddMods[accountId].clear();
    m_remoteDels[accountId].clear();
    m_accessTokens[accountId] = accessToken;
    m_emailAddresses[accountId] = emailAddress;

    QDateTime remoteSince;
    if (!initSyncAdapter(QString::number(accountId))
            || !readSyncStateData(&remoteSince, QString::number(accountId))
            || !readExtraStateData(accountId)) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("unable to init sync adapter - aborting sync Google contacts with account %1"))
              .arg(accountId));
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    incrementSemaphore(accountId);
    if (m_myContactsGroupAtomIds[accountId].isEmpty()) {
        // we need to determine the atom id of the My Contacts group
        // because we upload newly added contacts to that group.
        requestData(accountId, m_accessTokens[accountId], 0, QString(), remoteSince, true); // true = isGroupRequest
    } else {
        // we can just sync changes immediately
        determineRemoteChanges(remoteSince, QString::number(accountId));
    }
    decrementSemaphore(accountId);
}

void GoogleTwoWayContactSyncAdaptor::determineRemoteChanges(const QDateTime &remoteSince, const QString &accountId)
{
    int accId = accountId.toInt();
    requestData(accId, m_accessTokens[accId], 0, QString(), remoteSince);
}

void GoogleTwoWayContactSyncAdaptor::requestData(int accountId, const QString &accessToken, int startIndex, const QString &continuationRequest, const QDateTime &syncTimestamp, bool isGroupRequest)
{
    QUrl requestUrl;
    if (continuationRequest.isEmpty()) {
        QUrlQuery urlQuery;
        if (isGroupRequest) {
            requestUrl = QUrl(QStringLiteral("https://www.google.com/m8/feeds/groups/default/full"));
        } else {
            requestUrl = QUrl(QStringLiteral("https://www.google.com/m8/feeds/contacts/default/full/"));
            if (!syncTimestamp.isNull()) { // delta query
                urlQuery.addQueryItem("updated-min", syncTimestamp.toString(Qt::ISODate));
                urlQuery.addQueryItem("showdeleted", QStringLiteral("true"));
            }
        }
        if (startIndex >= 1) {
            urlQuery.addQueryItem ("start-index", QString::number(startIndex));
        }
        urlQuery.addQueryItem("max-results", QString::number(SOCIALD_GOOGLE_MAX_CONTACT_ENTRY_RESULTS));
        requestUrl.setQuery(urlQuery);
    } else {
        requestUrl = QUrl(continuationRequest);
    }

    QNetworkRequest req(requestUrl);
    req.setRawHeader("GData-Version", "3.0");
    req.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                     QString(QLatin1String("Bearer ") + accessToken).toUtf8());

    // we're requesting data.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);
    QNetworkReply *reply = networkAccessManager->get(req);
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("continuationRequest", continuationRequest);
        reply->setProperty("lastSyncTimestamp", syncTimestamp);
        reply->setProperty("startIndex", startIndex);
        if (isGroupRequest) {
            connect(reply, SIGNAL(finished()), this, SLOT(groupsFinishedHandler()));
        } else {
            connect(reply, SIGNAL(finished()), this, SLOT(contactsFinishedHandler()));
        }
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        m_apiRequestsRemaining[accountId] = m_apiRequestsRemaining[accountId] - 1;
        setupReplyTimeout(accountId, reply);
    } else {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to request data from Google account with id %1"))
              .arg(accountId));

        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
    }
}

void GoogleTwoWayContactSyncAdaptor::groupsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray data = reply->readAll();
    int startIndex = reply->property("startIndex").toInt();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QDateTime lastSyncTimestamp = reply->property("lastSyncTimestamp").toDateTime();
    bool isError = reply->property("isError").toBool();
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    if (isError) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error occurred when performing groups request for Google account %1"))
              .arg(accountId));

        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    } else if (data.isEmpty()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: no groups data in reply from Google with account %1"))
              .arg(accountId));

        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    GoogleContactStream parser(false, accountId);
    GoogleContactAtom *atom = parser.parse(data);

    if (!atom) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to parse groups data from reply from Google using account with id %1"))
              .arg(accountId));

        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    if (atom->entrySystemGroups().contains(QStringLiteral("Contacts"))) {
        // we have found the atom id of the group we need to upload new contacts to.
        QString myContactsGroupAtomId = atom->entrySystemGroups().value(QStringLiteral("Contacts"));
        m_myContactsGroupAtomIds[accountId] = myContactsGroupAtomId;
        if (myContactsGroupAtomId.isEmpty()) {
            // We don't consider this a fatal error,
            // instead, we just refuse to upsync new contacts.
            TRACE(SOCIALD_INFORMATION,
                  QString(QLatin1String("the My Contacts group was found, but atom id not parsed correctly for account: %1"))
                  .arg(accountId));
        }
        // we can now continue with contact sync.
        determineRemoteChanges(lastSyncTimestamp, QString::number(accountId));
    } else if (!atom->nextEntriesUrl().isEmpty()) {
        // request more groups if they exist.
        startIndex += SOCIALD_GOOGLE_MAX_CONTACT_ENTRY_RESULTS;
        requestData(accountId, accessToken, startIndex, atom->nextEntriesUrl(), lastSyncTimestamp, true); // true = isGroupRequest
    } else {
        // couldn't find the My Contacts group.
        // We don't consider this a fatal error,
        // instead we just refuse to upsync new contacts.
        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("unable to find My Contacts group when syncing Google contacts for account: %1"))
              .arg(accountId));
        // we can now continue with contact sync.
        determineRemoteChanges(lastSyncTimestamp, QString::number(accountId));
    }

    delete atom;
    decrementSemaphore(accountId);
}

void GoogleTwoWayContactSyncAdaptor::contactsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray data = reply->readAll();
    int startIndex = reply->property("startIndex").toInt();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QDateTime lastSyncTimestamp = reply->property("lastSyncTimestamp").toDateTime();
    bool isError = reply->property("isError").toBool();
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    if (isError) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error occurred when performing contacts request for Google account %1"))
              .arg(accountId));

        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    } else if (data.isEmpty()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: no contact data in reply from Google with account %1"))
              .arg(accountId));

        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    GoogleContactStream parser(false, accountId);
    GoogleContactAtom *atom = parser.parse(data);

    if (!atom) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to parse contacts data from reply from Google using account with id %1"))
              .arg(accountId));

        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    // for each remote contact, there are some associated XML elements which
    // could not be stored in QContactDetail form (eg, link URIs etc).
    // build up some datastructures to help us retrieve that information
    // when we need it.
    // we also store the etag data out-of-band to avoid spurious contact saves
    // when the etag changes are reported by the remote server.
    // finally, we can set the id of the contact.
    QList<QPair<QContact, QStringList> > remoteAddModContacts = atom->entryContacts();
    for (int i = 0; i < remoteAddModContacts.size(); ++i) {
        QContact c = remoteAddModContacts[i].first;
        m_unsupportedXmlElements[accountId].insert(
                c.detail<QContactGuid>().guid(),
                remoteAddModContacts[i].second);
        m_contactEtags[accountId].insert(c.detail<QContactGuid>().guid(), c.detail<QContactOriginMetadata>().id());
        c.setId(QContactId::fromString(m_contactIds[accountId].value(c.detail<QContactGuid>().guid())));
        m_remoteAddMods[accountId].append(c);
    }
    QList<QContact> remoteDelContacts = atom->deletedEntryContacts();
    for (int i = 0; i < remoteDelContacts.size(); ++i) {
        QContact c = remoteDelContacts[i];
        c.setId(QContactId::fromString(m_contactIds[accountId].value(c.detail<QContactGuid>().guid())));
        m_contactAvatars[accountId].remove(c.detail<QContactGuid>().guid()); // just in case the avatar was outstanding.
        m_remoteDels[accountId].append(c);
    }

    if (!atom->nextEntriesUrl().isEmpty()) {
        // request more if they exist.
        startIndex += SOCIALD_GOOGLE_MAX_CONTACT_ENTRY_RESULTS;
        requestData(accountId, accessToken, startIndex, atom->nextEntriesUrl(), lastSyncTimestamp);
    } else {
        // we're finished downloading the remote changes - we should sync local changes up.
        int addModCount = m_remoteAddMods[accountId].size(), removedCount = m_remoteDels[accountId].size();
        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("Google contact sync with account %1 got remote changes: a/m: %2 r: %3."))
              .arg(accountId).arg(addModCount).arg(removedCount));
        continueSync(accountId, accessToken);
    }

    delete atom;
    decrementSemaphore(accountId);
}

void GoogleTwoWayContactSyncAdaptor::continueSync(int accountId, const QString &accessToken)
{
    // for each of the addmods, we need to fixup the contact avatars.
    transformContactAvatars(m_remoteAddMods[accountId], accountId, accessToken);

    // now store the changes locally
    if (!storeRemoteChanges(m_remoteDels[accountId], &m_remoteAddMods[accountId], QString::number(accountId))) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("unable to store remote changes locally - aborting sync Google contacts for account %1"))
              .arg(accountId));
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        // note: don't decrement here - it's done by contactsFinishedHandler().
        return;
    }

    // update our mapping of GUID to QContactId
    foreach (const QContact &c, m_remoteAddMods[accountId]) {
        if (c.id().toString().trimmed().isEmpty()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("No local contact id specified for contact with guid %1 from account %2"))
                  .arg(c.detail<QContactGuid>().guid()).arg(accountId));
        } else {
            m_contactIds[accountId].insert(c.detail<QContactGuid>().guid(), c.id().toString());
        }
    }

    // now determine which local changes need to be upsynced to the remote server
    QSet<QContactDetail::DetailType> ignorableDetailTypes;
    // these are the "default" ignorable detail types from the TWCSA baseclass.
    ignorableDetailTypes.insert(QContactDetail__TypeDeactivated);
    ignorableDetailTypes.insert(QContactDetail::TypeDisplayLabel);
    ignorableDetailTypes.insert(QContactDetail::TypeGlobalPresence);
    ignorableDetailTypes.insert(QContactDetail__TypeIncidental);
    ignorableDetailTypes.insert(QContactDetail::TypePresence);
    ignorableDetailTypes.insert(QContactDetail::TypeOnlineAccount);
    ignorableDetailTypes.insert(QContactDetail__TypeStatusFlags);
    ignorableDetailTypes.insert(QContactDetail::TypeSyncTarget);
    ignorableDetailTypes.insert(QContactDetail::TypeTimestamp);
    // we add one detail type to the ignorable set: avatar, since we don't upsync avatar changes.
    ignorableDetailTypes.insert(QContactAvatar::Type);
    // fetch the local changes which occurred since last sync
    QDateTime localSince;
    QList<QContact> locallyAdded, locallyModified, locallyDeleted;
    if (!determineLocalChanges(&localSince, &locallyAdded, &locallyModified, &locallyDeleted, QString::number(accountId), ignorableDetailTypes)) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("unable to determine local changes - aborting sync Google contacts for account %1"))
              .arg(accountId));
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        // note: don't decrement here - it's done by contactsFinishedHandler().
        return;
    }

    // now push those changes up to google.
    upsyncLocalChanges(localSince, locallyAdded, locallyModified, locallyDeleted, QString::number(accountId));
}

void GoogleTwoWayContactSyncAdaptor::upsyncLocalChanges(const QDateTime &localSince,
                                                        const QList<QContact> &locallyAdded,
                                                        const QList<QContact> &locallyModified,
                                                        const QList<QContact> &locallyDeleted,
                                                        const QString &accountId)
{
    // add the etag field back to the locallyModified contacts.
    int accId = accountId.toInt();
    QList<QContact> updatedLocallyModified;
    for (int i = 0; i < locallyModified.size(); ++i) {
        QContact c = locallyModified.at(i);
        QContactOriginMetadata omd = c.detail<QContactOriginMetadata>();
        omd.setId(m_contactEtags[accId].value(c.detail<QContactGuid>().guid()));
        c.saveDetail(&omd);
        updatedLocallyModified.append(c);
    }

    QList<QPair<QContact, GoogleContactStream::UpdateType> > contactUpdatesToPost;
    foreach (const QContact &c, locallyAdded)
        contactUpdatesToPost.append(qMakePair(c, GoogleContactStream::Add));
    foreach (const QContact &c, updatedLocallyModified)
        contactUpdatesToPost.append(qMakePair(c, GoogleContactStream::Modify));
    foreach (const QContact &c, locallyDeleted) {
        contactUpdatesToPost.append(qMakePair(c, GoogleContactStream::Remove));
        m_contactAvatars[accId].remove(c.detail<QContactGuid>().guid()); // just in case the avatar was outstanding.
    }
    m_localChanges[accId] = contactUpdatesToPost;

    TRACE(SOCIALD_INFORMATION,
          QString(QLatin1String("Google account: %1 upsyncing local contact changes since:%2\n"
                                "    locally added:    %3\n"
                                "    locally modified: %4\n"
                                "    locally removed:  %5\n"))
          .arg(accId).arg(localSince.toString(Qt::ISODate)).arg(locallyAdded.count()).arg(locallyModified.count()).arg(locallyDeleted.count()));

    upsyncLocalChangesList(accId);
}

bool GoogleTwoWayContactSyncAdaptor::testAccountProvenance(const QContact &contact, const QString &accountId)
{
    return contact.detail<QContactGuid>().guid().startsWith(QStringLiteral("%1:").arg(accountId));
}

void GoogleTwoWayContactSyncAdaptor::upsyncLocalChangesList(int accountId)
{
    bool postedData = false;
    if (!m_accountSyncProfile || m_accountSyncProfile->syncDirection() != Buteo::SyncProfile::SYNC_DIRECTION_FROM_REMOTE) {
        // two-way sync is the default setting.  Upsync the changes.
        QMultiMap<GoogleContactStream::UpdateType, QPair<QContact, QStringList> > batch;
        for (int i = m_localChanges[accountId].size() - 1; i >= 0; --i) {
            QPair<QContact, GoogleContactStream::UpdateType> entry = m_localChanges[accountId].takeAt(i);
            QStringList extraXmlElements = m_unsupportedXmlElements[accountId].value(entry.first.detail<QContactGuid>().guid());
            if (entry.second == GoogleContactStream::Add) {
                // new contacts need to be inserted into the My Contacts group
                QString myContactsGroupAtomId = m_myContactsGroupAtomIds[accountId];
                if (myContactsGroupAtomId.isEmpty()) {
                    TRACE(SOCIALD_INFORMATION,
                          QString(QLatin1String("skipping upload of locally added contact %1 to account %2 due to unknown My Contacts group atom id"))
                          .arg(entry.first.id().toString()).arg(accountId));
                } else {
                    extraXmlElements.append(QStringLiteral("<gContact:groupMembershipInfo deleted=\"false\" href=\"%1\"></gContact:groupMembershipInfo>").arg(myContactsGroupAtomId));
                    batch.insertMulti(entry.second, qMakePair(entry.first, extraXmlElements));
                }
            } else {
                batch.insertMulti(entry.second, qMakePair(entry.first, extraXmlElements));
            }

            if (batch.size() == SOCIALD_GOOGLE_MAX_CONTACT_ENTRY_RESULTS || i == 0) {
                GoogleContactStream encoder(false, accountId, m_emailAddresses[accountId]);
                QByteArray encodedContactUpdates = encoder.encode(batch);
                storeToRemote(accountId, m_accessTokens[accountId], encodedContactUpdates);
                postedData = true;
                break;
            }
        }
    } else {
        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("skipping upload of local contacts changes due to profile direction setting for account %1"))
              .arg(accountId));
    }

    if (!postedData) {
        // nothing left to upsync.  attempt to download any outstanding avatars.
        queueOutstandingAvatars(accountId, m_accessTokens[accountId]);
    }
}

void GoogleTwoWayContactSyncAdaptor::storeToRemote(int accountId, const QString &accessToken, const QByteArray &encodedContactUpdates)
{
    QUrl requestUrl(QUrl(QString(QLatin1String("https://www.google.com/m8/feeds/contacts/default/full/batch"))));
    QNetworkRequest req(requestUrl);
    req.setRawHeader("GData-Version", "3.0");
    req.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                     QString(QLatin1String("Bearer ") + accessToken).toUtf8());
    req.setRawHeader(QString(QLatin1String("Content-Type")).toUtf8(),
                     QString(QLatin1String("application/atom+xml; charset=UTF-8; type=feed")).toUtf8());
    req.setHeader(QNetworkRequest::ContentLengthHeader, encodedContactUpdates.size());
    req.setRawHeader(QString(QLatin1String("If-Match")).toUtf8(),
                     QString(QLatin1String("*")).toUtf8());

    // we're posting data.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);
    QNetworkReply *reply = networkAccessManager->post(req, encodedContactUpdates);
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(postErrorHandler()));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(postErrorHandler()));
        connect(reply, SIGNAL(finished()), this, SLOT(postFinishedHandler()));
        m_apiRequestsRemaining[accountId] = m_apiRequestsRemaining[accountId] - 1;
        setupReplyTimeout(accountId, reply);
    } else {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to post contacts to Google account with id %1"))
              .arg(accountId));

        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
    }
}

void GoogleTwoWayContactSyncAdaptor::postFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray response = reply->readAll();
    int accountId = reply->property("accountId").toInt();
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    if (reply->property("isError").toBool()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("Error occurred posting contact data to google with account %1, got response: %2"))
              .arg(accountId).arg(QString::fromUtf8(response)));
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    GoogleContactStream parser(false, accountId);
    GoogleContactAtom *atom = parser.parse(response);
    QMap<QString, GoogleContactAtom::BatchOperationResponse> operationResponses = atom->batchOperationResponses();
    QMap<QString, QString> batchOperationIdsToGuids;

    bool errorOccurredInBatch = false;
    foreach (const GoogleContactAtom::BatchOperationResponse &response, operationResponses) {
        if (response.isError) {
            errorOccurredInBatch = true;
            TRACE(SOCIALD_DEBUG,
                  QString(QLatin1String("batch operation error:\n"
                                        "    id:     %1\n"
                                        "    type:   %2\n"
                                        "    code:   %3\n"
                                        "    reason: %4\n"
                                        "    descr:  %5\n"))
                  .arg(response.operationId)
                  .arg(response.type)
                  .arg(response.code)
                  .arg(response.reason)
                  .arg(response.reasonDescription));
        } else {
            batchOperationIdsToGuids.insert(response.operationId, response.contactGuid);
        }
    }

    if (errorOccurredInBatch) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("Error occurred during batch operation with Google account %1"))
              .arg(accountId));
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    // update our map of guid to contact id, now that we know which remote GUID was generated for which contactId.
    foreach (const QString &contactId, batchOperationIdsToGuids.keys()) {
        if (!batchOperationIdsToGuids.value(contactId).isEmpty()) {
            m_contactIds[accountId].insert(batchOperationIdsToGuids.value(contactId), contactId);
        }
    }

    // continue with more, if there were more than one page of updates to post.
    upsyncLocalChangesList(accountId);

    // finished with this request, so decrementing semaphore.
    decrementSemaphore(accountId);
}

void GoogleTwoWayContactSyncAdaptor::postErrorHandler()
{
    sender()->setProperty("isError", QVariant::fromValue<bool>(true));
}

void GoogleTwoWayContactSyncAdaptor::queueOutstandingAvatars(int accountId, const QString &accessToken)
{
    int queuedCount = 0;
    for (QMap<QString, QString>::const_iterator it = m_contactAvatars[accountId].constBegin();
            it != m_contactAvatars[accountId].constEnd(); ++it) {
        if (!it.value().isEmpty() && queueAvatarForDownload(accountId, accessToken, it.key(), it.value())) {
            queuedCount++;
        }
    }

    TRACE(SOCIALD_DEBUG,
          QString(QLatin1String("queued %1 avatars for download for account %2"))
          .arg(queuedCount)
          .arg(accountId));
}

bool GoogleTwoWayContactSyncAdaptor::queueAvatarForDownload(int accountId, const QString &accessToken, const QString &contactGuid, const QString &imageUrl)
{
    if (m_apiRequestsRemaining[accountId] > 0 && !m_queuedAvatarsForDownload[accountId].contains(contactGuid)) {
        m_apiRequestsRemaining[accountId] = m_apiRequestsRemaining[accountId] - 1;
        m_queuedAvatarsForDownload[accountId][contactGuid] = imageUrl;

        QVariantMap metadata;
        metadata.insert(IMAGE_DOWNLOADER_ACCOUNT_ID_KEY, accountId);
        metadata.insert(IMAGE_DOWNLOADER_TOKEN_KEY, accessToken);
        metadata.insert(IMAGE_DOWNLOADER_IDENTIFIER_KEY, contactGuid);
        incrementSemaphore(accountId);
        m_workerObject->queue(imageUrl, metadata);

        return true;
    }

    return false;
}

void GoogleTwoWayContactSyncAdaptor::transformContactAvatars(QList<QContact> &remoteContacts, int accountId, const QString &accessToken)
{
    // The avatar detail from the remote contact will be of the form:
    // https://www.google.com/m8/feeds/photos/media/user@gmail.com/userId
    // We need to:
    // 1) transform this to a local filename.
    // 2) determine if the local file exists.
    // 3) if not, trigger downloading the avatar.

    for (int i = 0; i < remoteContacts.size(); ++i) {
        QContact &curr(remoteContacts[i]);

        // We only deal with the first avatar from the contact.  If it has multiple,
        // then later avatars will not be transformed.  TODO: fix this.
        // We also only bother to do this for contacts with a GUID, as we don't
        // store locally any contact without one.
        QString contactGuid = curr.detail<QContactGuid>().guid();
        if (curr.details<QContactAvatar>().size() && !contactGuid.isEmpty()) {
            // we have a remote avatar which we need to transform.
            QContactAvatar avatar = curr.detail<QContactAvatar>();
            Q_FOREACH (const QContactAvatar &av, curr.details<QContactAvatar>()) {
                if (av.value(QContactAvatar__FieldAvatarMetadata).toString() == QStringLiteral("picture")) {
                    avatar = av;
                    break;
                }
            }
            QString remoteImageUrl = avatar.imageUrl().toString();
            if (!remoteImageUrl.isEmpty() && !avatar.imageUrl().isLocalFile()) {
                // transform to a local file name.
                QString localFileName = GoogleContactImageDownloader::staticOutputFile(
                        contactGuid, remoteImageUrl);

                // and trigger downloading the image, if it doesn't already exist.
                // this means that we shouldn't download images needlessly after
                // first sync, but it also means that if it updates/changes on the
                // server side, we also won't retrieve any updated image.
                if (QFile::exists(localFileName)) {
                    QImageReader reader(localFileName);
                    if (reader.canRead()) {
                        // avatar image already exists, update the detail in the contact.
                        avatar.setImageUrl(localFileName);
                        curr.saveDetail(&avatar);
                    } else {
                        // not a valid image file.  Could be artifact from an error.
                        QFile::remove(localFileName);
                    }
                }

                if (!QFile::exists(localFileName)) {
                    // temporarily remove the avatar from the contact
                    m_contactAvatars[accountId].insert(contactGuid, remoteImageUrl);
                    curr.removeDetail(&avatar);
                    // then trigger the download
                    queueAvatarForDownload(accountId, accessToken, contactGuid, remoteImageUrl);
                }
            }
        }
    }
}

void GoogleTwoWayContactSyncAdaptor::imageDownloaded(const QString &url, const QString &path,
                                                     const QVariantMap &metadata)
{
    Q_UNUSED(url)

    // Load finished, update the avatar, decrement semaphore
    int accountId = metadata.value(IMAGE_DOWNLOADER_ACCOUNT_ID_KEY).toInt();
    QString contactGuid = metadata.value(IMAGE_DOWNLOADER_IDENTIFIER_KEY).toString();

    // Empty path signifies that an error occurred.
    if (!path.isEmpty()) {
        // no longer outstanding.
        m_contactAvatars[accountId].remove(contactGuid);
        m_queuedAvatarsForDownload[accountId].remove(contactGuid);
        m_downloadedContactAvatars[accountId].insert(contactGuid, path);
    }

    decrementSemaphore(accountId);
}

void GoogleTwoWayContactSyncAdaptor::purgeAccount(int pid)
{
    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_GOOGLE_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);

    int purgeCount = 0;
    QList<QContactId> contactsToRemove;
    QList<QContact> localContacts = m_contactManager.contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);
    for (int i = 0; i < localContacts.size(); ++i) {
        const QContact &c(localContacts[i]);
        if (c.detail<QContactGuid>().guid().startsWith(QStringLiteral("%1:").arg(pid))) {
            // it was provided by this account.  Remove this one.
            contactsToRemove.append(c.id());
            purgeCount++;
        } else {
            // it was always provided by some other account only.  Don't modify this one.
        }
    }

    // now write the changes to the database.
    bool success = true;
    if (contactsToRemove.size()) {
        success = m_contactManager.removeContacts(contactsToRemove);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to remove stale contacts: %1 - during purge of account %2"))
                  .arg(m_contactManager.error()).arg(pid));
        }
    }

    // ensure we remove the OOB data for the account.
    // if we do not, restoring accounts from backup and then triggering sync could cause issues.
    QStringList purgeKeys;
    // purge the "build-in" OOB keys
    purgeKeys << QStringLiteral("prevRemote") << QStringLiteral("exportedIds")
              << QStringLiteral("remoteSince") << QStringLiteral("localSince")
              << QStringLiteral("possiblyUploadedAdditions")
              << QStringLiteral("definitelyDownloadedAdditions");
    // purge the extra OOB keys
    purgeKeys << QStringLiteral("myContactsGroupAtomId")
              << QStringLiteral("unsupportedElements")
              << QStringLiteral("contactEtags")
              << QStringLiteral("contactIds")
              << QStringLiteral("contactAvatars");

    // We can't rely on d->m_stateData[QString::number(pid)].m_oobScope containing the
    // correct value, as the purge codepath can be called from cleanUp() on account
    // removal, during which no cached state data exists.
    // Also, it may be called for an account which was previously removed but for which
    // artifacts still remain (eg, if msyncd wasn't running at the time that the account
    // was removed, due to a crash, etc) - in which case the cached value would be wrong.
    QString oobScope = QStringLiteral("%1-%2").arg(SOCIALD_GOOGLE_CONTACTS_SYNCTARGET).arg(pid);
    if (!d->m_engine->removeOOB(oobScope, purgeKeys)) {
        success = false;
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("Error occurred while purging OOB data for removed Google account %1"))
              .arg(pid));
    }

    if (success) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("Purged account %1 and successfully removed %2 contacts"))
                .arg(pid).arg(purgeCount));
    }
}

void GoogleTwoWayContactSyncAdaptor::finalize(int accountId)
{
    if (m_accessTokens[accountId].isEmpty()) {
        // account failure occurred before sync process was started.
        // in this case we have nothing left to do.
        return;
    }

    // first, ensure we update any avatars required.
    if (m_downloadedContactAvatars[accountId].size()) {
        // find the contacts we need to update from our mutated prev remote list.
        QMap<QString, int> mprGuidToIndex;
        for (int i = 0; i < d->m_stateData[QString::number(accountId)].m_mutatedPrevRemote.size(); ++i) {
            mprGuidToIndex.insert(d->m_stateData[QString::number(accountId)].m_mutatedPrevRemote[i].detail<QContactGuid>().guid(), i);
        }

        // create an update pair from the current mutationPrevRemote version and the new version (with updated avatar).
        QList<QPair<QContact, QContact> > contactAvatarUpdates;
        QMap<int, QContact> prevRemoteMutations;
        for (QMap<QString, QString>::const_iterator it = m_downloadedContactAvatars[accountId].constBegin();
                it != m_downloadedContactAvatars[accountId].constEnd(); ++it) {
            int idx = mprGuidToIndex.value(it.key(), -1);
            if (idx != -1) {
                // we have downloaded the avatar for this contact, and need to update it.
                QContact c = d->m_stateData[QString::number(accountId)].m_mutatedPrevRemote[idx];
                QContact modC = c;
                QContactAvatar a;
                Q_FOREACH (const QContactAvatar &av, modC.details<QContactAvatar>()) {
                    if (av.value(QContactAvatar__FieldAvatarMetadata).toString() == QStringLiteral("picture")) {
                        a = av;
                        break;
                    }
                }
                a.setValue(QContactAvatar__FieldAvatarMetadata, QVariant::fromValue<QString>(QStringLiteral("picture")));
                a.setImageUrl(it.value());
                modC.saveDetail(&a);
                contactAvatarUpdates.append(qMakePair(c, modC));
                prevRemoteMutations.insert(idx, modC);
                break;
            }
        }

        QContactManager::Error error;
        if (d->m_engine->storeSyncContacts(SOCIALD_GOOGLE_CONTACTS_SYNCTARGET,
                                            QtContactsSqliteExtensions::ContactManagerEngine::PreserveLocalChanges,
                                            &contactAvatarUpdates,
                                            &error)) {
            // mutate our prevRemote with the modified contact (containing added avatar).
            // note that we only do this if the mutation succeeds in the database first.
            for (QMap<int, QContact>::const_iterator it = prevRemoteMutations.constBegin();
                 it != prevRemoteMutations.constEnd(); ++it) {
                d->m_stateData[QString::number(accountId)].m_mutatedPrevRemote.replace(it.key(), it.value());
            }
        } else {
            TRACE(SOCIALD_ERROR,
                QString(QLatin1String("finalize: error adding avatars for %1 Google contacts from account %2"))
                .arg(contactAvatarUpdates.size()).arg(accountId));
        }
    }

    if (!storeExtraStateData(accountId) || !storeSyncStateData(QString::number(accountId))) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("unable to finalize sync of Google contacts with account %1"))
              .arg(accountId));
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
    }
}

void GoogleTwoWayContactSyncAdaptor::finalCleanup()
{
    // Synchronously find any contacts which need to be removed,
    // which were somehow "left behind" by the sync process.
    // Also, determine if any avatars were not synced, and remove those details.

    // first, get a list of all existing google account ids
    QList<int> googleAccountIds;
    QList<int> purgeAccountIds;
    QList<int> currentAccountIds;
    QList<uint> uaids = accountManager->accountList();
    foreach (uint uaid, uaids) {
        currentAccountIds.append(static_cast<int>(uaid));
    }
    foreach (int currId, currentAccountIds) {
        Accounts::Account *act = accountManager->account(currId);
        if (act) {
            if (act->providerName() == QString(QLatin1String("google"))) {
                // this account still exists, no need to purge its content.
                googleAccountIds.append(currId);
            }
            act->deleteLater();
        }
    }

    // second, get all contacts which have been synced from Google.
    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_GOOGLE_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);
    noRelationships.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactGuid::Type << QContactAvatar::Type);
    QList<QContact> googleContacts = m_contactManager.contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);

    // third, find all account ids from which contacts have been synced
    foreach (const QContact &contact, googleContacts) {
        QContactGuid guid = contact.detail<QContactGuid>();
        QStringList guidParts = guid.guid().split(":");
        QString accountIdStr = guidParts.size() ? guidParts.first() : QString();
        if (!accountIdStr.isEmpty()) {
            int purgeId = accountIdStr.toInt();
            if (purgeId && !googleAccountIds.contains(purgeId) && !purgeAccountIds.contains(purgeId)) {
                // this account no longer exists, and needs to be purged.
                purgeAccountIds.append(purgeId);
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Malformed GUID not of form <accountId>:<remoteGuid>";
        }
    }

    // fourth, remove any non-existent avatar details.
    // We save these first, in case some contacts get removed by purge.
    // Note that this entire codeblock is only required temporarily,
    // as the code from previous versions could leave "erroneous" avatars
    // on disk / in database, but the current code will not.
    // We should remove this block (and the subsequent save operation)
    // in a future update.
    QMap<QString, QContact> contactsToSave;
    for (int i = 0; i < googleContacts.size(); ++i) {
        QContact contact = googleContacts.at(i);

        const QString contactGuid(contact.detail<QContactGuid>().guid());
        QStringList guidParts = contactGuid.split(":");
        QString accountIdStr = guidParts.size() ? guidParts.first() : QString();
        if (!accountIdStr.isEmpty()) {
            int accountId = accountIdStr.toInt();

            // remove any nonexistent/error avatar details from this contact.
            QList<QContactAvatar> allAvatars = contact.details<QContactAvatar>();
            for (int j = 0; j < allAvatars.size(); ++j) {
                QContactAvatar av = allAvatars[j];
                if (!av.imageUrl().isEmpty()) {
                    // this avatar may have failed to sync.
                    QUrl avatarUrl = av.imageUrl();
                    QString avatarPath = av.imageUrl().toString();
                    if (avatarUrl.isLocalFile()) {
                        if (QFile::exists(avatarPath)) {
                            QImageReader reader(avatarPath);
                            if (!reader.canRead()) {
                                // remove artifacts of previous (failed) syncs if necessary.
                                QFile::remove(avatarPath);
                            }
                        }
                        if (!QFile::exists(avatarPath)) {
                            // we should remove this nonexistent avatar from this contact.
                            if (accountId != 0 && !purgeAccountIds.contains(accountId)) {
                                // download failed, remove it from the contact.
                                contact.removeDetail(&av);
                                contactsToSave[contactGuid] = contact;
                            }
                        }
                    }
                }
            }
        }
    }

    QList<QContact> saveList = contactsToSave.values();
    QList<QContactDetail::DetailType> typeMask; typeMask << QContactDetail::TypeAvatar;
    if (m_contactManager.saveContacts(&saveList, typeMask)) {
        TRACE(SOCIALD_INFORMATION,
            QString(QLatin1String("finalCleanup() fixed up avatars from %1 Google contacts"))
            .arg(saveList.size()));
    } else {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("finalCleanup() failed to save non-existent avatar removals for Google contacts")));
    }

    // fifth, purge all data for those account ids which no longer exist.
    if (purgeAccountIds.size()) {
        TRACE(SOCIALD_INFORMATION,
            QString(QLatin1String("finalCleanup() purging contacts from %1 non-existent Google accounts"))
            .arg(purgeAccountIds.size()));
        foreach (int purgeId, purgeAccountIds) {
            purgeAccount(purgeId);
        }
    }
}

// this function must be called directly after readSyncStateData()
bool GoogleTwoWayContactSyncAdaptor::readExtraStateData(int accountId)
{
    QMap<QString, QVariant> values;
    QStringList keys;
    keys << QStringLiteral("myContactsGroupAtomId")
         << QStringLiteral("unsupportedElements")
         << QStringLiteral("contactEtags")
         << QStringLiteral("contactIds")
         << QStringLiteral("contactAvatars");
    if (!d->m_engine->fetchOOB(d->m_stateData[QString::number(accountId)].m_oobScope, keys, &values)) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("failed to read extra data for %1 account %2"))
            .arg(d->m_syncTarget).arg(accountId));
        d->clear(QString::number(accountId));
        return false;
    }

    // m_myContactsGroupAtomIds
    QString myContactsGroupAtomId = values.value(QStringLiteral("myContactsGroupAtomId")).toString();
    m_myContactsGroupAtomIds.insert(accountId, myContactsGroupAtomId);

    // m_unsupportedElements
    QVariant ueValue = values.value(QStringLiteral("unsupportedElements"));
    QByteArray ueValueBA = ueValue.toByteArray();
    QJsonObject ueJsonObj = QJsonDocument::fromBinaryData(ueValueBA).object();
    QStringList contactGuids = ueJsonObj.keys();
    QMap<QString, QStringList> guidToUnsupportedElements;
    foreach (const QString &guid, contactGuids) {
        QVariantList unsupportedElementsVL = ueJsonObj.value(guid).toArray().toVariantList();
        QStringList unsupportedElements;
        foreach (const QVariant &v, unsupportedElementsVL) {
            if (!v.toString().isEmpty()) {
                unsupportedElements.append(v.toString());
            }
        }

        guidToUnsupportedElements.insert(guid, unsupportedElements);
    }
    m_unsupportedXmlElements[accountId] = guidToUnsupportedElements;

    // m_contactEtags
    QVariant ceValue = values.value(QStringLiteral("contactEtags"));
    QByteArray ceValueBA = ceValue.toByteArray();
    QJsonObject ceJsonObj = QJsonDocument::fromBinaryData(ceValueBA).object();
    contactGuids = ceJsonObj.keys();
    QMap<QString, QString> guidToContactEtag;
    foreach (const QString &guid, contactGuids) {
        guidToContactEtag.insert(guid, ceJsonObj.value(guid).toString());
    }
    m_contactEtags[accountId] = guidToContactEtag;

    // m_contactIds
    QVariant ciValue = values.value(QStringLiteral("contactIds"));
    QByteArray ciValueBA = ciValue.toByteArray();
    QJsonObject ciJsonObj = QJsonDocument::fromBinaryData(ciValueBA).object();
    contactGuids = ciJsonObj.keys();
    QMap<QString, QString> guidToContactId;
    foreach (const QString &guid, contactGuids) {
        guidToContactId.insert(guid, ciJsonObj.value(guid).toString());
    }
    m_contactIds[accountId] = guidToContactId;

    // m_contactAvatars
    QVariant caValue = values.value(QStringLiteral("contactAvatars"));
    QByteArray caValueBA = caValue.toByteArray();
    QJsonObject caJsonObj = QJsonDocument::fromBinaryData(caValueBA).object();
    contactGuids = caJsonObj.keys();
    QMap<QString, QString> guidToContactAvatar;
    foreach (const QString &guid, contactGuids) {
        guidToContactAvatar.insert(guid, caJsonObj.value(guid).toString());
    }
    m_contactAvatars[accountId] = guidToContactAvatar;
    TRACE(SOCIALD_INFORMATION,
            QString(QLatin1String("have %1 outstanding contact avatars to sync from account %2"))
            .arg(guidToContactAvatar.size()).arg(accountId));

    // Finally, if we're doing a "clean sync" we should pre-populate our prevRemote
    // list with the current state of the local database.
    // This is to avoid clean-syncs causing contact duplication.
    if (!d->m_stateData[QString::number(accountId)].m_localSince.isValid()) {
        QDateTime maxTimestamp;
        QList<QContact> existingContacts;
        QContactManager::Error error = QContactManager::NoError;
        if (!d->m_engine->fetchSyncContacts(SOCIALD_GOOGLE_CONTACTS_SYNCTARGET,
                                            QDateTime(),
                                            QList<QContactId>(),
                                            &existingContacts,
                                            0,
                                            0,
                                            &maxTimestamp,
                                            &error)) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("failed to fetch pre-existing contacts for account %1"))
                    .arg(accountId));
            d->clear(QString::number(accountId));
            return false;
        }

        // filter out any which don't come from this account.
        QList<QContact> prevRemote;
        QList<QContactId> exportedIds;
        foreach (const QContact &c, existingContacts) {
            if (c.detail<QContactGuid>().guid().startsWith(QStringLiteral("%1:").arg(accountId))) {
                prevRemote.append(c);
                exportedIds.append(c.id());
                m_contactIds[accountId].insert(c.detail<QContactGuid>().guid(), c.id().toString());
            } else {
                // if any came from the one-way sync adaptor, they will need to mangled to the new form.
                QStringList accountIds = c.detail<QContactOriginMetadata>().groupId().split(',');
                if (c.detail<QContactSyncTarget>().syncTarget() == SOCIALD_GOOGLE_CONTACTS_SYNCTARGET
                        && accountIds.contains(QString::number(accountId))
                        && !c.detail<QContactGuid>().guid().isEmpty()) {
                    // this actually belongs to this account, but was synced with the one-way adaptor.
                    // we mangle the Guid to the "new" form (accountId:serverGuid) but we do not
                    // change the QContactOriginMetadata.  This is so that when the contact is retrieved
                    // from the server, it will be flagged as being modified (since the "new" version
                    // will not have a QContactOriginMetadata detail) and thus it will be saved with the
                    // new guid detail (and without the QCOM detail).
                    QString newGuid(QStringLiteral("%1:%2").arg(accountId).arg(c.detail<QContactGuid>().guid()));
                    prevRemote.append(c);
                    exportedIds.append(c.id());
                    m_contactIds[accountId].insert(newGuid, c.id().toString());
                }
            }
        }

        // set our state data.
        d->m_stateData[QString::number(accountId)].m_prevRemote = prevRemote;
        d->m_stateData[QString::number(accountId)].m_exportedIds = exportedIds;
    }

    // done.
    return true;
}

// this function must be called directly before storeSyncStateData()
bool GoogleTwoWayContactSyncAdaptor::storeExtraStateData(int accountId)
{
    // m_myContactsGroupAtomIds
    QVariant mcghValue(m_myContactsGroupAtomIds[accountId]);

    // m_unsupportedXmlElements
    QJsonObject ueJsonObj;
    for (QMap<QString, QStringList>::const_iterator it = m_unsupportedXmlElements[accountId].constBegin();
            it != m_unsupportedXmlElements[accountId].constEnd(); ++it) {
        ueJsonObj.insert(it.key(), QJsonValue(QJsonArray::fromStringList(it.value())));
    }
    QJsonDocument ueJsonDoc(ueJsonObj);
    QVariant ueValue(ueJsonDoc.toBinaryData());

    // m_contactEtags
    QJsonObject ceJsonObj;
    for (QMap<QString, QString>::const_iterator it = m_contactEtags[accountId].constBegin();
            it != m_contactEtags[accountId].constEnd(); ++it) {
        ceJsonObj.insert(it.key(), QJsonValue(it.value()));
    }
    QJsonDocument ceJsonDoc(ceJsonObj);
    QVariant ceValue(ceJsonDoc.toBinaryData());

    // m_contactIds
    QJsonObject ciJsonObj;
    for (QMap<QString, QString>::const_iterator it = m_contactIds[accountId].constBegin();
            it != m_contactIds[accountId].constEnd(); ++it) {
        ciJsonObj.insert(it.key(), QJsonValue(it.value()));
    }
    QJsonDocument ciJsonDoc(ciJsonObj);
    QVariant ciValue(ciJsonDoc.toBinaryData());

    // m_contactAvatars
    QJsonObject caJsonObj;
    for (QMap<QString, QString>::const_iterator it = m_contactAvatars[accountId].constBegin();
            it != m_contactAvatars[accountId].constEnd(); ++it) {
        caJsonObj.insert(it.key(), QJsonValue(it.value()));
    }
    QJsonDocument caJsonDoc(caJsonObj);
    QVariant caValue(caJsonDoc.toBinaryData());

    // store to OOB
    QMap<QString, QVariant> values;
    values.insert("myContactsGroupAtomId", mcghValue);
    values.insert("unsupportedElements", ueValue);
    values.insert("contactEtags", ceValue);
    values.insert("contactIds", ciValue);
    values.insert("contactAvatars", caValue);
    if (!d->m_engine->storeOOB(d->m_stateData[QString::number(accountId)].m_oobScope, values)) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("failed to store extra state data for %1 account %2"))
            .arg(d->m_syncTarget).arg(accountId));
        d->clear(QString::number(accountId));
        return false;
    }

    return true;
}
