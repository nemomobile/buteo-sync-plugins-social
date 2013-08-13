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
#include <QtCore/QJsonObject>

class Account;

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
    virtual void sync(const QString &dataTypeString);

protected:
    static QJsonObject parseReplyData(const QByteArray &replyData, bool *ok);
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void purgeDataForOldAccounts(const QList<int> &oldIds) = 0;    // must at least implement these two
    virtual void beginSync(int accountId, const QString &accessToken) = 0; // must at least implement these two

protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void accountStatusChangeHandler();
    void signOnError(const QString &message);
    void signOnResponse(const QVariantMap &data);

private:
    bool initializeClientId();
    void signIn(Account *account);

private:
    QString m_clientId;
    bool m_validClientId;
};

#endif // FACEBOOKDATATYPESYNCADAPTOR_H
