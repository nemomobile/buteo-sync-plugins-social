/*
 * Copyright (C) 2013 Jolla Ltd. <chris.adams@jollamobile.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "facebookdatatypesyncadaptor.h"
#include "facebooksyncadaptor.h"
#include "trace.h"

#include <QtCore/QVariantMap>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QByteArray>

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
#include <qjson/parser.h>
#else
#include <QJsonDocument>
#endif

//libaccounts-qt
#include <Accounts/Manager>
#include <Accounts/Service>
#include <Accounts/Account>
#include <Accounts/AccountService>
#include <Accounts/AuthData>

//libsignon-qt
#include <SignOn/Identity>
#include <SignOn/SessionData>
#include <SignOn/AuthSession>

Q_DECLARE_METATYPE(SignOn::Identity*)

FacebookDataTypeSyncAdaptor::FacebookDataTypeSyncAdaptor(SyncService *parent, FacebookSyncAdaptor *fbsa, SyncService::DataType dataType)
    : SocialNetworkSyncAdaptor(parent)
    , m_fbsa(fbsa)
    , m_dataType(dataType)
{
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
        return;
    }

    // three stage process.
    // 1) if an account has been removed, we need to purge the data we retrieved with it
    // 2) if an account has been added, we need to pull data for the account
    // 3) for existing accounts, pull new data for the existing account

    QList<int> newIds, purgeIds, updateIds;
    m_fbsa->checkAccounts(m_dataType, &newIds, &purgeIds, &updateIds);
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
        Accounts::Account *act = m_fbsa->m_accountManager->account(accountId);
        if (!act) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: existing account with id %1 couldn't be retrieved"))
                    .arg(accountId));
            continue;
        }

        // grab out a valid identity for the sync service.
        Accounts::ServiceList enabledSrvs = act->enabledServices();
        if (!enabledSrvs.size()) {
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("account with id %1 has no enabled sync service"))
                    .arg(accountId));
            continue;
        }

        quint32 identityId = 0;
        Accounts::AccountService *asrv = 0;
        for (int i = 0; i < enabledSrvs.size(); ++i) {
            asrv = new Accounts::AccountService(act, enabledSrvs.at(i));
            if (!asrv) {
                continue;
            }
            identityId = asrv->authData().credentialsId();
            if (identityId != 0) {
                break;
            }

            asrv->deleteLater();
            asrv = 0;
        }

        if (identityId == 0) {
            TRACE(SOCIALD_INFORMATION,
                    QString(QLatin1String("account with id %1 has no valid credentials"))
                    .arg(accountId));
            continue;
        }

        SignOn::Identity *ident = SignOn::Identity::existingIdentity(identityId);
        if (!ident) {
            TRACE(SOCIALD_ERROR,
                    QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved"))
                    .arg(accountId));
            continue;
        }

        // we need the access token to perform requests.
        // set UiPolicy to NO_USER_INTERACTION because we don't want
        // to show any UI if we don't already have a token.
        Accounts::AuthData authData(asrv->authData());
        asrv->deleteLater();
        SignOn::AuthSession *session = ident->createSession(authData.method());
        QVariantMap sessionData = authData.parameters();
        sessionData.insert(QLatin1String("UiPolicy"), SignOn::NoUserInteractionPolicy);
        QVariant identVar = QVariant::fromValue<SignOn::Identity*>(ident);
        session->setProperty("ident", identVar);
        session->setProperty("accountId", accountId);
        connect(session, SIGNAL(error(SignOn::Error)), this, SLOT(signOnError(SignOn::Error)));
        connect(session, SIGNAL(response(SignOn::SessionData)), this, SLOT(signOnResponse(SignOn::SessionData)));
        session->process(sessionData, authData.mechanism());
    }
}

void FacebookDataTypeSyncAdaptor::signOnError(const SignOn::Error &err)
{
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession *>(sender());
    TRACE(SOCIALD_ERROR,
            QString(QLatin1String("error: credentials for account with id %1 couldn't be retrieved:"))
            .arg(session->property("accountId").toInt()) << err.message());
    SignOn::Identity *ident = session->property("ident").value<SignOn::Identity*>();
    ident->destroySession(session); // XXX: is this safe?  Does it deleteLater()?
    ident->deleteLater();
}

void FacebookDataTypeSyncAdaptor::signOnResponse(const SignOn::SessionData &sdata)
{
    QVariantMap data;
    QStringList sdpns = sdata.propertyNames();
    foreach (const QString &sdpn, sdpns) {
        data.insert(sdpn, sdata.getProperty(sdpn));
    }    

    QString accessToken;
    SignOn::AuthSession *session = qobject_cast<SignOn::AuthSession *>(sender());
    int accountId = static_cast<int>(session->property("accountId").toUInt());

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

    SignOn::Identity *ident = session->property("ident").value<SignOn::Identity*>();
    ident->destroySession(session); // XXX: is this safe?  Does it deleteLater()?
    ident->deleteLater();

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
}

QVariantMap FacebookDataTypeSyncAdaptor::parseReplyData(const QByteArray &replyData, bool *ok)
{
    QVariant parsed;

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    QJson::Parser jsonParser;
    parsed = jsonParser.parse(replyData, ok);
#else
    QJsonDocument jsonDocument = QJsonDocument::fromJson(replyData);
    *ok = !doc.isEmpty();
    parsed = doc.toVariant();
#endif

    if (*ok && parsed.type() == QVariant::Map) {
        return parsed.toMap();
    }
    *ok = false;
    return QVariantMap();
}
