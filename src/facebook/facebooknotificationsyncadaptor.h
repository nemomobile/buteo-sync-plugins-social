/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKNOTIFICATIONSYNCADAPTOR_H
#define FACEBOOKNOTIFICATIONSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <QtContacts/QContactManager>
#include <QtContacts/QContactAbstractRequest>
#include <QtContacts/QContactFetchRequest>
#include <QtContacts/QContactFetchHint>
#include <QtContacts/QContact>

class MEventFeed;

USE_CONTACTS_NAMESPACE

class FacebookNotificationSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookNotificationSyncAdaptor(SyncService *syncService, QObject *parent);
    ~FacebookNotificationSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestNotifications(int accountId, const QString &accessToken,
                              const QString &until = QString(), const QString &pagingToken = QString());
    bool haveAlreadyPostedNotification(const QString &notificationId, const QString &title, const QDateTime &createdTime);
    QContact findMatchingContact(const QString &nameString) const;

private Q_SLOTS:
    void finishedHandler();
    void contactFetchStateChangedHandler(QContactAbstractRequest::State newState);

private:
    QContactManager m_contactManager;
    QList<QContact> m_contacts;
    QContactFetchRequest *m_contactFetchRequest;
    MEventFeed *m_eventFeed;
    QMap<int, int> m_notificationsCount;

    // for busy/inactive detection.
    void decrementSemaphore(int accountId);
    void incrementSemaphore(int accountId);
    QMap<int, int> m_accountSyncSemaphores;
};

#endif // FACEBOOKNOTIFICATIONSYNCADAPTOR_H
