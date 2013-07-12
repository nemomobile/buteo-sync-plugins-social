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

USE_CONTACTS_NAMESPACE

class FacebookContactSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookContactSyncAdaptor(SyncService *syncService, QObject *parent);
    ~FacebookContactSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestData(int accountId, const QString &accessToken,
                     const QString &fbFriendId = QString(),
                     const QString &avatarUrl = QString(),
                     const QString &avatarType = QString(),
                     const QString &continuationRequest = QString(),
                     const QDateTime &syncTimestamp = QDateTime());
    void purgeAccount(int pid);

private Q_SLOTS:
    void friendsFinishedHandler();
    void avatarFinishedHandler();

private:
    QSqlDatabase m_contactSyncDb;
    QContactManager *m_contactManager;

    // for server-side removal detection.
    QMultiMap<int, QString> m_cachedFriendIds; // local friends per account
    QMultiMap<int, QString> m_serverFriendIds; // server-side friends per account
    void initRemovalDetectionLists();
    void clearRemovalDetectionLists(); // to avoid spurious removal of cached data if error occurs.
    void purgeDetectedRemovals();
    bool purgeFriend(const QString &friendId, int accountId, bool purgeContact);
    QList<QContactId> contactIdsForGuid(const QString &fbuid);
    QContact newOrExistingContact(const QString &fbuid, bool *isNewContact);
    bool avatarUrlIsDifferent(const QString &avatarType, const QString &fbFriendId, int accountId, const QString &avatarUrl);
    bool removeAvatarFromDisk(const QString &fbFriendId, int accountId, const QString &avatarType);
    void saveImageAndUpdateDatabase(int accountId, const QString &avatarType, const QString &fbFriendId, const QString &avatarUrl, const QByteArray &data);
    void parseContactDetails(const QVariantMap &blobDetails, int accountId);
    void requestAvatars(const QString &accessToken);
    void saveAvatars();
    void saveParsedContacts(int accountId);
    QMap<QString, QContact> m_contactsToSave;
    QStringList m_newContactsToSave;
    struct AvatarRequestData {
        int accountId; // we have to store accountId as the cache is processed asynchronously, unlike the m_contactsToSave cache.
        QString fbuid;
        QString url;
        QString type;
    };
    struct AvatarReplyData {
        int accountId;
        QString fbuid;
        QString url;
        QString type;
        QByteArray data;
    };
    QList<AvatarRequestData> m_avatarsToRequest;
    QList<AvatarReplyData> m_avatarsToSave;
    int m_avatarsSemaphore;

    // for busy/inactive detection.
    void decrementSemaphore(int accountId);
    void incrementSemaphore(int accountId);
    QMap<int, int> m_accountSyncSemaphores;
};

#endif // FACEBOOKCONTACTSYNCADAPTOR_H
