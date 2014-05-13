/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKDATATYPESYNCADAPTOR_H
#define FACEBOOKDATATYPESYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>
#include <QtCore/QJsonObject>

namespace Accounts {
    class Account;
}
namespace SignOn {
    class Error;
    class SessionData;
}

/*
    Abstract interface for all of the data-specific sync adaptors
    which pull data from the Facebook social network.
*/

class FacebookDataTypeSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    FacebookDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent);
    virtual ~FacebookDataTypeSyncAdaptor();
    virtual void sync(const QString &dataTypeString, int accountId);

protected:
    QString clientId();
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void beginSync(int accountId, const QString &accessToken) = 0;

protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void signOnError(const SignOn::Error &error);
    void signOnResponse(const SignOn::SessionData &responseData);

private:
    void loadClientId();
    void setCredentialsNeedUpdate(Accounts::Account *account);
    void signIn(Accounts::Account *account);
    bool m_triedLoading; // Is true if we tried to load (even if we failed)
    QString m_clientId;
};

#endif // FACEBOOKDATATYPESYNCADAPTOR_H
