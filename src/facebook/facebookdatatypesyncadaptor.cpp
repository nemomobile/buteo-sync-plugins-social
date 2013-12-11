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

// sailfish-components-accounts-qt5
#include <accountmanager.h>
#include <account.h>
#include <signinparameters.h>

//libsailfishkeyprovider
#include <sailfishkeyprovider.h>

//libsignon-qt: SignOn::NoUserInteractionPolicy
#include <SignOn/SessionData>

FacebookDataTypeSyncAdaptor::FacebookDataTypeSyncAdaptor(SyncService *syncService, SyncService::DataType dataType, QObject *parent)
    : SocialNetworkSyncAdaptor("facebook", dataType, syncService, parent), m_triedLoading(false)
{
}

FacebookDataTypeSyncAdaptor::~FacebookDataTypeSyncAdaptor()
{
}

void FacebookDataTypeSyncAdaptor::sync(const QString &dataTypeString)
{
    if (dataTypeString != SyncService::dataType(dataType)) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: facebook %1 sync adaptor was asked to sync %2"))
                .arg(SyncService::dataType(dataType)).arg(dataTypeString));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    if (clientId().isEmpty()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: client id couldn't be retrieved for facebook")));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    // three stage process.
    // 1) if an account has been removed, we need to purge the data we retrieved with it
    // 2) if an account has been added, we need to pull data for the account
    // 3) for existing accounts, pull new data for the existing account
    setStatus(SocialNetworkSyncAdaptor::Busy);

    QList<int> newIds, purgeIds, updateIds;
    // Implemented in socialsyncadaptor
    checkAccounts(dataType, &newIds, &purgeIds, &updateIds);
    purgeDataForOldAccounts(purgeIds); // call the derived-class purge entrypoint.
    updateDataForAccounts(newIds);
    updateDataForAccounts(updateIds);

    TRACE(SOCIALD_DEBUG,
            QString(QLatin1String("successfully triggered sync of %1: %2 purged, %3 new, %4 updated accounts"))
            .arg(SyncService::dataType(dataType)).arg(purgeIds.size()).arg(newIds.size()).arg(updateIds.size()));

    if (newIds.count() == 0 && updateIds.count() == 0) {
        setFinishedInactive(); // just had to purge, and we're done.
    }
}

void FacebookDataTypeSyncAdaptor::updateDataForAccounts(const QList<int> &accountIds)
{
    foreach (int accountId, accountIds) {
        // will be decremented by either signOnError or signOnResponse.
        // we increment them prior to the loop below to avoid spurious
        // "all are zero" causing setFinishedInactive() too early,
        // if one of the accounts could not be loaded.
        incrementSemaphore(accountId);
    }

    foreach (int accountId, accountIds) {
        Account *account = accountManager->account(accountId);
        if (!account) {
            TRACE(SOCIALD_ERROR,
                  QString(QLatin1String("error: existing account with id %1 couldn't be retrieved"))
                  .arg(accountId));
            decrementSemaphore(accountId);
            continue;
        }

        if (account->status() == Account::Initialized || account->status() == Account::Synced) {
            signIn(account);
        } else {
            connect(account, SIGNAL(statusChanged()), this, SLOT(accountStatusChangeHandler()));
        }
    }
}

void FacebookDataTypeSyncAdaptor::accountCredentialsChangeHandler()
{
    Account *account = qobject_cast<Account*>(sender());
    if (account->status() == Account::Initialized) {
        setCredentialsNeedUpdate(account);
    }
}

void FacebookDataTypeSyncAdaptor::accountStatusChangeHandler()
{
    Account *account = qobject_cast<Account*>(sender());
    if (account->status() == Account::Initialized || account->status() == Account::Synced) {
        // Not anymore interested about status changes of this account instance
        account->disconnect(this);
        signIn(account);
    }
}

void FacebookDataTypeSyncAdaptor::signOnError(const QString &err, int errorType)
{
    Account *account = qobject_cast<Account*>(sender());
    int accountId = account->identifier();
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved:"))
            .arg(accountId) << err);
    setStatus(SocialNetworkSyncAdaptor::Error);

    // if the error is because credentials have expired, we
    // set the CredentialsNeedUpdate key.
    if (errorType == Account::SignInCredentialsExpiredError) {
        setCredentialsNeedUpdate(account);
    } else {
        account->disconnect(this);
    }

    // if we couldn't sign in, we can't sync with this account.
    decrementSemaphore(accountId);
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

    decrementSemaphore(accountId);
}

void FacebookDataTypeSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray replyData = reply->readAll();
    int accountId = reply->property("accountId").toInt();

    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: %1 request with account %2 experienced error: %3"))
            .arg(SyncService::dataType(dataType)).arg(accountId).arg(err));
    // set "isError" on the reply so that adapters know to ignore the result in the finished() handler
    reply->setProperty("isError", QVariant::fromValue<bool>(true));
    // Note: not all errors are "unrecoverable" errors, so we don't change the status here.

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (ok && parsed.contains(QLatin1String("error"))) {
        QJsonObject errorReply = parsed.value("error").toObject();
        // Password Changed on server side
        if (errorReply.value("code").toDouble() == 190 &&
                errorReply.value("error_subcode").toDouble() == 460) {
            int accountId = reply->property("accountId").toInt();
            Account *account = accountManager->account(accountId);
            if (account->status() == Account::Initialized) {
                setCredentialsNeedUpdate(account);
            } else {
                connect(account, SIGNAL(statusChanged()), this, SLOT(accountCredentialsChangeHandler()));
            }
        }
    }

    disconnect(reply);
    reply->deleteLater();
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
            .arg(SyncService::dataType(dataType)).arg(sender()->property("accountId").toInt()).arg(sslerrs));
    // set "isError" on the reply so that adapters know to ignore the result in the finished() handler
    sender()->setProperty("isError", QVariant::fromValue<bool>(true));
    // Note: not all errors are "unrecoverable" errors, so we don't change the status here.
}

QString FacebookDataTypeSyncAdaptor::clientId()
{
    if (!m_triedLoading) {
        loadClientId();
    }
    return m_clientId;
}

void FacebookDataTypeSyncAdaptor::loadClientId()
{
    m_triedLoading = true;
    char *cClientId = NULL;
    int cSuccess = SailfishKeyProvider_storedKey("facebook", "facebook-sync", "client_id", &cClientId);
    if (cSuccess != 0 || cClientId == NULL) {
        return;
    }

    m_clientId = QLatin1String(cClientId);
    free(cClientId);
    return;
}

void FacebookDataTypeSyncAdaptor::setCredentialsNeedUpdate(Account *account)
{
    // Not anymore interested about status changes of this account instance
    account->disconnect(this);
    qWarning() << "sociald:Facebook: setting CredentialsNeedUpdate to true for account:" << account->identifier();
    account->setConfigurationValue("facebook-sync", "CredentialsNeedUpdate", QVariant::fromValue<bool>(true));
    account->setConfigurationValue("facebook-sync", "CredentialsNeedUpdateFrom", QVariant::fromValue<QString>(QString::fromLatin1("sociald-facebook")));
    account->sync();
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
    sip->setParameter(QLatin1String("ClientId"), clientId());
    sip->setParameter(QLatin1String("UiPolicy"), SignInParameters::NoUserInteractionPolicy);

    connect(account, SIGNAL(signInError(QString,int)), this, SLOT(signOnError(QString,int)));
    connect(account, SIGNAL(signInResponse(QVariantMap)), this, SLOT(signOnResponse(QVariantMap)));
    account->signIn("Jolla", "Jolla", sip);
}
