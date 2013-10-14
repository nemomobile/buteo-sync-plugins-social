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

#include <socialcache/facebookpostsdatabase.h>

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
    void finalize();

private:
    void requestMe(int accountId, const QString &accessToken);
    void requestPosts(int accountId, const QString &accessToken);
    bool fromIsSelfContact(const QString &fromName, const QString &fromFbUid) const;

private Q_SLOTS:
    void finishedMeHandler();
    void finishedPostsHandler();
    void contactFetchStateChangedHandler(QContactAbstractRequest::State newState);

private:
    FacebookPostsDatabase m_db;
    QContactManager *m_contactManager;
    QList<QContact> m_contacts;
    QContact m_selfContact;
    QContactFetchRequest *m_contactFetchRequest;

    QMap<int, QString> m_selfFacebookUserIds;
};

#endif // FACEBOOKPOSTSYNCADAPTOR_H
