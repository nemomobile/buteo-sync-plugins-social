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

#include "syncservice.h"

class QSqlDatabase;
class SyncService;
class QNetworkAccessManager;
namespace Accounts { class Manager; }

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
    SocialNetworkSyncAdaptor(QString serviceName, SyncService *syncService, QObject *parent);
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
    QDateTime lastSyncTimestamp(const QString &serviceName, const QString &dataType, const QString &accountId) const;
    bool updateLastSyncTimestamp(const QString &serviceName, const QString &dataType, const QString &accountId, const QDateTime &timestamp);
    QDateTime whenSyncedDatum(const QString &serviceName, const QString &datumIdentifier) const;
    bool markSyncedDatum(const QString &localIdentifier, const QString &serviceName, const QString &dataType, const QString &accountId, const QDateTime &createdTimestamp, const QDateTime &syncedTimestamp, const QString &datumIdentifier);
    bool removeAllData(const QString &serviceName, const QString &dataType, const QString &accountId);
    QStringList accountIdsWithSyncTimestamp(const QString &serviceName, const QString &dataType);
    QStringList syncedDatumLocalIdentifiers(const QString &serviceName, const QString &dataType, const QString &accountId) const;
    void beginTransaction();
    void endTransaction();
    void changeStatus(Status status);

    Status m_status;
    bool m_enabled;
    QString m_serviceName;
    Accounts::Manager *m_accountManager;
    QNetworkAccessManager *m_qnam;

private:
    SyncService *q;
};

#endif // SOCIALNETWORKSYNCADAPTOR_H
