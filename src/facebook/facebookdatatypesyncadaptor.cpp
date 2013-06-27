/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "facebookdatatypesyncadaptor.h"
#include "trace.h"

#include <QtCore/QVariantMap>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>

#include <QJsonDocument>

// sailfish-components-accounts-qt5
#include <accountmanager.h>
#include <account.h>
#include <signinparameters.h>

//libsailfishkeyprovider
#include <sailfishkeyprovider.h>

//libsignon-qt: SignOn::NoUserInteractionPolicy
#include <SignOn/SessionData>

FacebookDataTypeSyncAdaptor::FacebookDataTypeSyncAdaptor(SyncService *syncService, SyncService::DataType dataType, QObject *parent)
    : SocialNetworkSyncAdaptor("facebook", syncService, parent)
    , m_dataType(dataType)
{
    m_validClientId = initializeClientId();
}

FacebookDataTypeSyncAdaptor::~FacebookDataTypeSyncAdaptor()
{
}

void FacebookDataTypeSyncAdaptor::sync(const QString &dataType)
{
    if (dataType != SyncService::dataType(m_dataType)) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: facebook %1 sync adaptor was asked to sync %2"))
                .arg(SyncService::dataType(m_dataType)).arg(dataType));
        changeStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    if (!m_validClientId) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: client id couldn't be retrieved for facebook")));
        changeStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    // three stage process.
    // 1) if an account has been removed, we need to purge the data we retrieved with it
    // 2) if an account has been added, we need to pull data for the account
    // 3) for existing accounts, pull new data for the existing account

    QList<int> newIds, purgeIds, updateIds;
    // Implemented in socialsyncadaptor
    checkAccounts(m_dataType, &newIds, &purgeIds, &updateIds);
    purgeDataForOldAccounts(purgeIds); // call the derived-class purge entrypoint.
    updateDataForAccounts(newIds);
    updateDataForAccounts(updateIds);

    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("successfully triggered sync of %1: %2 purged, %3 new, %4 updated accounts"))
            .arg(SyncService::dataType(m_dataType)).arg(purgeIds.size()).arg(newIds.size()).arg(updateIds.size()));
}

void FacebookDataTypeSyncAdaptor::updateDataForAccounts(const QList<int> &accountIds)
{
    foreach (int accountId, accountIds) {
        Account *account = m_accountManager->account(accountId);
        if (!account) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: existing account with id %1 couldn't be retrieved"))
                  .arg(accountId));
            continue;
        }
        if (account->status() == Account::Initialized || account->status() == Account::Synced) {
            signIn(account);
        } else {
            connect(account, SIGNAL(statusChanged()), this, SLOT(accountStatusChangeHandler()));
        }
    }
}

void FacebookDataTypeSyncAdaptor::accountStatusChangeHandler()
{
    Account *account = qobject_cast<Account*>(sender());
    if (account->status() == Account::Initialized || account->status() == Account::Synced)
    {
        // Not anymore interested about status changes of this account instance
        account->disconnect(this);
        signIn(account);
    }
}

void FacebookDataTypeSyncAdaptor::signOnError(const QString &err)
{
    Account *account = qobject_cast<Account*>(sender());
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved:"))
            .arg(account->identifier()) << err);
    account->disconnect(this);
    changeStatus(SocialNetworkSyncAdaptor::Error);
}

void FacebookDataTypeSyncAdaptor::signOnResponse(const QVariantMap &data)
{
    QString accessToken;
    Account *account = qobject_cast<Account*>(sender());
    int accountId = account->identifier();
    if (data.contains(QLatin1String("AccessToken"))) {
        accessToken = data.value(QLatin1String("AccessToken")).toString();
        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("signon response for account %1 contained access token %2"))
                .arg(accountId).arg(accessToken));
    } else {
        TRACE(SOCIALD_INFORMATION,
                QString(QLatin1String("signon response for account with id %1 contained no access token"))
                .arg(accountId));
    }

    account->disconnect(this);
    if (!accessToken.isEmpty()) {
        beginSync(accountId, accessToken); // call the derived-class sync entrypoint.
    }
}

void FacebookDataTypeSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
{
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: %1 request with account %2 experienced error: %3"))
            .arg(SyncService::dataType(m_dataType)).arg(sender()->property("accountId").toInt()).arg(err));
    // the error is an incomprehensible enum value, but that doesn't matter to users.
    changeStatus(SocialNetworkSyncAdaptor::Error);
}

void FacebookDataTypeSyncAdaptor::sslErrorsHandler(const QList<QSslError> &errs)
{
    QString sslerrs;
    foreach (const QSslError &e, errs) {
        sslerrs += e.errorString() + "; ";
    }
    if (errs.size() > 0) {
        sslerrs.chop(2);
    }
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: %1 request with account %2 experienced ssl errors: %3"))
            .arg(SyncService::dataType(m_dataType)).arg(sender()->property("accountId").toInt()).arg(sslerrs));

    changeStatus(SocialNetworkSyncAdaptor::Error);
}

QVariantMap FacebookDataTypeSyncAdaptor::parseReplyData(const QByteArray &replyData, bool *ok)
{
    QVariant parsed;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyData);
    *ok = !jsonDocument.isEmpty();
    parsed = jsonDocument.toVariant();
    if (*ok && parsed.type() == QVariant::Map) {
        return parsed.toMap();
    }
    *ok = false;
    return QVariantMap();
}

bool FacebookDataTypeSyncAdaptor::initializeClientId()
{
    char *cClientId = NULL;
    int cSuccess = SailfishKeyProvider_storedKey("facebook", "facebook-sync", "client_id", &cClientId);
    if (cSuccess != 0 || cClientId == NULL) {
        return false;
    }

    m_clientId = QLatin1String(cClientId);
    free(cClientId);
    return true;
}

void FacebookDataTypeSyncAdaptor::signIn(Account *account)
{
    // grab out a valid identity for the sync service.
    if (!account->isEnabledWithService("facebook-sync")) {
        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("account with id %1 has no enabled facebook sync service"))
              .arg(account->identifier()));
        return;
    }

    SignInParameters *sip = account->signInParameters("facebook-sync");
    sip->setParameter(QLatin1String("ClientId"), m_clientId);
    sip->setParameter(QLatin1String("UiPolicy"), SignOn::NoUserInteractionPolicy);

    connect(account, SIGNAL(signInError(QString)), this, SLOT(signOnError(QString)));
    connect(account, SIGNAL(signInResponse(QVariantMap)), this, SLOT(signOnResponse(QVariantMap)));
    account->signIn("Jolla", "Jolla", sip);
}
