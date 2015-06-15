/****************************************************************************
 **
 ** Copyright (C) 2013-2015 Jolla Ltd.
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

#ifndef TWITTERNOTIFICATIONSYNCADAPTOR_H
#define TWITTERNOTIFICATIONSYNCADAPTOR_H

#include "twitterdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <socialcache/twitternotificationsdatabase.h>

class Notification;
class TwitterNotificationSyncAdaptor : public TwitterDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    TwitterNotificationSyncAdaptor(QObject *parent);
    ~TwitterNotificationSyncAdaptor();

    QString syncServiceName() const;

protected: // implementing TwitterDataTypeSyncAdaptor interface
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
    void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);
    void finalize(int accountId);

private:
    void requestNotifications(int accountId, const QString &oauthToken,
                              const QString &oauthTokenSecret,
                              const QString &sinceTweetId = QString(),
                              const QString &followersCursor = QString());

private Q_SLOTS:
    void finishedMentionsHandler();
    void finishedRetweetsHandler();
    void finishedFollowersHandler();
    void finishedUserShowHandler();

private:
    enum TwitterNotificationType {
        Mention = 0,
        Retweet,
        Follower
    };
    Notification * createNotification(int accountId, TwitterNotificationType ntype);
    Notification * findNotification(int accountId, TwitterNotificationType ntype);
    TwitterNotificationsDatabase m_db;
    QDateTime m_lastSyncTimestamp;
    QSet<QString> m_followerIds;
    bool m_firstTimeSync;
};

#endif // TWITTERNOTIFICATIONSYNCADAPTOR_H
