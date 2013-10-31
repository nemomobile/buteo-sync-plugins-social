/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "googlecontactsyncadaptor.h"
#include "googlecontactstream.h"
#include "googlecontactatom.h"
#include "syncservice.h"
#include "trace.h"

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QFile>
#include <QtCore/QByteArray>

#include <QtContacts/QContactManager>
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

#include <qtcontacts-extensions.h>
#include <qtcontacts-extensions_impl.h>
#include <qcontactoriginmetadata_impl.h>
#include <QContactOriginMetadata>

#include <socialcache/abstractimagedownloader.h>
#include <socialcache/abstractimagedownloader_p.h>

#define SOCIALD_GOOGLE_CONTACTS_SYNCTARGET QLatin1String("google")
#define SOCIALD_GOOGLE_MAX_CONTACT_ENTRY_RESULTS 500

static const char *TOKEN_KEY = "url";
static const char *ACCOUNT_ID_KEY = "account_id";

static QContactManager *aggregatingContactManager(QObject *parent)
{
    QContactManager *retn = new QContactManager(
            QString::fromLatin1("org.nemomobile.contacts.sqlite"),
            QMap<QString, QString>(),
            parent);
    if (retn->managerName() != QLatin1String("org.nemomobile.contacts.sqlite")) {
        // the manager specified is not the aggregating manager we depend on.
        delete retn;
        return 0;
    }

    return retn;
}

class GoogleContactImageDownloader: public AbstractImageDownloader
{
    Q_OBJECT

public:
    explicit GoogleContactImageDownloader();
    static QString staticOutputFile(const QString &url);
protected:
    QNetworkReply * createReply(const QString &url, const QVariantMap &metadata);
    // This is a reimplemented method, used by AbstractImageDownloader
    QString outputFile(const QString &url, const QVariantMap &data) const;
private:
    Q_DECLARE_PRIVATE(AbstractImageDownloader)
};

GoogleContactImageDownloader::GoogleContactImageDownloader()
    : AbstractImageDownloader()
{
}

QString GoogleContactImageDownloader::staticOutputFile(const QString &url)
{
    // We create the identifier by appending the type to the real identifier
    if (url.isEmpty()) {
        return QString();
    }

    // XXX TODO: change to Google once we modify the SocialSyncInterface to include it.
    return makeOutputFile(SocialSyncInterface::Facebook, SocialSyncInterface::Contacts, url);
}

QNetworkReply * GoogleContactImageDownloader::createReply(const QString &url,
                                                          const QVariantMap &metadata)
{
    Q_D(AbstractImageDownloader);

    QString accessToken = metadata.value(TOKEN_KEY).toString();
    QNetworkRequest request(url);
    request.setRawHeader("GData-Version", "3.0");
    request.setRawHeader(QString(QLatin1String("Authorization")).toUtf8(),
                         QString(QLatin1String("Bearer ") + accessToken).toUtf8());
    return d->networkAccessManager->get(request);
}

QString GoogleContactImageDownloader::outputFile(const QString &url, const QVariantMap &data) const
{
    Q_UNUSED(data)
    return staticOutputFile(url);
}

//------------------

GoogleContactSyncAdaptor::GoogleContactSyncAdaptor(SyncService *syncService, QObject *parent)
    : GoogleDataTypeSyncAdaptor(syncService, SyncService::Contacts, parent)
    , m_contactManager(aggregatingContactManager(this))
    , m_workerObject(new GoogleContactImageDownloader())
{
    setInitialActive(false);
    if (!m_contactManager) {
        TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: no aggregating contact manager exists - Google contacts sync will be inactive")));
        return;
    }

    connect(m_workerObject, &AbstractImageDownloader::imageDownloaded,
            this, &GoogleContactSyncAdaptor::imageDownloaded);

    // can sync, enabled
    setInitialActive(true);
}

GoogleContactSyncAdaptor::~GoogleContactSyncAdaptor()
{
    m_workerObject->deleteLater();
}

void GoogleContactSyncAdaptor::sync(const QString &dataType)
{
    // call superclass impl.
    GoogleDataTypeSyncAdaptor::sync(dataType);
}

