/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef TWITTERDATATYPESYNCADAPTOR_H
#define TWITTERDATATYPESYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"
#include "syncservice.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>

class Account;

/*
    Abstract interface for all of the data-specific sync adaptors
    which pull data from the Twitter social network.
*/

class TwitterDataTypeSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    TwitterDataTypeSyncAdaptor(SyncService *syncService, SyncService::DataType dataType, QObject *parent);
    virtual ~TwitterDataTypeSyncAdaptor();
    virtual void sync(const QString &dataTypeString);

protected:
    static QDateTime parseTwitterDateTime(const QString &tdt);
    virtual QString authorizationHeader(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &requestMethod, const QString &requestUrl, const QList<QPair<QString, QString> > &parameters);
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void purgeDataForOldAccounts(const QList<int> &oldIds) = 0; // must at least implement these two
    virtual void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret) = 0; // must at least implement these two
    QString consumerKey();
    QString consumerSecret();
protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void accountStatusChangeHandler();
    void signOnError(const QString &err, int errorType);
    void signOnResponse(const QVariantMap &data);

private:
    void loadConsumerKeyAndSecret();
    void signIn(Account *account);
    bool m_triedLoading; // Is true if we tried to load (even if we failed)
    QString m_consumerKey;
    QString m_consumerSecret;
};

#endif // TWITTERDATATYPESYNCADAPTOR_H
