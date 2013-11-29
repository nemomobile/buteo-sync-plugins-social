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
    ~FacebookContactSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);
    void finalCleanup();

private:
    void requestData(int accountId, const QString &accessToken,
                     const QString &continuationRequest = QString(),
                     const QDateTime &syncTimestamp = QDateTime());
    void purgeAccount(int accountId);

private Q_SLOTS:
    void friendsFinishedHandler();
    void slotImageDownloaded(const QString &url, const QString &path, const QVariantMap &data);

private:
    QContactManager *m_contactManager;
    FacebookContactImageDownloader *m_workerObject;
    QMap<int, QList<QContact> > m_remoteContacts; // accountId to contacts to save.
    QMap<int, QList<QPair<QString, QVariantMap> > > m_queuedAvatarDownloads;

    QList<QContactId> contactIdsForGuid(const QString &fbuid);
    QContact newOrExistingContact(const QString &fbuid, bool *isNewContact);
    QContact parseContactDetails(const QJsonObject &blobDetails, int accountId, bool *needsSaving);
    bool storeToLocal(const QString &accessToken, int accountId, int *addedCount, int *modifiedCount, int *removedCount);
};

#endif // FACEBOOKCONTACTSYNCADAPTOR_H