void GoogleContactSyncAdaptor::purgeDataForOldAccounts(const QList<int> &purgeIds)
{
    foreach (int pid, purgeIds) {
        // first, purge all data from QtContacts
        purgeAccount(pid);
    }
}

void GoogleContactSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    // clear our cache lists if necessary.
    m_remoteContacts[accountId].clear();

    // begin requesting data.
    requestData(accountId, accessToken);
}

void GoogleContactSyncAdaptor::requestData(int accountId, const QString &accessToken, int startIndex, const QString &continuationRequest, const QDateTime &syncTimestamp)
{
    QDateTime timestamp = syncTimestamp.isValid() ? syncTimestamp :
                          lastSyncTimestamp(QLatin1String("google"),
                                            SyncService::dataType(SyncService::Contacts),
                                            accountId);

    QUrl requestUrl;
    if (continuationRequest.isEmpty()) {
        requestUrl = QUrl(QString(QLatin1String("https://www.google.com/m8/feeds/contacts/default/full/")));
        QUrlQuery urlQuery;
        //For now, always query all contact information.  In the future, use the sync time as below.  TODO!
        //if (!timestamp.isNull())
        //    urlQuery.addQueryItem("updated-min", timestamp.toString(Qt::ISODate));
        if (startIndex > 1) {
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
        reply->setProperty("lastSyncTimestamp", timestamp);
        reply->setProperty("startIndex", startIndex);
        connect(reply, SIGNAL(finished()), this, SLOT(contactsFinishedHandler()));
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
    } else {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to request contacts from Google account with id %1"))
              .arg(accountId));

        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
    }
}

void GoogleContactSyncAdaptor::contactsFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray data = reply->readAll();
    int startIndex = reply->property("startIndex").toInt();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QDateTime lastSyncTimestamp = reply->property("lastSyncTimestamp").toDateTime();
    reply->deleteLater();
    if (data.isEmpty()) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: no contact data in reply from Google with account %1"))
              .arg(accountId));

        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    GoogleContactStream parser(false);
    GoogleContactAtom *atom = parser.parse(data);

    if (!atom) {
        TRACE(SOCIALD_ERROR,
              QString(QLatin1String("error: unable to parse contacts data from reply from Google using account with id %1"))
              .arg(accountId));

        setStatus(SocialNetworkSyncAdaptor::Error);
        decrementSemaphore(accountId);
        return;
    }

    QList<QContact> remoteContacts = atom->entryContacts();
    if (remoteContacts.size() > 0) {
        m_remoteContacts[accountId].append(remoteContacts);
    }

    if (!atom->nextEntriesUrl().isEmpty()) {
        // request more if they exist.
        startIndex += SOCIALD_GOOGLE_MAX_CONTACT_ENTRY_RESULTS;
        requestData(accountId, accessToken, startIndex, atom->nextEntriesUrl(), lastSyncTimestamp);
    } else {
        // we're finished - we should attempt to update our local cache.
        int addedCount = 0, modifiedCount = 0, removedCount = 0;
        bool success = storeToLocal(accessToken, accountId, &addedCount, &modifiedCount, &removedCount);
        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("Google contact sync with account %1 finished with result: %2: a: %3 m: %4 r: %5"))
              .arg(accountId).arg(success ? "SUCCESS" : "ERROR").arg(addedCount).arg(modifiedCount).arg(removedCount));
    }

    delete atom;
    decrementSemaphore(accountId);
}

