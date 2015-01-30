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

#include "facebooksignonsyncadaptor.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>

#include <QNetworkRequest>

FacebookSignonSyncAdaptor::FacebookSignonSyncAdaptor(QObject *parent)
    : FacebookDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Signon, parent)
{
    setInitialActive(true);
}

FacebookSignonSyncAdaptor::~FacebookSignonSyncAdaptor()
{
}

QString FacebookSignonSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("facebook-sync"); // TODO: change name of service to facebook-signon!
}

void FacebookSignonSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    // call superclass impl.
    FacebookDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void FacebookSignonSyncAdaptor::purgeDataForOldAccount(int, SocialNetworkSyncAdaptor::PurgeMode)
{
    // Nothing to do.
}

void FacebookSignonSyncAdaptor::finalize(int)
{
    // nothing to do
}

void FacebookSignonSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    // We can't do a verify token request (debug_token) as that requires an app_access_token
    // which Facebook states should only be retrieved from a server-to-server request as
    // it involves the clientSecret.

    // Similarly, we can't exchange a short-lived token for a long-lived token, as that
    // flow also requires a clientSecret and thus needs a server-to-server request.

    // In short, we have to use a "normal" request using the access token and hope that
    // that is sufficient to refresh the existing short-lived token.
    // If it is not (for example, if the user turns their phone off over night) then we
    // need to raise the CredentialsNeedUpdate flag and have the user log in again.

    // Perform a "get the current user" request with the specified authorization token.
    QList<QPair<QString, QString> > queryItems;
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("access_token")), accessToken));
    queryItems.append(QPair<QString, QString>(QString(QLatin1String("fields")), QLatin1String("id")));
    QUrl url(QLatin1String("https://graph.facebook.com/me"));
    QUrlQuery query(url);
    query.setQueryItems(queryItems);
    url.setQuery(query);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));

    if (reply) {
        reply->setProperty("accountId", accountId);
        reply->setProperty("accessToken", accessToken);
        connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(errorHandler(QNetworkReply::NetworkError)));
        connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(sslErrorsHandler(QList<QSslError>)));
        connect(reply, SIGNAL(finished()), this, SLOT(requestFinishedHandler()));

        // we're requesting data.  Increment the semaphore so that we know we're still busy.
        setupReplyTimeout(accountId, reply);
        incrementSemaphore(accountId);
    } else {
        SOCIALD_LOG_ERROR("unable to verify access token via network request for Facebook account:" << accountId);
    }
}

void FacebookSignonSyncAdaptor::requestFinishedHandler()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    bool isError = reply->property("isError").toBool();
    int accountId = reply->property("accountId").toInt();
    QString accessToken = reply->property("accessToken").toString();
    QByteArray replyData = reply->readAll();
    reply->disconnect(this);
    reply->deleteLater();
    removeReplyTimeout(accountId, reply);

    bool ok = false;
    QJsonObject parsed = parseJsonObjectReplyData(replyData, &ok);
    if (!isError && ok && parsed.contains(QStringLiteral("id"))) {
        // Request was successful.  the token expiry will be refreshed server-side.
        // We need to manually extend the device-side expiry time.
        // We don't know how long it'll be extended server-side for,
        // and they don't tell us, so let's assume it's 7 days.
        forceTokenExpiry(7 * 86400, accountId, accessToken); // 7 days
    } else if (isError && ok) {
        if (parsed.contains("error")) {
            QJsonObject errorObj = parsed.value(QStringLiteral("error")).toObject();
            QString errorType = errorObj.value(QStringLiteral("type")).toString();
            double errorCode = errorObj.value(QStringLiteral("code")).toDouble();
            QString errorMessage = errorObj.value(QStringLiteral("message")).toString();

            if (errorType == QStringLiteral("OAuthException")
                    || errorCode == 190
                    || errorCode == 102
                    || errorCode == 10
                    || (errorCode >= 200 && errorCode <= 299)) {
                // the account is in a state which requires user intervention
                SOCIALD_LOG_ERROR("access token has expired for Facebook account" << accountId <<
                                  ":" <<  errorCode << "," << errorType << "," << errorMessage);
                forceTokenExpiry(0, accountId, accessToken);
            } else {
                // other error (downtime / service disruption / etc)
                // ignore this one.
            }
        } else {
            // unknown response from server.  Probably a networking error or similar.
            // ignore this one.
        }
    } else if (reply->error() == QNetworkReply::UnknownContentError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 400) {
        // for some strange reason Facebook will return this error
        // if the token has been manually revoked.  There is no
        // response data in this case.
        // the account is in a state which requires user intervention
        SOCIALD_LOG_ERROR("access token has presumably been revoked for Facebook account" << accountId);
        forceTokenExpiry(0, accountId, accessToken);
    } else {
        // could have been a network error, or something.
        // we treat it as a sync error, but not a signon error.
        SOCIALD_LOG_ERROR("unable to parse response information for verification request for Facebook account:" << accountId);
    }

    decrementSemaphore(accountId);
}

Accounts::Account *FacebookSignonSyncAdaptor::loadAccount(int accountId)
{
    Accounts::Account *acc = 0;
    if (m_accounts.contains(accountId)) {
        acc = m_accounts[accountId];
    } else {
        acc = m_accountManager.account(accountId);
        if (!acc) {
            SOCIALD_LOG_ERROR("Facebook account" << accountId << "was deleted during signon refresh sync");
            return 0;
        } else {
            m_accounts.insert(accountId, acc);
        }
    }

    Accounts::Service srv = m_accountManager.service(syncServiceName());
    if (!srv.isValid()) {
        SOCIALD_LOG_ERROR("invalid service" << syncServiceName() <<
                          "specified for refresh sync with Facebook account" << accountId);
        return 0;
    }

    return acc;
}

