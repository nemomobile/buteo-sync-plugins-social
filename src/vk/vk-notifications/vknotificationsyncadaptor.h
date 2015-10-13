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

#include <QList>
#include <QJsonObject>

class Notification;

class VKNotificationSyncAdaptor : public VKDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    VKNotificationSyncAdaptor(QObject *parent);
    ~VKNotificationSyncAdaptor();

    QString syncServiceName() const;

protected: // implementing VKDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestNotifications(int accountId, const QString &accessToken,
                              const QString &until = QString(),
                              const QString &pagingToken = QString());

private Q_SLOTS:
    void finishedHandler();

private:
    void saveVKNotificationFromObject(int accountId, const QJsonObject &notif, const QList<UserProfile> &userProfiles);
    struct NotificationData {
        NotificationData() : accountId(0) {}
        NotificationData(int accountId, const QJsonObject &notification, const QJsonArray &profiles)
            : accountId(accountId), notification(notification), profiles(profiles) {}
        int accountId;
        QJsonObject notification;
        QJsonArray profiles;
    };
    QList<NotificationData> m_notificationsToAdd;
    VKNotificationsDatabase m_db;
};

#endif // VKNOTIFICATIONSYNCADAPTOR_H