bool GoogleContactSyncAdaptor::storeToLocal(const QString &accessToken, int accountId, int *addedCount, int *modifiedCount, int *removedCount)
{
    // steps:
    // 1) load current data from backend
    // 2) determine delta (add/mod/rem)
    // 3) apply delta

    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_GOOGLE_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);

    QList<QContact> remoteContacts = transformContactAvatars(m_remoteContacts[accountId], accountId, accessToken);
    QList<QContact> localContacts = m_contactManager->contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);
    QList<QContact> remoteToSave;
    QList<QContactId> localToRemove;
    QList<QContactId> foundLocal;
    QString accountIdStr = QString::number(accountId);

    // we always use the remote server's data in conflicts
    for (int i = 0; i < remoteContacts.size(); ++i) {
        QContact rc = remoteContacts[i];
        QString guid = rc.detail<QContactGuid>().guid();
        if (guid.isEmpty()) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("skipping: cannot store remote google contact with no guid:")) << rc);
            continue;
        }

        bool foundLocalToModify = false;
        for (int j = 0; j < localContacts.size(); ++j) {
            const QContact &lc = localContacts[j];
            if (lc.detail<QContactGuid>().guid() == guid) {
                // we clobber local data with remote data.
                foundLocalToModify = true;
                rc.setId(lc.id());
                foundLocal.append(lc.id());

                // however, we do need to see if more than one account provides this contact.
                QContactOriginMetadata metaData = lc.detail<QContactOriginMetadata>();
                QStringList accountIds = metaData.groupId().split(',');
                if (accountIds.contains(accountIdStr)) {
                    rc.saveDetail(&metaData);
                } else {
                    accountIds.append(accountIdStr);
                    metaData.setGroupId(accountIds.join(QString::fromLatin1(",")));
                    rc.saveDetail(&metaData);
                }

                break;
            }
        }

        if (foundLocalToModify) {
            *modifiedCount += 1;
        } else {
            // adding a new contact
            *addedCount += 1;
            // need new metadata.
            QContactOriginMetadata metadata = rc.detail<QContactOriginMetadata>();
            metadata.setGroupId(accountIdStr);
            rc.saveDetail(&metadata);
        }

        remoteToSave.append(rc);
    }

    // any local contacts which exist without a remote counterpart
    // are "stale" and should be removed.  Alternatively, if the
    // contact is provided by a different account as well, we need
    // to remove this account from the metadata.
    for (int i = 0; i < localContacts.size(); ++i) {
        QContact lc = localContacts.at(i);
        if (!foundLocal.contains(lc.id())) {
            QContactOriginMetadata metadata = lc.detail<QContactOriginMetadata>();
            QStringList accountIds = metadata.groupId().split(',');
            if (accountIds.contains(accountIdStr)) {
                // this account used to provide this contact, but now does not.
                accountIds.removeAll(accountIdStr);
                if (accountIds.isEmpty()) {
                    // no other account provides this contact, it can be removed.
                    localToRemove.append(lc.id());
                    *removedCount += 1;
                } else {
                    // at least one other account provides this contact also.
                    metadata.setGroupId(accountIds.join(QString::fromLatin1(",")));
                    lc.saveDetail(&metadata);
                    remoteToSave.append(lc); // actually updating a local.
                    *removedCount += 1;      // but we consider it a removal from the account's pov.
                }
            } else {
                // it was always provided by some other account only.  Don't modify this one.
            }
        }
    }

    // now write the changes to the database.
    bool success = true;
    if (remoteToSave.size()) {
        success = m_contactManager->saveContacts(&remoteToSave);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to save contacts: %1 - with account %2"))
                  .arg(m_contactManager->error()).arg(accountId));
        }
    }
    if (localToRemove.size()) {
        success = m_contactManager->removeContacts(localToRemove);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to remove stale contacts: %1 - with account %2"))
                  .arg(m_contactManager->error()).arg(accountId));
        }
    }

    m_remoteContacts[accountId].clear();
    return success;
}

