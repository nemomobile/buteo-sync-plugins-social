/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKNOTIFICATIONSYNCADAPTOR_H
#define FACEBOOKNOTIFICATIONSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"
#include <socialcache/facebooknotificationsdatabase.h>

class Notification;

class FacebookNotificationSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookNotificationSyncAdaptor(QObject *parent);
    ~FacebookNotificationSyncAdaptor();

    QString syncServiceName() const;

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestNotifications(int accountId, const QString &accessToken,
                              const QString &until = QString(),
                              const QString &pagingToken = QString());

private Q_SLOTS:
    void finishedHandler();

private:
    FacebookNotificationsDatabase m_db;
};

#endif // FACEBOOKNOTIFICATIONSYNCADAPTOR_H
