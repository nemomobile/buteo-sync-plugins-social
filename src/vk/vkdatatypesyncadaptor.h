/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#ifndef VKDATATYPESYNCADAPTOR_H
#define VKDATATYPESYNCADAPTOR_H

#include "socialnetworksyncadaptor.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslError>
#include <QtCore/QJsonObject>

class Account;
class QJsonObject;

/*
    Abstract interface for all of the data-specific sync adaptors
    which pull data from the VK social network.
*/

class VKDataTypeSyncAdaptor : public SocialNetworkSyncAdaptor
{
    Q_OBJECT

public:
    class UserProfile
    {
    public:
        UserProfile();
        ~UserProfile();

        UserProfile(const UserProfile &other);
        UserProfile &operator=(const UserProfile &other);

        static UserProfile fromJsonObject(const QJsonObject &object);

        QString name() const;

        int uid;
        QString firstName;
        QString lastName;
        QString icon;
    };

    VKDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent);
    virtual ~VKDataTypeSyncAdaptor();
    virtual void sync(const QString &dataTypeString, int accountId);

protected:
    QString clientId();
    virtual void updateDataForAccounts(const QList<int> &accountIds);
    virtual void beginSync(int accountId, const QString &accessToken) = 0;

protected Q_SLOTS:
    virtual void errorHandler(QNetworkReply::NetworkError err);
    virtual void sslErrorsHandler(const QList<QSslError> &errs);

private Q_SLOTS:
    void accountCredentialsChangeHandler();
    void accountStatusChangeHandler();
    void signOnError(const QString &message, int errorType);
    void signOnResponse(const QVariantMap &data);

private:
    void loadClientId();
    void setCredentialsNeedUpdate(Account *account);
    void signIn(Account *account);
    bool m_triedLoading; // Is true if we tried to load (even if we failed)
    QString m_clientId;
};

#endif // VKDATATYPESYNCADAPTOR_H
