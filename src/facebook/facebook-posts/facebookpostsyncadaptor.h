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
    FacebookPostSyncAdaptor(QObject *parent);
    ~FacebookPostSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId = 0);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private:
    void requestMe(int accountId, const QString &accessToken);
    void requestPosts(int accountId, const QString &accessToken);
    bool fromIsSelfContact(const QString &fromName, const QString &fromFbUid) const;

private Q_SLOTS:
    void finishedMeHandler();
    void finishedPostsHandler();

private:
    FacebookPostsDatabase m_db;
    QContactManager *m_contactManager;
    QContact m_selfContact;
    QMap<int, QString> m_selfFacebookUserIds;
};

#endif // FACEBOOKPOSTSYNCADAPTOR_H
