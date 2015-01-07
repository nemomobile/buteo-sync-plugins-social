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

#include "facebookdatatypesyncadaptor.h"
#include "trace.h"

#include <QtCore/QVariantMap>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>

//libsailfishkeyprovider
#include <sailfishkeyprovider.h>

// libaccounts-qt5
#include <Accounts/Manager>
#include <Accounts/Account>
#include <Accounts/Service>
#include <Accounts/AccountService>

//libsignon-qt: SignOn::NoUserInteractionPolicy
#include <SignOn/Identity>
#include <SignOn/AuthSession>
#include <SignOn/SessionData>

FacebookDataTypeSyncAdaptor::FacebookDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::DataType dataType, QObject *parent)
    : SocialNetworkSyncAdaptor("facebook", dataType, parent), m_triedLoading(false)
{
}

FacebookDataTypeSyncAdaptor::~FacebookDataTypeSyncAdaptor()
{
}

void FacebookDataTypeSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    if (dataTypeString != SocialNetworkSyncAdaptor::dataTypeName(dataType)) {
        SOCIALD_LOG_ERROR("Facebook" << SocialNetworkSyncAdaptor::dataTypeName(dataType) <<
                          "sync adaptor was asked to sync" << dataTypeString);
        setStatus(SocialNetworkSyncAdaptor::Error);
        return;
    }

    if (clientId().isEmpty()) {
        SOCIALD_LOG_ERROR("client id couldn't be retrieved for Facebook account" << accountId);
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

        SOCIALD_LOG_DEBUG("successfully triggered sync of" <<
                          SocialNetworkSyncAdaptor::dataTypeName(dataType) << "," <<
                          "purged" << purgeIds.size() << "accounts");

        setFinishedInactive(); // just had to purge, and we're done.
    } else {
        // single account sync.
        updateDataForAccounts(QList<int>() << accountId);
        SOCIALD_LOG_DEBUG("successfully triggered sync with profile:" << m_accountSyncProfile->name());
    }
}

void FacebookDataTypeSyncAdaptor::updateDataForAccounts(const QList<int> &accountIds)
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
        Accounts::Account *account = accountManager->account(accountId);
        if (!account) {
            SOCIALD_LOG_ERROR("existing account with id" << accountId << "couldn't be retrieved");
            setStatus(SocialNetworkSyncAdaptor::Error);
            decrementSemaphore(accountId);
            continue;
        }

        signIn(account);
    }
}

void FacebookDataTypeSyncAdaptor::errorHandler(QNetworkReply::NetworkError err)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    QByteArray replyData = reply->readAll();
    int accountId = reply->property("accountId").toInt();

    SOCIALD_LOG_ERROR(SocialNetworkSyncAdaptor::dataTypeName(dataType) <<
                      "request with account" << accountId << "experienced error:" << err);
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
            Accounts::Account *account = accountManager->account(accountId);
            if (account) {
                setCredentialsNeedUpdate(account);
            }
        }
    }
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
    SOCIALD_LOG_ERROR(SocialNetworkSyncAdaptor::dataTypeName(dataType) <<
                      "request with account" << sender()->property("accountId").toInt() <<
                      "experienced ssl errors:" << sslerrs);
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

void FacebookDataTypeSyncAdaptor::setCredentialsNeedUpdate(Accounts::Account *account)
{
    qWarning() << "sociald:Facebook: setting CredentialsNeedUpdate to true for account:" << account->id();
    Accounts::Service srv(accountManager->service(syncServiceName()));
    account->selectService(srv);
    account->setValue(QStringLiteral("CredentialsNeedUpdate"), QVariant::fromValue<bool>(true));
    account->setValue(QStringLiteral("CredentialsNeedUpdateFrom"), QVariant::fromValue<QString>(QString::fromLatin1("sociald-facebook")));
    account->selectService(Accounts::Service());
    account->syncAndBlock();
}

void FacebookDataTypeSyncAdaptor::signIn(Accounts::Account *account)
{
    // Fetch consumer key and secret from keyprovider
    int accountId = account->id();
    if (!checkAccount(account) || clientId().isEmpty()) {
        decrementSemaphore(accountId);
        return;
    }

    // grab out a valid identity for the sync service.
    Accounts::Service srv(accountManager->service(syncServiceName()));
    account->selectService(srv);
    SignOn::Identity *identity = account->credentialsId() > 0 ? SignOn::Identity::existingIdentity(account->credentialsId()) : 0;
    if (!identity) {
        SOCIALD_LOG_ERROR("account" << accountId << "has no valid credentials, cannot sign in");
        decrementSemaphore(accountId);
        return;
    }

    Accounts::AccountService accSrv(account, srv);
    QString method = accSrv.authData().method();
    QString mechanism = accSrv.authData().mechanism();
    SignOn::AuthSession *session = identity->createSession(method);
    if (!session) {
        SOCIALD_LOG_ERROR("could not create signon session for account" << accountId);
        identity->deleteLater();
        decrementSemaphore(accountId);
        return;
    }

    QVariantMap signonSessionData = accSrv.authData().parameters();
    signonSessionData.insert("ClientId", clientId());
    signonSessionData.insert("UiPolicy", SignOn::NoUserInteractionPolicy);

    connect(session, SIGNAL(response(SignOn::SessionData)),
            this, SLOT(signOnResponse(SignOn::SessionData)),
            Qt::UniqueConnection);
    connect(session, SIGNAL(error(SignOn::Error)),
            this, SLOT(signOnError(SignOn::Error)),
            Qt::UniqueConnection);

    session->setProperty("account", QVariant::fromValue<Accounts::Account*>(account));
    session->setProperty("identity", QVariant::fromValue<SignOn::Identity*>(identity));
    session->process(SignOn::SessionData(signonSessionData), mechanism);
}

void FacebookDataTypeSyncAdaptor::signOnError(const SignOn::Error &error)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    Accounts::Account *account = session->property("account").value<Accounts::Account*>();
    SignOn::Identity *identity = session->property("identity").value<SignOn::Identity*>();
    int accountId = account->id();
    SOCIALD_LOG_ERROR("credentials for account with id" << accountId <<
                      "couldn't be retrieved:" << error.type() << error.message());

    // if the error is because credentials have expired, we
    // set the CredentialsNeedUpdate key.
    if (error.type() == SignOn::AuthSession::UserInteractionError) {
        setCredentialsNeedUpdate(account);
    }

    session->disconnect(this);
    identity->destroySession(session);
    identity->deleteLater();
    account->deleteLater();

    // if we couldn't sign in, we can't sync with this account.
    setStatus(SocialNetworkSyncAdaptor::Error);
    decrementSemaphore(accountId);
}

void FacebookDataTypeSyncAdaptor::signOnResponse(const SignOn::SessionData &responseData)
{
    QVariantMap data;
    foreach (const QString &key, responseData.propertyNames()) {
        data.insert(key, responseData.getProperty(key));
    }

    QString accessToken;
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    Accounts::Account *account = session->property("account").value<Accounts::Account*>();
    SignOn::Identity *identity = session->property("identity").value<SignOn::Identity*>();
    int accountId = account->id();
    if (data.contains(QLatin1String("AccessToken"))) {
        accessToken = data.value(QLatin1String("AccessToken")).toString();
    } else {
        SOCIALD_LOG_INFO("signon response for account with id" << accountId << "contained no access token");
    }

    session->disconnect(this);
    identity->destroySession(session);
    identity->deleteLater();
    account->deleteLater();

    if (!accessToken.isEmpty()) {
        beginSync(accountId, accessToken); // call the derived-class sync entrypoint.
    }

    decrementSemaphore(accountId);
}
