/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef VKNOTIFICATIONSYNCADAPTOR_H
#define VKNOTIFICATIONSYNCADAPTOR_H

#include "vkdatatypesyncadaptor.h"
#include <socialcache/vknotificationsdatabase.h>

class Notification;

class VKNotificationSyncAdaptor : public VKDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    VKNotificationSyncAdaptor(QObject *parent);
    ~VKNotificationSyncAdaptor();

    QString syncServiceName() const;

protected: // implementing VKDataTypeSyncAdaptor interface
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
    VKNotificationsDatabase m_db;
};

#endif // VKNOTIFICATIONSYNCADAPTOR_H
