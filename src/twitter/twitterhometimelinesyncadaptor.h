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

#include <QtContacts/QContactManager>
#include <QtContacts/QContact>
#include <QtContacts/QContactFetchRequest>

class MEventFeed;

USE_CONTACTS_NAMESPACE

class TwitterHomeTimelineSyncAdaptor : public TwitterDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    TwitterHomeTimelineSyncAdaptor(SyncService *syncService, QObject *parent);
    ~TwitterHomeTimelineSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing TwitterDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);

private:
    void requestMe(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);
    void requestPosts(int accountId, const QString &oauthToken, const QString &oauthTokenSecret,
                      const QString &sinceId = QString(), const QString &fromUserId = QString());
    bool haveAlreadyPostedEvent(const QString &postId, const QString &text, const QDateTime &createdTime);
    bool fromIsSelfContact(const QString &fromName, const QString &fromTwUid) const;

private Q_SLOTS:
    void finishedMeHandler();
    void finishedPostsHandler();
    void contactFetchStateChangedHandler(QContactAbstractRequest::State newState);

private:
    QContactManager m_contactManager;
    QList<QContact> m_contacts;
    QContact m_selfContact;
    QContactFetchRequest *m_contactFetchRequest;
    MEventFeed *m_eventFeed;
    QStringList m_selfTuids; // twitter user id strings of "me" objects
    QMap<QString, QString> m_selfTScreenNames; // map of user id string to screen name
    QMap<int, QString> m_accountProfileImage; // map of user profile images

    // for busy/inactive detection.
    void decrementSemaphore(int accountId);
    void incrementSemaphore(int accountId);
    QMap<int, int> m_accountSyncSemaphores;
};

#endif // TWITTERHOMETIMELINESYNCADAPTOR_H
