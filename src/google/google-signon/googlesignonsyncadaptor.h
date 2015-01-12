/****************************************************************************
 **
 ** Copyright (C) 2013-2014 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ** This program/library is free software; you can redistribute it and/or
 ** modify it under the terms of the GNU Lesser General Public License
 ** version 2.1 as published by the Free Software Foundation.
 **
 ** This program/library is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 ** Lesser General Public License for more details.
 **
 ** You should have received a copy of the GNU Lesser General Public
 ** License along with this program/library; if not, write to the Free
 ** Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 ** 02110-1301 USA
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
    void purgeDataForOldAccount(int oldId, SocialNetworkSyncAdaptor::PurgeMode mode);
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
