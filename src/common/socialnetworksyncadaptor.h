/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
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
class SocialImagesDatabase;

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

    enum PurgeMode {
        SyncPurge = 0,
        CleanUpPurge
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

    virtual void sync(const QString &dataType, int accountId = 0);
    virtual void purgeDataForOldAccount(int accountId, PurgeMode mode = SyncPurge) = 0;
    virtual void abortSync(Sync::SyncStatus status);

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

    // whether the sync has been aborted (perhaps due to network connection loss)
    bool syncAborted() const;

    // Semaphore system
    void incrementSemaphore(int accountId);
    void decrementSemaphore(int accountId);

    // network reply timeouts
    void setupReplyTimeout(int accountId, QNetworkReply *reply);
    void removeReplyTimeout(int accountId, QNetworkReply *reply);
    void triggerReplyTimeouts();

    // Parsing methods
    static QJsonObject parseJsonObjectReplyData(const QByteArray &replyData, bool *ok);
    static QJsonArray parseJsonArrayReplyData(const QByteArray &replyData, bool *ok);

    // Cache management
    void purgeCachedImages(SocialImagesDatabase *database, int accountId);
    void purgeExpiredImages(SocialImagesDatabase *database, int accountId);

    const SocialNetworkSyncAdaptor::DataType m_dataType;
    Accounts::Manager * const m_accountManager;
    QNetworkAccessManager * const m_networkAccessManager;
    Buteo::SyncProfile *m_accountSyncProfile;

protected Q_SLOTS:
    virtual void timeoutReply();

private:
    SocialNetworkSyncDatabase *m_syncDb;
    SocialNetworkSyncAdaptor::Status m_status;
    bool m_enabled;
    bool m_syncAborted;
    QString m_serviceName;
    QMap<int, int> m_accountSyncSemaphores;
    QMap<int, QMap<QNetworkReply*, QTimer *> > m_networkReplyTimeouts;
};

#endif // SOCIALNETWORKSYNCADAPTOR_H
