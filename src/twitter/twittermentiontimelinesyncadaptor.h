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

#include <QtContacts/QContactManager>
#include <QtContacts/QContactAbstractRequest>
#include <QtContacts/QContactFetchRequest>
#include <QtContacts/QContactFetchHint>
#include <QtContacts/QContact>

USE_CONTACTS_NAMESPACE

class Notification;
class TwitterMentionTimelineSyncAdaptor : public TwitterDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    TwitterMentionTimelineSyncAdaptor(SyncService *syncService, QObject *parent);
    ~TwitterMentionTimelineSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing TwitterDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret);

private:
    void requestNotifications(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &sinceTweetId = QString());
    QContact findMatchingContact(const QString &nameString) const;

private Q_SLOTS:
    void finishedHandler();
    void contactFetchStateChangedHandler(QContactAbstractRequest::State newState);

private:
    Notification * createNotification(int accountId);
    Notification * findNotification(int accountId);
    QContactManager m_contactManager;
    QList<QContact> m_contacts;
    QContactFetchRequest *m_contactFetchRequest;
};

#endif // TWITTERMENTIONTIMELINESYNCADAPTOR_H
