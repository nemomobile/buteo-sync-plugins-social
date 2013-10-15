/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef TWITTERHOMETIMELINESYNCADAPTOR_H
#define TWITTERHOMETIMELINESYNCADAPTOR_H

#include "twitterdatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <socialcache/twitterpostsdatabase.h>

class TwitterHomeTimelineSyncAdaptor : public TwitterDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    TwitterHomeTimelineSyncAdaptor(SyncService *syncService, QObject *parent);
    ~TwitterHomeTimelineSyncAdaptor();

protected: // implementing TwitterDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);
    void finalize(int accountId);

private:
    void requestMe(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);
    void requestPosts(int accountId, const QString &oauthToken, const QString &oauthTokenSecret,
                      const QString &sinceId = QString(), const QString &fromUserId = QString());
    bool fromIsSelfContact(const QString &fromName, const QString &fromTwUid) const;

private Q_SLOTS:
    void finishedMeHandler();
    void finishedPostsHandler();

private:
    TwitterPostsDatabase m_db;
    QMap<int, QString> m_accountProfileImage;
    QStringList m_selfTuids; // twitter user id strings of "me" objects
    QMap<QString, QString> m_selfTScreenNames; // map of user id string to screen name
};

#endif // TWITTERHOMETIMELINESYNCADAPTOR_H
