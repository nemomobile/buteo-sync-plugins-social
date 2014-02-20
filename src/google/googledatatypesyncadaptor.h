/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef GOOGLEDATATYPESYNCADAPTOR_H
#define GOOGLEDATATYPESYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>
#include <QtCore/QJsonObject>

class Account;

/*
    Abstract interface for all of the data-specific sync adaptors
    which pull data from Google's online services.
*/

class GoogleDataTypeSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    GoogleDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent);
    virtual ~GoogleDataTypeSyncAdaptor();
    virtual void sync(const QString &dataTypeString, int accountId);

protected:
    QString clientId();
    QString clientSecret();
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void beginSync(int accountId, const QString &accessToken) = 0;
    virtual void finalCleanup();

protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void accountCredentialsChangeHandler();
    void accountStatusChangeHandler();
    void signOnError(const QString &message, int errorType);
    void signOnResponse(const QVariantMap &data);

private:
    void loadClientIdAndSecret();
    void setCredentialsNeedUpdate(Account *account);
    void signIn(Account *account);
    bool m_triedLoading; // Is true if we tried to load (even if we failed)
    QString m_clientId;
    QString m_clientSecret;
};

#endif // GOOGLEDATATYPESYNCADAPTOR_H
