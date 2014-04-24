/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef GOOGLESIGNONSYNCADAPTOR_H
#define GOOGLESIGNONSYNCADAPTOR_H

#include "googledatatypesyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtCore/QList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/AccountService>
#include <SignOn/Identity>
#include <SignOn/AuthSession>
#include <SignOn/SessionData>
#include <SignOn/Error>

class GoogleSignonSyncAdaptor : public GoogleDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    GoogleSignonSyncAdaptor(QObject *parent);
    ~GoogleSignonSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId = 0);

protected: // implementing GoogleDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private Q_SLOTS:
    void initialSignonResponse(const SignOn::SessionData &responseData);
    void forceTokenExpiryResponse(const SignOn::SessionData &responseData);
    void refreshTokenResponse(const SignOn::SessionData &responseData);
    void signonError(const SignOn::Error &error);
    void triggerRefresh();

private:
    Accounts::Account *loadAccount(int accountId);
    void raiseCredentialsNeedUpdateFlag(int accountId);
    void lowerCredentialsNeedUpdateFlag(int accountId);
    void refreshTokens(int accountId);

    Accounts::Manager m_accountManager;
    QMap<int, Accounts::Account *> m_accounts;
    QMap<int, SignOn::Identity *> m_idents;
};

#endif // GOOGLESIGNONSYNCADAPTOR_H
