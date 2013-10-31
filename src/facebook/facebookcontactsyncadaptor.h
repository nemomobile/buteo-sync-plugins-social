/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKCONTACTSYNCADAPTOR_H
#define FACEBOOKCONTACTSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>
#include <QtSql/QSqlDatabase>

#include <QtContacts/QContactManager>
#include <QtContacts/QContact>

#include <socialcache/facebookcontactsdatabase.h>

USE_CONTACTS_NAMESPACE

class FacebookContactImageDownloader;
class FacebookContactSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookContactSyncAdaptor(SyncService *syncService, QObject *parent);

    void sync(const QString &dataType);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestData(int accountId, const QString &accessToken,
                     const QString &continuationRequest = QString(),
                     const QDateTime &syncTimestamp = QDateTime());
    void purgeAccount(int accountId);

Q_SIGNALS:
    // Used internally
    void requestQueue(const QString &url, const QVariantMap &data);

private Q_SLOTS:
    void friendsFinishedHandler();
    void slotImageDownloaded(const QString &url, const QString &path, const QVariantMap &data);

private:
    FacebookContactsDatabase m_db;
    QContactManager *m_contactManager;

    // for server-side removal detection.
    QMap<int, QSet<QString> > m_cachedFriendIds; // local friends per account
    void initRemovalDetection(int accountId);
    void clearRemovalDetectionLists(); // to avoid spurious removal of cached data if error occurs.
    void purgeDetectedRemovals();
    bool purgeContacts(const QStringList &friendIds, int accountId);
    QList<QContactId> contactIdsForGuid(const QString &fbuid);
    QContact newOrExistingContact(const QString &fbuid, bool *isNewContact);
    void parseContactDetails(const QJsonObject &blobDetails, int accountId);
    QMap<QString, QContact> m_contactsToSave;
    QList<QContact> m_newContactsToSave;
    FacebookContactImageDownloader *m_workerObject;
    QSet<int> m_populatingAvatarsAccountsId;
};

#endif // FACEBOOKCONTACTSYNCADAPTOR_H
