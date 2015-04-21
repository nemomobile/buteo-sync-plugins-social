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

#include "googlesignonsyncadaptor.h"
#include "trace.h"

#include <QtCore/QPair>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

GoogleSignonSyncAdaptor::GoogleSignonSyncAdaptor(QObject *parent)
    : GoogleDataTypeSyncAdaptor(SocialNetworkSyncAdaptor::Signon, parent)
{
    setInitialActive(true);
}

GoogleSignonSyncAdaptor::~GoogleSignonSyncAdaptor()
{
}

QString GoogleSignonSyncAdaptor::syncServiceName() const
{
    return QStringLiteral("google-sync"); // TODO: change name of service to google-signon!
}

void GoogleSignonSyncAdaptor::sync(const QString &dataTypeString, int accountId)
{
    // call superclass impl.
    GoogleDataTypeSyncAdaptor::sync(dataTypeString, accountId);
}

void GoogleSignonSyncAdaptor::purgeDataForOldAccount(int, SocialNetworkSyncAdaptor::PurgeMode)
{
    // Nothing to do.
}

void GoogleSignonSyncAdaptor::finalize(int)
{
    // nothing to do
}

void GoogleSignonSyncAdaptor::beginSync(int accountId, const QString &accessToken)
{
    Q_UNUSED(accessToken);
    refreshTokens(accountId);
}

Accounts::Account *GoogleSignonSyncAdaptor::loadAccount(int accountId)
{
    Accounts::Account *acc = 0;
    if (m_accounts.contains(accountId)) {
        acc = m_accounts[accountId];
    } else {
        acc = Accounts::Account::fromId(&m_accountManager, accountId, this);
        if (!acc) {
            SOCIALD_LOG_ERROR(
                    QString(QLatin1String("error: Google account %1 was deleted during signon refresh sync"))
                    .arg(accountId));
            return 0;
        } else {
            m_accounts.insert(accountId, acc);
        }
    }

    Accounts::Service srv = m_accountManager.service(syncServiceName());
    if (!srv.isValid()) {
        SOCIALD_LOG_ERROR(
                QString(QLatin1String("error: invalid service %1 specified for refresh sync with Google account: %2"))
                .arg(syncServiceName()).arg(accountId));
        return 0;
    }

    return acc;
}

void GoogleSignonSyncAdaptor::raiseCredentialsNeedUpdateFlag(int accountId)
{
    Accounts::Account *acc = loadAccount(accountId);
    if (acc) {
        SOCIALD_LOG_ERROR("GSSA: raising CredentialsNeedUpdate flag");
        Accounts::Service srv = m_accountManager.service(syncServiceName());
        acc->selectService(srv);
        acc->setValue(QStringLiteral("CredentialsNeedUpdate"), QVariant::fromValue<bool>(true));
        acc->setValue(QStringLiteral("CredentialsNeedUpdateFrom"), QVariant::fromValue<QString>(QString::fromLatin1("sociald-google-signon")));
        acc->selectService(Accounts::Service());
        acc->syncAndBlock();
    }
}

void GoogleSignonSyncAdaptor::lowerCredentialsNeedUpdateFlag(int accountId)
{
    Accounts::Account *acc = loadAccount(accountId);
    if (acc) {
        SOCIALD_LOG_ERROR("GSSA: lowering CredentialsNeedUpdate flag");
        Accounts::Service srv = m_accountManager.service(syncServiceName());
        acc->selectService(srv);
        acc->setValue(QStringLiteral("CredentialsNeedUpdate"), QVariant::fromValue<bool>(false));
        acc->remove(QStringLiteral("CredentialsNeedUpdateFrom"));
        acc->selectService(Accounts::Service());
        acc->syncAndBlock();
    }
}

void GoogleSignonSyncAdaptor::refreshTokens(int accountId)
{
    Accounts::Account *acc = loadAccount(accountId);
    if (!acc) {
        return;
    }

    // First perform a "normal" signon.  Then force token expiry.  Then signon to refresh the tokens.
    Accounts::Service srv(m_accountManager.service(syncServiceName()));
    acc->selectService(srv);
    SignOn::Identity *identity = acc->credentialsId() > 0 ? SignOn::Identity::existingIdentity(acc->credentialsId()) : 0;
    if (!identity) {
        SOCIALD_LOG_ERROR(
                QString(QLatin1String("error: Google account %1 has no valid credentials, cannot perform refresh sync"))
                .arg(accountId));
        return;
    }

    Accounts::AccountService *accSrv = new Accounts::AccountService(acc, srv);
    if (!accSrv) {
        SOCIALD_LOG_ERROR(
                QString(QLatin1String("error: Google account %1 has no valid account service, cannot perform refresh sync"))
                .arg(accountId));
        identity->deleteLater();
        return;
    }

    QString method = accSrv->authData().method();
    QString mechanism = accSrv->authData().mechanism();
    SignOn::AuthSession *session = identity->createSession(method);
    if (!session) {
        SOCIALD_LOG_ERROR(
                QString(QLatin1String("error: could not create signon session for Google account %1, cannot perform refresh sync"))
                .arg(accountId));
        accSrv->deleteLater();
        identity->deleteLater();
        return;
    }

    QVariantMap signonSessionData = accSrv->authData().parameters();
    signonSessionData.insert("ClientId", clientId());
    signonSessionData.insert("ClientSecret", clientSecret());
    signonSessionData.insert("UiPolicy", SignOn::NoUserInteractionPolicy);

    connect(session, SIGNAL(response(SignOn::SessionData)),
            this, SLOT(initialSignonResponse(SignOn::SessionData)),
            Qt::UniqueConnection);
    connect(session, SIGNAL(error(SignOn::Error)),
            this, SLOT(signonError(SignOn::Error)),
            Qt::UniqueConnection);

    incrementSemaphore(accountId);
    session->setProperty("accountId", accountId);
    session->setProperty("mechanism", mechanism);
    session->setProperty("signonSessionData", signonSessionData);
    m_idents.insert(accountId, identity);
    session->process(SignOn::SessionData(signonSessionData), mechanism);
}