void FacebookSignonSyncAdaptor::raiseCredentialsNeedUpdateFlag(int accountId)
{
    Accounts::Account *acc = loadAccount(accountId);
    if (acc) {
        SOCIALD_LOG_ERROR("FBSSA: raising CredentialsNeedUpdate flag");
        Accounts::Service srv = m_accountManager.service(syncServiceName());
        acc->selectService(srv);
        acc->setValue(QStringLiteral("CredentialsNeedUpdate"), QVariant::fromValue<bool>(true));
        acc->setValue(QStringLiteral("CredentialsNeedUpdateFrom"), QVariant::fromValue<QString>(QString::fromLatin1("sociald-facebook-signon")));
        acc->selectService(Accounts::Service());
        acc->syncAndBlock();
    }
}

void FacebookSignonSyncAdaptor::lowerCredentialsNeedUpdateFlag(int accountId)
{
    Accounts::Account *acc = loadAccount(accountId);
    if (acc) {
        SOCIALD_LOG_ERROR("FBSSA: lowering CredentialsNeedUpdate flag");
        Accounts::Service srv = m_accountManager.service(syncServiceName());
        acc->selectService(srv);
        acc->setValue(QStringLiteral("CredentialsNeedUpdate"), QVariant::fromValue<bool>(false));
        acc->remove(QStringLiteral("CredentialsNeedUpdateFrom"));
        acc->selectService(Accounts::Service());
        acc->syncAndBlock();
    }
}

void FacebookSignonSyncAdaptor::forceTokenExpiry(int seconds, int accountId, const QString &accessToken)
{
    Accounts::Account *acc = loadAccount(accountId);
    if (acc) {
        // force expiry of cached tokens to signon db via ProvidedTokens hook
        Accounts::Service srv(m_accountManager.service(syncServiceName()));
        acc->selectService(srv);
        SignOn::Identity *identity = acc->credentialsId() > 0 ? SignOn::Identity::existingIdentity(acc->credentialsId()) : 0;
        if (!identity) {
            SOCIALD_LOG_ERROR("Facebook account" << accountId << "has no valid credentials, cannot perform refresh sync");
            return;
        }

        Accounts::AccountService *accSrv = new Accounts::AccountService(acc, srv);
        if (!accSrv) {
            SOCIALD_LOG_ERROR("Facebook account" << accountId << "has no valid account service, cannot perform refresh sync");
            identity->deleteLater();
            return;
        }

        QString method = accSrv->authData().method();
        QString mechanism = accSrv->authData().mechanism();
        SignOn::AuthSession *session = identity->createSession(method);
        if (!session) {
            SOCIALD_LOG_ERROR("could not create signon session for Facebook account" << accountId << "cannot perform refresh sync");
            accSrv->deleteLater();
            identity->deleteLater();
            return;
        }

        QVariantMap providedTokens;
        providedTokens.insert("AccessToken", accessToken);
        providedTokens.insert("RefreshToken", QString());
        providedTokens.insert("ExpiresIn", seconds);

        QVariantMap signonSessionData = accSrv->authData().parameters();
        signonSessionData.insert("ClientId", clientId());
        signonSessionData.insert("UiPolicy", SignOn::NoUserInteractionPolicy);
        signonSessionData.insert("ProvidedTokens", providedTokens);

        connect(session, SIGNAL(response(SignOn::SessionData)),
                this, SLOT(forceTokenExpiryResponse(SignOn::SessionData)),
                Qt::UniqueConnection);
        connect(session, SIGNAL(error(SignOn::Error)),
                this, SLOT(forceTokenExpiryError(SignOn::Error)),
                Qt::UniqueConnection);

        incrementSemaphore(accountId);
        session->setProperty("accountId", accountId);
        session->setProperty("seconds", seconds);
        session->process(SignOn::SessionData(signonSessionData), mechanism);
    }
}

void FacebookSignonSyncAdaptor::forceTokenExpiryResponse(const SignOn::SessionData &responseData)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    int accountId = session->property("accountId").toInt();
    int seconds = session->property("seconds").toInt();

    QVariantMap vmrd;
    foreach (const QString &key, responseData.propertyNames()) {
        vmrd.insert(key, responseData.getProperty(key));
    }

    SOCIALD_LOG_DEBUG("forcibly updated cache for Facebook account" << accountId << "," <<
                      "ExpiresIn now:" << vmrd.value("ExpiresIn").toInt() << ", expected" << seconds);

    if (seconds == 0) {
        // successfully forced expiry
        SOCIALD_LOG_ERROR("forced expiry for reportedly invalid token");
        raiseCredentialsNeedUpdateFlag(accountId);
    } else {
        // successfully forced new ExpiresIn value
        lowerCredentialsNeedUpdateFlag(accountId);
    }
    decrementSemaphore(accountId);
}

void FacebookSignonSyncAdaptor::forceTokenExpiryError(const SignOn::Error &error)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    int accountId = session->property("accountId").toInt();
    int seconds = session->property("seconds").toInt();

    SOCIALD_LOG_INFO("got signon error when performing force-expire for Facebook account" <<
                     accountId << ":" << error.type() << "," << error.message());

    if (seconds == 0) {
        // we treat the error as if it was a success, since we need to update the credentials anyway.
        SOCIALD_LOG_ERROR("forced expiry for reportedly invalid token failed");
        raiseCredentialsNeedUpdateFlag(accountId);
    } else {
        // don't raise or lower the flag.  If was previously not raised,
        // presumably it's because ExpiresIn hadn't reached zero.
    }
    decrementSemaphore(accountId);
}

