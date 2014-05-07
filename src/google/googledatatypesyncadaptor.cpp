/****************************************************************************
 **
 ** Copyright (C) 2013 Jolla Ltd.
 ** Contact: Chris Adams <chris.adams@jollamobile.com>
 **
 ****************************************************************************/

#include "googledatatypesyncadaptor.h"
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

GoogleDataTypeSyncAdaptor::GoogleDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent)
    : SocialNetworkSyncAdaptor("google", dataType, parent), m_triedLoading(false)
{
}

GoogleDataTypeSyncAdaptor::~GoogleDataTypeSyncAdaptor()
{
}

void GoogleDataTypeSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    if (dataTypeString != SocialNetworkSyncAdaptor::dataTypeName(dataType)) {
        TRACE(SOCIALD_ERROR,
                QString(QLatin1String("error: Google %1 sync adaptor was asked to sync %2"))
                .arg(SocialNetworkSyncAdaptor::dataTypeName(dataType)).arg(dataTypeString));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    if (clientId().isEmpty()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: client id couldn't be retrieved for Google")));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    if (clientSecret().isEmpty()) {
        TRACE(SOCIALD_ERROR, QString(QLatin1String("error: client secret couldn't be retrieved for Google")));
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    // either a single-account sync, or an all-account sync.
    // an all-account sync is a three stage process.
    // 1) if an account has been removed, we need to purge the data we retrieved with it
    // 2) if an account has been added, we need to pull data for the account
    // 3) for existing accounts, pull new data for the existing account
    setStatus(SocialNetworkSyncAdaptor::Busy);

    QList<int> newIds, purgeIds, updateIds;
    if (accountId == 0) {
        // all account sync.  determine accounts added/removed/need updating.
        checkAccounts(dataType, &newIds, &purgeIds, &updateIds);

        // We only actually perform the purge operation for all-account (template) syncs.
        purgeDataForOldAccounts(purgeIds); // call the derived-class purge entrypoint.

        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("successfully triggered sync of %1: purged %2 accounts"))
                .arg(SocialNetworkSyncAdaptor::dataTypeName(dataType)).arg(purgeIds.size()));

        setFinishedInactive(); // just had to purge, and we're done.
    } else {
        // single account sync.
        updateDataForAccounts(QList<int>() << accountId);

        TRACE(SOCIALD_DEBUG,
                QString(QLatin1String("successfully triggered sync with profile: %1"))
                .arg(m_accountSyncProfile->name()));
    }
}

void GoogleDataTypeSyncAdaptor::updateDataForAccounts(const QList<int> &accountIds)
{
    if (accountIds.size() != 1) {
        // Since the "split monolithic plugin" refactoring, this function
        // should only ever be called for a single account.
        // TODO: refactor all of the plugins even more completely, to
        // remove the per-accountId state data (maps etc) and "fix" the
        // function signatures to match the new per-account paradigm.
        qWarning() << Q_FUNC_INFO << "called with multiple accounts - ERROR!";
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

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
            setStatus(SocialNetworkSyncAdaptor::Error);
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

void GoogleDataTypeSyncAdaptor::finalCleanup()
{
}

void GoogleDataTypeSyncAdaptor::accountCredentialsChangeHandler()
{
    Account *account = qobject_cast<Account*>(sender());
    if (account->status() == Account::Initialized) {
        setCredentialsNeedUpdate(account);
    }
}

void GoogleDataTypeSyncAdaptor::accountStatusChangeHandler()
{
    Account *account = qobject_cast<Account*>(sender());
    if (account->status() == Account::Initialized || account->status() == Account::Synced) {
        // Not anymore interested about status changes of this account instance
        account->disconnect(this);
        signIn(account);
    }
}

void GoogleDataTypeSyncAdaptor::signOnError(const QString &err, int errorType)
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

void GoogleDataTypeSyncAdaptor::signOnResponse(const QVariantMap &data)
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

void GoogleDataTypeSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
{
    // Google sends error code 204 (HTTP code 401) for Unauthorized Error
    // Note: sometimes it sends it spuriously - see JB#15645
    // For now, don't raise the flag, until we can solve
    // any API rate limit issues associated with avatars
    // which might cause this (if multiple accounts are involved).
    // Another possible cause might be: if the ExpiresIn time
    // is small (less than 30 seconds, say) it's possible that the
    // access token will expire _during_ the sync process.
    // XXX TODO: check expires time, force refresh if < 30.
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (err == QNetworkReply::AuthenticationRequiredError) {
        //int accountId = sender()->property("accountId").toInt();
        //Account *account = accountManager->account(accountId);
        //if (account->status() == Account::Initialized) {
        //    setCredentialsNeedUpdate(account);
        //} else {
        //    connect(account, SIGNAL(statusChanged()), this, SLOT(accountCredentialsChangeHandler()));
        //}
        // instead of triggering CredentialsNeedUpdate, print some debugging.
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray jsonBody = reply->readAll();
        qWarning() << "sociald:Google: would normally set CredentialsNeedUpdate for account"
                   << reply->property("accountId").toInt() << "but could be spurious\n"
                   << "    Http code:" << httpCode << "\n"
                   << "    Json body:\n" << jsonBody << "\n";
    }

    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: %1 request with account %2 experienced error: %3\n%4"))
            .arg(SocialNetworkSyncAdaptor::dataTypeName(dataType))
            .arg(sender()->property("accountId").toInt())
            .arg(err)
            .arg(QString::fromUtf8(reply->readAll())));
    // set "isError" on the reply so that adapters know to ignore the result in the finished() handler
    reply->setProperty("isError", QVariant::fromValue<bool>(true));
    // Note: not all errors are "unrecoverable" errors, so we don't change the status here.
}

void GoogleDataTypeSyncAdaptor::sslErrorsHandler(const QList<QSslError> &errs)
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
            .arg(SocialNetworkSyncAdaptor::dataTypeName(dataType)).arg(sender()->property("accountId").toInt()).arg(sslerrs));
    // set "isError" on the reply so that adapters know to ignore the result in the finished() handler
    sender()->setProperty("isError", QVariant::fromValue<bool>(true));
    // Note: not all errors are "unrecoverable" errors, so we don't change the status here.
}

