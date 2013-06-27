/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKDATATYPESYNCADAPTOR_H
#define FACEBOOKDATATYPESYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"
#include "syncservice.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

//libaccounts-qt
#include <Accounts/Manager>

//libsignon-qt
#include <SignOn/SessionData>
#include <SignOn/Error>

/*
    Abstract interface for all of the data-specific sync adaptors
    which pull data from the Facebook social network.
*/

class FacebookDataTypeSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    FacebookDataTypeSyncAdaptor(SyncService *syncService, SyncService::DataType dataType, QObject *parent);
    virtual ~FacebookDataTypeSyncAdaptor();
    virtual void sync(const QString &dataType);

protected:
    static QVariantMap parseReplyData(const QByteArray &replyData, bool *ok);
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void purgeDataForOldAccounts(const QList<int> &oldIds) = 0;    // must at least implement these two
    virtual void beginSync(int accountId, const QString &accessToken) = 0; // must at least implement these two
    SyncService::DataType m_dataType;

protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void signOnError(const SignOn::Error &err);
    void signOnResponse(const SignOn::SessionData &sdata);
};

#endif // FACEBOOKDATATYPESYNCADAPTOR_H
