/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef FACEBOOKSIGNONSYNCADAPTOR_H
#define FACEBOOKSIGNONSYNCADAPTOR_H

#include "facebookdatatypesyncadaptor.h"

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

class FacebookSignonSyncAdaptor : public FacebookDataTypeSyncAdaptor
{
    Q_OBJECT

public:
    FacebookSignonSyncAdaptor(QObject *parent);
    ~FacebookSignonSyncAdaptor();

    QString syncServiceName() const;
    void sync(const QString &dataTypeString, int accountId = 0);

protected: // implementing FacebookDataTypeSyncAdaptor interface
    void purgeDataForOldAccounts(const QList<int> &oldIds);
    void beginSync(int accountId, const QString &accessToken);
    void finalize(int accountId);

private Q_SLOTS:
    void requestFinishedHandler();
    void forceTokenExpiryResponse(const SignOn::SessionData &responseData);
    void forceTokenExpiryError(const SignOn::Error &error);

private:
    Accounts::Account *loadAccount(int accountId);
    void raiseCredentialsNeedUpdateFlag(int accountId);
    void lowerCredentialsNeedUpdateFlag(int accountId);
    void forceTokenExpiry(int seconds, int accountId, const QString &accessToken);

    Accounts::Manager m_accountManager;
    QMap<int, Accounts::Account *> m_accounts;
};

#endif // FACEBOOKSIGNONSYNCADAPTOR_H