QString GoogleDataTypeSyncAdaptor::clientId()
{
    if (!m_triedLoading) {
        loadClientIdAndSecret();
    }
    return m_clientId;
}

QString GoogleDataTypeSyncAdaptor::clientSecret()
{
    if (!m_triedLoading) {
        loadClientIdAndSecret();
    }
    return m_clientSecret;
}

void GoogleDataTypeSyncAdaptor::loadClientIdAndSecret()
{
    m_triedLoading = true;
    char *cClientId = NULL;
    char *cClientSecret = NULL;

    int cSuccess = SailfishKeyProvider_storedKey("google", "google-sync", "client_id", &cClientId);
    if (cClientId == NULL) {
        return;
    } else if (cSuccess != 0) {
        free(cClientId);
        return;
    }

    m_clientId = QLatin1String(cClientId);
    free(cClientId);

    cSuccess = SailfishKeyProvider_storedKey("google", "google-sync", "client_secret", &cClientSecret);
    if (cClientSecret == NULL) {
        return;
    } else if (cSuccess != 0) {
        free(cClientSecret);
        return;
    }

    m_clientSecret = QLatin1String(cClientSecret);
    free(cClientSecret);
}

void GoogleDataTypeSyncAdaptor::setCredentialsNeedUpdate(Account *account)
{
    // Not anymore interested about status changes of this account instance
    account->disconnect(this);
    qWarning() << "sociald:Google: setting CredentialsNeedUpdate to true for account:" << account->identifier();
    account->setConfigurationValue(syncServiceName(), "CredentialsNeedUpdate", QVariant::fromValue<bool>(true));
    account->setConfigurationValue(syncServiceName(), "CredentialsNeedUpdateFrom", QVariant::fromValue<QString>(QString::fromLatin1("sociald-google")));
    account->sync();
}

void GoogleDataTypeSyncAdaptor::signIn(Account *account)
{
    // grab out a valid identity for the sync service.
    if (!account->isEnabledWithService(syncServiceName())) {
        TRACE(SOCIALD_INFORMATION,
              QString(QLatin1String("account with id %1 is not enabled with service %2"))
              .arg(account->identifier()).arg(syncServiceName()));
        decrementSemaphore(account->identifier());
        return;
    }

    SignInParameters *sip = account->signInParameters(syncServiceName());
    sip->setParameter(QLatin1String("ClientId"), clientId());
    sip->setParameter(QLatin1String("ClientSecret"), clientSecret());
    sip->setParameter(QLatin1String("UiPolicy"), SignInParameters::NoUserInteractionPolicy);

    connect(account, SIGNAL(signInError(QString,int)), this, SLOT(signOnError(QString,int)));
    connect(account, SIGNAL(signInResponse(QVariantMap)), this, SLOT(signOnResponse(QVariantMap)));
    account->signIn("Jolla", "Jolla", sip);
}
