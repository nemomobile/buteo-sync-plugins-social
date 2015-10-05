/****************************************************************************
 **
 ** Copyright (C) 2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "vkcontactsyncadaptor.h"
#include "vkcontactimagedownloader.h"

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
#include <QtCore/QJsonValue>
#include <QtGui/QImageReader>

#include <QtContacts/QContactDetailFilter>
#include <QtContacts/QContactIntersectionFilter>
#include <QtContacts/QContact>
#include <QtContacts/QContactSyncTarget>
#include <QtContacts/QContactGuid>
#include <QtContacts/QContactName>
#include <QtContacts/QContactNickname>
#include <QtContacts/QContactAvatar>
#include <QtContacts/QContactAddress>
#include <QtContacts/QContactUrl>
#include <QtContacts/QContactGender>
#include <QtContacts/QContactNote>
#include <QtContacts/QContactBirthday>
#include <QtContacts/QContactPhoneNumber>
#include <QtContacts/QContactEmailAddress>

//libaccounts-qt5
#include <Accounts/Account>
#include <Accounts/Manager>

#define SOCIALD_VK_CONTACTS_SYNCTARGET QLatin1String("vk")
#define SOCIALD_VK_MAX_CONTACT_ENTRY_RESULTS 50

static const char *IMAGE_DOWNLOADER_TOKEN_KEY = "token";
static const char *IMAGE_DOWNLOADER_ACCOUNT_ID_KEY = "account_id";
static const char *IMAGE_DOWNLOADER_IDENTIFIER_KEY = "identifier";

namespace {
    bool saveNonexportableDetail(QContact &c, QContactDetail &d)
    {
        d.setValue(QContactDetail__FieldNonexportable, QVariant::fromValue<bool>(true));
        return c.saveDetail(&d);
    }

    int matchingContactIndex(QContact *contact, const QList<QContact> &existing, bool *hasChanged)
    {
        QContact fromServer = *contact;
        const QString &guidstr = contact->detail<QContactGuid>().guid();
        for (int i = 0; i < existing.size(); ++i) {
            const QContact &c(existing[i]);
            if (c.detail<QContactGuid>().guid() == guidstr) {
                // we've found this contact in the local database
                // determine whether it was modified remotely
                *hasChanged = false;
                QContact modified = c;
                if (c.detail<QContactName>().firstName() != fromServer.detail<QContactName>().firstName()
                        || c.detail<QContactName>().lastName() != fromServer.detail<QContactName>().lastName()) {
                    QContactName modName = modified.detail<QContactName>();
                    modName.setFirstName(fromServer.detail<QContactName>().firstName());
                    modName.setLastName(fromServer.detail<QContactName>().lastName());
                    saveNonexportableDetail(modified, modName);
                    *hasChanged = true;
                }
                if (c.detail<QContactGender>().gender() != fromServer.detail<QContactGender>().gender()) {
                    QContactGender modGender = modified.detail<QContactGender>();
                    modGender.setGender(fromServer.detail<QContactGender>().gender());
                    saveNonexportableDetail(modified, modGender);
                    *hasChanged = true;
                }
                if (c.detail<QContactBirthday>().dateTime() != fromServer.detail<QContactBirthday>().dateTime()) {
                    QContactBirthday modBirthday = modified.detail<QContactBirthday>();
                    modBirthday.setDateTime(fromServer.detail<QContactBirthday>().dateTime());
                    saveNonexportableDetail(modified, modBirthday);
                    *hasChanged = true;
                }
                if (c.detail<QContactNickname>().nickname() != fromServer.detail<QContactNickname>().nickname()) {
                    QContactNickname modNickname = modified.detail<QContactNickname>();
                    modNickname.setNickname(fromServer.detail<QContactNickname>().nickname());
                    saveNonexportableDetail(modified, modNickname);
                    *hasChanged = true;
                }
                if (c.detail<QContactAvatar>().imageUrl() != fromServer.detail<QContactAvatar>().imageUrl()) {
                    QContactAvatar modAvatar = modified.detail<QContactAvatar>();
                    modAvatar.setImageUrl(fromServer.detail<QContactAvatar>().imageUrl()); // XXX TODO: transform first!
                    saveNonexportableDetail(modified, modAvatar);
                    *hasChanged = true;
                }
                if (c.detail<QContactAddress>().locality() != fromServer.detail<QContactAddress>().locality()) {
                    QContactAddress modAddr = modified.detail<QContactAddress>();
                    modAddr.setLocality(fromServer.detail<QContactAddress>().locality());
                    saveNonexportableDetail(modified, modAddr);
                    *hasChanged = true;
                }
                const QList<QContactPhoneNumber> &mphns(modified.details<QContactPhoneNumber>());
                const QList<QContactPhoneNumber> &sphns(fromServer.details<QContactPhoneNumber>());
                // there should be exactly zero, one or two phone numbers on VK.
                QContactPhoneNumber mMobile, mHome, sMobile, sHome;
                foreach (const QContactPhoneNumber &phn, mphns) {
                    if (phn.subTypes().contains(QContactPhoneNumber::SubTypeMobile)) {
                        mMobile = phn;
                    } else {
                        mHome = phn;
                    }
                }
                foreach (const QContactPhoneNumber &phn, sphns) {
                    if (phn.subTypes().contains(QContactPhoneNumber::SubTypeMobile)) {
                        sMobile = phn;
                    } else {
                        sHome = phn;
                    }
                }
                if (mMobile.number() != sMobile.number()) {
                    mMobile.setNumber(sMobile.number());
                    saveNonexportableDetail(modified, mMobile);
                    *hasChanged = true;
                }
                if (mHome.number() != sHome.number()) {
                    mHome.setNumber(sHome.number());
                    saveNonexportableDetail(modified, mHome);
                    *hasChanged = true;
                }
                return i;
            }
        }

        // the contact doesn't exist in the local database.
        // it must be a remote addition.
        return -1;
    }

    void calculateRemoteDelta(const QList<QContact> &allReceivedContacts, const QList<QContact> &localVkContacts, QList<QContact> *additions, QList<QContact> *modifications, QList<QContact> *deletions)
    {
        // determine deletions
        for (int i = 0; i < localVkContacts.size(); ++i) {
            const QContact &c(localVkContacts[i]);
            const QString &existingGuid = c.detail<QContactGuid>().guid();
            bool found = false;
            foreach (const QContact &rc, allReceivedContacts) {
                if (rc.detail<QContactGuid>().guid() == existingGuid) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // must have been deleted remotely.
                deletions->append(c);
            }
        }

        // determine additions/modifications
        for (int i = 0; i < allReceivedContacts.size(); ++i) {
            QContact c = allReceivedContacts[i];
            bool hasChanged = false;
            int idx = matchingContactIndex(&c, localVkContacts, &hasChanged);
            if (idx < 0) {
                // new contact
                additions->append(c);
            } else if (hasChanged) {
                // modified contact
                modifications->append(c);
            }
        }
    }
}

VKContactSyncAdaptor::VKContactSyncAdaptor(QObject *parent)
    : VKDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Contacts, parent)
    , QtContactsSqliteExtensions::TwoWayContactSyncAdapter(QStringLiteral("vk"))
    , m_workerObject(new VKContactImageDownloader())
{
    connect(m_workerObject, &AbstractImageDownloader::imageDownloaded,
            this, &VKContactSyncAdaptor::imageDownloaded);

    // can sync, enabled
    setInitialActive(true);
}

VKContactSyncAdaptor::~VKContactSyncAdaptor()
{
    delete m_workerObject;
}

QString VKContactSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("vk-contacts");
}

void VKContactSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    m_apiRequestsRemaining[accountId] = 99; // assume we can make up to 99 requests per sync, before being throttled.

    // call superclass impl.
    VKDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void VKContactSyncAdaptor::purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode)
{
    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_VK_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);

    int purgeCount = 0;
    QList<QContactId> contactsToRemove;
    QList<QContact> localContacts = m_contactManager.contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);
    for (int i = 0; i < localContacts.size(); ++i) {
        const QContact &c(localContacts[i]);
        if (c.detail<QContactGuid>().guid().startsWith(QStringLiteral("%1:").arg(oldId))) {
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
            SOCIALD_LOG_ERROR("Failed to remove stale contacts during purge of account" << oldId << ":" << m_contactManager.error());
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
    purgeKeys << QStringLiteral("contactIds")
              << QStringLiteral("contactAvatars");

    // we cannot use the oob scope cached in (d->m_stateData[QString::number(oldId)].m_oobScope
    // here, as the state data is only initialized during sync, and thus is not available
    // in cleanUp() codepath.
    QString oobScope = QStringLiteral("%1-%2").arg(SOCIALD_VK_CONTACTS_SYNCTARGET).arg(oldId);
    if (!d->m_engine->removeOOB(oobScope, purgeKeys)) {
        success = false;
        SOCIALD_LOG_ERROR("Error occurred while purging OOB data for removed VK account:" << oldId);
    }

    if (success) {
        SOCIALD_LOG_INFO("Purged account" << oldId << "and successfully removed" << purgeCount << "contacts");
    }
}

void VKContactSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    // clear our cache lists if necessary.
    m_localChanges[accountId].clear();
    m_remoteContacts[accountId].clear();
    m_remoteAddMods[accountId].clear();
    m_remoteDels[accountId].clear();
    m_accessTokens[accountId] = accessToken;

    QDateTime remoteSince;
    if (!initSyncAdapter(QString::number(accountId))
            || !readSyncStateData(&remoteSince, QString::number(accountId))) {
        SOCIALD_LOG_ERROR("unable to init sync adapter - aborting sync VK contacts with account:" << accountId);
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    determineRemoteChanges(remoteSince, QString::number(accountId));
}

void VKContactSyncAdaptor::determineRemoteChanges(const QDateTime &remoteSince, const QString &accountId)
{
    int accId = accountId.toInt();
    requestData(accId, m_accessTokens[accId], 0, QString(), remoteSince);
}

void VKContactSyncAdaptor::requestData(int accountId, const QString &accessToken, int startIndex, const QString &continuationRequest, const QDateTime &syncTimestamp)
{
    QUrl requestUrl;
    if (continuationRequest.isEmpty()) {
        QUrlQuery urlQuery;
        requestUrl = QUrl(QStringLiteral("https://api.vk.com/method/friends.get"));
        if (startIndex >= 1) {
            urlQuery.addQueryItem ("offset", QString::number(startIndex));
        }
        urlQuery.addQueryItem("count", QString::number(SOCIALD_VK_MAX_CONTACT_ENTRY_RESULTS));
        urlQuery.addQueryItem("fields", QStringLiteral("uid,first_name,last_name,sex,screen_name,bdate,photo_max,contacts,city"));
        urlQuery.addQueryItem("access_token", accessToken);
        urlQuery.addQueryItem("v", QStringLiteral("5.21")); // version
        requestUrl.setQuery(urlQuery);
    } else {
        requestUrl = QUrl(continuationRequest);
    }

    QNetworkRequest req(requestUrl);

    // we're requesting data.  Increment the semaphore so that we know we're still busy.
    incrementSemaphore(accountId);
    QNetworkReply *reply = m_networkAccessManager->get(req);
    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        reply->setProperty("continuationRequest", continuationRequest);
        reply->setProperty("lastSyncTimestamp", syncTimestamp);
        reply->setProperty("startIndex", startIndex);
        connect(reply, SIGNAL(finished()), this, SLOT(contactsFinishedHandler()));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        m_apiRequestsRemaining[accountId] = m_apiRequestsRemaining[accountId] - 1;
        setupReplyTimeout(accountId, reply);
    } else {
        SOCIALD_LOG_ERROR("unable to request data from VK account with id:" << accountId);
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
    }
}

void VKContactSyncAdaptor::contactsFinishedHandler()
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

    SOCIALD_LOG_TRACE("received VK friends data for account:" << accountId << ":");
    Q_FOREACH (const QString &line, QString::fromUtf8(data).split('\n', QString::SkipEmptyParts)) {
        SOCIALD_LOG_TRACE(line);
    }

    if (isError) {
        SOCIALD_LOG_ERROR("error occurred when performing contacts request for VK account:" << accountId);
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    } else if (data.isEmpty()) {
        SOCIALD_LOG_ERROR("no contact data in reply from VK with account:" << accountId);
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    // parse the remote contact information from the response
    QString continuationUrl;
    QJsonObject obj = QJsonDocument::fromJson(data).object(); // XXX TODO: check errors / continuations / etc.
    m_remoteContacts[accountId].append(parseContacts(obj.value("response").toObject().value("items").toArray(), accountId, accessToken, &continuationUrl));

    if (syncAborted()) {
        SOCIALD_LOG_INFO("sync aborted, not continuing sync of contacts from VK with account:" << accountId);
    } else if (!continuationUrl.isEmpty()) {
        startIndex += SOCIALD_VK_MAX_CONTACT_ENTRY_RESULTS;
        requestData(accountId, accessToken, startIndex, continuationUrl, lastSyncTimestamp);
    } else {
        // we're finished downloading the remote changes - we should sync local changes up.
        continueSync(accountId, accessToken);
    }

    decrementSemaphore(accountId);
}

void VKContactSyncAdaptor::continueSync(int accountId, const QString &accessToken)
{
    Q_UNUSED(accessToken) // maybe needed for avatar?

    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_VK_CONTACTS_SYNCTARGET);

    // now that we have all of the contacts received, we can determine
    // whether any deletions have occurred, and also additions/modifications.
    QList<QContact> adds, mods, dels;
    QList<QContact> currentContacts = m_contactManager.contacts(syncTargetFilter);
    for (int i = currentContacts.size() - 1; i >= 0; --i) {
        if (!currentContacts[i].detail<QContactGuid>().guid().startsWith(QStringLiteral("%1:").arg(accountId))) {
            // not from this account.
            currentContacts.removeAt(i);
        }
    }

    calculateRemoteDelta(m_remoteContacts[accountId], currentContacts, &adds, &mods, &dels);
    SOCIALD_LOG_INFO("VK contact sync with account" << accountId <<
                     "got remote changes: a:" << adds.size() << "m:" << mods.size() << "r:" << dels.size());

    for (int i = 0; i < adds.size(); ++i) {
        QContact c = adds[i];
        m_remoteAddMods[accountId].append(c);
        SOCIALD_LOG_TRACE("have added contact:" <<
                          c.detail<QContactName>().firstName() <<
                          c.detail<QContactName>().lastName() <<
                          c.detail<QContactDisplayLabel>().label());
    }
    for (int i = 0; i < mods.size(); ++i) {
        QContact c = mods[i];
        // we have to do this, because the id reported in the contact might have changed between syncs
        // and the sync adapter contract requires that the id we set in it be the same as the id we
        // got for it previously, rather than the current database state.
        c.setId(QContactId::fromString(m_contactIds[accountId].value(c.detail<QContactGuid>().guid())));
        m_remoteAddMods[accountId].append(c);
        SOCIALD_LOG_TRACE("have modified contact:" <<
                          c.detail<QContactName>().firstName() <<
                          c.detail<QContactName>().lastName() <<
                          c.detail<QContactDisplayLabel>().label());
    }
    for (int i = 0; i < dels.size(); ++i) {
        QContact c = dels[i];
        c.setId(QContactId::fromString(m_contactIds[accountId].value(c.detail<QContactGuid>().guid())));
        m_contactAvatars[accountId].remove(c.detail<QContactGuid>().guid()); // just in case the avatar was outstanding.
        m_remoteDels[accountId].append(c);
        SOCIALD_LOG_TRACE("have removed contact:" <<
                          c.detail<QContactName>().firstName() <<
                          c.detail<QContactName>().lastName() <<
                          c.detail<QContactDisplayLabel>().label());
    }

    // now store the changes locally
    if (!storeRemoteChanges(m_remoteDels[accountId], &m_remoteAddMods[accountId], QString::number(accountId))) {
        SOCIALD_LOG_ERROR("unable to store remote changes locally - aborting sync VK contacts for account" << accountId);
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        // note: don't decrement here - it's done by contactsFinishedHandler().
        return;
    }

    // update our mapping of GUID to QContactId
    foreach (const QContact &c, m_remoteAddMods[accountId]) {
        if (c.id().toString().trimmed().isEmpty()) {
            SOCIALD_LOG_ERROR("No local contact id specified for contact with guid" << c.detail<QContactGuid>().guid() <<
                              "from account" << accountId);
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
        SOCIALD_LOG_ERROR("unable to determine local changes - aborting sync VK contacts for account" << accountId);
        purgeSyncStateData(QString::number(accountId));
        setStatus(SocialNetworkSyncAdaptor::Error);
        // note: don't decrement here - it's done by contactsFinishedHandler().
        return;
    }

    // now push those changes up to VK.
    upsyncLocalChanges(localSince, locallyAdded, locallyModified, locallyDeleted, QString::number(accountId));
}

void VKContactSyncAdaptor::upsyncLocalChanges(const QDateTime &localSince,
                                              const QList<QContact> &locallyAdded,
                                              const QList<QContact> &locallyModified,
                                              const QList<QContact> &locallyDeleted,
                                              const QString &accountId)
{
    int accId = accountId.toInt();
    QList<QPair<QContact, VKContactSyncAdaptor::UpdateType> > contactUpdatesToPost;
    foreach (const QContact &c, locallyAdded)
        contactUpdatesToPost.append(qMakePair(c, VKContactSyncAdaptor::Add));
    foreach (const QContact &c, locallyModified)
        contactUpdatesToPost.append(qMakePair(c, VKContactSyncAdaptor::Modify));
    foreach (const QContact &c, locallyDeleted) {
        contactUpdatesToPost.append(qMakePair(c, VKContactSyncAdaptor::Remove));
        m_contactAvatars[accId].remove(c.detail<QContactGuid>().guid()); // just in case the avatar was outstanding.
    }
    m_localChanges[accId] = contactUpdatesToPost;

    SOCIALD_LOG_INFO("VK account:" << accId <<
                     "upsyncing local contact changes since:" << localSince.toString(Qt::ISODate) <<
                     "-> local A/M/R:" << locallyAdded.count() << "/" << locallyModified.count() << "/" << locallyDeleted.count());

    upsyncLocalChangesList(accId);
}

bool VKContactSyncAdaptor::testAccountProvenance(const QContact &contact, const QString &accountId)
{
    return contact.detail<QContactGuid>().guid().startsWith(QStringLiteral("%1:").arg(accountId));
}

void VKContactSyncAdaptor::upsyncLocalChangesList(int accountId)
{
    bool postedData = false;
    if (!m_accountSyncProfile || m_accountSyncProfile->syncDirection() != Buteo::SyncProfile::SYNC_DIRECTION_FROM_REMOTE) {
        // we don't yet support upsync
        SOCIALD_LOG_INFO("upload of local contacts changes for VK account" << accountId << "not yet implemented");
    } else {
        SOCIALD_LOG_INFO("skipping upload of local contacts changes due to profile direction setting for account" << accountId);
    }

    if (!postedData) {
        // nothing left to upsync.  attempt to download any outstanding avatars.
        queueOutstandingAvatars(accountId, m_accessTokens[accountId]);
    }
}

void VKContactSyncAdaptor::queueOutstandingAvatars(int accountId, const QString &accessToken)
{
    if (syncAborted()) {
        SOCIALD_LOG_DEBUG("sync aborted, skipping queuing avatars for contacts from VK account:" << accountId);
        return;
    }

    int queuedCount = 0;
    for (QMap<QString, QString>::const_iterator it = m_contactAvatars[accountId].constBegin();
            it != m_contactAvatars[accountId].constEnd(); ++it) {
        if (!it.value().isEmpty() && queueAvatarForDownload(accountId, accessToken, it.key(), it.value())) {
            queuedCount++;
        }
    }

    SOCIALD_LOG_DEBUG("queued" << queuedCount << "avatars for download for account" << accountId);
}

bool VKContactSyncAdaptor::queueAvatarForDownload(int accountId, const QString &accessToken, const QString &contactGuid, const QString &imageUrl)
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


QList<QContact> VKContactSyncAdaptor::parseContacts(const QJsonArray &json, int accountId, const QString &accessToken, QString *continuationUrl)
{
    // XXX TODO: parse the continuation URL!!
    Q_UNUSED(continuationUrl)

    QList<QContact> retn;
    QJsonArray::const_iterator it = json.constBegin();
    for ( ; it != json.constEnd(); ++it) {
        const QJsonObject &obj((*it).toObject());
        if (obj.isEmpty()) continue;

        QString mobilePhone = obj.value("mobile_phone").toString();
        QString homePhone = obj.value("home_phone").toString();

        if (mobilePhone.isEmpty() && homePhone.isEmpty()) {
            // no contact information, skip
            continue;
        }

        // build the contact.
        QContact c;

        QContactGuid guid;
        int uidint = static_cast<int>(obj.value("uid").toDouble()); // horrible hack.
        guid.setGuid(QStringLiteral("%1:%2").arg(accountId).arg(QString::number(uidint)));
        saveNonexportableDetail(c, guid);

        QContactName name;
        name.setFirstName(obj.value("first_name").toString());
        name.setLastName(obj.value("last_name").toString());
        saveNonexportableDetail(c, name);

        if (obj.value("sex").toDouble() > 0) {
            double genderVal = obj.value("sex").toDouble();
            QContactGender gender;
            if (genderVal == 1.0) {
                gender.setGender(QContactGender::GenderFemale);
            } else {
                gender.setGender(QContactGender::GenderMale);
            }
            saveNonexportableDetail(c, gender);
        }

        if (!obj.value("bdate").toString().isEmpty() && obj.value("bdate").toString().length() > 5) {
            // DD.MM.YYYY form, we ignore DD.MM (yearless) form response.
            QContactBirthday birthday;
            birthday.setDateTime(QDateTime::fromString(obj.value("bdate").toString(), "dd.MM.yyyy"));
            saveNonexportableDetail(c, birthday);
        }

        if (!obj.value("screen_name").toString().isEmpty() &&
                obj.value("screen_name").toString() != QStringLiteral("id%1").arg(c.detail<QContactGuid>().guid())) {
            QContactNickname nickname;
            nickname.setNickname(obj.value("screen_name").toString());
            saveNonexportableDetail(c, nickname);
        }

        if (!obj.value("photo_max").toString().isEmpty() &&
                !obj.value("photo_max").toString().startsWith(QStringLiteral("http://vk.com/images/camera_"))) {
            QContactAvatar avatar;
            avatar.setImageUrl(QUrl(obj.value("photo_max").toString()));
            avatar.setValue(QContactAvatar__FieldAvatarMetadata, QStringLiteral("picture"));
            saveNonexportableDetail(c, avatar);
        }

        if (!obj.value("city").toObject().isEmpty() && !obj.value("city").toObject().value("title").toString().isEmpty()) {
            QContactAddress addr;
            addr.setLocality(obj.value("city").toObject().value("title").toString());
            saveNonexportableDetail(c, addr);
        }

        if (!mobilePhone.isEmpty()) {
            QContactPhoneNumber num;
            num.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeMobile);
            num.setNumber(obj.value("mobile_phone").toString());
            saveNonexportableDetail(c, num);
        }

        if (!homePhone.isEmpty()) {
            QContactPhoneNumber num;
            num.setContexts(QContactDetail::ContextHome);
            num.setSubTypes(QList<int>() << QContactPhoneNumber::SubTypeLandline);
            num.setNumber(obj.value("mobile_phone").toString());
            saveNonexportableDetail(c, num);
        }

        retn.append(c);
    }

    // fixup the contact avatars.
    transformContactAvatars(retn, accountId, accessToken);
    return retn;
}

void VKContactSyncAdaptor::transformContactAvatars(QList<QContact> &remoteContacts, int accountId, const QString &accessToken)
{
    // The avatar detail from the remote contact will be some remote URL.
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
                QString localFileName = VKContactImageDownloader::staticOutputFile(
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
                        saveNonexportableDetail(curr, avatar);
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

void VKContactSyncAdaptor::imageDownloaded(const QString &url, const QString &path,
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

void VKContactSyncAdaptor::finalize(int accountId)
{
    if (syncAborted()) {
        SOCIALD_LOG_DEBUG("sync aborted, skipping finalize of VK contacts from account:" << accountId);
    } else {
        SOCIALD_LOG_DEBUG("finalizing VK contacts sync with account:" << accountId);
        // first, ensure we update any avatars required.
        if (m_downloadedContactAvatars[accountId].size()) {
            // load all VK contacts from the database.  We need all details, to avoid clobber.
            QContactDetailFilter syncTargetFilter;
            syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
            syncTargetFilter.setValue(SOCIALD_VK_CONTACTS_SYNCTARGET);
            QList<QContact> VKContacts = m_contactManager.contacts(syncTargetFilter);

            // find the contacts we need to update.
            QMap<QString, QContactAvatar> avatarsToSave;
            QMap<QString, QContact> contactsToSave;
            for (QMap<QString, QString>::const_iterator it = m_downloadedContactAvatars[accountId].constBegin();
                    it != m_downloadedContactAvatars[accountId].constEnd(); ++it) {
                for (int i = 0; i < VKContacts.size(); ++i) {
                    const QString &contactGuid(VKContacts[i].detail<QContactGuid>().guid());
                    if (it.key() == contactGuid) {
                        // we have downloaded the avatar for this contact, and need to update it.
                        QContact c = VKContacts[i];
                        QContactAvatar a;
                        Q_FOREACH (const QContactAvatar &av, c.details<QContactAvatar>()) {
                            if (av.value(QContactAvatar__FieldAvatarMetadata).toString() == QStringLiteral("picture")) {
                                a = av;
                                break;
                            }
                        }
                        a.setValue(QContactAvatar__FieldAvatarMetadata, QVariant::fromValue<QString>(QStringLiteral("picture")));
                        a.setImageUrl(it.value());
                        saveNonexportableDetail(c, a);
                        contactsToSave[contactGuid] = c;
                        avatarsToSave[contactGuid] = a;
                        break;
                    }
                }
            }

            QList<QContact> saveList = contactsToSave.values();
            if (m_contactManager.saveContacts(&saveList)) {
                SOCIALD_LOG_INFO("finalize: added avatars for" << saveList.size() << "VK contacts from account" << accountId);

                // update our mutated prev remote versions with the added avatar detail.
                foreach (const QContact &c, saveList) {
                    const QString &contactGuid(c.detail<QContactGuid>().guid());
                    for (int i = 0; i < d->m_stateData[QString::number(accountId)].m_mutatedPrevRemote.size(); ++i) {
                        if (contactGuid == d->m_stateData[QString::number(accountId)].m_mutatedPrevRemote[i].detail<QContactGuid>().guid()) {
                            QContact mprc = d->m_stateData[QString::number(accountId)].m_mutatedPrevRemote[i];
                            QContactAvatar avatar = avatarsToSave[contactGuid];
                            saveNonexportableDetail(mprc, avatar);
                            d->m_stateData[QString::number(accountId)].m_mutatedPrevRemote.replace(i, mprc);
                            break;
                        }
                    }
                }
            } else {
                SOCIALD_LOG_ERROR("finalize: error adding avatars for" << saveList.size() << "VK contacts from account" << accountId);
            }
        }

        if (!storeSyncStateData(QString::number(accountId))) {
            SOCIALD_LOG_ERROR("unable to finalize sync of VK contacts with account" << accountId);
            purgeSyncStateData(QString::number(accountId));
            setStatus(SocialNetworkSyncAdaptor::Error);
        }
    }
}

void VKContactSyncAdaptor::finalCleanup()
{
    // Synchronously find any contacts which need to be removed,
    // which were somehow "left behind" by the sync process.
    // Also, determine if any avatars were not synced, and remove those details.

    // first, get a list of all existing VK account ids
    QList<int> VKAccountIds;
    QList<int> purgeAccountIds;
    QList<int> currentAccountIds;
    QList<uint> uaids = m_accountManager->accountList();
    foreach (uint uaid, uaids) {
        currentAccountIds.append(static_cast<int>(uaid));
    }
    foreach (int currId, currentAccountIds) {
        Accounts::Account *act = Accounts::Account::fromId(m_accountManager, currId, this);
        if (act) {
            if (act->providerName() == QString(QLatin1String("vk"))) {
                // this account still exists, no need to purge its content
                VKAccountIds.append(currId);
            }
            act->deleteLater();
        }
    }

    // second, get all contacts which have been synced from VK.
    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_VK_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);
    noRelationships.setDetailTypesHint(QList<QContactDetail::DetailType>() << QContactGuid::Type << QContactAvatar::Type);
    QList<QContact> VKContacts = m_contactManager.contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);

    // third, find all account ids from which contacts have been synced
    foreach (const QContact &contact, VKContacts) {
        QContactGuid guid = contact.detail<QContactGuid>();
        QStringList guidParts = guid.guid().split(":");
        QString accountIdStr = guidParts.size() ? guidParts.first() : QString();
        if (!accountIdStr.isEmpty()) {
            int purgeId = accountIdStr.toInt();
            if (purgeId && !VKAccountIds.contains(purgeId)
                    && !purgeAccountIds.contains(purgeId)) {
                // this account no longer exists, and needs to be purged.
                purgeAccountIds.append(purgeId);
            }
        } else {
            qWarning() << Q_FUNC_INFO << "Malformed GUID not of form <accountId>:<remoteGuid>";
        }
    }

    // fourth, remove any non-existent avatar details.
    // We save these first, in case some contacts get removed by purge.
    QMap<QString, QContact> contactsToSave;
    for (int i = 0; i < VKContacts.size(); ++i) {
        QContact contact = VKContacts.at(i);
        const QString contactGuid(contact.detail<QContactGuid>().guid());
        // remove any nonexistent/error avatar details
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
                        // download failed, remove it from the contact.
                        contact.removeDetail(&av);
                        contactsToSave[contactGuid] = contact;
                    }
                }
            }
        }
    }

    QList<QContact> saveList = contactsToSave.values();
    if (m_contactManager.saveContacts(&saveList)) {
        SOCIALD_LOG_INFO("finalCleanup() fixed up avatars from" << saveList.size() << "VK contacts");
    } else {
        SOCIALD_LOG_ERROR("finalCleanup() failed to save non-existent avatar removals for VK contacts");
    }

    // fifth, purge all data for those account ids which no longer exist.
    if (purgeAccountIds.size()) {
        SOCIALD_LOG_INFO("finalCleanup() purging contacts from" << purgeAccountIds.size() << "non-existent VK accounts");
        foreach (int purgeId, purgeAccountIds) {
            purgeDataForOldAccount(purgeId, SocialNetworkSyncAdaptor::SyncPurge);
        }
    }
}

bool VKContactSyncAdaptor::readSyncStateData(QDateTime *remoteSince, const QString &accountId, TwoWayContactSyncAdapter::ReadStateMode readMode)
{
    // read the standard state data
    if (!TwoWayContactSyncAdapter::readSyncStateData(remoteSince, accountId, readMode)) {
        SOCIALD_LOG_ERROR("failed to read standard state data for VK account" << accountId);
        return false;
    }

    bool ok = false;
    int accId = accountId.toInt(&ok);
    if (accId == 0 || !ok) {
        SOCIALD_LOG_ERROR("invalid account id specified to readSyncStateData:" << accountId);
        return false;
    }

    // then read the extra state data which is specific to this sync adapter.
    QMap<QString, QVariant> values;
    QStringList keys;
    keys << QStringLiteral("contactIds")
         << QStringLiteral("contactAvatars");
    if (!d->m_engine->fetchOOB(d->m_stateData[accountId].m_oobScope, keys, &values)) {
        SOCIALD_LOG_ERROR("failed to read extra data for VK account" << accountId);
        d->clear(accountId);
        return false;
    }

    // m_contactIds
    QVariant ciValue = values.value(QStringLiteral("contactIds"));
    QByteArray ciValueBA = ciValue.toByteArray();
    QJsonObject ciJsonObj = QJsonDocument::fromBinaryData(ciValueBA).object();
    QStringList contactGuids = ciJsonObj.keys();
    QMap<QString, QString> guidToContactId;
    foreach (const QString &guid, contactGuids) {
        guidToContactId.insert(guid, ciJsonObj.value(guid).toString());
    }
    m_contactIds[accId] = guidToContactId;

    // m_contactAvatars
    QVariant caValue = values.value(QStringLiteral("contactAvatars"));
    QByteArray caValueBA = caValue.toByteArray();
    QJsonObject caJsonObj = QJsonDocument::fromBinaryData(caValueBA).object();
    contactGuids = caJsonObj.keys();
    QMap<QString, QString> guidToContactAvatar;
    foreach (const QString &guid, contactGuids) {
        guidToContactAvatar.insert(guid, caJsonObj.value(guid).toString());
    }
    m_contactAvatars[accId] = guidToContactAvatar;
    SOCIALD_LOG_INFO("have" << guidToContactAvatar.size() << "outstanding contact avatars to sync from account" << accountId);

    // done.
    return true;
}

bool VKContactSyncAdaptor::storeSyncStateData(const QString &accountId)
{
    bool ok = false;
    int accId = accountId.toInt(&ok);
    if (accId == 0 || !ok) {
        SOCIALD_LOG_ERROR("invalid account id specified to storeSyncStateData:" << accountId);
        return false;
    }

    // m_contactIds
    QJsonObject ciJsonObj;
    for (QMap<QString, QString>::const_iterator it = m_contactIds[accId].constBegin();
            it != m_contactIds[accId].constEnd(); ++it) {
        ciJsonObj.insert(it.key(), QJsonValue(it.value()));
    }
    QJsonDocument ciJsonDoc(ciJsonObj);
    QVariant ciValue(ciJsonDoc.toBinaryData());

    // m_contactAvatars
    QJsonObject caJsonObj;
    for (QMap<QString, QString>::const_iterator it = m_contactAvatars[accId].constBegin();
            it != m_contactAvatars[accId].constEnd(); ++it) {
        caJsonObj.insert(it.key(), QJsonValue(it.value()));
    }
    QJsonDocument caJsonDoc(caJsonObj);
    QVariant caValue(caJsonDoc.toBinaryData());

    // store to OOB
    QMap<QString, QVariant> values;
    values.insert("contactIds", ciValue);
    values.insert("contactAvatars", caValue);
    if (!d->m_engine->storeOOB(d->m_stateData[accountId].m_oobScope, values)) {
        SOCIALD_LOG_ERROR("failed to store extra state data for VK account:" << accId);
        d->clear(accountId);
        return false;
    }

    return TwoWayContactSyncAdapter::storeSyncStateData(accountId);
}

bool VKContactSyncAdaptor::purgeSyncStateData(const QString &accountId, bool purgePartialSyncStateData)
{
    QStringList purgeKeys;
    purgeKeys << QStringLiteral("contactIds")
              << QStringLiteral("contactAvatars");
    QString oobScope = d->m_stateData[accountId].m_oobScope;
    bool extraPurgeSuccess = d->m_engine->removeOOB(oobScope, purgeKeys);
    bool normalPurgeSuccess = TwoWayContactSyncAdapter::purgeSyncStateData(accountId, purgePartialSyncStateData);
    return extraPurgeSuccess && normalPurgeSuccess;
}
