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

//libaccounts-qt
#include <Accounts/Manager>

//libsignon-qt
#include <SignOn/SessionData>
#include <SignOn/Error>

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
    virtual void sync(const QString &dataType);

protected:
    static QVariant parseReplyData(const QByteArray &replyData, bool *ok);
    static QDateTime parseTwitterDateTime(const QString &tdt);
    virtual QString authorizationHeader(int accountId, const QString &oauthToken, const QString &oauthTokenSecret, const QString &requestMethod, const QString &requestUrl, const QList<QPair<QString, QString> > &parameters);
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void purgeDataForOldAccounts(const QList<int> &oldIds) = 0; // must at least implement these two
    virtual void beginSync(int accountId, const QString &oauthToken, const QString &oauthTokenSecret) = 0; // must at least implement these two
    SyncService::DataType m_dataType;

protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void signOnError(const SignOn::Error &err);
    void signOnResponse(const SignOn::SessionData &sdata);

private:
    bool consumerKeyAndSecret(QString &consumerKey, QString &consumerSecret);
};

#endif // TWITTERDATATYPESYNCADAPTOR_H
