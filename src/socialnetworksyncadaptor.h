/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef SOCIALNETWORKSYNCADAPTOR_H
#define SOCIALNETWORKSYNCADAPTOR_H

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "syncservice.h"

class QSqlDatabase;
class SyncService;
class QNetworkAccessManager;
class AccountManager;
class SocialNetworkSyncDatabase;

struct SyncedDatum
{
    QString accountIdentifier;
    QString localIdentifier;
    QString serviceName;
    QString dataType;
    QDateTime createdTimestamp;
    QDateTime syncedTimestamp;
    QString datumIdentifier;
};

class SocialNetworkSyncAdaptor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY enabledChanged)

public:
    enum Status {
        Initializing = 0,
        Inactive,
        Busy,
        Error,
        Invalid
    };

public:
    SocialNetworkSyncAdaptor(QString serviceName, SyncService::DataType dataType,
                             SyncService *syncService, QObject *parent);
    virtual ~SocialNetworkSyncAdaptor();

    Status status() const;
    bool enabled() const;
    QString serviceName() const;
    void checkAccounts(SyncService::DataType dataType, QList<int> *newIds, QList<int> *purgeIds, QList<int> *updateIds);
    virtual void sync(const QString &dataType); // do we need a "queueSync"? should this function have a return value?

Q_SIGNALS:
    void statusChanged();
    void enabledChanged();

protected:
    virtual void finalize(int accountId);
    QDateTime lastSyncTimestamp(const QString &serviceName, const QString &dataType,
                                int accountId) const;
    bool updateLastSyncTimestamp(const QString &serviceName, const QString &dataType,
                                 int accountId, const QDateTime &timestamp);
    QDateTime whenSyncedDatum(const QString &serviceName, const QString &datumIdentifier) const;
    bool markSyncedData(const QList<SyncedDatum> &data);
    QString syncedDatumLocalIdentifier(const QString &serviceName, const QString &dataType, const QString &datumIdentifier);
    QStringList removeAllData(const QString &serviceName, const QString &dataType, const QString &accountId, bool *ok = 0);
    QList<int> syncedAccounts(const QString &dataType);
    QList<int> syncedDatumAccountIds(const QString &localIdentifier);
    bool beginTransaction();
    bool endTransaction();
    void setStatus(Status status);
    void setInitialActive(bool enabled);
    void setFinishedInactive();

    // Semaphore system
    const SyncService::DataType dataType;
    void incrementSemaphore(int accountId);
    void decrementSemaphore(int accountId);

    // Parsing methods
    static QJsonObject parseJsonObjectReplyData(const QByteArray &replyData, bool *ok);
    static QJsonArray parseJsonArrayReplyData(const QByteArray &replyData, bool *ok);

    AccountManager *const accountManager;
    QNetworkAccessManager * const networkAccessManager; // Do not allow the pointer to be changed


private:
    SocialNetworkSyncDatabase *m_syncDb;
    Status m_status;
    bool m_enabled;
    QString m_serviceName;
    SyncService *m_syncService;
    QMap<int, int> m_accountSyncSemaphores;
};

#endif // SOCIALNETWORKSYNCADAPTOR_H
