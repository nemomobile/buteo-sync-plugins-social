/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKPOSTSYNCADAPTOR_H
#define FACEBOOKPOSTSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

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

class FacebookPostSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookPostSyncAdaptor(SyncService *syncService, QObject *parent);
    ~FacebookPostSyncAdaptor();

    void sync(const QString &dataType);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);

private:
    void requestMe(int accountId, const QString &accessToken);
    void requestPosts(int accountId, const QString &accessToken,
                              const QString &until = QString(), const QString &pagingToken = QString());
    bool haveAlreadyPostedEvent(const QString &postId, const QString &title, const QDateTime &createdTime);
    bool fromIsSelfContact(const QString &fromName, const QString &fromFbUid) const;

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
    QStringList m_selfFbuids; // facebook user id strings of "me" objects

    // for busy/inactive detection.
    void decrementSemaphore(int accountId);
    void incrementSemaphore(int accountId);
    QMap<int, int> m_accountSyncSemaphores;
};

#endif // FACEBOOKPOSTSYNCADAPTOR_H
