/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef GOOGLECONTACTSYNCADAPTER_H
#define GOOGLECONTACTSYNCADAPTER_H

#include "googledatatypesyncadaptor.h"

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

class GoogleContactImageDownloader;
class GoogleContactSyncAdaptor : public GoogleDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    GoogleContactSyncAdaptor(SyncService *syncService, QObject *parent);
    ~GoogleContactSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing GoogleDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestData(int accountId,
                     const QString &accessToken,
                     int startIndex = 0,
                     const QString &continuationRequest = QString(),
                     const QDateTime &syncTimestamp = QDateTime());
    void purgeAccount(int pid);

private Q_SLOTS:
    void contactsFinishedHandler();
    void imageDownloaded(const QString &url, const QString &path, const QVariantMap &metadata);

private:
    QList<QContact> transformContactAvatars(const QList<QContact> &remoteContacts, int accountId, const QString &accessToken);
    void downloadContactAvatarImage(int accountId, const QString &accessToken, const QUrl &imageUrl, const QString &filename);
    bool storeToLocal(const QString &accessToken, int accountId, int *addedCount, int *modifiedCount, int *removedCount);

private:
    QContactManager *m_contactManager;
    GoogleContactImageDownloader *m_workerObject;
    QMap<int, QList<QContact> > m_remoteContacts; // accountId to contacts to save.
};

#endif // GOOGLECONTACTSYNCADAPTER_H