QList<QContact> GoogleContactSyncAdaptor::transformContactAvatars(const QList<QContact> &remoteContacts, int accountId, const QString &accessToken)
{
    // The avatar detail from the remote contact will be of the form:
    // https://www.google.com/m8/feeds/photos/media/user@gmail.com/userId
    // We need to:
    // 1) transform this to a local filename.
    // 2) determine if the local file exists.
    // 3) if not, trigger downloading the avatar.

    QList<QContact> retn;
    for (int i = 0; i < remoteContacts.size(); ++i) {
        QContact curr = remoteContacts.at(i);

        // We only deal with the first avatar from the contact.  If it has multiple,
        // then later avatars will not be transformed.  TODO: fix this.
        // We also only bother to do this for contacts with a GUID, as we don't
        // store locally any contact without one.
        if (curr.details<QContactAvatar>().size() && !curr.detail<QContactGuid>().guid().isEmpty()) {
            // we have a remote avatar which we need to transform.
            QContactAvatar avatar = curr.detail<QContactAvatar>();
            QString remoteImageUrl = avatar.imageUrl().toString();
            if (!remoteImageUrl.isEmpty()) {
                QVariantMap metadata;
                metadata.insert(ACCOUNT_ID_KEY, accountId);
                metadata.insert(TOKEN_KEY, accessToken);

                // transform to a local file name.
                QString localFileName = GoogleContactImageDownloader::staticOutputFile(remoteImageUrl);
                avatar.setImageUrl(localFileName);

                // update the value in the current contact.
                curr.saveDetail(&avatar);

                // and trigger downloading the image, if it doesn't already exist.
                // this means that we shouldn't download images needlessly after
                // first sync, but it also means that if it updates/changes on the
                // server side, we also won't retrieve any updated image.
                if (!QFile::exists(localFileName)) {
                    incrementSemaphore(accountId);
                    m_workerObject->queue(remoteImageUrl, metadata);
                }
            }
        }

        retn.append(curr);
    }

    return retn;
}

void GoogleContactSyncAdaptor::imageDownloaded(const QString &url, const QString &path,
                                               const QVariantMap &metadata)
{
    Q_UNUSED(url)
    Q_UNUSED(path)

    // Load finished, decrement semaphore
    int accountId = metadata.value(ACCOUNT_ID_KEY).toInt();
    decrementSemaphore(accountId);
}

void GoogleContactSyncAdaptor::purgeAccount(int pid)
{
    int purgeCount = 0;
    int modifiedCount = 0;

    QContactDetailFilter syncTargetFilter;
    syncTargetFilter.setDetailType(QContactDetail::TypeSyncTarget, QContactSyncTarget::FieldSyncTarget);
    syncTargetFilter.setValue(SOCIALD_GOOGLE_CONTACTS_SYNCTARGET);
    QContactFetchHint noRelationships;
    noRelationships.setOptimizationHints(QContactFetchHint::NoRelationships);

    QString accountIdStr = QString::number(pid);
    QList<QContact> localContacts = m_contactManager->contacts(syncTargetFilter, QList<QContactSortOrder>(), noRelationships);
    QList<QContact> contactsToUpdate;
    QList<QContactId> contactsToRemove;
    for (int i = 0; i < localContacts.size(); ++i) {
        QContact c = localContacts.at(i);
        QContactOriginMetadata metadata = c.detail<QContactOriginMetadata>();
        QStringList accountIds = metadata.groupId().split(',');
        if (accountIds.contains(accountIdStr)) {
            // this account used to provide this contact, and we're purging this account.
            accountIds.removeAll(accountIdStr);
            if (accountIds.isEmpty()) {
                // no other account provides this contact, it can be removed.
                contactsToRemove.append(c.id());
                purgeCount += 1;
            } else {
                // at least one other account provides this contact also.
                metadata.setGroupId(accountIds.join(QString::fromLatin1(",")));
                c.saveDetail(&metadata);
                contactsToUpdate.append(c);
                modifiedCount += 1;
            }
        } else {
            // it was always provided by some other account only.  Don't modify this one.
        }
    }

    // now write the changes to the database.
    bool success = true;
    if (contactsToUpdate.size()) {
        success = m_contactManager->saveContacts(&contactsToUpdate);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to update contacts: %1 - during purge of account %2"))
                  .arg(m_contactManager->error()).arg(pid));
        }
    }
    if (contactsToRemove.size()) {
        success = m_contactManager->removeContacts(contactsToRemove);
        if (!success) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("Failed to remove stale contacts: %1 - during purge of account %2"))
                  .arg(m_contactManager->error()).arg(pid));
        }
    }

    if (success) {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("purged account %1 and successfully removed %2 friends (kept %3 modified friends)"))
                .arg(pid).arg(purgeCount).arg(modifiedCount));
    }
}

#include "googlecontactsyncadaptor.moc"
