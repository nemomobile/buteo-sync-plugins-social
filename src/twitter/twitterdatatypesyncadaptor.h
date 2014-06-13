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

#ifndef TWITTERDATATYPESYNCADAPTOR_H
#define TWITTERDATATYPESYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

namespace Accounts {
    class Account;
}
namespace SignOn {
    class Error;
    class SessionData;
}

/*
    Abstract interface for all of the data-specific sync adaptors
    which pull data from the Twitter social network.
*/

class TwitterDataTypeSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    TwitterDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent);
    virtual ~TwitterDataTypeSyncAdaptor();
    virtual void sync(const QString &dataTypeString, int accountId);

protected:
    static QDateTime parseTwitterDateTime(const QString &tdt);
    virtual QString authorizationHeader(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &requestMethod, const QString &requestUrl, const QList<QPair<QString, QString> > &parameters);
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret) = 0;
    QString consumerKey();
    QString consumerSecret();
protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void signOnError(const SignOn::Error &error);
    void signOnResponse(const SignOn::SessionData &sessionData);

private:
    void loadConsumerKeyAndSecret();
    void setCredentialsNeedUpdate(Accounts::Account *account);
    void signIn(Accounts::Account *account);
    bool m_triedLoading; // Is true if we tried to load (even if we failed)
    QString m_consumerKey;
    QString m_consumerSecret;
};

#endif // TWITTERDATATYPESYNCADAPTOR_H
