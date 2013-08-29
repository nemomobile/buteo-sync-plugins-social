/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef TWITTERMENTIONTIMELINESYNCADAPTOR_H
#define TWITTERMENTIONTIMELINESYNCADAPTOR_H

#include "twitterdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

class Notification;
class TwitterMentionTimelineSyncAdaptor : public TwitterDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    TwitterMentionTimelineSyncAdaptor(SyncService *syncService, QObject *parent);
    ~TwitterMentionTimelineSyncAdaptor();

protected: // implementing TwitterDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);

private:
    void requestNotifications(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &sinceTweetId = QString());

private Q_SLOTS:
    void finishedHandler();

private:
    Notification * createNotification(int accountId);
    Notification * findNotification(int accountId);
};

#endif // TWITTERMENTIONTIMELINESYNCADAPTOR_H