void GoogleSignonSyncAdaptor::initialSignonResponse(const SignOn::SessionData &responseData)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    session->disconnect(this);

    if (syncAborted()) {
        // don't expire the tokens - we may have lost network connectivity
        // while we were attempting to perform signon sync, and that would
        // leave us in a position where we're unable to automatically recover.
        int accountId = session->property("accountId").toInt();
        SOCIALD_LOG_INFO("aborting signon sync refresh");
        decrementSemaphore(accountId);
        return;
    }

    connect(session, SIGNAL(response(SignOn::SessionData)),
            this, SLOT(forceTokenExpiryResponse(SignOn::SessionData)),
            Qt::UniqueConnection);
    connect(session, SIGNAL(error(SignOn::Error)),
            this, SLOT(signonError(SignOn::Error)),
            Qt::UniqueConnection);

    QString mechanism = session->property("mechanism").toString();
    QVariantMap signonSessionData = session->property("signonSessionData").toMap();

    // Now expire the tokens.
    QVariantMap providedTokens;
    providedTokens.insert("AccessToken", responseData.getProperty(QStringLiteral("AccessToken")).toString());
    providedTokens.insert("RefreshToken", responseData.getProperty(QStringLiteral("RefreshToken")).toString());
    providedTokens.insert("ExpiresIn", 2);
    signonSessionData.insert("ProvidedTokens", providedTokens);

    session->process(SignOn::SessionData(signonSessionData), mechanism);
}

void GoogleSignonSyncAdaptor::forceTokenExpiryResponse(const SignOn::SessionData &)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    session->disconnect(this);

    QString mechanism = session->property("mechanism").toString();
    QVariantMap signonSessionData = session->property("signonSessionData").toMap();

    QTimer *timer = new QTimer(this);
    timer->setInterval(4000);
    timer->setSingleShot(true);
    timer->setProperty("mechanism", mechanism);
    timer->setProperty("signonSessionData", signonSessionData);
    timer->setProperty("session", QVariant::fromValue<SignOn::AuthSession*>(session));
    connect(timer, SIGNAL(timeout()), this, SLOT(triggerRefresh()));
    timer->start();
}

void GoogleSignonSyncAdaptor::triggerRefresh()
{
    QTimer *timer = qobject_cast<QTimer*>(sender());
    timer->deleteLater();

    QString mechanism = timer->property("mechanism").toString();
    QVariantMap signonSessionData = timer->property("signonSessionData").toMap();

    SignOn::AuthSession *session = timer->property("session").value<SignOn::AuthSession*>();
    connect(session, SIGNAL(response(SignOn::SessionData)),
            this, SLOT(refreshTokenResponse(SignOn::SessionData)),
            Qt::UniqueConnection);
    connect(session, SIGNAL(error(SignOn::Error)),
            this, SLOT(signonError(SignOn::Error)),
            Qt::UniqueConnection);

    session->process(SignOn::SessionData(signonSessionData), mechanism);
}

void GoogleSignonSyncAdaptor::refreshTokenResponse(const SignOn::SessionData &responseData)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    int accountId = session->property("accountId").toInt();
    session->disconnect(this);

    SignOn::Identity *identity = m_idents.take(accountId);
    if (identity) {
        identity->destroySession(session);
        identity->deleteLater();
    } else {
        session->deleteLater();
    }

    SOCIALD_LOG_INFO(
            QString(QLatin1String("successfully performed signon refresh for Google account %1: new ExpiresIn: %3"))
            .arg(accountId).arg(responseData.getProperty("ExpiresIn").toInt()));

    lowerCredentialsNeedUpdateFlag(accountId);
    decrementSemaphore(accountId);
}

void GoogleSignonSyncAdaptor::signonError(const SignOn::Error &error)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession*>(sender());
    int accountId = session->property("accountId").toInt();
    session->disconnect(this);

    SignOn::Identity *identity = m_idents.take(accountId);
    if (identity) {
        identity->destroySession(session);
        identity->deleteLater();
    } else {
        session->deleteLater();
    }

    bool raiseFlag = error.type() == SignOn::Error::UserInteraction;
    SOCIALD_LOG_INFO(
            QString(QLatin1String("got signon error when performing signon refresh for Google account %1: %2: %3.  Raising flag? %4"))
            .arg(accountId).arg(error.type()).arg(error.message()).arg(raiseFlag));

    if (raiseFlag) {
        // UserInteraction error is returned if user interaction is required.
        raiseCredentialsNeedUpdateFlag(accountId);
    }

    decrementSemaphore(accountId);
}

