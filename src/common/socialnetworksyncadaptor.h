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
#include <QtCore/QMap>
#include <QtCore/QList>

#include "buteosyncfw_p.h"

class QSqlDatabase;
class QNetworkAccessManager;
class QTimer;
class QNetworkReply;
class SocialNetworkSyncDatabase;

namespace Accounts {
    class Account;
    class Manager;
}

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

    enum DataType {
        Contacts = 1,   // "Contacts"
        Calendars,      // "Calendars"
        Notifications,  // "Notifications"
        Images,         // "Images"
        Videos,         // "Videos"
        Posts,          // "Posts"
        Messages,       // "Messages"
        Emails,         // "Emails"
        Signon          // "Signon" -- for refreshing AccessTokens etc.
    };
    static QStringList validDataTypes();
    static QString dataTypeName(DataType t);

public:
    SocialNetworkSyncAdaptor(const QString &serviceName, SocialNetworkSyncAdaptor::DataType dataType, QObject *parent);
    virtual ~SocialNetworkSyncAdaptor();

    virtual QString syncServiceName() const = 0;
    void setAccountSyncProfile(Buteo::SyncProfile* perAccountSyncProfile);

    Status status() const;
    bool enabled() const;
    QString serviceName() const;
    void checkAccounts(SocialNetworkSyncAdaptor::DataType dataType, QList<int> *newIds, QList<int> *purgeIds, QList<int> *updateIds);
    virtual void sync(const QString &dataType, int accountId = 0); // do we need a "queueSync"? should this function have a return value?
    virtual void purgeDataForOldAccounts(const QList<int> &accountIds) = 0;

Q_SIGNALS:
    void statusChanged();
    void enabledChanged();

protected:
    virtual bool checkAccount(Accounts::Account *account);
    virtual void finalCleanup();
    virtual void finalize(int accountId);
    QDateTime lastSyncTimestamp(const QString &serviceName, const QString &dataType,
                                int accountId) const;
    bool updateLastSyncTimestamp(const QString &serviceName, const QString &dataType,
                                 int accountId, const QDateTime &timestamp);
    QList<int> syncedAccounts(const QString &dataType);
    void setStatus(Status status);
    void setInitialActive(bool enabled);
    void setFinishedInactive();

    // Semaphore system
    void incrementSemaphore(int accountId);
    void decrementSemaphore(int accountId);

    // network reply timeouts
    void setupReplyTimeout(int accountId, QNetworkReply *reply);
    void removeReplyTimeout(int accountId, QNetworkReply *reply);

    // Parsing methods
    static QJsonObject parseJsonObjectReplyData(const QByteArray &replyData, bool *ok);
    static QJsonArray parseJsonArrayReplyData(const QByteArray &replyData, bool *ok);

    const SocialNetworkSyncAdaptor::DataType dataType;
    Accounts::Manager * const accountManager;
    QNetworkAccessManager * const networkAccessManager;
    Buteo::SyncProfile *m_accountSyncProfile;

protected Q_SLOTS:
    virtual void timeoutReply();

private:
    SocialNetworkSyncDatabase *m_syncDb;
    SocialNetworkSyncAdaptor::Status m_status;
    bool m_enabled;
    QString m_serviceName;
    QMap<int, int> m_accountSyncSemaphores;
    QMap<int, QMap<QNetworkReply*, QTimer *> > m_networkReplyTimeouts;
};

#endif // SOCIALNETWORKSYNCADAPTOR_H
